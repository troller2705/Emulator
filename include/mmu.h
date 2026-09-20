#pragma once
#include <cstdint>
#include <vector>
#include <array>

class MMU {
public:
    MMU();
    
    // The two most important functions in the emulator
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);
    
    // Pass the ROM data from the Core to the MMU
    void load_rom(const std::vector<uint8_t>& rom_data);

private:
    std::vector<uint8_t> m_rom;
    
    // The physical RAM chips inside the console
    std::array<uint8_t, 0x2000> m_vram; // 8KB Video RAM
    std::array<uint8_t, 0x2000> m_wram; // 8KB Work RAM
    std::array<uint8_t, 0x80>   m_hram; // 127 bytes High RAM
};