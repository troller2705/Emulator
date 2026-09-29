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
    m_vram.fill(0);
    m_wram.fill(0);
    m_hram.fill(0);
    m_oam.fill(0);
}

void MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0x0150) return;

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

    if (address >= 0xFF40 && address <= 0xFF4B) {
        return m_ppu.read_register(address);
    }

    if (address >= 0xFF04 && address <= 0xFF07) {
        return m_timer ? m_timer->read_register(address) : 0xFF;
    }

    if (address >= 0x8000 && address <= 0x9FFF) {
        return m_vram[address - 0x8000];
    }
    if (address >= 0xC000 && address <= 0xDFFF) {
        return m_wram[address - 0xC000];
    }
    if (address >= 0xFF80 && address <= 0xFFFE) {
        return m_hram[address - 0xFF80];
    }
    if (address >= 0xFE00 && address <= 0xFE9F) {
        return m_oam[address - 0xFE00];
    }
    if (address >= 0xFF10 && address <= 0xFF3F) {
        return m_apu.read_register(address);
    }

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

    if (address >= 0xFF40 && address <= 0xFF4B) {
        m_ppu.write_register(address, value);
        return;
    }
    if (address >= 0xFF10 && address <= 0xFF3F) {
        m_apu.write_register(address, value);
        return;
    }

    if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
    }
    else if (address >= 0xC000 && address <= 0xDFFF) {
        m_wram[address - 0xC000] = value;
    }
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        m_hram[address - 0xFF80] = value;
    }
    else if (address >= 0xFE00 && address <= 0xFE9F) {
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