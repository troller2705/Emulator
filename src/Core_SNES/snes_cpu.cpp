#include "../../include/Core_SNES/snes_cpu.h"
#include "../../include/Core_SNES/snes_mmu.h"
#include <iostream>

SNES_CPU::SNES_CPU(SNES_MMU& mmu) : m_mmu(mmu) {
    reset();
}

void SNES_CPU::reset() {
    // 1. Hardcoded startup state for 65C816
    flag_e = true;   // Boot into 6502 Emulation Mode
    flag_m = true;   // Force 8-bit Accumulator
    flag_x = true;   // Force 8-bit Index Registers
    flag_d = false;  // Decimal mode disabled
    flag_i = true;   // Interrupts disabled
    
    PB = 0x00;       // Program Bank starts at 0x00
    DB = 0x00;       // Data Bank starts at 0x00
    D  = 0x0000;     // Direct Page starts at 0
    S.W = 0x01FF;    // Stack pointer starts at 0x01FF in Emulation Mode
    
    // 2. Fetch the 16-bit Reset Vector from 0x00FFFC
    uint8_t pc_low  = m_mmu.read8(0x00FFFC);
    uint8_t pc_high = m_mmu.read8(0x00FFFD);
    PC = (pc_high << 8) | pc_low;

    std::cout << "[SNES_CPU] Reset triggered. Booting from PC: 0x" 
              << std::hex << PC << std::dec << "\n";
}

uint8_t SNES_CPU::fetch8() {
    // 65C816 fetches use a 24-bit address: (Program Bank << 16) | PC
    uint32_t addr = (PB << 16) | PC;
    PC++;
    return m_mmu.read8(addr);
}

uint16_t SNES_CPU::fetch16() {
    uint8_t low = fetch8();
    uint8_t high = fetch8();
    return low | (high << 8);
}

void SNES_CPU::push8(uint8_t val) {
    m_mmu.write8(S.W, val);
    if (flag_e) S.L--; // Emulation mode locks SP to 0x0100-0x01FF
    else        S.W--; // Native mode uses full 16-bit SP
}

uint8_t SNES_CPU::pop8() {
    if (flag_e) S.L++;
    else        S.W++;
    return m_mmu.read8(S.W);
}

