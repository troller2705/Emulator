#pragma once
#include "ICartridge.h"
#include <vector>

class MBC5 : public ICartridge {
private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram;
    
    bool m_ram_enabled = false;
    uint16_t m_rom_bank = 1; // 9-bit value, requires uint16_t
    uint8_t m_ram_bank = 0;

public:
    MBC5(const std::vector<uint8_t>& rom_data, size_t ram_size) 
        : m_rom(rom_data), m_ram(ram_size, 0) {}

    uint8_t read(uint16_t address) override {
        if (address <= 0x3FFF) {
            return m_rom[address % m_rom.size()];
        } 
        else if (address >= 0x4000 && address <= 0x7FFF) {
            // No "Bank 0 becomes 1" quirk in MBC5!
            uint32_t mapped_addr = (m_rom_bank * 0x4000) + (address - 0x4000);
            return m_rom[mapped_addr % m_rom.size()];
        } 
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram_enabled || m_ram.empty()) return 0xFF;
            uint32_t mapped_addr = (m_ram_bank * 0x2000) + (address - 0xA000);
            return m_ram[mapped_addr % m_ram.size()];
        }
        return 0xFF;
    }

    void write(uint16_t address, uint8_t value) override {
        if (address <= 0x1FFF) {
            m_ram_enabled = ((value & 0x0F) == 0x0A);
        } 
        else if (address >= 0x2000 && address <= 0x2FFF) {
            // Lower 8 bits of the ROM bank
            m_rom_bank = (m_rom_bank & 0x0100) | value;
        } 
        else if (address >= 0x3000 && address <= 0x3FFF) {
            // 9th bit of the ROM bank
            m_rom_bank = (m_rom_bank & 0x00FF) | ((value & 0x01) << 8);
        } 
        else if (address >= 0x4000 && address <= 0x5FFF) {
            // RAM Bank (0x00-0x0F). 
            // Note: Carts with rumble motors use bit 3 here to trigger rumble!
            m_ram_bank = value & 0x0F;
        } 
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram_enabled || m_ram.empty()) return;
            uint32_t mapped_addr = (m_ram_bank * 0x2000) + (address - 0xA000);
            m_ram[mapped_addr % m_ram.size()] = value;
        }
    }

    uint8_t* get_sram_ptr() override { return m_ram.data(); }
    size_t get_sram_size() override { return m_ram.size(); }
};