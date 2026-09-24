#pragma once
#include <cstdint>
#include <vector>
#include <array>

#include <ppu.h>

class MMU {
public:
    MMU();

    PPU* get_ppu() { return &m_ppu; }
    
    // The two most important functions in the emulator
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    
    // Pass the ROM data from the Core to the MMU
    void load_rom(const std::vector<uint8_t>& rom_data);

private:
    std::vector<uint8_t> m_rom;
    std::array<uint8_t, 160> m_oam;

    PPU m_ppu; // Declare the PPU instance here

    int m_current_rom_bank = 1;
    
    // The physical RAM chips inside the console
    std::array<uint8_t, 0x2000> m_vram; // 8KB Video RAM
    std::array<uint8_t, 0x2000> m_wram; // 8KB Work RAM
    std::array<uint8_t, 0x2000> m_sram; // 8KB Cartridge RAM
    std::array<uint8_t, 0x80>   m_hram; // 127 bytes High RAM

    uint8_t m_if = 0; // 0xFF0F - Interrupt Flag
    uint8_t m_ie = 0; // 0xFFFF - Interrupt Enable
};