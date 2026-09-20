#pragma once
#include <cstdint>
#include "mmu.h" // The CPU needs to talk to the MMU

// We use a struct to hold the 8-bit pair, and a union to overlap them with a 16-bit integer
union RegisterPair {
    uint16_t word;     // The 16-bit representation (e.g., HL)
    struct {
        // NOTE: Game Boy is Little Endian! 
        // The lower byte comes first in memory.
        uint8_t low;   // e.g., L
        uint8_t high;  // e.g., H
    };
};

class CPU {
public:
    // We pass a reference to the MMU so the CPU can read/write memory
    explicit CPU(MMU& mmu);

    // This is the core function we will call from run_frame()
    int clock_instruction();

    // Internal Registers
    RegisterPair AF; // A = high, F = low (Flags)
    RegisterPair BC;
    RegisterPair DE;
    RegisterPair HL;

    uint16_t SP;     // Stack Pointer
    uint16_t PC;     // Program Counter

    // Interrupt flags
    bool m_interrupts_enabled;

private:
    MMU& m_mmu;

    // Helper functions for the F (Flags) register
    void set_flag_z(bool value); // Zero
    void set_flag_n(bool value); // Subtract
    void set_flag_h(bool value); // Half Carry
    void set_flag_c(bool value); // Carry

    // Helper operations
    void op_cp(uint8_t value);
    void op_add(uint8_t value);
    void op_load_16(RegisterPair& reg);
};