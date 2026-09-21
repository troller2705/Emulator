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

    int execute_cb(uint8_t cb_opcode);

    // Internal Registers
    RegisterPair AF; // A = high, F = low (Flags)
    RegisterPair BC;
    RegisterPair DE;
    RegisterPair HL;

    uint16_t SP;     // Stack Pointer
    uint16_t PC;     // Program Counter

    // Interrupt flags
    bool m_interrupts_enabled;
    bool m_halted = false;

private:
    MMU& m_mmu;

    // Helper functions for the F (Flags) register
    void set_flag_z(bool value);
    void set_flag_n(bool value);
    void set_flag_h(bool value);
    void set_flag_c(bool value);

    // ADD THESE FOUR LINES:
    bool get_flag_z() const { return (AF.low & (1 << 7)) != 0; }
    bool get_flag_n() const { return (AF.low & (1 << 6)) != 0; }
    bool get_flag_h() const { return (AF.low & (1 << 5)) != 0; }
    bool get_flag_c() const { return (AF.low & (1 << 4)) != 0; }

    // Helper operations
    void op_cp(uint8_t value);
    void op_add(uint8_t value);
    void op_load_16(RegisterPair& reg);
    void op_or(uint8_t value);
    void op_dec(uint8_t& reg);
    void op_inc(uint8_t& reg);
    void op_and(uint8_t value);
    void op_add_hl(uint16_t value);
    void op_xor(uint8_t value);
    void op_bit(uint8_t bit, uint8_t value);
    void op_sbc(uint8_t value);
    void op_adc(uint8_t value);
    void op_sub(uint8_t value);

    void stack_push(uint16_t value);
    uint16_t stack_pop();

};