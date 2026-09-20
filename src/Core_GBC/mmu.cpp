#include "mmu.h"
#include <iostream>

MMU::MMU() {
    m_vram.fill(0);
    m_wram.fill(0);
    m_hram.fill(0);
}

void MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    m_rom = rom_data;
}

uint8_t MMU::read(uint16_t address) {
    if (address <= 0x7FFF) {
        // ROM Data. 
        // (Note: We will add MBC banking logic here later to support the full 1MB).
        // SAFETY CHECK: Ensure we don't read past the end of the loaded ROM file
        if (address < m_rom.size()) {
            return m_rom[address];
        } else {
            return 0xFF; // Return default value if accessing out of bounds
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
    
    // Open bus behavior: Real Game Boys usually return 0xFF for unmapped memory
    return 0xFF; 
}

void MMU::write(uint16_t address, uint8_t value) {
    if (address <= 0x7FFF) {
        // The game is trying to send a command to the MBC chip!
        // We will handle this when we build the MBC class.
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
    // Other hardware writes (like playing a sound or changing the joypad state) 
    // will be routed here later.
}