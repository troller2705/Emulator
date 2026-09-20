#include "cpu.h"
#include <iostream>

CPU::CPU(MMU& mmu) : m_mmu(mmu) {
    // Initial hardware states after the Game Boy Boot ROM finishes
    AF.word = 0x01B0;
    BC.word = 0x0013;
    DE.word = 0x00D8;
    HL.word = 0x014D;
    SP      = 0xFFFE;
    PC      = 0x0100; // Execution always begins at 0x0100!

    m_interrupts_enabled = false;
}

int CPU::clock_instruction() {
    // 1. FETCH Opcode
    uint8_t opcode = m_mmu.read(PC);

    // Remember where we started for debugging purposes
    uint16_t current_pc = PC;
    PC++;

    int cycles = 0;

    // 2. DECODE & EXECUTE
    switch (opcode) {
        // --- JUMPS ---
        case 0xC3: { // JP a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            PC = (high << 8) | low;
            cycles = 16;
            break;
        }

        // --- LOADS (8-bit) ---
        case 0x3E: // LD A, d8
            AF.high = m_mmu.read(PC++);
            cycles = 8;
            break;

        case 0x7F: // LD A, A
            AF.high = AF.high;
            cycles = 4;
            break;

        case 0x78: // LD A, B
            AF.high = BC.high;
            cycles = 4;
            break;

            // --- COMPARES ---
        case 0xFE: // CP d8
            op_cp(m_mmu.read(PC++));
            cycles = 8;
            break;

        case 0xBF: // CP A
            op_cp(AF.high);
            cycles = 4;
            break;

        case 0xB8: // CP B
            op_cp(BC.high);
            cycles = 4;
            break;
    }

    return cycles;
}

// Bit manipulation helpers for the Flags Register (F)
// The flags are stored in the top 4 bits of the F register (Bit 7, 6, 5, 4)
void CPU::set_flag_z(bool value) {
    if (value) AF.low |= (1 << 7);
    else       AF.low &= ~(1 << 7);
}

void CPU::set_flag_n(bool value) {
    if (value) AF.low |= (1 << 6);
    else       AF.low &= ~(1 << 6);
}

void CPU::set_flag_h(bool value) {
    if (value) AF.low |= (1 << 5);
    else       AF.low &= ~(1 << 5);
}

void CPU::set_flag_c(bool value) {
    if (value) AF.low |= (1 << 4);
    else       AF.low &= ~(1 << 4);
}

void CPU::op_cp(uint8_t value) {
    uint8_t a = AF.high;
    int result = a - value;

    set_flag_z(result == 0);
    set_flag_n(true);
    set_flag_h((a & 0x0F) < (value & 0x0F));
    set_flag_c(a < value);
}