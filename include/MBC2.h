#pragma once
#include "ICartridge.h"
#include <vector>

class MBC2 : public ICartridge {
private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram; // Built-in 512x4-bit RAM
    
    bool m_ram_enabled = false;
    uint8_t m_rom_bank = 1;

public:
    // Ignore the header's ram_size, MBC2 always has exactly 512 bytes internally
    MBC2(const std::vector<uint8_t>& rom_data, size_t /*ram_size*/) 
        : m_rom(rom_data), m_ram(512, 0) {}

    uint8_t read(uint16_t address) override {
        if (address <= 0x3FFF) {
            return m_rom[address % m_rom.size()];
        } 
        else if (address >= 0x4000 && address <= 0x7FFF) {
            uint32_t mapped_addr = (m_rom_bank * 0x4000) + (address - 0x4000);
            return m_rom[mapped_addr % m_rom.size()];
        } 
        else if (address >= 0xA000 && address <= 0xA1FF) {
            if (!m_ram_enabled) return 0xFF;
            // Real hardware only returns the bottom 4 bits; the top 4 are undefined (usually 1)
            return m_ram[address - 0xA000] | 0xF0; 
        }
        return 0xFF;
    }

    void write(uint16_t address, uint8_t value) override {
        if (address <= 0x3FFF) {
            // MBC2 Quirk: The 8th bit of the ADDRESS determines the register being written to!
            if ((address & 0x0100) == 0) {
                // Address bit 8 is 0: Enable/Disable RAM
                m_ram_enabled = ((value & 0x0F) == 0x0A);
            } else {
                // Address bit 8 is 1: Select ROM Bank
                m_rom_bank = value & 0x0F; // Only bottom 4 bits are used
                if (m_rom_bank == 0) m_rom_bank = 1;
            }
        } 
        else if (address >= 0xA000 && address <= 0xA1FF) {
            if (!m_ram_enabled) return;
            // Only the bottom 4 bits are physically saved in the chip
            m_ram[address - 0xA000] = value & 0x0F; 
        }
    }

    uint8_t* get_sram_ptr() override { return m_ram.data(); }
    size_t get_sram_size() override { return m_ram.size(); }
};