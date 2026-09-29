#include "timer.h"
#include "mmu.h" // Assuming this is your MMU header

Timer::Timer(MMU& mmu) : m_mmu(mmu), m_div_counter(0), m_tima_counter(1024) {}

int Timer::get_frequency_cycles() const {
    uint8_t tac = m_mmu.read(0xFF07);
    switch (tac & 0x03) {
        case 0: return 1024; // 4096 Hz
        case 1: return 16;   // 262144 Hz
        case 2: return 64;   // 65536 Hz
        case 3: return 256;  // 16384 Hz
    }
    return 1024;
}

void Timer::reset_div() {
    // When DIV is written to, reset both the register and the internal cycle counter
    m_mmu.write(0xFF04, 0x00);
    m_div_counter = 0;
}

void Timer::tick(int cycles) {
    // 1. Advance the DIV register (increments every 256 cycles)
    m_div_counter += cycles;
    if (m_div_counter >= 256) {
        m_div_counter -= 256;
        uint8_t current_div = m_mmu.read(0xFF04);
        m_mmu.write(0xFF04, current_div + 1); // Normal memory write, don't trigger reset
    }

    // 2. Check if TIMA is enabled (Bit 2 of TAC)
    uint8_t tac = m_mmu.read(0xFF07);
    if ((tac & (1 << 2)) != 0) {

        m_tima_counter -= cycles;

        // If enough cycles have passed to step the timer
        if (m_tima_counter <= 0) {
            // Reset the counter to the current frequency threshold
            m_tima_counter += get_frequency_cycles();

            uint8_t tima = m_mmu.read(0xFF05);

            if (tima == 0xFF) {
                // Timer Overflow!

                // Reload TIMA with the value in TMA (0xFF06)
                uint8_t tma = m_mmu.read(0xFF06);
                m_mmu.write(0xFF05, tma);

                // Request a Timer Interrupt (Bit 2 of IF at 0xFF0F)
                uint8_t if_reg = m_mmu.read(0xFF0F);
                m_mmu.write(0xFF0F, if_reg | (1 << 2));

            } else {
                // Normal increment
                m_mmu.write(0xFF05, tima + 1);
            }
        }
    }
}