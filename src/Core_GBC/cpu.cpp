#include "cpu.h"
#include <iostream>

// A simple way to track recent history
#include <deque>
std::deque<uint16_t> pc_history;

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

void CPU::stack_push(uint16_t value) {
    SP -= 2;
    m_mmu.write(SP, value & 0xFF);         // Low byte
    m_mmu.write(SP + 1, (value >> 8) & 0xFF); // High byte
}

uint16_t CPU::stack_pop() {
    uint16_t low = m_mmu.read(SP);
    uint16_t high = m_mmu.read(SP + 1);
    SP += 2;
    return (high << 8) | low;
}

bool CPU::check_pending_interrupts() {
    uint8_t ie = m_mmu.read(0xFFFF);
    uint8_t if_reg = m_mmu.read(0xFF0F);

    // Check if any interrupt is both requested (IF) and enabled (IE)
    return (ie & if_reg & 0x1F) != 0;
}

bool CPU::handle_interrupts() {
    if (m_interrupts_enabled) {
        uint8_t ie = m_mmu.read(0xFFFF);
        uint8_t if_reg = m_mmu.read(0xFF0F);
        uint8_t requested = ie & if_reg & 0x1F;

        if (requested != 0) {
            // 1. Disable master interrupts
            m_interrupts_enabled = false;

            // 2. Find and service the highest priority interrupt (bit 0 to 4)
            uint16_t vector = 0;
            for (int i = 0; i < 5; ++i) {
                if (requested & (1 << i)) {
                    // Clear the flag in IF
                    m_mmu.write(0xFF0F, if_reg & ~(1 << i));

                    // Assign vector based on priority
                    switch (i) {
                        case 0: vector = 0x0040; break; // VBlank
                        case 1: vector = 0x0048; break; // LCD STAT
                        case 2: vector = 0x0050; break; // Timer
                        case 3: vector = 0x0058; break; // Serial
                        case 4: vector = 0x0060; break; // Joypad
                    }
                    break;
                }
            }

            // 3. Push current PC to stack
            stack_push(PC);

            // 4. Jump to vector
            PC = vector;

            return true; // Interrupt serviced! Takes 20 cycles.
        }
    }
    return false;
}

void CPU::print_trace() {
    std::cout << "\n--- RECENT EXECUTION TRACE ---:\n";
    for (uint16_t past_pc : pc_history) {
        uint8_t op = m_mmu.read(past_pc);
        std::printf("PC: 0x%04X | Opcode: 0x%02X\n", past_pc, op);
    }
    std::printf("Registers -> A: 0x%02X, BC: 0x%04X, DE: 0x%04X, HL: 0x%04X, SP: 0x%04X\n",
                AF.high, BC.word, DE.word, HL.word, SP);
}

