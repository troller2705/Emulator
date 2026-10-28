#include "mmu.h"
#include "MBC1.h"
#include "MBC2.h"
#include "MBC3.h"
#include "MBC5.h"
#include "ROMOnly.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include "timer.h"

MMU::MMU() {
    for (auto& bank : m_vram) {
        bank.fill(0);
    }
    for (auto& bank : m_wram) {
        bank.fill(0);
    }
    m_hram.fill(0);
    m_oam.fill(0);
}

void MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0x0150) return;

    // Check if the game supports or requires CGB
    uint8_t cgb_flag = rom_data[0x0143];
    m_is_cgb = (cgb_flag == 0x80 || cgb_flag == 0xC0);

    // Parse the RAM size byte from the cartridge header
    size_t ram_size = 0;
    switch (rom_data[0x0149]) {
        case 2: ram_size = 0x2000; break;  // 8KB
        case 3: ram_size = 0x8000; break;  // 32KB
        case 4: ram_size = 0x20000; break; // 128KB
        case 5: ram_size = 0x10000; break; // 64KB
    }

    uint8_t cart_type = rom_data[0x0147];

    if (cart_type == 0x00 || cart_type == 0x08 || cart_type == 0x09) {
        m_cart = std::make_unique<ROMOnly>(rom_data, ram_size);
        std::cout << "Mapper loaded: ROM Only\n";
    }
    else if (cart_type >= 0x01 && cart_type <= 0x03) {
        m_cart = std::make_unique<MBC1>(rom_data, ram_size);
        std::cout << "Mapper loaded: MBC1\n";
    }
    else if (cart_type == 0x05 || cart_type == 0x06) {
        m_cart = std::make_unique<MBC2>(rom_data, ram_size);
        std::cout << "Mapper loaded: MBC2\n";
    }
    else if (cart_type >= 0x0F && cart_type <= 0x13) {
        m_cart = std::make_unique<MBC3>(rom_data, ram_size);
        std::cout << "Mapper loaded: MBC3\n";
    }
    else if (cart_type >= 0x19 && cart_type <= 0x1E) {
        m_cart = std::make_unique<MBC5>(rom_data, ram_size);
        std::cout << "Mapper loaded: MBC5\n";
    }
    else {
        std::cout << "Warning: Unsupported mapper type (0x" << std::hex << (int)cart_type << ")\n";
    }
}

void MMU::load_battery(const std::string& save_path) {
    if (!m_cart || m_cart->get_sram_size() == 0) return;

    std::ifstream file(save_path, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(m_cart->get_sram_ptr()), m_cart->get_sram_size());
        std::cout << "Loaded save file: " << save_path << "\n";
    } else {
        std::cout << "No existing save file found. Starting fresh.\n";
    }
}

void MMU::save_battery(const std::string& save_path) {
    if (!m_cart || m_cart->get_sram_size() == 0) return;

    std::ofstream file(save_path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(m_cart->get_sram_ptr()), m_cart->get_sram_size());
        std::cout << "Saved game to: " << save_path << "\n";
    } else {
        std::cerr << "Failed to create save file!\n";
    }
}

uint8_t MMU::read(uint16_t address) {
    // --- DELEGATE TO CARTRIDGE ---
    if (address <= 0x7FFF || (address >= 0xA000 && address <= 0xBFFF)) {
        return m_cart ? m_cart->read(address) : 0xFF;
    }

    // --- INTERNAL MEMORY ROUTING ---
    if (address == 0xFF00) {
        uint8_t val = m_joypad_select | 0xCF;
        if ((m_joypad_select & 0x10) == 0) {
            val &= (m_joypad_state >> 4) | 0xF0;
        }
        if ((m_joypad_select & 0x20) == 0) {
            val &= (m_joypad_state & 0x0F) | 0xF0;
        }
        return val;
    }

    if (address == 0xFF0F) return m_if | 0xE0;
    if (address == 0xFFFF) return m_ie;

    if ((address >= 0xFF40 && address <= 0xFF4B) || (address >= 0xFF68 && address <= 0xFF6B)) {
        return m_ppu.read_register(address);
    }

    if (address >= 0xFF51 && address <= 0xFF54) return 0xFF; // These registers are write-only
    if (address == 0xFF55) return m_hdma5;

    if (address >= 0xFF04 && address <= 0xFF07) {
        return m_timer ? m_timer->read_register(address) : 0xFF;
    }

    if (address >= 0x8000 && address <= 0x9FFF) {
        uint8_t bank = m_vbk & 0x01;
        return m_vram[bank][address - 0x8000];
    }

    // Read VBK Register
    if (address == 0xFF4F) return m_vbk | 0xFE; // Top 7 bits are always 1

    if (address >= 0xC000 && address <= 0xCFFF) {
        return m_wram[0][address - 0xC000];
    }
    if (address >= 0xD000 && address <= 0xDFFF) {
        uint8_t bank = m_svbk & 0x07;
        if (bank == 0) bank = 1;
        return m_wram[bank][address - 0xD000];
    }

    // Read SVBK Register
    if (address == 0xFF70) return m_svbk | 0xF8; // Top 5 bits are always 1

    if (address >= 0xFF80 && address <= 0xFFFE) {
        return m_hram[address - 0xFF80];
    }
    if (address >= 0xFE00 && address <= 0xFE9F) {
        return m_oam[address - 0xFE00];
    }
    if (address >= 0xFF10 && address <= 0xFF3F) {
        return m_apu.read_register(address);
    }

    if (address == 0xFF4D) return m_key1 | 0x7E;

    return 0xFF;
}

