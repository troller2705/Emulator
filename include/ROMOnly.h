#pragma once
#include "ICartridge.h"
#include <vector>

class ROMOnly : public ICartridge {
private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram; // Some ROM Only carts had 8KB of RAM

public:
    ROMOnly(const std::vector<uint8_t>& rom_data, size_t ram_size) 
        : m_rom(rom_data), m_ram(ram_size, 0) {}

    uint8_t read(uint16_t address) override {
        if (address <= 0x7FFF) {
            return m_rom[address % m_rom.size()];
        } 
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (m_ram.empty()) return 0xFF;
            return m_ram[(address - 0xA000) % m_ram.size()];
        }
        return 0xFF;
    }

    void write(uint16_t address, uint8_t value) override {
        // ROM is read-only, so we only handle RAM writes
        if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram.empty()) {
                m_ram[(address - 0xA000) % m_ram.size()] = value;
            }
        }
    }

    uint8_t* get_sram_ptr() override { return m_ram.data(); }
    size_t get_sram_size() override { return m_ram.size(); }
};