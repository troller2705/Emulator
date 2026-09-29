#pragma once
#include <cstdint>

class MMU;

class Timer {
private:
    MMU& m_mmu;
    int m_div_counter = 0;
    int m_tima_counter = 1024;

    // Timer owns its own memory registers now
    uint8_t m_div = 0;
    uint8_t m_tima = 0;
    uint8_t m_tma = 0;
    uint8_t m_tac = 0;

    int get_frequency_cycles() const;

public:
    Timer(MMU& mmu);

    void tick(int cycles);

    // Standardized hardware interface
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);
};