int SNES_CPU::step() {
    uint8_t opcode = fetch8();
    int cycles = 2; // Base cycles

    switch (opcode) {
        case 0x18: { // CLC - Clear Carry
            flag_c = false;
            cycles = 2;
            break;
        }
        case 0x78: { // SEI - Set Interrupt Disable
            flag_i = true;
            cycles = 2;
            break;
        }
        case 0xFB: { // XCE - Exchange Carry and Emulation
            // This is how the SNES switches between 8-bit 6502 mode and 16-bit native mode!
            bool temp = flag_c;
            flag_c = flag_e;
            flag_e = temp;

            if (flag_e) {
                // Falling back to 6502 emulation forces M and X to 8-bit
                flag_m = true;
                flag_x = true;
                S.H = 0x01; // Stack is forced to Page 1 (0x0100 - 0x01FF)
            }
            cycles = 2;
            break;
        }
        case 0xC2: { // REP #imm - Reset Processor Status Bits
            uint8_t imm = fetch8();
            set_p(get_p() & ~imm);
            cycles = 3;
            break;
        }
        case 0xE2: { // SEP #imm - Set Processor Status Bits
            uint8_t imm = fetch8();
            set_p(get_p() | imm);
            cycles = 3;
            break;
        }
        case 0x5C: { // JMP long - Jump to 24-bit address
            uint16_t target_pc = fetch16();
            uint8_t  target_pb = fetch8();
            PC = target_pc;
            PB = target_pb;
            cycles = 4;
            break;
        }
        case 0x8F: { // STA long - Store Accumulator to 24-bit address
            uint16_t addr_low = fetch16();
            uint8_t  addr_bank = fetch8();
            uint32_t addr = (addr_bank << 16) | addr_low;

            if (flag_m) {
                // 8-bit Accumulator store
                m_mmu.write8(addr, A.L);
                cycles = 5;
            } else {
                // 16-bit Accumulator store
                m_mmu.write16(addr, A.W);
                cycles = 6;
            }
            break;
        }
        case 0xA2: { // LDX #imm
            if (flag_x) {
                // 8-bit Index
                X.L = fetch8();
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
                cycles = 2;
            } else {
                // 16-bit Index
                X.W = fetch16();
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
                cycles = 3;
            }
            break;
        }
        case 0x9A: { // TXS - Transfer X to Stack Pointer
            if (flag_e) {
                S.L = X.L; // In Emulation mode, Stack is locked to Page 1 (S.H = 0x01)
            } else {
                S.W = X.W;
            }
            // TXS does not affect Z or N flags
            cycles = 2;
            break;
        }
        case 0xA9: { // LDA #imm
            if (flag_m) {
                // 8-bit Accumulator
                A.L = fetch8();
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 2;
            } else {
                // 16-bit Accumulator
                A.W = fetch16();
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 3;
            }
            break;
        }
        case 0x9C: { // STZ abs - Store Zero Absolute
            uint16_t addr_low = fetch16();
            // Absolute addressing uses the Data Bank (DB) register
            uint32_t addr = (DB << 16) | addr_low;

            if (flag_m) {
                // 8-bit Store
                m_mmu.write8(addr, 0x00);
                cycles = 4;
            } else {
                // 16-bit Store
                m_mmu.write16(addr, 0x0000);
                cycles = 5;
            }
            break;
        }
        case 0x8D: { // STA abs - Store Accumulator Absolute
            uint16_t addr_low = fetch16();
            uint32_t addr = (DB << 16) | addr_low;

            if (flag_m) {
                m_mmu.write8(addr, A.L);
                cycles = 4;
            } else {
                m_mmu.write16(addr, A.W);
                cycles = 5;
            }
            break;
        }
        case 0x8E: { // STX abs - Store X Absolute
            uint16_t addr_low = fetch16();
            uint32_t addr = (DB << 16) | addr_low;

            if (flag_x) {
                m_mmu.write8(addr, X.L);
                cycles = 4;
            } else {
                m_mmu.write16(addr, X.W);
                cycles = 5;
            }
            break;
        }
        case 0x8C: { // STY abs - Store Y Absolute
            uint16_t addr_low = fetch16();
            uint32_t addr = (DB << 16) | addr_low;

            if (flag_x) {
                m_mmu.write8(addr, Y.L);
                cycles = 4;
            } else {
                m_mmu.write16(addr, Y.W);
                cycles = 5;
            }
            break;
        }
        case 0x5B: { // TCD - Transfer 16-bit Accumulator to Direct Page Register
            D = A.W;
            // TCD always updates N and Z based on the 16-bit value transferred
            flag_z = (D == 0);
            flag_n = (D & 0x8000) != 0;
            cycles = 2;
            break;
        }
        case 0x1B: { // TCS - Transfer 16-bit Accumulator to Stack Pointer
            if (flag_e) {
                S.L = A.L; // Emulation mode locks SP to Page 1
            } else {
                S.W = A.W;
            }
            // TCS is unique: it does NOT affect any CPU flags!
            cycles = 2;
            break;
        }
        case 0xA0: { // LDY #imm
            if (flag_x) {
                Y.L = fetch8();
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
                cycles = 2;
            } else {
                Y.W = fetch16();
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
                cycles = 3;
            }
            break;
        }
        case 0xE0: { // CPX #imm
            if (flag_x) {
                uint8_t imm = fetch8();
                uint16_t result = X.L - imm;
                flag_z = ((result & 0xFF) == 0);
                flag_n = (result & 0x80) != 0;
                flag_c = (X.L >= imm);
                cycles = 2;
            } else {
                uint16_t imm = fetch16();
                uint32_t result = X.W - imm;
                flag_z = ((result & 0xFFFF) == 0);
                flag_n = (result & 0x8000) != 0;
                flag_c = (X.W >= imm);
                cycles = 3;
            }
            break;
        }
        case 0xC0: { // CPY #imm
            if (flag_x) {
                uint8_t imm = fetch8();
                uint16_t result = Y.L - imm;
                flag_z = ((result & 0xFF) == 0);
                flag_n = (result & 0x80) != 0;
                flag_c = (Y.L >= imm);
                cycles = 2;
            } else {
                uint16_t imm = fetch16();
                uint32_t result = Y.W - imm;
                flag_z = ((result & 0xFFFF) == 0);
                flag_n = (result & 0x8000) != 0;
                flag_c = (Y.W >= imm);
                cycles = 3;
            }
            break;
        }
        case 0x9F: { // STA long, X - Store Accumulator Absolute Long Indexed with X
            uint16_t addr_low = fetch16();
            uint8_t  addr_bank = fetch8();

            // Long indexed addressing combines the 24-bit base address with X,
            // and allows the result to cross bank boundaries natively!
            uint32_t addr = ((addr_bank << 16) | addr_low) + (flag_x ? X.L : X.W);
            addr &= 0xFFFFFF;

            if (flag_m) {
                m_mmu.write8(addr, A.L);
                cycles = 5;
            } else {
                m_mmu.write16(addr, A.W);
                cycles = 6;
            }
            break;
        }
        case 0xE8: { // INX - Increment X
            if (flag_x) {
                X.L++;
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
            } else {
                X.W++;
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0xD0: { // BNE - Branch if Not Equal (Zero flag clear)
            int8_t offset = static_cast<int8_t>(fetch8());
            if (!flag_z) {
                PC += offset;
                cycles = 3; // Branch taken
            } else {
                cycles = 2; // Branch not taken
            }
            break;
        }
        case 0x98: { // TYA - Transfer Y to Accumulator
            if (flag_m) {
                A.L = Y.L;
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                // If A is 16-bit, check if Y is 8-bit to determine if we zero the high byte
                A.W = flag_x ? Y.L : Y.W;
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0xA8: { // TAY - Transfer Accumulator to Y
            if (flag_x) {
                Y.L = A.L;
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
            } else {
                Y.W = flag_m ? A.L : A.W;
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x8A: { // TXA - Transfer X to Accumulator
            if (flag_m) {
                A.L = X.L;
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                A.W = flag_x ? X.L : X.W;
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0xAA: { // TAX - Transfer Accumulator to X
            if (flag_x) {
                X.L = A.L;
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
            } else {
                X.W = flag_m ? A.L : A.W;
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x38: { // SEC - Set Carry Flag
            flag_c = true;
            cycles = 2;
            break;
        }
        case 0xD8: { // CLD - Clear Decimal Mode
            flag_d = false;
            cycles = 2;
            break;
        }
        case 0xF8: { // SED - Set Decimal Mode
            flag_d = true;
            cycles = 2;
            break;
        }
        case 0x58: { // CLI - Clear Interrupt Disable
            flag_i = false;
            cycles = 2;
            break;
        }
        case 0xB8: { // CLV - Clear Overflow Flag
            flag_v = false;
            cycles = 2;
            break;
        }
        case 0xE9: { // SBC #imm - Subtract with Carry Immediate
            if (flag_m) {
                uint8_t imm = fetch8();
                uint16_t op2 = imm ^ 0xFF; // Invert bits for subtraction (one's complement)
                uint16_t result = A.L + op2 + (flag_c ? 1 : 0);

                // Overflow (V) occurs if the sign of both operands was the same, but the result sign differs
                flag_v = ((A.L ^ result) & (op2 ^ result) & 0x80) != 0;
                flag_c = (result > 0xFF);

                A.L = result & 0xFF;
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 2;
            } else {
                uint16_t imm = fetch16();
                uint32_t op2 = imm ^ 0xFFFF;
                uint32_t result = A.W + op2 + (flag_c ? 1 : 0);

                flag_v = ((A.W ^ result) & (op2 ^ result) & 0x8000) != 0;
                flag_c = (result > 0xFFFF);

                A.W = result & 0xFFFF;
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 3;
            }
            break;
        }
        case 0x69: { // ADC #imm - Add with Carry Immediate
            if (flag_m) {
                uint8_t imm = fetch8();
                uint16_t result = A.L + imm + (flag_c ? 1 : 0);

                flag_v = (~(A.L ^ imm) & (A.L ^ result) & 0x80) != 0;
                flag_c = (result > 0xFF);

                A.L = result & 0xFF;
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 2;
            } else {
                uint16_t imm = fetch16();
                uint32_t result = A.W + imm + (flag_c ? 1 : 0);

                flag_v = (~(A.W ^ imm) & (A.W ^ result) & 0x8000) != 0;
                flag_c = (result > 0xFFFF);

                A.W = result & 0xFFFF;
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 3;
            }
            break;
        }
        case 0xCA: { // DEX - Decrement X
            if (flag_x) {
                X.L--;
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
            } else {
                X.W--;
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x88: { // DEY - Decrement Y
            if (flag_x) {
                Y.L--;
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
            } else {
                Y.W--;
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0xC8: { // INY - Increment Y
            if (flag_x) {
                Y.L++;
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
            } else {
                Y.W++;
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x10: { // BPL - Branch on Plus (Negative flag clear)
            int8_t offset = static_cast<int8_t>(fetch8());
            if (!flag_n) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0x30: { // BMI - Branch on Minus (Negative flag set)
            int8_t offset = static_cast<int8_t>(fetch8());
            if (flag_n) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0xF0: { // BEQ - Branch if Equal (Zero flag set)
            int8_t offset = static_cast<int8_t>(fetch8());
            if (flag_z) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0x90: { // BCC - Branch if Carry Clear (Also BLT - Branch if Less Than)
            int8_t offset = static_cast<int8_t>(fetch8());
            if (!flag_c) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0xB0: { // BCS - Branch if Carry Set (Also BGE - Branch if Greater/Equal)
            int8_t offset = static_cast<int8_t>(fetch8());
            if (flag_c) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0x50: { // BVC - Branch if Overflow Clear
            int8_t offset = static_cast<int8_t>(fetch8());
            if (!flag_v) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0x70: { // BVS - Branch if Overflow Set
            int8_t offset = static_cast<int8_t>(fetch8());
            if (flag_v) {
                PC += offset;
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0x20: { // JSR abs - Jump to Subroutine Absolute
            uint16_t target_pc = fetch16();

            // 65C816 JSR pushes the address of the LAST byte of the instruction (PC - 1)
            // It pushes the High byte first, then the Low byte.
            uint16_t return_addr = PC - 1;
            push8((return_addr >> 8) & 0xFF);
            push8(return_addr & 0xFF);

            PC = target_pc;
            cycles = 6;
            break;
        }
        case 0x60: { // RTS - Return from Subroutine
            // Pull Low byte first, then High byte
            uint8_t low = pop8();
            uint8_t high = pop8();

            // RTS pulls PC - 1, so we add 1 to get to the next instruction
            PC = ((high << 8) | low) + 1;
            cycles = 6;
            break;
        }
        case 0x22: { // JSL long - Jump to Subroutine Long
            uint16_t target_pc = fetch16();
            uint8_t  target_pb = fetch8();

            uint16_t return_addr = PC - 1;
            push8(PB); // Long jumps also save the current Program Bank
            push8((return_addr >> 8) & 0xFF);
            push8(return_addr & 0xFF);

            PC = target_pc;
            PB = target_pb;
            cycles = 8;
            break;
        }
        case 0x6B: { // RTL - Return from Subroutine Long
            uint8_t low = pop8();
            uint8_t high = pop8();
            PB = pop8(); // Pull the Program Bank back out

            PC = ((high << 8) | low) + 1;
            cycles = 6;
            break;
        }
        case 0x08: { // PHP - Push Processor Status
            push8(get_p());
            cycles = 3;
            break;
        }
        case 0x28: { // PLP - Pull Processor Status
            set_p(pop8());
            cycles = 4;
            break;
        }
        case 0x48: { // PHA - Push Accumulator
            if (flag_m) {
                // 8-bit push
                push8(A.L);
                cycles = 3;
            } else {
                // 16-bit push (High byte pushed first, then Low byte)
                push8(A.H);
                push8(A.L);
                cycles = 4;
            }
            break;
        }
        case 0x68: { // PLA - Pull Accumulator
            if (flag_m) {
                // 8-bit pull
                A.L = pop8();
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 4;
            } else {
                // 16-bit pull (Low byte pulled first, then High byte)
                A.L = pop8();
                A.H = pop8();
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0xC9: { // CMP #imm - Compare Accumulator Immediate
            if (flag_m) {
                uint8_t imm = fetch8();
                uint16_t result = A.L - imm;

                flag_z = ((result & 0xFF) == 0);
                flag_n = (result & 0x80) != 0;
                flag_c = (A.L >= imm);
                cycles = 2;
            } else {
                uint16_t imm = fetch16();
                uint32_t result = A.W - imm;

                flag_z = ((result & 0xFFFF) == 0);
                flag_n = (result & 0x8000) != 0;
                flag_c = (A.W >= imm);
                cycles = 3;
            }
            break;
        }
        case 0xCD: { // CMP abs - Compare Accumulator Absolute
            uint16_t addr_low = fetch16();
            uint32_t addr = (DB << 16) | addr_low;

            if (flag_m) {
                uint8_t val = m_mmu.read8(addr);
                uint16_t result = A.L - val;

                flag_z = ((result & 0xFF) == 0);
                flag_n = (result & 0x80) != 0;
                flag_c = (A.L >= val);
                cycles = 4;
            } else {
                uint16_t val = m_mmu.read16(addr);
                uint32_t result = A.W - val;

                flag_z = ((result & 0xFFFF) == 0);
                flag_n = (result & 0x8000) != 0;
                flag_c = (A.W >= val);
                cycles = 5;
            }
            break;
        }
        case 0x80: { // BRA - Branch Relative Always
            int8_t offset = static_cast<int8_t>(fetch8());
            PC += offset;
            cycles = 3;
            break;
        }
        case 0x82: { // BRL - Branch Relative Long Always
            int16_t offset = static_cast<int16_t>(fetch16());
            PC += offset;
            cycles = 4;
            break;
        }
        case 0xB7: { // LDA [dp], Y - Load Accumulator Indirect Long Indexed
            uint8_t dp_offset = fetch8();
            uint16_t dp_addr = D + dp_offset;

            // Read a 24-bit base address from the Direct Page
            uint32_t base_addr = m_mmu.read8(dp_addr) |
                                (m_mmu.read8(dp_addr + 1) << 8) |
                                (m_mmu.read8(dp_addr + 2) << 16);

            // Add Y to form the final 24-bit address (allowing it to cross banks)
            uint32_t final_addr = (base_addr + (flag_x ? Y.L : Y.W)) & 0xFFFFFF;

            if (flag_m) {
                A.L = m_mmu.read8(final_addr);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 6;
            } else {
                A.W = m_mmu.read16(final_addr);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 7; // Note: In hardware, takes an extra cycle if D.L != 0
            }
            break;
        }
        case 0x97: { // STA [dp], Y - Store Accumulator Indirect Long Indexed
            uint8_t dp_offset = fetch8();
            uint16_t dp_addr = D + dp_offset;

            uint32_t base_addr = m_mmu.read8(dp_addr) |
                                (m_mmu.read8(dp_addr + 1) << 8) |
                                (m_mmu.read8(dp_addr + 2) << 16);

            uint32_t final_addr = (base_addr + (flag_x ? Y.L : Y.W)) & 0xFFFFFF;

            if (flag_m) {
                m_mmu.write8(final_addr, A.L);
                cycles = 6;
            } else {
                m_mmu.write16(final_addr, A.W);
                cycles = 7;
            }
            break;
        }
        case 0x2A: { // ROL A - Rotate Left Accumulator
            if (flag_m) {
                bool old_carry = flag_c;
                flag_c = (A.L & 0x80) != 0;
                A.L = (A.L << 1) | (old_carry ? 1 : 0);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                bool old_carry = flag_c;
                flag_c = (A.W & 0x8000) != 0;
                A.W = (A.W << 1) | (old_carry ? 1 : 0);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x6A: { // ROR A - Rotate Right Accumulator
            if (flag_m) {
                bool old_carry = flag_c;
                flag_c = (A.L & 0x01) != 0;
                A.L = (A.L >> 1) | (old_carry ? 0x80 : 0);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                bool old_carry = flag_c;
                flag_c = (A.W & 0x01) != 0;
                A.W = (A.W >> 1) | (old_carry ? 0x8000 : 0);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x0A: { // ASL A - Arithmetic Shift Left Accumulator
            if (flag_m) {
                flag_c = (A.L & 0x80) != 0;
                A.L = (A.L << 1);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                flag_c = (A.W & 0x8000) != 0;
                A.W = (A.W << 1);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x4A: { // LSR A - Logical Shift Right Accumulator
            if (flag_m) {
                flag_c = (A.L & 0x01) != 0;
                A.L = (A.L >> 1);
                flag_z = (A.L == 0);
                flag_n = false; // Always clear for LSR
            } else {
                flag_c = (A.W & 0x01) != 0;
                A.W = (A.W >> 1);
                flag_z = (A.W == 0);
                flag_n = false; // Always clear for LSR
            }
            cycles = 2;
            break;
        }
        case 0xEB: { // XBA - Exchange B and A (High and Low bytes of Accumulator)
            uint8_t temp = A.L;
            A.L = A.H;
            A.H = temp;

            // XBA always sets Z and N based on the new 8-bit low byte (A.L), ignoring flag_m!
            flag_z = (A.L == 0);
            flag_n = (A.L & 0x80) != 0;
            cycles = 3;
            break;
        }
        case 0x1A: { // INC A - Increment Accumulator
            if (flag_m) {
                A.L++;
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                A.W++;
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x3A: { // DEC A - Decrement Accumulator
            if (flag_m) {
                A.L--;
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
            } else {
                A.W--;
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
            }
            cycles = 2;
            break;
        }
        case 0x74: { // STZ dp, X - Store Zero Direct Page Indexed with X
            uint8_t dp_offset = fetch8();
            uint16_t addr = D + dp_offset + (flag_x ? X.L : X.W);

            if (flag_m) {
                m_mmu.write8(addr, 0x00);
                cycles = 4;
            } else {
                m_mmu.write16(addr, 0x0000);
                cycles = 5;
            }
            break;
        }
        case 0xB5: { // LDA dp, X - Load Accumulator Direct Page Indexed with X
            uint8_t dp_offset = fetch8();
            uint16_t addr = D + dp_offset + (flag_x ? X.L : X.W);

            if (flag_m) {
                A.L = m_mmu.read8(addr);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 4;
            } else {
                A.W = m_mmu.read16(addr);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0x95: { // STA dp, X - Store Accumulator Direct Page Indexed with X
            uint8_t dp_offset = fetch8();
            uint16_t addr = D + dp_offset + (flag_x ? X.L : X.W);

            if (flag_m) {
                m_mmu.write8(addr, A.L);
                cycles = 4;
            } else {
                m_mmu.write16(addr, A.W);
                cycles = 5;
            }
            break;
        }
        case 0x9E: { // STZ abs, X - Store Zero Absolute Indexed with X
            uint16_t addr_low = fetch16();
            uint32_t addr = (((DB << 16) | addr_low) + (flag_x ? X.L : X.W)) & 0xFFFFFF;

            if (flag_m) {
                m_mmu.write8(addr, 0x00);
                cycles = 5;
            } else {
                m_mmu.write16(addr, 0x0000);
                cycles = 6;
            }
            break;
        }
        case 0xBD: { // LDA abs, X - Load Accumulator Absolute Indexed with X
            uint16_t addr_low = fetch16();
            uint32_t addr = (((DB << 16) | addr_low) + (flag_x ? X.L : X.W)) & 0xFFFFFF;

            if (flag_m) {
                A.L = m_mmu.read8(addr);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 4;
            } else {
                A.W = m_mmu.read16(addr);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0x9D: { // STA abs, X - Store Accumulator Absolute Indexed with X
            uint16_t addr_low = fetch16();
            uint32_t addr = (((DB << 16) | addr_low) + (flag_x ? X.L : X.W)) & 0xFFFFFF;

            if (flag_m) {
                m_mmu.write8(addr, A.L);
                cycles = 5;
            } else {
                m_mmu.write16(addr, A.W);
                cycles = 6;
            }
            break;
        }
        case 0xB9: { // LDA abs, Y - Load Accumulator Absolute Indexed with Y
            uint16_t addr_low = fetch16();
            uint32_t addr = (((DB << 16) | addr_low) + (flag_x ? Y.L : Y.W)) & 0xFFFFFF;

            if (flag_m) {
                A.L = m_mmu.read8(addr);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 4;
            } else {
                A.W = m_mmu.read16(addr);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0x99: { // STA abs, Y - Store Accumulator Absolute Indexed with Y
            uint16_t addr_low = fetch16();
            uint32_t addr = (((DB << 16) | addr_low) + (flag_x ? Y.L : Y.W)) & 0xFFFFFF;

            if (flag_m) {
                m_mmu.write8(addr, A.L);
                cycles = 5;
            } else {
                m_mmu.write16(addr, A.W);
                cycles = 6;
            }
            break;
        }
        case 0xE6: { // INC dp - Increment Memory Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) + 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 5;
            } else {
                uint16_t val = m_mmu.read16(addr) + 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 6;
            }
            break;
        }
        case 0xC6: { // DEC dp - Decrement Memory Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) - 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 5;
            } else {
                uint16_t val = m_mmu.read16(addr) - 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 6;
            }
            break;
        }
        case 0xF6: { // INC dp, X - Increment Memory Direct Page Indexed with X
            uint16_t addr = (D + fetch8() + (flag_x ? X.L : X.W)) & 0xFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) + 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 6;
            } else {
                uint16_t val = m_mmu.read16(addr) + 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 7;
            }
            break;
        }
        case 0xD6: { // DEC dp, X - Decrement Memory Direct Page Indexed with X
            uint16_t addr = (D + fetch8() + (flag_x ? X.L : X.W)) & 0xFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) - 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 6;
            } else {
                uint16_t val = m_mmu.read16(addr) - 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 7;
            }
            break;
        }
        case 0xEE: { // INC abs - Increment Memory Absolute
            uint32_t addr = (DB << 16) | fetch16();
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) + 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 6;
            } else {
                uint16_t val = m_mmu.read16(addr) + 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 7;
            }
            break;
        }
        case 0xCE: { // DEC abs - Decrement Memory Absolute
            uint32_t addr = (DB << 16) | fetch16();
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) - 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 6;
            } else {
                uint16_t val = m_mmu.read16(addr) - 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 7;
            }
            break;
        }
        case 0xFE: { // INC abs, X - Increment Memory Absolute Indexed with X
            uint32_t addr = (((DB << 16) | fetch16()) + (flag_x ? X.L : X.W)) & 0xFFFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) + 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 7;
            } else {
                uint16_t val = m_mmu.read16(addr) + 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 8;
            }
            break;
        }
        case 0xDE: { // DEC abs, X - Decrement Memory Absolute Indexed with X
            uint32_t addr = (((DB << 16) | fetch16()) + (flag_x ? X.L : X.W)) & 0xFFFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr) - 1;
                m_mmu.write8(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x80) != 0;
                cycles = 7;
            } else {
                uint16_t val = m_mmu.read16(addr) - 1;
                m_mmu.write16(addr, val);
                flag_z = (val == 0);
                flag_n = (val & 0x8000) != 0;
                cycles = 8;
            }
            break;
        }
        case 0xA5: { // LDA dp - Load Accumulator Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_m) {
                A.L = m_mmu.read8(addr);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 3;
            } else {
                A.W = m_mmu.read16(addr);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 4;
            }
            break;
        }
        case 0xA6: { // LDX dp - Load X Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_x) {
                X.L = m_mmu.read8(addr);
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
                cycles = 3;
            } else {
                X.W = m_mmu.read16(addr);
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
                cycles = 4;
            }
            break;
        }
        case 0xA4: { // LDY dp - Load Y Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_x) {
                Y.L = m_mmu.read8(addr);
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
                cycles = 3;
            } else {
                Y.W = m_mmu.read16(addr);
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
                cycles = 4;
            }
            break;
        }
        case 0x86: { // STX dp - Store X Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_x) {
                m_mmu.write8(addr, X.L);
                cycles = 3;
            } else {
                m_mmu.write16(addr, X.W);
                cycles = 4;
            }
            break;
        }
        case 0x84: { // STY dp - Store Y Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_x) {
                m_mmu.write8(addr, Y.L);
                cycles = 3;
            } else {
                m_mmu.write16(addr, Y.W);
                cycles = 4;
            }
            break;
        }
        case 0xC5: { // CMP dp - Compare Accumulator Direct Page
            uint16_t addr = (D + fetch8()) & 0xFFFF;
            if (flag_m) {
                uint8_t val = m_mmu.read8(addr);
                uint16_t result = A.L - val;
                flag_z = ((result & 0xFF) == 0);
                flag_n = (result & 0x80) != 0;
                flag_c = (A.L >= val);
                cycles = 3;
            } else {
                uint16_t val = m_mmu.read16(addr);
                uint32_t result = A.W - val;
                flag_z = ((result & 0xFFFF) == 0);
                flag_n = (result & 0x8000) != 0;
                flag_c = (A.W >= val);
                cycles = 4;
            }
            break;
        }
        case 0xAD: { // LDA abs - Load Accumulator Absolute
            uint32_t addr = (DB << 16) | fetch16();
            if (flag_m) {
                A.L = m_mmu.read8(addr);
                flag_z = (A.L == 0);
                flag_n = (A.L & 0x80) != 0;
                cycles = 4;
            } else {
                A.W = m_mmu.read16(addr);
                flag_z = (A.W == 0);
                flag_n = (A.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0xAE: { // LDX abs - Load X Absolute
            uint32_t addr = (DB << 16) | fetch16();
            if (flag_x) {
                X.L = m_mmu.read8(addr);
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
                cycles = 4;
            } else {
                X.W = m_mmu.read16(addr);
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0xAC: { // LDY abs - Load Y Absolute
            uint32_t addr = (DB << 16) | fetch16();
            if (flag_x) {
                Y.L = m_mmu.read8(addr);
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
                cycles = 4;
            } else {
                Y.W = m_mmu.read16(addr);
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0x5A: { // PHY - Push Y Register
            if (flag_x) {
                push8(Y.L);
                cycles = 3;
            } else {
                push8(Y.H);
                push8(Y.L);
                cycles = 4;
            }
            break;
        }
        case 0x7A: { // PLY - Pull Y Register
            if (flag_x) {
                Y.L = pop8();
                flag_z = (Y.L == 0);
                flag_n = (Y.L & 0x80) != 0;
                cycles = 4;
            } else {
                Y.L = pop8();
                Y.H = pop8();
                flag_z = (Y.W == 0);
                flag_n = (Y.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        case 0xDA: { // PHX - Push X Register
            if (flag_x) {
                push8(X.L);
                cycles = 3;
            } else {
                push8(X.H);
                push8(X.L);
                cycles = 4;
            }
            break;
        }
        case 0xFA: { // PLX - Pull X Register
            if (flag_x) {
                X.L = pop8();
                flag_z = (X.L == 0);
                flag_n = (X.L & 0x80) != 0;
                cycles = 4;
            } else {
                X.L = pop8();
                X.H = pop8();
                flag_z = (X.W == 0);
                flag_n = (X.W & 0x8000) != 0;
                cycles = 5;
            }
            break;
        }
        default: {
            std::printf("\n--- CRASH: Unimplemented SNES Opcode 0x%02X ---\n", opcode);
            std::printf("At 24-bit Address: %02X:%04X\n", PB, PC - 1);
            std::printf("Flags -> M:%d X:%d E:%d\n", flag_m, flag_x, flag_e);
            exit(1);
        }
    }

    return cycles;
}

uint8_t SNES_CPU::get_p() const {
    uint8_t p = 0;
    if (flag_n) p |= 0x80;
    if (flag_v) p |= 0x40;
    if (flag_m) p |= 0x20;
    if (flag_x) p |= 0x10;
    if (flag_d) p |= 0x08;
    if (flag_i) p |= 0x04;
    if (flag_z) p |= 0x02;
    if (flag_c) p |= 0x01;
    return p;
}

void SNES_CPU::set_p(uint8_t val) {
    flag_n = (val & 0x80) != 0;
    flag_v = (val & 0x40) != 0;
    flag_m = (val & 0x20) != 0;
    flag_x = (val & 0x10) != 0;
    flag_d = (val & 0x08) != 0;
    flag_i = (val & 0x04) != 0;
    flag_z = (val & 0x02) != 0;
    flag_c = (val & 0x01) != 0;
    
    // If in emulation mode, Accumulator and Index registers are strictly 8-bit
    if (flag_e) {
        flag_m = true;
        flag_x = true;
    }
}