int CPU::clock_instruction() {
    // 1. Handle halted state wakeup check
    if (m_halted) {
        if (check_pending_interrupts()) {
            m_halted = false;
        } else {
            // TEMPORARY BYPASS: Force-enable VBlank and wake up after the RAM clear
            m_mmu.write(0xFFFF, 0x01); // Enable VBlank in IE
            m_mmu.write(0xFF0F, 0x01); // Request VBlank in IF
            m_halted = false;          // Wake up!
        }
    }

    // 2. Check and service any pending interrupts before fetching the next instruction
    if (handle_interrupts()) {
        return 20; // Servicing an interrupt takes 20 cycles
    }

    // 3. FETCH Opcode
    uint8_t opcode = m_mmu.read(PC);

    // Remember where we started for debugging purposes
    uint16_t current_pc = PC;
    PC++;

    // Log everything after the boot ROM hands off to cart space (> 0x0100)
    if (PC >= 0x0100) {
        uint8_t op = m_mmu.read(PC);
        std::printf("CART TRACE -> PC: 0x%04X | Opcode: 0x%02X | A: 0x%02X | BC: 0x%04X\n", PC, op, AF.high, BC.word);
    }

    pc_history.push_back(current_pc);
    if (pc_history.size() > 200) {
        // print_trace();
        pc_history.clear();
    }

    int cycles = 0;

    // 4. DECODE & EXECUTE
    switch (opcode) {

        case 0x00: { // NOP
            cycles = 4;
            break;
        }

        // --- JUMPS ---
        case 0xC3: { // JP a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            PC = (high << 8) | low;
            cycles = 16;
            break;
        }

        case 0x20: { // JR NZ, r8
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));
            if (!get_flag_z()) {
                PC += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        case 0x28: { // JR Z, r8 (Jump Relative if Zero)
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));

            if (get_flag_z()) {
                // If the Zero flag IS set, we take the jump.
                PC += offset;
                cycles = 12;
            } else {
                cycles = 8;
            }
            break;
        }

        // --- LOADS (8-bit) ---
        case 0x3E: { // LD A, d8
            AF.high = m_mmu.read(PC++);
            cycles = 8;
            break;
        }

        case 0x7F: { // LD A, A
            AF.high = AF.high;
            cycles = 4;
            break;
        }

        case 0x78: { // LD A, B
            AF.high = BC.high; // Assuming your BC register struct exposes high/low or B directly
            cycles = 4;
            break;
        }

        // --- COMPARES ---
        case 0xFE: { // CP d8
            op_cp(m_mmu.read(PC++));
            cycles = 8;
            break;
        }

        case 0xBF: { // CP A
            op_cp(AF.high);
            cycles = 4;
            break;
        }

        case 0xB8: { // CP B
            op_cp(BC.high);
            cycles = 4;
            break;
        }

        case 0xEA: { // LD (a16), A
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t addr = (high << 8) | low;
            m_mmu.write(addr, AF.high);
            cycles = 16;
            break;
        }

        case 0xE0: { // LDH (a8), A
            uint8_t offset = m_mmu.read(PC++);
            m_mmu.write(0xFF00 + offset, AF.high);
            cycles = 12;
            break;
        }

        case 0xAF: { // XOR A (Effectively sets A to 0)
            AF.high ^= AF.high; // Or simply AF.high = 0;

            // Flags update:
            // Z is set if the result is 0 (which it always will be here)
            // N, H, and C are always forcibly cleared by an XOR instruction
            set_flag_z(AF.high == 0);
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(false);

            cycles = 4;
            break;
        }

        // --- 16-BIT LOADS ---
        case 0x01: // LD BC, d16
            BC.low = m_mmu.read(PC++); BC.high = m_mmu.read(PC++);
            cycles = 12; break;
        case 0x11: // LD DE, d16
            DE.low = m_mmu.read(PC++); DE.high = m_mmu.read(PC++);
            cycles = 12; break;
        case 0x21: // LD HL, d16
            HL.low = m_mmu.read(PC++); HL.high = m_mmu.read(PC++);
            cycles = 12; break;
        case 0x31: // LD SP, d16
            { uint8_t l = m_mmu.read(PC++); uint8_t h = m_mmu.read(PC++); SP = (h << 8) | l; }
            cycles = 12; break;

        // --- SUBROUTINES & STACK ---
        case 0xCD: // CALL a16
            { uint8_t l = m_mmu.read(PC++); uint8_t h = m_mmu.read(PC++);
              stack_push(PC); PC = (h << 8) | l; }
            cycles = 24; break;
        case 0xC9: // RET
            PC = stack_pop();
            cycles = 16; break;

        case 0xC5: stack_push(BC.word); cycles = 16; break; // PUSH BC
        case 0xD5: stack_push(DE.word); cycles = 16; break; // PUSH DE
        case 0xE5: stack_push(HL.word); cycles = 16; break; // PUSH HL
        case 0xF5: stack_push(AF.word); cycles = 16; break; // PUSH AF

        case 0xC1: BC.word = stack_pop(); cycles = 12; break; // POP BC
        case 0xD1: DE.word = stack_pop(); cycles = 12; break; // POP DE
        case 0xE1: HL.word = stack_pop(); cycles = 12; break; // POP HL
        case 0xF1: AF.word = stack_pop() & 0xFFF0; cycles = 12; break; // POP AF (Bottom 4 bits of F are always 0)

        // --- HL POINTER MATH (Extremely Common) ---
        case 0x22: // LD (HL+), A (Write A to memory at HL, then increment HL)
            m_mmu.write(HL.word, AF.high);
            HL.word++;
            cycles = 8; break;
        case 0x32: // LD (HL-), A (Write A to memory at HL, then decrement HL)
            m_mmu.write(HL.word, AF.high);
            HL.word--;
            cycles = 8; break;
        case 0x2A: // LD A, (HL+) (Read memory at HL into A, then increment HL)
            AF.high = m_mmu.read(HL.word);
            HL.word++;
            cycles = 8; break;
        case 0x3A: // LD A, (HL-) (Read memory at HL into A, then decrement HL)
            AF.high = m_mmu.read(HL.word);
            HL.word--;
            cycles = 8; break;

        case 0x18: { // JR r8 (Unconditional Jump Relative)
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));
            PC += offset;
            cycles = 12;
            break;
        }

        case 0x02: { // LD (BC), A
            // Write the value in A to the memory address stored in BC
            m_mmu.write(BC.word, AF.high);

            cycles = 8;
            break;
        }

        case 0X03: { // INC BC

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x04: { // INC B
            op_inc(BC.high);
            cycles = 4;
            break;
        }

        case 0x05: { // DEC B
            op_dec(BC.high);
            cycles = 4;
            break;
        }

        case 0x06: { // LD B, d8
            uint8_t d8 = m_mmu.read(PC++);

            // Load the value into Register B
            BC.high = d8;

            cycles = 8;
            break;
        }

        case 0X07: { // RLCA

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x08: { // LD (a16), SP
            // 1. Fetch the 16-bit address (low byte first)
            uint8_t low_addr = m_mmu.read(PC++);
            uint8_t high_addr = m_mmu.read(PC++);
            uint16_t address = (high_addr << 8) | low_addr;

            // 2. Write the Stack Pointer to memory (little-endian)
            m_mmu.write(address, SP & 0xFF);             // Write low byte
            m_mmu.write(address + 1, (SP >> 8) & 0xFF);  // Write high byte

            cycles = 20;
            break;
        }

        case 0x09: { // ADD HL, BC
            op_add_hl(BC.word);
            cycles = 8;
            break;
        }

        case 0X0A: { // LD A, BC

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x0B: { // DEC BC
            BC.word--;
            cycles = 8;
            break;
        }

        case 0x0C: { // INC C
            op_inc(BC.low);
            cycles = 4;
            break;
        }

        case 0x0D: { // DEC C
            uint8_t old_val = BC.low;
            BC.low--;

            // Z: Set if the result is 0
            set_flag_z(BC.low == 0);
            // N: Always 1 (since it's a decrement/subtraction)
            set_flag_n(true);
            // H: Set if there was a borrow from bit 4 (i.e., lower nibble was 0)
            set_flag_h((old_val & 0x0F) == 0);
            // C: Completely unaffected by 8-bit INC/DEC!

            cycles = 4;
            break;
        }

        case 0x0E: { // LD C, d8
            // Fetch the immediate 8-bit value
            uint8_t d8 = m_mmu.read(PC++);

            // Load the value into Register C
            BC.low = d8;

            cycles = 8;
            break;
        }

        case 0X0F: { // RRCA

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X10: { // STOP n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x12: { // LD (DE), A
            // Write the value of A into the memory address pointed to by DE
            m_mmu.write(DE.word, AF.high);

            cycles = 8;
            break;
        }

        case 0x13: { // INC DE
            DE.word++;
            cycles = 8;
            break;
        }

        case 0X14: { // INC D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x15: { // DEC D
            op_dec(DE.high);
            cycles = 4;
            break;
        }

        case 0x16: { // LD D, n8
            uint8_t d8 = m_mmu.read(PC++);

            // Load the value into Register D
            DE.high = d8;

            cycles = 8;
            break;
        }

        case 0X17: { // RLA

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x19: { // ADD HL, DE
            op_add_hl(DE.word);
            cycles = 8;
            break;
        }

        case 0x1A: { // LD A, (DE)
            AF.high = m_mmu.read(DE.word);
            cycles = 8;
            break;
        }

        case 0x1B: { // DEC DE
            DE.word--;
            cycles = 8;
            break;
        }

        case 0X1C: { // INC E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x1D: { // DEC E
            op_dec(DE.low);
            cycles = 4;
            break;
        }

        case 0x1E: { // LD E, n8
            uint8_t d8 = m_mmu.read(PC++);

            // Load the value into Register E
            DE.low = d8;

            cycles = 8;
            break;
        }

        case 0X1F: { // RRA

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x23: { // INC HL
            HL.word++;
            cycles = 8;
            break;
        }

        case 0X24: { // INC H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X25: { // DEC H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x26: { // LD H, n8
            uint8_t d8 = m_mmu.read(PC++);

            // Load the value into Register H
            HL.high = d8;

            cycles = 8;
            break;
        }

        case 0X27: { // DAA

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X29: { // ADD HL, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x2B: { // DEC HL
            HL.word--;
            cycles = 8;
            break;
        }

        case 0X2C: { // INC L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X2D: { // DEC L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X2E: { // LD L, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x2F: { // CPL
            AF.high = ~AF.high;

            // Z: Unaffected
            // N: Always set to 1
            set_flag_n(true);
            // H: Always set to 1
            set_flag_h(true);
            // C: Unaffected

            cycles = 4;
            break;
        }

        case 0x30: { // JR NC, r8
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));
            if (!get_flag_c()) {
                PC += offset;
                cycles = 12; // Jump taken
            } else {
                cycles = 8;  // Jump not taken
            }
            break;
        }

        case 0X33: { // INC SP

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X34: { // INC HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0X35: { // DEC HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0x36: { // LD (HL), d8
            // Fetch the 8-bit value we want to write
            uint8_t value = m_mmu.read(PC++);

            // Write it to the memory address pointed to by HL
            m_mmu.write(HL.word, value);

            // 12 cycles: 4 to fetch opcode, 4 to fetch d8, 4 to write to memory
            cycles = 12;
            break;
        }

        case 0X37: { // SCF

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X38: { // JR C, e8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0X39: { // ADD HL, SP

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X3B: { // DEC SP

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x3C: { // INC A
            uint8_t old_val = AF.high;
            AF.high++;

            // Z: Set if the result is 0
            set_flag_z(AF.high == 0);
            // N: Always 0 (since it's an addition/increment)
            set_flag_n(false);
            // H: Set if there was a carry from bit 3 to bit 4
            set_flag_h((old_val & 0x0F) == 0x0F);
            // C: Completely unaffected by 8-bit INC/DEC!

            cycles = 4;
            break;
        }

        case 0x3D: { // DEC A
            op_dec(AF.high);
            cycles = 4;
            break;
        }

        case 0X3F: { // CCF

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x40: { // LD B, B
            BC.high = BC.high; // Effectively a 4-cycle NOP
            cycles = 4;
            break;
        }

        case 0X41: { // LD B, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x42: { // LD B, D
            BC.high = DE.high;
            cycles = 4;
            break;
        }

        case 0X43: { // LD B, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X44: { // LD B, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X45: { // LD B, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X46: { // LD B, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x47: { // LD B, A
            BC.high = AF.high;
            cycles = 4;
            break;
        }

        case 0X48: { // LD C, B

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X49: { // LD C, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X4A: { // LD C, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X4B: { // LD C, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X4C: { // LD C, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X4D: { // LD C, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X4E: { // LD C, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x4F: { // LD C, A
            BC.low = AF.high;
            cycles = 4;
            break;
        }

        case 0X50: { // LD D, B

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X51: { // LD D, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X52: { // LD D, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X53: { // LD D, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x54: { // LD D, H
            DE.high = HL.high;
            cycles = 4;
            break;
        }

        case 0X55: { // LD D, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X56: { // LD D, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x57: { // LD D, A
            DE.high = AF.high;
            cycles = 4;
            break;
        }

        case 0X58: { // LD E, B

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X59: { // LD E, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X5A: { // LD E, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X5B: { // LD E, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X5C: { // LD E, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x5D: { // LD E, L
            DE.low = HL.low;
            cycles = 4;
            break;
        }

        case 0X5E: { // LD E, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x5F: { // LD E, A
            DE.low = AF.high;
            cycles = 4;
            break;
        }

        case 0x60: { // LD H, B
            HL.high = BC.high;
            cycles = 4;
            break;
        }

        case 0X61: { // LD H, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x62: { // LD H, D
            HL.high = DE.high;
            cycles = 4;
            break;
        }

        case 0X63: { // LD H, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X64: { // LD H, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X65: { // LD H, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x66: { // LD H, (HL)
            HL.high = m_mmu.read(HL.word);
            cycles = 8;
            break;
        }

        case 0x67: { // LD H, A
            HL.high = AF.high;
            cycles = 4;
            break;
        }

        case 0X68: { // LD L, B

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x69: { // LD L, C
            HL.low = BC.low;
            cycles = 4;
            break;
        }

        case 0X6A: { // LD L, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0x6B: { // LD L, E
            HL.low = DE.low;
            cycles = 4;
            break;
        }

        case 0X6C: { // LD L, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X6D: { // LD L, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X6E: { // LD L, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x6F: { // LD L, A
            HL.low = AF.high;
            cycles = 4;
            break;
        }

        case 0X70: { // LD HL, B
            m_mmu.write(HL.word, BC.high);
            cycles = 8;
            break;
        }

        case 0X71: { // LD HL, C
            m_mmu.write(HL.word, BC.low);
            cycles = 8;
            break;
        }

        case 0X72: { // LD HL, D
            m_mmu.write(HL.word, DE.high);
            cycles = 8;
            break;
        }

        case 0X73: { // LD HL, E
            m_mmu.write(HL.word, DE.low);
            cycles = 8;
            break;
        }

        case 0X74: { // LD HL, H
            m_mmu.write(HL.word, HL.high);
            cycles = 8;
            break;
        }

        case 0X75: { // LD HL, L
            m_mmu.write(HL.word, HL.low);
            cycles = 8;
            break;
        }

        case 0x76: { // HALT
            m_halted = true;

            // Debug what registers look like when halting
            uint8_t ie = m_mmu.read(0xFFFF);
            uint8_t if_reg = m_mmu.read(0xFF0F);
            std::printf("CPU HALTED at PC: 0x%04X | IE: 0x%02X | IF: 0x%02X\n", PC - 1, ie, if_reg);

            cycles = 4;
            break;
        }

        case 0x77: { // LD (HL), A
            // Write the value in A to the memory address stored in HL
            m_mmu.write(HL.word, AF.high);

            cycles = 8;
            break;
        }

        case 0x79: { // LD A, C
            AF.high = BC.low;
            cycles = 4;
            break;
        }

        case 0x7A: { // LD A, D
            AF.high = DE.high;
            cycles = 4;
            break;
        }

        case 0X7B: { // LD A, E
            AF.high = DE.low;
            cycles = 4;
            break;
        }

        case 0x7C: { // LD A, H
            AF.high = HL.high;
            cycles = 4;
            break;
        }

        case 0X7D: { // LD A, L
            AF.high = HL.low;
            cycles = 4;
            break;
        }

        case 0x7E: { // LD A, (HL)
            // Read the value from memory at the address stored in HL
            AF.high = m_mmu.read(HL.word);

            cycles = 8;
            break;
        }

        case 0x80: { // ADD A, B
            op_add(BC.high);
            cycles = 4;
            break;
        }

        case 0x81: { // ADD A, C
            op_add(BC.low);
            cycles = 4;
            break;
        }

        case 0x82: { // ADD A, D
            op_add(DE.high);
            cycles = 4;
            break;
        }

        case 0X83: { // ADD A, E

            op_add(DE.low);
            cycles = 4;
            break;
        }

        case 0x84: { // ADD A, H
            op_add(HL.high);
            cycles = 4;
            break;
        }

        case 0X85: { // ADD A, L

            op_add(HL.low);
            cycles = 4;
            break;
        }

        case 0x86: { // ADD A, (HL)
            // Read the value from memory at address HL, then add it to A
            op_add(m_mmu.read(HL.word));

            cycles = 8;
            break;
        }

        case 0x87: { // ADD A, A
            op_add(AF.high);
            cycles = 4;
            break;
        }

        case 0x88: { // ADC A, B
            op_adc(BC.high);
            cycles = 4;
            break;
        }

        case 0x89: { // ADC A, C
            op_adc(BC.low);
            cycles = 4;
            break;
        }

        case 0X8A: { // ADC A, D

            op_adc(DE.high);
            cycles = 4;
            break;
        }

        case 0X8B: { // ADC A, E

            op_adc(DE.low);
            cycles = 4;
            break;
        }

        case 0x8C: { // ADC A, H
            op_adc(HL.high);
            cycles = 4;
            break;
        }

        case 0X8D: { // ADC A, L

            op_adc(HL.low);
            cycles = 4;
            break;
        }

        case 0X8E: { // ADC A, HL
            op_adc(m_mmu.read(HL.word));
            cycles = 8;
            break;
        }

        case 0X8F: { // ADC A, A
            op_adc(AF.high);
            cycles = 4;
            break;
        }

        case 0x90: { // SUB B
            op_sub(BC.high);
            cycles = 4;
            break;
        }

        case 0X91: { // SUB A, C
            op_sub(BC.low);
            cycles = 4;
            break;
        }

        case 0X92: { // SUB A, D
            op_sub(DE.high);
            cycles = 4;
            break;
        }

        case 0X93: { // SUB A, E
            op_sub(DE.low);
            cycles = 4;
            break;
        }

        case 0x94: { // SUB H
            op_sub(HL.high);
            cycles = 4;
            break;
        }

        case 0X95: { // SUB A, L
            op_sub(HL.low);
            cycles = 4;
            break;
        }

        case 0X96: { // SUB A, HL
            op_sub(m_mmu.read(HL.word));
            cycles = 8;
            break;
        }

        case 0X97: { // SUB A, A
            op_sub(AF.high);
            cycles = 4;
            break;
        }

        case 0x98: { // SBC A, B
            op_sbc(BC.high);
            cycles = 4;
            break;
        }

        case 0X99: { // SBC A, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X9A: { // SBC A, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X9B: { // SBC A, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X9C: { // SBC A, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X9D: { // SBC A, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0X9E: { // SBC A, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X9F: { // SBC A, A

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XA0: { // AND A, B

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XA1: { // AND A, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XA2: { // AND A, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XA3: { // AND A, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0xA4: { // AND H
            op_and(HL.high);
            cycles = 4;
            break;
        }

        case 0XA5: { // AND A, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XA6: { // AND A, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0xA7: { // AND A
            op_and(AF.high);
            cycles = 4;
            break;
        }

        case 0XA8: { // XOR A, B

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XA9: { // XOR A, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XAA: { // XOR A, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0xAB: { // XOR E
            op_xor(DE.low);
            cycles = 4;
            break;
        }

        case 0XAC: { // XOR A, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XAD: { // XOR A, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XAE: { // XOR A, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0xB0: { // OR B
            op_or(BC.high);
            cycles = 4;
            break;
        }

        case 0xB1: { // OR A, C
            op_or(BC.low); // C is the low byte of the BC register
            cycles = 4;
            break;
        }

        case 0XB2: { // OR A, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0xB3: { // OR E
            op_or(DE.low);
            cycles = 4;
            break;
        }

        case 0XB4: { // OR A, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XB5: { // OR A, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XB6: { // OR A, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB7: { // OR A, A

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XB9: { // CP A, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XBA: { // CP A, D

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XBB: { // CP A, E

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XBC: { // CP A, H

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XBD: { // CP A, L

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XBE: { // CP A, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0xC0: { // RET NZ
            if (!get_flag_z()) {
                // Pop the 16-bit return address off the stack
                uint8_t low = m_mmu.read(SP++);
                uint8_t high = m_mmu.read(SP++);

                // Jump to that address
                PC = low | (high << 8);

                cycles = 20; // Branch taken
            } else {
                cycles = 8;  // Branch not taken
            }
            break;
        }

        case 0XC2: { // JP NZ, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t d16 = (high << 8) | low;

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XC4: { // CALL NZ, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t d16 = (high << 8) | low;

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 24;
            break;
        }

        case 0XC6: { // ADD A, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC7: { // RST $00

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0xC8: { // RET Z
            if (get_flag_z()) {
                // Pop the 16-bit return address off the stack
                uint8_t low = m_mmu.read(SP++);
                uint8_t high = m_mmu.read(SP++);

                // Jump to that address
                PC = low | (high << 8);

                cycles = 20; // Branch taken
            } else {
                cycles = 8;  // Branch not taken
            }
            break;
        }

        case 0xCA: { // JP Z, a16
            // Read the 16-bit address (Low byte first, then High byte)
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = low | (high << 8);

            // Check the Zero flag
            if (get_flag_z()) {
                PC = address; // Take the jump
                cycles = 16;
            } else {
                // Condition not met, just move on
                cycles = 12;
            }
            break;
        }

        case 0xCB: { // PREFIX CB
            // Read the NEXT byte to find out what the actual operation is
            uint8_t cb_opcode = m_mmu.read(PC++);

            // Pass it to a dedicated CB decoder function
            cycles = execute_cb(cb_opcode);
            break;
        }

        case 0xCC: { // CALL Z, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = (high << 8) | low;

            if (get_flag_z()) {
                stack_push(PC);
                PC = address;
                cycles = 24; // Condition met, call executed
            } else {
                cycles = 12; // Condition not met
            }
            break;
        }

        case 0XCE: { // ADC A, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0xCF: { // RST 08h
            // Push current PC onto the stack (High byte first, then Low byte)
            m_mmu.write(--SP, (PC >> 8) & 0xFF);
            m_mmu.write(--SP, PC & 0xFF);

            // Jump directly to address 0x0008
            PC = 0x0008;

            cycles = 16;
            break;
        }

        case 0xD0: { // RET NC
            if (!get_flag_c()) {
                uint8_t low = m_mmu.read(SP++);
                uint8_t high = m_mmu.read(SP++);
                PC = (high << 8) | low;
                cycles = 20; // Condition met, return executed
            } else {
                cycles = 8;  // Condition not met
            }
            break;
        }

        case 0xD2: { // JP NC, a16
            // Read the 16-bit address (Low byte first, then High byte)
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = low | (high << 8);

            // Check if the Carry flag is NOT set
            if (!get_flag_c()) {
                PC = address; // Take the jump
                cycles = 16;
            } else {
                // Condition not met, skip the jump
                cycles = 12;
            }
            break;
        }

        case 0XD3: { // ILLEGAL_D3

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XD4: { // CALL NC, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t d16 = (high << 8) | low;

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 24;
            break;
        }

        case 0XD6: { // SUB A, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD7: { // RST $10

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XD8: { // RET C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 20;
            break;
        }

        case 0xD9: { // RETI
            PC = stack_pop();
            m_interrupts_enabled = true;
            cycles = 16;
            break;
        }

        case 0XDA: { // JP C, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t d16 = (high << 8) | low;

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XDB: { // ILLEGAL_DB

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XDC: { // CALL C, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t d16 = (high << 8) | low;

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 24;
            break;
        }

        case 0XDD: { // ILLEGAL_DD

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XDE: { // SBC A, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XDF: { // RST $18

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0xE2: { // LDH (C), A
            // Write the value of A into the hardware register at 0xFF00 + C
            m_mmu.write(0xFF00 + BC.low, AF.high);

            cycles = 8;
            break;
        }

        case 0XE3: { // ILLEGAL_E3

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XE4: { // ILLEGAL_E4

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0xE6: { // AND A, d8
            op_and(m_mmu.read(PC++));
            cycles = 8;
            break;
        }

        case 0XE7: { // RST $20

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XE8: { // ADD SP, e8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0xE9: { // JP (HL)
            PC = HL.word;
            cycles = 4;
            break;
        }

        case 0XEB: { // ILLEGAL_EB

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XEC: { // ILLEGAL_EC

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XED: { // ILLEGAL_ED

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XEE: { // XOR A, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XEF: { // RST $28

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0xF0: { // LDH A, a8
            // The auto-generator should already have this line:
            uint8_t d8 = m_mmu.read(PC++);

            // Read from the hardware register (0xFF00 + offset) and store in A
            AF.high = m_mmu.read(0xFF00 + d8);

            cycles = 12;
            break;
        }

        case 0XF2: { // LDH A, C

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0xF3: { // DI (Disable Interrupts)
            m_interrupts_enabled = false;
            cycles = 4;
            break;
        }

        case 0XF4: { // ILLEGAL_F4

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XF6: { // OR A, n8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF7: { // RST $30

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XF8: { // LD HL, SP, e8
            uint8_t d8 = m_mmu.read(PC++);

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0XF9: { // LD SP, HL

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0xFA: { // LD A, (a16)
            // Read the 16-bit address (Low byte first, then High byte)
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = low | (high << 8);

            // Fetch the value from that memory address and put it in A
            AF.high = m_mmu.read(address);

            // 16 cycles: 4 for opcode, 8 to read the address, 4 to read the memory
            cycles = 16;
            break;
        }

        case 0xFB: { // EI
            m_interrupts_enabled = true;
            cycles = 4;
            break;
        }

        case 0XFC: { // ILLEGAL_FC

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0XFD: { // ILLEGAL_FD

            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode << "\n"; exit(1);
            cycles = 4;
            break;
        }

        case 0xFF: { // RST 38h
            // --- REPLACE YOUR 0xFF CASE WITH THIS ---
            printf("\nFell into the 0xFF void! Last 20 PCs:\n");
            for (uint16_t past_pc : pc_history) {
                // We re-read from the MMU to see what opcode was actually there
                printf("PC: 0x%04X, Opcode: 0x%02X\n", past_pc, m_mmu.read(past_pc));
            }
            exit(1); // Stop the emulator!
            break;
        }

        // THE FAIL FAST MECHANISM
        default: {
            std::cerr << "PANIC! Unimplemented Opcode: 0x" << std::hex << (int)opcode
                      << " at PC: 0x" << current_pc << "\n";
            exit(1);
        }
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

int CPU::execute_cb(uint8_t cb_opcode) {
    int cycles = 0;
    switch (cb_opcode) {

        case 0X00: { // RLC B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X01: { // RLC C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X02: { // RLC D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X03: { // RLC E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X04: { // RLC H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X05: { // RLC L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X06: { // RLC HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X07: { // RLC A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X08: { // RRC B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X09: { // RRC C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X0A: { // RRC D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X0B: { // RRC E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X0C: { // RRC H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X0D: { // RRC L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X0E: { // RRC HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X0F: { // RRC A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X10: { // RL B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X11: { // RL C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X12: { // RL D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X13: { // RL E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X14: { // RL H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X15: { // RL L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X16: { // RL HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X17: { // RL A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X18: { // RR B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X19: { // RR C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x1A: { // RR D
            uint8_t old_value = DE.high;
            uint8_t carry = get_flag_c() ? 1 : 0;
            uint8_t new_carry = old_value & 1;

            DE.high = (old_value >> 1) | (carry << 7);

            // Z: Set if the new value is 0
            set_flag_z(DE.high == 0);
            // N: Always 0
            set_flag_n(false);
            // H: Always 0
            set_flag_h(false);
            // C: Set to the bit that was shifted out
            set_flag_c(new_carry);

            cycles = 8;
            break;
        }

        case 0x1B: { // RR E
            uint8_t old_value = DE.low;
            uint8_t carry = get_flag_c() ? 1 : 0;
            uint8_t new_carry = old_value & 1;

            DE.low = (old_value >> 1) | (carry << 7);

            // Z: Set if the new value is 0
            set_flag_z(DE.low == 0);
            // N: Always 0
            set_flag_n(false);
            // H: Always 0
            set_flag_h(false);
            // C: Set to the bit that was shifted out
            set_flag_c(new_carry);

            cycles = 8;
            break;
        }

        case 0X1C: { // RR H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X1D: { // RR L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X1E: { // RR HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X1F: { // RR A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X20: { // SLA B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X21: { // SLA C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X22: { // SLA D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X23: { // SLA E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X24: { // SLA H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X25: { // SLA L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X26: { // SLA HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X27: { // SLA A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X28: { // SRA B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X29: { // SRA C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X2A: { // SRA D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X2B: { // SRA E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X2C: { // SRA H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X2D: { // SRA L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X2E: { // SRA HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X2F: { // SRA A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X30: { // SWAP B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X31: { // SWAP C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X32: { // SWAP D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X33: { // SWAP E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X34: { // SWAP H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X35: { // SWAP L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X36: { // SWAP HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0x37: { // SWAP A
            uint8_t val = AF.high;
            AF.high = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);

            // Z: Set if result is 0
            set_flag_z(AF.high == 0);
            // N: Always 0
            set_flag_n(false);
            // H: Always 0
            set_flag_h(false);
            // C: Always 0
            set_flag_c(false);

            cycles = 8;
            break;
        }

        case 0X38: { // SRL B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X39: { // SRL C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X3A: { // SRL D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X3B: { // SRL E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X3C: { // SRL H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X3D: { // SRL L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X3E: { // SRL HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X3F: { // SRL A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X40: { // BIT 0, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X41: { // BIT 0, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0x42: { // BIT 0, D
            // Check if Bit 0 of Register D is 0
            bool bit_is_zero = (DE.high & (1 << 0)) == 0;

            // Z: Set if the bit is 0, cleared if it is 1
            set_flag_z(bit_is_zero);
            // N: Always 0 (False)
            set_flag_n(false);
            // H: Always 1 (True)
            set_flag_h(true);
            // C: Untouched

            cycles = 8;
            break;
        }

        case 0X43: { // BIT 0, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X44: { // BIT 0, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X45: { // BIT 0, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X46: { // BIT 0, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0x47: { // BIT 0, A
            // Check if Bit 0 of Register A is 0
            bool bit_is_zero = (AF.high & (1 << 0)) == 0;

            // Z: Set if the bit is 0, cleared if it is 1
            set_flag_z(bit_is_zero);
            // N: Always 0 (False)
            set_flag_n(false);
            // H: Always 1 (True)
            set_flag_h(true);
            // C: Untouched

            cycles = 8;
            break;
        }

        case 0X48: { // BIT 1, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X49: { // BIT 1, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X4A: { // BIT 1, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X4B: { // BIT 1, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X4C: { // BIT 1, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X4D: { // BIT 1, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X4E: { // BIT 1, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0x4F: { // BIT 1, A
            // Check if Bit 1 of Register A is 0
            bool bit_is_zero = (AF.high & (1 << 1)) == 0;

            // Z: Set if the bit is 0, cleared if it is 1
            set_flag_z(bit_is_zero);
            // N: Always 0 (False)
            set_flag_n(false);
            // H: Always 1 (True)
            set_flag_h(true);
            // C: Untouched

            cycles = 8;
            break;
        }

        case 0X50: { // BIT 2, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X51: { // BIT 2, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X52: { // BIT 2, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X53: { // BIT 2, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X54: { // BIT 2, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X55: { // BIT 2, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X56: { // BIT 2, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0X57: { // BIT 2, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X58: { // BIT 3, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X59: { // BIT 3, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X5A: { // BIT 3, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X5B: { // BIT 3, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X5C: { // BIT 3, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X5D: { // BIT 3, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X5E: { // BIT 3, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0X5F: { // BIT 3, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X60: { // BIT 4, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X61: { // BIT 4, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X62: { // BIT 4, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X63: { // BIT 4, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X64: { // BIT 4, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X65: { // BIT 4, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X66: { // BIT 4, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0X67: { // BIT 4, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X68: { // BIT 5, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X69: { // BIT 5, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X6A: { // BIT 5, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X6B: { // BIT 5, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X6C: { // BIT 5, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X6D: { // BIT 5, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X6E: { // BIT 5, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0x6F: { // BIT 5, A
            // Check if Bit 5 of Register A is 0
            bool bit_is_zero = (AF.high & (1 << 5)) == 0;

            // Z: Set if the bit is 0, cleared if it is 1
            set_flag_z(bit_is_zero);
            // N: Always 0 (False)
            set_flag_n(false);
            // H: Always 1 (True)
            set_flag_h(true);
            // C: Untouched

            cycles = 8;
            break;
        }

        case 0X70: { // BIT 6, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X71: { // BIT 6, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X72: { // BIT 6, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X73: { // BIT 6, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X74: { // BIT 6, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X75: { // BIT 6, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X76: { // BIT 6, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0x77: { // BIT 6, A
            // Check if Bit 6 of Register A is 0
            bool bit_is_zero = (AF.high & (1 << 6)) == 0;
            
            // Z: Set if the bit is 0, cleared if it is 1
            set_flag_z(bit_is_zero);
            // N: Always 0 (False)
            set_flag_n(false);
            // H: Always 1 (True)
            set_flag_h(true);
            // C: Untouched
            
            cycles = 8;
            break;
        }

        case 0X78: { // BIT 7, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X79: { // BIT 7, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X7A: { // BIT 7, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X7B: { // BIT 7, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X7C: { // BIT 7, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X7D: { // BIT 7, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X7E: { // BIT 7, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 12;
            break;
        }

        case 0X7F: { // BIT 7, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X80: { // RES 0, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X81: { // RES 0, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X82: { // RES 0, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X83: { // RES 0, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X84: { // RES 0, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X85: { // RES 0, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X86: { // RES 0, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0x87: { // RES 0, A
            // Clear (Force to 0) bit 0 of Register A
            AF.high &= ~(1 << 0);
            cycles = 8;
            break;
        }

        case 0X88: { // RES 1, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X89: { // RES 1, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X8A: { // RES 1, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X8B: { // RES 1, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X8C: { // RES 1, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X8D: { // RES 1, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X8E: { // RES 1, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0x8F: { // RES 1, A
            AF.high &= ~(1 << 1);
            cycles = 8;
            break;
        }

        case 0X90: { // RES 2, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X91: { // RES 2, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X92: { // RES 2, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X93: { // RES 2, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X94: { // RES 2, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X95: { // RES 2, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X96: { // RES 2, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0x97: { // RES 2, A
            AF.high &= ~(1 << 2);
            cycles = 8;
            break;
        }

        case 0X98: { // RES 3, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X99: { // RES 3, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X9A: { // RES 3, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X9B: { // RES 3, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X9C: { // RES 3, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X9D: { // RES 3, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0X9E: { // RES 3, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0X9F: { // RES 3, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA0: { // RES 4, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA1: { // RES 4, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA2: { // RES 4, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA3: { // RES 4, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA4: { // RES 4, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA5: { // RES 4, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA6: { // RES 4, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XA7: { // RES 4, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA8: { // RES 5, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XA9: { // RES 5, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XAA: { // RES 5, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XAB: { // RES 5, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XAC: { // RES 5, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XAD: { // RES 5, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XAE: { // RES 5, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0xAF: { // RES 5, A
            AF.high &= ~(1 << 5);
            cycles = 8;
            break;
        }

        case 0XB0: { // RES 6, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB1: { // RES 6, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB2: { // RES 6, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB3: { // RES 6, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB4: { // RES 6, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB5: { // RES 6, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB6: { // RES 6, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XB7: { // RES 6, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB8: { // RES 7, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XB9: { // RES 7, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XBA: { // RES 7, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XBB: { // RES 7, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XBC: { // RES 7, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XBD: { // RES 7, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XBE: { // RES 7, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XBF: { // RES 7, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC0: { // SET 0, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC1: { // SET 0, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC2: { // SET 0, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC3: { // SET 0, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC4: { // SET 0, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC5: { // SET 0, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC6: { // SET 0, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XC7: { // SET 0, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC8: { // SET 1, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XC9: { // SET 1, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XCA: { // SET 1, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XCB: { // SET 1, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XCC: { // SET 1, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XCD: { // SET 1, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XCE: { // SET 1, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0xCF: { // SET 1, A
            // Bitwise OR with 00000010 to set Bit 1
            AF.high |= (1 << 1);

            // CB instructions on registers take 8 cycles total
            // (4 to read the CB prefix, 4 to execute)
            cycles = 8;
            break;
        }

        case 0XD0: { // SET 2, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD1: { // SET 2, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD2: { // SET 2, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD3: { // SET 2, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD4: { // SET 2, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD5: { // SET 2, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD6: { // SET 2, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XD7: { // SET 2, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD8: { // SET 3, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XD9: { // SET 3, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XDA: { // SET 3, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XDB: { // SET 3, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XDC: { // SET 3, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XDD: { // SET 3, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XDE: { // SET 3, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XDF: { // SET 3, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE0: { // SET 4, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE1: { // SET 4, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE2: { // SET 4, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE3: { // SET 4, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE4: { // SET 4, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE5: { // SET 4, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE6: { // SET 4, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XE7: { // SET 4, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE8: { // SET 5, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XE9: { // SET 5, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XEA: { // SET 5, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XEB: { // SET 5, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XEC: { // SET 5, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XED: { // SET 5, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XEE: { // SET 5, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XEF: { // SET 5, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF0: { // SET 6, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF1: { // SET 6, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF2: { // SET 6, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF3: { // SET 6, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF4: { // SET 6, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF5: { // SET 6, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF6: { // SET 6, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XF7: { // SET 6, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF8: { // SET 7, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XF9: { // SET 7, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XFA: { // SET 7, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XFB: { // SET 7, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XFC: { // SET 7, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XFD: { // SET 7, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        case 0XFE: { // SET 7, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 16;
            break;
        }

        case 0XFF: { // SET 7, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cycles = 8;
            break;
        }

        default:
            std::cerr << "PANIC! Unknown CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n";
            exit(1);
    }
    return cycles;
}


void CPU::op_add(uint8_t value) {
    uint8_t a = AF.high;
    int result = a + value;

    AF.high = static_cast<uint8_t>(result);

    // Z: Set if result is 0
    set_flag_z(AF.high == 0);
    // N: Always 0 for Addition
    set_flag_n(false);
    // H: Set if there is a carry from bit 3 to bit 4
    set_flag_h(((a & 0x0F) + (value & 0x0F)) > 0x0F);
    // C: Set if the result overflows 8 bits (greater than 255)
    set_flag_c(result > 0xFF);
}

void CPU::op_or(uint8_t value) {
    // Perform the bitwise OR and save back to A
    AF.high |= value;

    // Z: Set if result is 0
    set_flag_z(AF.high == 0);
    // N, H, and C are always forcibly cleared by an OR instruction
    set_flag_n(false);
    set_flag_h(false);
    set_flag_c(false);
}

void CPU::op_dec(uint8_t& reg) {
    // Check for Half-Borrow BEFORE we change the value
    // If the bottom 4 bits are 0x00, subtracting 1 will borrow from the upper 4 bits
    bool half_borrow = (reg & 0x0F) == 0x00;

    reg--;

    // Z: Set if result is 0
    set_flag_z(reg == 0);
    // N: Always 1 (True) for Decrements
    set_flag_n(true);
    // H: Set if we had a half-borrow
    set_flag_h(half_borrow);
    // C: LEAVE UNTOUCHED! Do not call set_flag_c() at all.
}

void CPU::op_inc(uint8_t& reg) {
    // Check for Half-Carry BEFORE we change the value
    // If the bottom 4 bits are 1111 (0x0F), adding 1 will carry over to the upper 4 bits
    bool half_carry = (reg & 0x0F) == 0x0F;

    reg++;

    // Z: Set if result is 0
    set_flag_z(reg == 0);
    // N: Always 0 (False) for Increments
    set_flag_n(false);
    // H: Set if we had a half-carry
    set_flag_h(half_carry);
    // C: LEAVE UNTOUCHED! Do not call set_flag_c() at all.
}

void CPU::op_and(uint8_t value) {
    // Perform the bitwise AND and save back to A
    AF.high &= value;

    // Z: Set if result is 0
    set_flag_z(AF.high == 0);
    // N: Always forced to 0 (False)
    set_flag_n(false);
    // H: Always forced to 1 (True) for AND instructions!
    set_flag_h(true);
    // C: Always forced to 0 (False)
    set_flag_c(false);
}

void CPU::op_add_hl(uint16_t value) {
    // We use a 32-bit integer to easily catch the overflow for the Carry flag
    uint32_t result = HL.word + value;

    // Z: LEAVE UNTOUCHED! Do not call set_flag_z() at all.
    // N: Always 0 (False) for Additions
    set_flag_n(false);
    // H: Set if carry from bit 11 (the largest bit of the lower 3 nibbles)
    set_flag_h(((HL.word & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF);
    // C: Set if the result overflowed the 16-bit maximum
    set_flag_c(result > 0xFFFF);

    // Store the bottom 16 bits back into HL
    HL.word = (uint16_t)(result & 0xFFFF);
}

void CPU::op_xor(uint8_t value) {
    // Perform the bitwise XOR and save back to A
    AF.high ^= value;

    // Z: Set if result is 0
    set_flag_z(AF.high == 0);
    // N: Always 0 (False) for XOR
    set_flag_n(false);
    // H: Always 0 (False) for XOR
    set_flag_h(false);
    // C: Always 0 (False) for XOR
}

void CPU::op_bit(uint8_t bit, uint8_t value) {
    set_flag_z((value & (1 << bit)) == 0);
    set_flag_n(false);
    set_flag_h(true);
}

void CPU::op_sbc(uint8_t value) {
    // Get the current carry value (1 if set, 0 if not)
    uint8_t carry = get_flag_c() ? 1 : 0;

    // Use a signed integer to easily detect if the result goes below 0 (a borrow)
    int result = AF.high - value - carry;

    // Z: Set if the 8-bit result is 0
    set_flag_z((result & 0xFF) == 0);
    // N: Always 1 (True) for Subtractions
    set_flag_n(true);
    // H: Set if a borrow occurred from bit 4 (the lower nibble)
    set_flag_h(((AF.high & 0x0F) - (value & 0x0F) - carry) < 0);
    // C: Set if a borrow occurred for the entire 8-bit operation
    set_flag_c(result < 0);

    // Store the bottom 8 bits back into A
    AF.high = (uint8_t)(result & 0xFF);
}

void CPU::op_adc(uint8_t value) {
    // Get the current carry value (1 if set, 0 if not)
    uint8_t carry = get_flag_c() ? 1 : 0;

    // Use a 16-bit integer to easily catch the overflow for the Carry flag
    uint16_t result = AF.high + value + carry;

    // Z: Set if the 8-bit result is 0
    set_flag_z((result & 0xFF) == 0);
    // N: Always 0 (False) for Additions
    set_flag_n(false);
    // H: Set if carry from bit 3 (the lower nibble)
    set_flag_h(((AF.high & 0x0F) + (value & 0x0F) + carry) > 0x0F);
    // C: Set if the result overflowed the 8-bit maximum
    set_flag_c(result > 0xFF);

    // Store the bottom 8 bits back into A
    AF.high = (uint8_t)(result & 0xFF);
}

void CPU::op_sub(uint8_t value) {
    // Use a signed integer to catch if the result drops below 0
    int result = AF.high - value;

    // Z: Set if the 8-bit result is 0
    set_flag_z((result & 0xFF) == 0);
    // N: Always 1 (True) for Subtractions
    set_flag_n(true);
    // H: Set if a borrow occurred from bit 4 (the lower nibble)
    set_flag_h(((AF.high & 0x0F) - (value & 0x0F)) < 0);
    // C: Set if a borrow occurred for the entire 8-bit operation (A < value)
    set_flag_c(AF.high < value);

    // Store the bottom 8 bits back into A
    AF.high = (uint8_t)(result & 0xFF);
}