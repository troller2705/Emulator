#include "mmu.h"
#include <iostream>
#include <cstdlib>

MMU::MMU() {
    m_vram.fill(0);
    m_wram.fill(0);
    m_hram.fill(0);
}

void MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    m_rom = rom_data;
}

uint8_t MMU::read(uint16_t address) {
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
    else if (address >= 0xC000 && address <= 0xDFFF) {
        return m_wram[address - 0xC000];
    }
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        return m_hram[address - 0xFF80];
    }

    // --- TEMPORARY VBLANK HACK ---
    if (address == 0xFF44) {
        return rand() % 154;
    }

    return 0xFF; 
}

void MMU::write(uint16_t address, uint8_t value) {
    if (address <= 0x7FFF) {
        // MBC1 Bank Switching: Writes to 0x2000 - 0x3FFF change the ROM bank
        if (address >= 0x2000 && address <= 0x3FFF) {
            // MBC1 only looks at the bottom 5 bits for this register
            m_current_rom_bank = value & 0x1F;

            // Hardware quirk: Bank 0 is automatically converted to Bank 1
            if (m_current_rom_bank == 0) {
                m_current_rom_bank = 1;
            }
        }
        // Even if we don't handle other MBC registers yet, we MUST return here
        // so we don't accidentally write to read-only memory!
        return;
    }
    else if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
    }
    else if (address >= 0xC000 && address <= 0xDFFF) {
        m_wram[address - 0xC000] = value;
    }
    else if (address >= 0xFF80 && address <= 0xFFFE) {
        m_hram[address - 0xFF80] = value;
    }
}