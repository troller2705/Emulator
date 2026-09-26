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
    if (address == 0xFF00) {
        uint8_t val = m_joypad_select | 0xCF; // Top bits are always 1
        if ((m_joypad_select & 0x10) == 0) { // Direction selected
            val &= (m_joypad_state >> 4) | 0xF0;
        }
        if ((m_joypad_select & 0x20) == 0) { // Action Buttons selected
            val &= (m_joypad_state & 0x0F) | 0xF0;
        }
        return val;
    }
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
        if (!m_sram_enabled) return 0xFF;
        if (m_current_ram_bank <= 0x03) {
            uint32_t offset = (m_current_ram_bank * 0x2000) + (address - 0xA000);
            return m_sram[offset];
        }
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
    if (address == 0xFF00) {
        // Only bits 4 and 5 are writable by the CPU
        m_joypad_select = value & 0x30;
        return;
    }
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
        // --- MBC3 BANK SWITCHING ---
        if (address <= 0x1FFF) {
            // Enable / Disable SRAM and RTC
            m_sram_enabled = ((value & 0x0F) == 0x0A);
        }
        else if (address >= 0x2000 && address <= 0x3FFF) {
            // MBC3 writes all 7 bits of the ROM bank at once!
            uint8_t bank = value & 0x7F;
            if (bank == 0) bank = 1; // Hardware quirk: Bank 0 becomes 1
            m_current_rom_bank = bank;
        }
        else if (address >= 0x4000 && address <= 0x5FFF) {
            // RAM Bank OR Real-Time Clock (RTC) Register Select
            if (value <= 0x03) {
                m_current_ram_bank = value;
            } else if (value >= 0x08 && value <= 0x0C) {
                // RTC register selected (Stub this out for now)
            }
        }
        else if (address >= 0x6000 && address <= 0x7FFF) {
            // Latch Clock Data (Stub this out for now)
        }
        return;
    }
    else if (address >= 0x8000 && address <= 0x9FFF) {
        m_vram[address - 0x8000] = value;
    }
    else if (address >= 0xA000 && address <= 0xBFFF) {
        if (!m_sram_enabled) return;

        // In MBC3, values 0-3 point to physical SRAM.
        // (Values 0x08-0x0C point to the RTC registers, which we ignore)
        if (m_current_ram_bank <= 0x03) {
            uint32_t offset = (m_current_ram_bank * 0x2000) + (address - 0xA000);
            m_sram[offset] = value;
        }
        return;
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

    // Check if a directional button transitioned from unpressed (1) to pressed (0)
    if ((m_joypad_select & 0x10) == 0) {
        if ((m_joypad_state & ~new_state) & 0xF0) request_interrupt = true;
    }
    // Check if an action button transitioned from unpressed (1) to pressed (0)
    if ((m_joypad_select & 0x20) == 0) {
        if ((m_joypad_state & ~new_state) & 0x0F) request_interrupt = true;
    }

    m_joypad_state = new_state;

    // Trigger Joypad Interrupt (Bit 4 of IF) to wake the CPU from Stop/Halt
    if (request_interrupt) {
        m_if |= 0x10;
    }
}