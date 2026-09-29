#pragma once
#include "ICartridge.h"
#include <vector>

class MBC3 : public ICartridge {
private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_ram;
    
    bool m_ram_rtc_enabled = false;
    uint8_t m_rom_bank = 1;
    uint8_t m_ram_bank = 0; // 0x00-0x03 for RAM, 0x08-0x0C for RTC

public:
    MBC3(const std::vector<uint8_t>& rom_data, size_t ram_size) 
        : m_rom(rom_data), m_ram(ram_size, 0) {}

    uint8_t read(uint16_t address) override {
        if (address <= 0x3FFF) {
            // MBC3 always maps Bank 0 statically
            return m_rom[address % m_rom.size()];
        } 
        else if (address >= 0x4000 && address <= 0x7FFF) {
            uint32_t mapped_addr = (m_rom_bank * 0x4000) + (address - 0x4000);
            return m_rom[mapped_addr % m_rom.size()];
        } 
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram_rtc_enabled) return 0xFF;
            
            // Values 0x00-0x03 map to physical SRAM
            if (m_ram_bank <= 0x03 && !m_ram.empty()) {
                uint32_t mapped_addr = (m_ram_bank * 0x2000) + (address - 0xA000);
                return m_ram[mapped_addr % m_ram.size()];
            } 
            // Values 0x08-0x0C map to RTC registers
            else if (m_ram_bank >= 0x08 && m_ram_bank <= 0x0C) {
                return 0x00; // Stubbed: Pokémon Red ignores this
            }
        }
        return 0xFF;
    }

    void write(uint16_t address, uint8_t value) override {
        if (address <= 0x1FFF) {
            m_ram_rtc_enabled = ((value & 0x0F) == 0x0A);
        } 
        else if (address >= 0x2000 && address <= 0x3FFF) {
            // MBC3 updates all 7 bits of the ROM bank directly
            uint8_t bank = value & 0x7F;
            if (bank == 0) bank = 1; 
            m_rom_bank = bank;
        } 
        else if (address >= 0x4000 && address <= 0x5FFF) {
            // Selects either the RAM bank or RTC register
            m_ram_bank = value;
        } 
        else if (address >= 0x6000 && address <= 0x7FFF) {
            // Latch Clock Data (Stubbed for Pokémon Red)
        } 
        else if (address >= 0xA000 && address <= 0xBFFF) {
            if (!m_ram_rtc_enabled) return;
            
            if (m_ram_bank <= 0x03 && !m_ram.empty()) {
                uint32_t mapped_addr = (m_ram_bank * 0x2000) + (address - 0xA000);
                m_ram[mapped_addr % m_ram.size()] = value;
            } 
        }
    }

    uint8_t* get_sram_ptr() override { return m_ram.data(); }
    size_t get_sram_size() override { return m_ram.size(); }
};