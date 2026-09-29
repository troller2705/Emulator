#pragma once
#include <cstdint>
#include <vector>
#include <array>

#include <ppu.h>
#include <string>

class Timer; // 1. Forward declare the Timer class here

class MMU {
public:
    MMU();

    PPU* get_ppu() { return &m_ppu; }

    // The two most important functions in the emulator
    uint8_t read(uint16_t address);
    void write(uint16_t address, uint8_t value);

    // Pass the ROM data from the Core to the MMU
    void load_rom(const std::vector<uint8_t>& rom_data);

    void set_joypad_state(uint8_t new_state);

    // 2. Add the link function so the application layer can connect them
    void link_timer(Timer* timer) {
        m_timer = timer;
    }

    void load_battery(const std::string& save_path);
    void save_battery(const std::string& save_path);

private:
    Timer* m_timer = nullptr; // 3. Add the internal pointer

    std::vector<uint8_t> m_rom;
    std::array<uint8_t, 160> m_oam;

    PPU m_ppu; // Declare the PPU instance here

    int m_current_rom_bank = 1;
    
    // The physical RAM chips inside the console
    std::array<uint8_t, 0x2000> m_vram; // 8KB Video RAM
    std::array<uint8_t, 0x2000> m_wram; // 8KB Work RAM
    std::array<uint8_t, 0x8000> m_sram; // 32KB Cartridge RAM
    std::array<uint8_t, 0x80>   m_hram; // 127 bytes High RAM

    uint8_t m_mbc1_mode = 0;
    uint8_t m_current_ram_bank = 0;
    bool m_sram_enabled = false;

    uint8_t m_if = 0; // 0xFF0F - Interrupt Flag
    uint8_t m_ie = 0; // 0xFFFF - Interrupt Enable
    uint8_t m_div = 0;  // 0xFF04
    uint8_t m_tima = 0; // 0xFF05
    uint8_t m_tma = 0;  // 0xFF06
    uint8_t m_tac = 0;  // 0xFF07

    uint8_t m_joypad_select = 0xCF;
    uint8_t m_joypad_state = 0xFF; // 0 = pressed, 1 = unpressed
};