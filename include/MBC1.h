#pragma once
#include "ICartridge.h"
#include <vector>

class MBC1 : public ICartridge {
private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram;

    bool m_ram_enabled = false;

    // Hardware registers
    uint8_t m_reg1 = 1; // 5-bit register (Lower ROM Bank bits)
    uint8_t m_reg2 = 0; // 2-bit register (Upper ROM Bank bits OR RAM Bank)
    uint8_t m_mode = 0; // 1-bit register (0 = ROM Banking, 1 = RAM Banking)

public:
    MBC1(const std::vector<uint8_t>& rom_data, size_t ram_size)
        : m_rom(rom_data), m_ram(ram_size, 0) {}

    uint8_t read(uint16_t address) override {
        if (address <= 0x3FFF) {
            // Hardware Quirk: In Mode 1, the fixed Bank 0 actually shifts!
            uint8_t bank_zero = (m_mode == 1) ? (m_reg2 << 5) : 0;
            uint32_t mapped_addr = (bank_zero * 0x4000) + address;
            return m_rom[mapped_addr % m_rom.size()];
        }
        else if (address >= 0x4000 && address <= 0x7FFF) {
            // The active ROM bank is ALWAYS a combination of reg2 and reg1
            uint8_t rom_bank = (m_reg2 << 5) | m_reg1;
            uint32_t mapped_addr = (rom_bank * 0x4000) + (address - 0x4000);
            return m_rom[mapped_addr % m_rom.size()];
        }
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram_enabled || m_ram.empty()) return 0xFF;
            // RAM bank is only affected by reg2 if in Mode 1
            uint8_t ram_bank = (m_mode == 1) ? m_reg2 : 0;
            uint32_t mapped_addr = (ram_bank * 0x2000) + (address - 0xA000);
            return m_ram[mapped_addr % m_ram.size()];
        }
        return 0xFF;
    }

    void write(uint16_t address, uint8_t value) override {
        if (address <= 0x1FFF) {
            m_ram_enabled = ((value & 0x0F) == 0x0A);
        }
        else if (address >= 0x2000 && address <= 0x3FFF) {
            // Register 1: Lower 5 bits. Cannot be 0.
            m_reg1 = value & 0x1F;
            if (m_reg1 == 0) m_reg1 = 1;
        }
        else if (address >= 0x4000 && address <= 0x5FFF) {
            // Register 2: Always updates the internal 2-bit value!
            m_reg2 = value & 0x03;
        }
        else if (address >= 0x6000 && address <= 0x7FFF) {
            // Mode Select
            m_mode = value & 0x01;
        }
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram_enabled || m_ram.empty()) return;
            uint8_t ram_bank = (m_mode == 1) ? m_reg2 : 0;
            uint32_t mapped_addr = (ram_bank * 0x2000) + (address - 0xA000);
            m_ram[mapped_addr % m_ram.size()] = value;
        }
    }

    uint8_t* get_sram_ptr() override { return m_ram.data(); }
    size_t get_sram_size() override { return m_ram.size(); }
};