void MMU::write(uint16_t address, uint8_t value) {
    // --- DELEGATE TO CARTRIDGE ---
    if (address <= 0x7FFF || (address >= 0xA000 && address <= 0xBFFF)) {
        if (m_cart) m_cart->write(address, value);
        return;
    }

    // --- INTERNAL MEMORY ROUTING ---
    if (address == 0xFF00) {
        m_joypad_select = value & 0x30;
        return;
    }

    if (address == 0xFF0F) { m_if = value; return; }
    if (address == 0xFFFF) { m_ie = value; return; }

    if (address >= 0xFF04 && address <= 0xFF07) {
        if (m_timer) m_timer->write_register(address, value);
        return;
    }

    if (address == 0xFF46) {
        uint16_t source_base = static_cast<uint16_t>(value) << 8;
        for (int i = 0; i < 160; i++) {
            uint8_t b = read(source_base + i);
            write(0xFE00 + i, b);
        }
        return;
    }

    if ((address >= 0xFF40 && address <= 0xFF4B) || (address >= 0xFF68 && address <= 0xFF6B)) {
        m_ppu.write_register(address, value);
        return;
    }

    if (address >= 0xFF10 && address <= 0xFF3F) {
        m_apu.write_register(address, value);
        return;
    }

    if (address >= 0x8000 && address <= 0x9FFF) {
        uint8_t bank = m_vbk & 0x01;
        m_vram[bank][address - 0x8000] = value;
        return;
    }

    // Write VBK Register
    if (address == 0xFF4F) {
        if (m_is_cgb) m_vbk = value & 0x01;
        return;
    }

    if (address == 0xFF4D) {
        // Bit 7 is hardware read-only. We only allow the game to change Bit 0.
        m_key1 = (m_key1 & 0x80) | (value & 0x01);
        return;
    }

    if (address >= 0xC000 && address <= 0xCFFF) {
        m_wram[0][address - 0xC000] = value;
        return;
    }
    if (address >= 0xD000 && address <= 0xDFFF) {
        uint8_t bank = m_svbk & 0x07;
        if (bank == 0) bank = 1;
        m_wram[bank][address - 0xD000] = value;
        return;
    }

    // 0xFF51: HDMA1 (Source High)
    if (address == 0xFF51) {
        m_hdma_src = (m_hdma_src & 0x00FF) | (value << 8);
        return;
    }
    // 0xFF52: HDMA2 (Source Low) - Lower 4 bits are hardware-masked to 0
    if (address == 0xFF52) {
        m_hdma_src = (m_hdma_src & 0xFF00) | (value & 0xF0);
        return;
    }
    // 0xFF53: HDMA3 (Dest High) - Upper 3 bits are ignored, mapped strictly to VRAM (0x8000)
    if (address == 0xFF53) {
        m_hdma_dst = (m_hdma_dst & 0x00FF) | (((value & 0x1F) << 8) | 0x8000);
        return;
    }
    // 0xFF54: HDMA4 (Dest Low) - Lower 4 bits are hardware-masked to 0
    if (address == 0xFF54) {
        m_hdma_dst = (m_hdma_dst & 0xFF00) | (value & 0xF0);
        return;
    }
    // 0xFF55: HDMA5 (Length/Mode/Start)
    if (address == 0xFF55) {
        bool is_hdma = (value & 0x80) != 0;
        uint8_t length_blocks = (value & 0x7F) + 1; // Length is expressed in 16-byte blocks
        uint16_t transfer_length = length_blocks * 16;

        if (m_hdma_active && !is_hdma) {
            // Writing a 0 to bit 7 while an HDMA is active cancels it.
            m_hdma_active = false;
            m_hdma5 = (m_hdma5 & 0x7F) | 0x80; // Set bit 7 to 1 to indicate stopped state
        } else if (is_hdma) {
            // Bit 7 is 1: Start H-Blank DMA
            m_hdma5 = value & 0x7F; // Clear bit 7 to indicate active
            m_hdma_active = true;
        } else {
            // Bit 7 is 0: Start General DMA (Immediate transfer)
            perform_dma_copy(transfer_length);
            m_hdma5 = 0xFF; // Transfer completes instantly
            m_hdma_active = false;
        }
        return;
    }

    // Write SVBK Register
    if (address == 0xFF70) {
        if (m_is_cgb) m_svbk = value & 0x07;
        return;
    }
    if (address >= 0xFF80 && address <= 0xFFFE) {
        m_hram[address - 0xFF80] = value;
    }
    if (address >= 0xFE00 && address <= 0xFE9F) {
        m_oam[address - 0xFE00] = value;
    }
}

void MMU::set_joypad_state(uint8_t new_state) {
    bool request_interrupt = false;
    if ((m_joypad_select & 0x10) == 0) {
        if ((m_joypad_state & ~new_state) & 0xF0) request_interrupt = true;
    }
    if ((m_joypad_select & 0x20) == 0) {
        if ((m_joypad_state & ~new_state) & 0x0F) request_interrupt = true;
    }

    m_joypad_state = new_state;

    if (request_interrupt) {
        m_if |= 0x10;
    }
}

void MMU::perform_dma_copy(uint16_t length) {
    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte = read(m_hdma_src);
        write(m_hdma_dst, byte);
        m_hdma_src++;
        m_hdma_dst++;
    }
}

void MMU::step_hdma() {
    if (!m_hdma_active) return;

    // HDMA transfers exactly 16 bytes per H-Blank
    perform_dma_copy(16);

    // Decrement the length (lower 7 bits of HDMA5)
    uint8_t blocks_remaining = m_hdma5 & 0x7F;

    if (blocks_remaining == 0) {
        // Transfer complete: mark inactive and set HDMA5 to 0xFF
        m_hdma_active = false;
        m_hdma5 = 0xFF;
    } else {
        // Update HDMA5 with the new decremented block count
        m_hdma5 = (m_hdma5 & 0x80) | (blocks_remaining - 1);
    }
}