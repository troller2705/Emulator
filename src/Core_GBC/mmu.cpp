#include "mmu.h"
#include <iostream>
#include <cstdlib>

MMU::MMU() {
    m_vram.fill(0);
    m_wram.fill(0);
    m_hram.fill(0);
    m_oam.fill(0);
    m_sram.fill(0);
}

void MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    m_rom = rom_data;
}

uint8_t MMU::read(uint16_t address) {
    if (address == 0xFF0F) return m_if | 0xE0; // Top 3 bits are always 1
    if (address == 0xFFFF) return m_ie;
    if (address >= 0xFF40 && address <= 0xFF4B) {
        return m_ppu.read_register(address);
    }
    if (address <= 0x3FFF) {
        // ROM Bank 00 (Fixed - Always points to the start of the ROM)
        if (address < m_rom.size()) {
            return m_rom[address];
        } else {
            return 0xFF;
        }
    }
    else if (address >= 0x4000 && address <= 0x7FFF) {
        // ROM Bank 01-7F (Switchable)
        uint32_t offset = address - 0x4000;
        uint32_t real_address = (m_current_rom_bank * 0x4000) + offset;

        if (real_address < m_rom.size()) {
            return m_rom[real_address];
        } else {
            return 0xFF;
        }
    }
    else if (address >= 0x8000 && address <= 0x9FFF) {
        return m_vram[address - 0x8000];
    }
    else if (address >= 0xA000 && address <= 0xBFFF) {
        return m_sram[address - 0xA000];
    }
    else if (address >= 0xC000 && address <= 0xDFFF) {
        return m_wram[address - 0xC000];
    }
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        return m_hram[address - 0xFF80];
    }
    else if (address >= 0xFE00 && address <= 0xFE9F) {
        return m_oam[address - 0xFE00];
    }

    // --- TEMPORARY VBLANK HACK ---
    if (address == 0xFF44) {
        return rand() % 154;
    }

    return 0xFF; 
}

void MMU::write(uint16_t address, uint8_t value) {
    if (address == 0xFF0F) { m_if = value; return; }
    if (address == 0xFFFF) { m_ie = value; return; }
    if (address == 0xFF46) {
        // Start DMA transfer from source address (value * 0x100) to OAM (0xFE00)
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
    if (address <= 0x7FFF) {
        // MBC1 Bank Switching
        if (address >= 0x2000 && address <= 0x3FFF) {
            // Set the lower 5 bits of the ROM bank
            uint8_t lower_5 = value & 0x1F;
            if (lower_5 == 0) lower_5 = 1; // Hardware quirk: Bank 0 becomes 1

            // Preserve the top bits, replace the bottom 5
            m_current_rom_bank = (m_current_rom_bank & 0x60) | lower_5;
        }
        else if (address >= 0x4000 && address <= 0x5FFF) {
            // Set the upper 2 bits of the ROM bank (Bits 5 and 6)
            uint8_t upper_2 = value & 0x03;

            // Preserve the bottom 5 bits, replace the top 2
            m_current_rom_bank = (m_current_rom_bank & 0x1F) | (upper_2 << 5);
        }
        return;
    }
    else if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
    }
    else if (address >= 0xA000 && address <= 0xBFFF) {
        m_sram[address - 0xA000] = value;
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