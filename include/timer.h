#pragma once
#include <cstdint>

class MMU; // Forward declaration to avoid circular includes

class Timer {
private:
    MMU& m_mmu;
    int m_div_counter;
    int m_tima_counter;

    // Helper to get the current cycle threshold based on TAC bits 1-0
    int get_frequency_cycles() const;

public:
    Timer(MMU& mmu);

    void tick(int cycles);

    // Call this from your MMU whenever the CPU attempts to write to 0xFF04
    void reset_div();
};