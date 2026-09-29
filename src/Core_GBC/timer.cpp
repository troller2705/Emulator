#include "timer.h"
#include "mmu.h"

Timer::Timer(MMU& mmu) : m_mmu(mmu) {}

int Timer::get_frequency_cycles() const {
    switch (m_tac & 0x03) {
        case 0: return 1024; // 4096 Hz
        case 1: return 16;   // 262144 Hz
        case 2: return 64;   // 65536 Hz
        case 3: return 256;  // 16384 Hz
    }
    return 1024;
}

uint8_t Timer::read_register(uint16_t address) const {
    switch (address) {
        case 0xFF04: return m_div;
        case 0xFF05: return m_tima;
        case 0xFF06: return m_tma;
        case 0xFF07: return m_tac;
    }
    return 0xFF;
}

void Timer::write_register(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF04:
            // The CPU writing ANY value to DIV resets it to 0
            m_div = 0;
            m_div_counter = 0;
            break;
        case 0xFF05: m_tima = value; break;
        case 0xFF06: m_tma = value; break;
        case 0xFF07: m_tac = value; break;
    }
}

void Timer::tick(int cycles) {
    // 1. Advance the DIV register
    m_div_counter += cycles;
    if (m_div_counter >= 256) {
        m_div_counter -= 256;
        m_div++; // Internal increment, bypasses the MMU!
    }

    // 2. Check if TIMA is enabled (Bit 2 of TAC)
    if ((m_tac & (1 << 2)) != 0) {
        m_tima_counter -= cycles;

        if (m_tima_counter <= 0) {
            m_tima_counter += get_frequency_cycles();

            if (m_tima == 0xFF) {
                m_tima = m_tma; // Timer Overflow!

                // Request a Timer Interrupt from the MMU (Bit 2 of IF)
                uint8_t if_reg = m_mmu.read(0xFF0F);
                m_mmu.write(0xFF0F, if_reg | (1 << 2));
            } else {
                m_tima++;
            }
        }
    }
}