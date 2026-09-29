#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include "ppu.h"
#include "apu.h"
#include "ICartridge.h"

class Timer; // 1. Forward declare the Timer class here

class MMU {
public:
    MMU();

    PPU* get_ppu() { return &m_ppu; }
    const PPU* get_ppu() const { return &m_ppu; }
    APU* get_apu() { return &m_apu; }
    const APU* get_apu() const { return &m_apu; }

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
    Timer* m_timer = nullptr;
    PPU m_ppu;
    APU m_apu;

    std::unique_ptr<ICartridge> m_cart = nullptr; // REPLACES m_rom and m_sram!

    std::array<uint8_t, 160> m_oam;
    std::array<uint8_t, 0x2000> m_vram;
    std::array<uint8_t, 0x2000> m_wram;
    std::array<uint8_t, 0x80>   m_hram;

    // Timer and Interrupt registers
    uint8_t m_if = 0;
    uint8_t m_ie = 0;

    uint8_t m_joypad_select = 0xCF;
    uint8_t m_joypad_state = 0xFF;
};