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
            m_halted = false; // A real interrupt arrived! Wake up!
        } else {
            // If no interrupt is pending, do NOTHING and just burn 4 cycles
            return 4;
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
        // std::printf("CART TRACE -> PC: 0x%04X | Opcode: 0x%02X | A: 0x%02X | BC: 0x%04X\n", PC, op, AF.high, BC.word);
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

        case 0x03: { // INC BC
            // 1. Increment the 16-bit register pair BC
            BC.word++;

            // Note: 16-bit INC/DEC instructions do NOT affect any CPU flags!

            // 8 cycles for a 16-bit register increment
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

        case 0x07: { // RLCA
            // 1. Grab the top bit (Bit 7) of A
            uint8_t top_bit = (AF.high >> 7) & 1;

            // 2. Shift A left by 1, and wrap the old top bit into Bit 0
            AF.high = (AF.high << 1) | top_bit;

            // 3. Set the flags
            set_flag_z(false);     // Always 0 for RLCA!
            set_flag_n(false);     // Always 0
            set_flag_h(false);     // Always 0
            set_flag_c(top_bit == 1); // Set if the bit that fell off was 1

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

        case 0x0A: { // LD A, (BC)
            // 1. Read the value from memory at the address stored in BC
            uint8_t value = m_mmu.read(BC.word);

            // 2. Store the value into A (AF.high)
            AF.high = value;

            // 8 cycles: 4 for opcode fetch, 4 to read memory
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

        case 0x0F: { // RRCA
            // 1. Grab the bottom bit (Bit 0) before rotating
            uint8_t old_bit_0 = AF.high & 1;

            // 2. Rotate right by 1 and loop old_bit_0 into Bit 7
            AF.high = (AF.high >> 1) | (old_bit_0 << 7);

            // 3. Set flags (Z is unconditionally cleared for RRCA)
            set_flag_z(false);
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(old_bit_0 == 1);

            cycles = 4;
            break;
        }

        case 0X10: { // STOP n8
            // STOP is technically a 2-byte instruction, so read and discard the 0x00
            m_mmu.read(PC++);
            m_halted = true; // Acts similarly to HALT on a standard DMG
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

        case 0x14: { // INC D
            // 1. Check for half carry (overflow from bit 3 to bit 4)
            bool half_carry = ((DE.high & 0x0F) + 1) > 0x0F;

            // 2. Increment D
            DE.high++;

            // 3. Update flags
            set_flag_z(DE.high == 0); // Set if result is 0
            set_flag_n(false);      // N is cleared for INC
            set_flag_h(half_carry); // Set if carry from bit 3
            // C: Untouched

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

        case 0x17: { // RLA
            uint8_t old_carry = get_flag_c() ? 1 : 0;
            uint8_t old_bit_7 = (AF.high >> 7) & 1;

            AF.high = (AF.high << 1) | old_carry;

            // Quirky GB behavior: RLA and RRA unconditionally clear the Zero flag!
            set_flag_z(false);
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(old_bit_7 == 1);

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

        case 0x1C: { // INC E
            // 1. Check for half carry (overflow from bit 3 to bit 4)
            bool half_carry = ((DE.low & 0x0F) + 1) > 0x0F;

            // 2. Increment E (DE.low)
            DE.low++;

            // 3. Update flags
            set_flag_z(DE.low == 0); // Set if result is 0
            set_flag_n(false);       // N is cleared for INC
            set_flag_h(half_carry);  // Set if carry from bit 3
            // C: Untouched

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

        case 0x1F: { // RRA
            uint8_t old_carry = get_flag_c() ? 1 : 0;
            uint8_t old_bit_0 = AF.high & 1;

            AF.high = (AF.high >> 1) | (old_carry << 7);

            // Zero flag is unconditionally cleared
            set_flag_z(false);
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(old_bit_0 == 1);

            cycles = 4;
            break;
        }

        case 0x23: { // INC HL
            HL.word++;
            cycles = 8;
            break;
        }

        case 0x24: { // INC H
            op_inc(HL.high);
            cycles = 4;
            break;
        }

        case 0x25: { // DEC H
            op_dec(HL.high);
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

        case 0x27: { // DAA
            uint8_t a = AF.high;
            int adjust = 0;
            bool carry = get_flag_c();

            // If we previously added/subtracted and crossed the BCD boundary, adjust
            if (get_flag_h() || (!get_flag_n() && (a & 0x0F) > 9)) {
                adjust |= 0x06;
            }
            if (carry || (!get_flag_n() && a > 0x99)) {
                adjust |= 0x60;
                carry = true;
            }

            if (get_flag_n()) {
                a -= (uint8_t)adjust;
            } else {
                a += (uint8_t)adjust;
            }

            set_flag_z(a == 0);
            set_flag_h(false); // DAA always clears Half-Carry
            set_flag_c(carry);

            AF.high = a;
            cycles = 4;
            break;
        }

        case 0x29: { // ADD HL, HL
            op_add_hl(HL.word);
            cycles = 8;
            break;
        }

        case 0x2B: { // DEC HL
            HL.word--;
            cycles = 8;
            break;
        }

        case 0x2C: { // INC L
            op_inc(HL.low);
            cycles = 4;
            break;
        }

        case 0x2D: { // DEC L
            op_dec(HL.low);
            cycles = 4;
            break;
        }

        case 0x2E: { // LD L, n8
            // 1. Read the immediate 8-bit value from PC and increment PC
            uint8_t value = m_mmu.read(PC++);

            // 2. Store the value into L (HL.low)
            HL.low = value;

            // 8 cycles: 4 for opcode fetch, 4 for immediate operand fetch
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
            SP++;
            cycles = 8;
            break;
        }

        case 0x34: { // INC (HL)
            // 1. Read value from memory at HL
            uint8_t value = m_mmu.read(HL.word);

            // 2. Check for half carry (overflow from bit 3 to bit 4)
            bool half_carry = ((value & 0x0F) + 1) > 0x0F;

            // 3. Increment the value
            value++;

            // 4. Write the incremented value back to memory
            m_mmu.write(HL.word, value);

            // 5. Update flags
            set_flag_z(value == 0); // Set if result is 0
            set_flag_n(false);      // N is cleared for INC
            set_flag_h(half_carry); // Set if carry from bit 3
            // C: Untouched

            // 12 cycles: 4 for opcode fetch, 4 to read memory, 4 to write memory
            cycles = 12;
            break;
        }

        case 0x35: { // DEC (HL)
            // 1. Read value from memory at HL
            uint8_t value = m_mmu.read(HL.word);

            // 2. Check for half carry (borrow from bit 4, i.e., lower nibble was 0)
            bool half_carry = (value & 0x0F) == 0;

            // 3. Decrement the value
            value--;

            // 4. Write the decremented value back to memory
            m_mmu.write(HL.word, value);

            // 5. Update flags
            set_flag_z(value == 0); // Set if result is 0
            set_flag_n(true);       // N is set for DEC
            set_flag_h(half_carry); // Set if borrow from bit 4
            // C: Untouched

            // 12 cycles: 4 for opcode fetch, 4 to read memory, 4 to write memory
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

        case 0x37: { // SCF
            // Z flag is unaffected
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(true);

            cycles = 4;
            break;
        }

        case 0x38: { // JR C, e8
            // Read the immediate value as a SIGNED 8-bit integer
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));

            // Check if the Carry flag is set
            if (get_flag_c()) {
                PC += offset; // Perform the jump
                cycles = 12;
            } else {
                cycles = 8;   // Skip the jump
            }
            break;
        }

        case 0X39: { // ADD HL, SP
            op_add_hl(SP);
            cycles = 8;
            break;
        }

        case 0X3B: { // DEC SP
            SP--;
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

        case 0x3F: { // CCF
            // Invert the carry flag
            set_flag_c(!get_flag_c());

            // N and H are cleared, Z is unaffected
            set_flag_n(false);
            set_flag_h(false);

            cycles = 4;
            break;
        }

        case 0x40: { // LD B, B
            BC.high = BC.high; // Effectively a 4-cycle NOP
            cycles = 4;
            break;
        }

        case 0x41: { // LD B, C
            BC.high = BC.low;
            cycles = 4;
            break;
        }

        case 0x42: { // LD B, D
            BC.high = DE.high;
            cycles = 4;
            break;
        }

        case 0x43: { // LD B, E
            // 1. Copy E (DE.low) into B (BC.high)
            BC.high = DE.low;

            // 4 cycles for a register-to-register load
            cycles = 4;
            break;
        }

        case 0x44: { // LD B, H
            BC.high = HL.high;
            cycles = 4;
            break;
        }

        case 0X45: { // LD B, L
            BC.high = HL.low;
            cycles = 4;
            break;
        }

        case 0x46: { // LD B, (HL)
            // 1. Read the value from memory at the address stored in HL
            uint8_t value = m_mmu.read(HL.word);

            // 2. Store the value into B (BC.high)
            BC.high = value;

            // 8 cycles: 4 for opcode fetch, 4 to read memory
            cycles = 8;
            break;
        }

        case 0x47: { // LD B, A
            BC.high = AF.high;
            cycles = 4;
            break;
        }

        case 0X48: { // LD C, B
            BC.low = BC.high;
            cycles = 4;
            break;
        }

        case 0X49: { // LD C, C
            BC.low = BC.low;
            cycles = 4;
            break;
        }

        case 0x4A: { // LD C, D
            BC.low = DE.high;
            cycles = 4;
            break;
        }

        case 0X4B: { // LD C, E
            BC.low = DE.low;
            cycles = 4;
            break;
        }

        case 0X4C: { // LD C, H
            BC.low = HL.high;
            cycles = 4;
            break;
        }

        case 0x4D: { // LD C, L
            BC.low = HL.low;
            cycles = 4;
            break;
        }

        case 0X4E: { // LD C, HL
            BC.low = m_mmu.read(HL.word);
            cycles = 8;
            break;
        }

        case 0x4F: { // LD C, A
            BC.low = AF.high;
            cycles = 4;
            break;
        }

        case 0x50: { // LD D, B
            DE.high = BC.high;
            cycles = 4;
            break;
        }

        case 0x51: { // LD D, C
            DE.high = BC.low;
            cycles = 4;
            break;
        }

        case 0X52: { // LD D, D
            DE.high = DE.high;
            cycles = 4;
            break;
        }

        case 0x53: { // LD D, E
            // 1. Copy E (DE.low) into D (DE.high)
            DE.high = DE.low;

            // 4 cycles for a register-to-register load
            cycles = 4;
            break;
        }

        case 0x54: { // LD D, H
            DE.high = HL.high;
            cycles = 4;
            break;
        }

        case 0X55: { // LD D, L
            DE.high = HL.low;
            cycles = 4;
            break;
        }

        case 0x56: { // LD D, (HL)
            DE.high = m_mmu.read(HL.word);
            cycles = 8;
            break;
        }

        case 0x57: { // LD D, A
            DE.high = AF.high;
            cycles = 4;
            break;
        }

        case 0x58: { // LD E, B
            // 1. Copy B (BC.high) into E (DE.low)
            DE.low = BC.high;

            // 4 cycles for a register-to-register load
            cycles = 4;
            break;
        }

        case 0x59: { // LD E, C
            // Copy the value of C (BC.low) into E (DE.low)
            DE.low = BC.low;

            // 4 cycles for a register-to-register load
            cycles = 4;
            break;
        }

        case 0X5A: { // LD E, D
            DE.low = DE.high;
            cycles = 4;
            break;
        }

        case 0X5B: { // LD E, E
            DE.low = DE.low;
            cycles = 4;
            break;
        }

        case 0X5C: { // LD E, H
            DE.low = HL.high;
            cycles = 4;
            break;
        }

        case 0x5D: { // LD E, L
            DE.low = HL.low;
            cycles = 4;
            break;
        }

        case 0x5E: { // LD E, (HL)
            // 1. Read the value from memory at the address stored in HL
            uint8_t value = m_mmu.read(HL.word);

            // 2. Store the value into E (DE.low)
            DE.low = value;

            // 8 cycles: 4 for opcode fetch, 4 to read memory
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
            HL.high = BC.low;
            cycles = 4;
            break;
        }

        case 0x62: { // LD H, D
            HL.high = DE.high;
            cycles = 4;
            break;
        }

        case 0X63: { // LD H, E
            HL.high = DE.low;
            cycles = 4;
            break;
        }

        case 0X64: { // LD H, H
            HL.high = HL.high;
            cycles = 4;
            break;
        }

        case 0X65: { // LD H, L
            HL.high = HL.low;
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

        case 0x68: { // LD L, B
            HL.low = BC.high;
            cycles = 4;
            break;
        }

        case 0x69: { // LD L, C
            HL.low = BC.low;
            cycles = 4;
            break;
        }

        case 0X6A: { // LD L, D
            HL.low = DE.high;
            cycles = 4;
            break;
        }

        case 0x6B: { // LD L, E
            HL.low = DE.low;
            cycles = 4;
            break;
        }

        case 0X6C: { // LD L, H
            HL.low = HL.high;
            cycles = 4;
            break;
        }

        case 0X6D: { // LD L, L
            HL.low = HL.low;
            cycles = 4;
            break;
        }

        case 0x6E: { // LD L, (HL)
            HL.low = m_mmu.read(HL.word);
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
            // std::printf("CPU HALTED at PC: 0x%04X | IE: 0x%02X | IF: 0x%02X\n", PC - 1, ie, if_reg);

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
            op_sbc(BC.low);
            cycles = 4;
            break;
        }

        case 0X9A: { // SBC A, D
            op_sbc(DE.high);
            cycles = 4;
            break;
        }

        case 0X9B: { // SBC A, E
            op_sbc(DE.low);
            cycles = 4;
            break;
        }

        case 0X9C: { // SBC A, H
            op_sbc(HL.high);
            cycles = 4;
            break;
        }

        case 0X9D: { // SBC A, L
            op_sbc(HL.low);
            cycles = 4;
            break;
        }

        case 0X9E: { // SBC A, HL
            op_sbc(m_mmu.read(HL.word));
            cycles = 8;
            break;
        }

        case 0X9F: { // SBC A, A
            op_sbc(AF.high);
            cycles = 4;
            break;
        }

        case 0xA0: { // AND B
            op_and(BC.high);
            cycles = 4;
            break;
        }

        case 0XA1: { // AND A, C
            op_and(BC.low);
            cycles = 4;
            break;
        }

        case 0XA2: { // AND A, D
            op_and(DE.high);
            cycles = 4;
            break;
        }

        case 0xA3: { // AND E
            op_and(DE.low);
            cycles = 4;
            break;
        }

        case 0xA4: { // AND H
            op_and(HL.high);
            cycles = 4;
            break;
        }

        case 0XA5: { // AND A, L
            op_and(HL.low);
            cycles = 4;
            break;
        }

        case 0xA6: { // AND (HL)
            op_and(m_mmu.read(HL.word));
            cycles = 8;
            break;
        }

        case 0xA7: { // AND A
            op_and(AF.high);
            cycles = 4;
            break;
        }

        case 0xA8: { // XOR B
            op_xor(BC.high);
            cycles = 4;
            break;
        }

        case 0XA9: { // XOR A, C
            op_xor(BC.low);
            cycles = 4;
            break;
        }

        case 0XAA: { // XOR A, D
            op_xor(DE.high);
            cycles = 4;
            break;
        }

        case 0xAB: { // XOR E
            op_xor(DE.low);
            cycles = 4;
            break;
        }

        case 0XAC: { // XOR A, H
            op_xor(HL.high);
            cycles = 4;
            break;
        }

        case 0XAD: { // XOR A, L
            op_xor(HL.low);
            cycles = 4;
            break;
        }

        case 0xAE: { // XOR (HL)
            // Read the value from memory at HL and XOR it with A
            op_xor(m_mmu.read(HL.word));

            // 8 cycles: 4 for opcode fetch, 4 to read memory
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

        case 0xB2: { // OR D
            op_or(DE.high);

            cycles = 4;
            break;
        }

        case 0xB3: { // OR E
            op_or(DE.low);
            cycles = 4;
            break;
        }

        case 0xB4: { // OR H
            // Perform bitwise OR between A and H and update flags
            op_or(HL.high);

            // 4 cycles for a register-to-register operation
            cycles = 4;
            break;
        }

        case 0XB5: { // OR A, L
            op_or(HL.low);
            cycles = 4;
            break;
        }

        case 0xB6: { // OR (HL)
            // Read the value from memory at HL and OR it with A
            op_or(m_mmu.read(HL.word));

            // 8 cycles: 4 for opcode fetch, 4 to read memory
            cycles = 8;
            break;
        }

        case 0XB7: { // OR A, A
            op_or(AF.high);
            cycles = 4;
            break;
        }

        case 0xB9: { // CP C
            op_cp(BC.low);
            cycles = 4;
            break;
        }

        case 0xBA: { // CP D
            op_cp(DE.high);
            cycles = 4;
            break;
        }

        case 0xBB: { // CP E
            op_cp(DE.low);
            cycles = 4;
            break;
        }

        case 0xBC: { // CP H
            op_cp(HL.high);
            cycles = 4;
            break;
        }

        case 0xBD: { // CP L
            op_cp(HL.low);
            cycles = 4;
            break;
        }

        case 0xBE: { // CP (HL)
            op_cp(m_mmu.read(HL.word));
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

        case 0xC2: { // JP NZ, a16
            // 1. Read the 16-bit absolute address (little-endian: low byte first)
            uint8_t low_byte = m_mmu.read(PC++);
            uint8_t high_byte = m_mmu.read(PC++);
            uint16_t address = (high_byte << 8) | low_byte;

            // 2. Check if the Zero flag is NOT set
            if (!get_flag_z()) {
                PC = address; // Perform the jump
                cycles = 16;
            } else {
                cycles = 12;  // Skip the jump
            }
            break;
        }

        case 0xC4: { // CALL NZ, a16
            // 1. Fetch the 16-bit target address (low byte first)
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = (high << 8) | low;

            // 2. Check if the Zero flag is NOT set
            if (!get_flag_z()) {
                // Push the current PC (return address) onto the stack
                stack_push(PC);

                // Jump to the target address
                PC = address;

                cycles = 24; // Condition met, call takes 24 cycles
            } else {
                cycles = 12; // Condition not met, skip the call
            }
            break;
        }

        case 0xC6: { // ADD A, n8
            // 1. Fetch the 8-bit immediate value
            uint8_t d8 = m_mmu.read(PC++);

            // 2. Calculate the result
            uint16_t result = AF.high + d8;

            // 3. Update flags
            set_flag_z((result & 0xFF) == 0);
            set_flag_n(false);
            set_flag_h(((AF.high & 0x0F) + (d8 & 0x0F)) > 0x0F);
            set_flag_c(result > 0xFF);

            // 4. Store the lower 8 bits back in A
            AF.high = static_cast<uint8_t>(result & 0xFF);

            cycles = 8;
            break;
        }

        case 0XC7: { // RST $00
            stack_push(PC);
            PC = 0x0000;
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
            op_adc(m_mmu.read(PC++));
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

        case 0xD4: { // CALL NC, a16
            // 1. Fetch the 16-bit target address (low byte first)
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = (high << 8) | low;

            // 2. Check if the Carry flag is NOT set
            if (!get_flag_c()) {
                // Push the current PC (return address) onto the stack
                stack_push(PC);

                // Jump to the target address
                PC = address;

                cycles = 24; // Condition met, call takes 24 cycles
            } else {
                cycles = 12; // Condition not met, skip the call
            }
            break;
        }

        case 0xD6: { // SUB A, n8
            uint8_t d8 = m_mmu.read(PC++);

            op_sub(d8);

            cycles = 8;
            break;
        }

        case 0XD7: { // RST $10
            stack_push(PC);
            PC = 0x0010;
            cycles = 16;
            break;
        }

        case 0xD8: { // RET C
            if (get_flag_c()) {
                // 1. Pop the return address from the stack (low byte first, then high byte)
                uint8_t low_byte = m_mmu.read(SP++);
                uint8_t high_byte = m_mmu.read(SP++);

                // 2. Restore PC to the return address
                PC = (high_byte << 8) | low_byte;

                cycles = 20; // 4 to check + 16 for stack pop
            } else {
                cycles = 8;  // Condition false, takes fewer cycles
            }
            break;
        }

        case 0xD9: { // RETI
            PC = stack_pop();
            m_interrupts_enabled = true;
            cycles = 16;
            break;
        }

        case 0xDA: { // JP C, a16
            // 1. Fetch the 16-bit target address (low byte first, then high byte)
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = (high << 8) | low;

            // 2. Check the carry flag and jump if set
            if (get_flag_c()) {
                PC = address;
                cycles = 16; // Condition met, jump takes 16 cycles
            } else {
                cycles = 12; // Condition not met, takes 12 cycles
            }
            break;
        }

        case 0XDC: { // CALL C, a16
            uint8_t low = m_mmu.read(PC++);
            uint8_t high = m_mmu.read(PC++);
            uint16_t address = (high << 8) | low;

            if (get_flag_c()) {
                stack_push(PC);
                PC = address;
                cycles = 24;
            } else {
                cycles = 12;
            }
            break;
        }

        case 0xDE: { // SBC A, d8
            // 1. Fetch the 8-bit immediate value
            uint8_t d8 = m_mmu.read(PC++);

            // 2. Subtract value + carry from A and update flags
            op_sbc(d8);

            // 8 cycles: 4 for opcode fetch, 4 to read the immediate value
            cycles = 8;
            break;
        }

        case 0XDF: { // RST $18
            stack_push(PC);
            PC = 0x0018;
            cycles = 16;
            break;
        }

        case 0xE2: { // LDH (C), A
            // Write the value of A into the hardware register at 0xFF00 + C
            m_mmu.write(0xFF00 + BC.low, AF.high);

            cycles = 8;
            break;
        }

        case 0xE6: { // AND A, d8
            op_and(m_mmu.read(PC++));
            cycles = 8;
            break;
        }

        case 0XE7: { // RST $20
            stack_push(PC);
            PC = 0x0020;
            cycles = 16;
            break;
        }

        case 0xE8: { // ADD SP, e8
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));

            set_flag_z(false);
            set_flag_n(false);
            set_flag_h(((SP & 0x0F) + (offset & 0x0F)) > 0x0F);
            set_flag_c(((SP & 0xFF) + (offset & 0xFF)) > 0xFF);

            // Apply directly to the Stack Pointer
            SP = SP + offset;

            cycles = 16;
            break;
        }

        case 0xE9: { // JP (HL)
            PC = HL.word;
            cycles = 4;
            break;
        }

        case 0xEE: { // XOR n8
            // 1. Fetch the 8-bit immediate value
            uint8_t d8 = m_mmu.read(PC++);

            // 2. Perform bitwise XOR with A
            AF.high ^= d8;

            // 3. Update flags (Z is set if result is 0, N, H, and C are cleared)
            set_flag_z(AF.high == 0);
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(false);

            cycles = 8;
            break;
        }

        case 0XEF: { // RST $28
            stack_push(PC);
            PC = 0x0028;
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

        case 0XF2: { // LDH A, (C)
            // Read from the hardware register (0xFF00 + C) and store in A
            AF.high = m_mmu.read(0xFF00 + BC.low);
            cycles = 8;
            break;
        }

        case 0xF3: { // DI (Disable Interrupts)
            m_interrupts_enabled = false;
            cycles = 4;
            break;
        }

        case 0xF6: { // OR n8
            // 1. Fetch the 8-bit immediate value
            uint8_t d8 = m_mmu.read(PC++);

            // 2. Perform bitwise OR with A
            AF.high |= d8;

            // 3. Update flags (Z is set if result is 0, N, H, and C are cleared)
            set_flag_z(AF.high == 0);
            set_flag_n(false);
            set_flag_h(false);
            set_flag_c(false);

            cycles = 8;
            break;
        }

        case 0XF7: { // RST $30
            stack_push(PC);
            PC = 0x0030;
            cycles = 16;
            break;
        }

        case 0xF8: { // LD HL, SP+e8
            // Read the immediate value as a SIGNED 8-bit integer (-128 to 127)
            int8_t offset = static_cast<int8_t>(m_mmu.read(PC++));

            // Z and N are always cleared
            set_flag_z(false);
            set_flag_n(false);

            // H and C flags are calculated based on the lower 8-bits!
            set_flag_h(((SP & 0x0F) + (offset & 0x0F)) > 0x0F);
            set_flag_c(((SP & 0xFF) + (offset & 0xFF)) > 0xFF);

            // Store the final 16-bit result in HL
            HL.word = SP + offset;

            cycles = 12;
            break;
        }

        case 0xF9: { // LD SP, HL
            // Copy the 16-bit value from HL directly into the Stack Pointer
            SP = HL.word;

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

        case 0xD3:
        case 0xDB:
        case 0xDD:
        case 0xE3:
        case 0xE4:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xF4:
        case 0xFC:
        case 0xFD: { // ILLEGAL OPCODES
            std::printf("\n--- CRASH: Hit Illegal Opcode 0x%02X at PC 0x%04X ---\n", opcode, current_pc);

            // Dump the last 200 instructions and the exact register state
            print_trace();

            exit(1);
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
    // Decode the CB opcode matrix: [xx] [yyy] [zzz]
    uint8_t category  = (cb_opcode >> 6) & 0x03; // Top 2 bits (Operation Category)
    uint8_t op_or_bit = (cb_opcode >> 3) & 0x07; // Middle 3 bits (Specific Op or Bit 0-7)
    uint8_t reg_code  = cb_opcode & 0x07;        // Bottom 3 bits (Target Register)

    // 1. Fetch the value based on the target register
    uint8_t value = 0;
    int cycles = 8; // Most register operations take 8 cycles

    switch (reg_code) {
        case 0: value = BC.high; break;
        case 1: value = BC.low;  break;
        case 2: value = DE.high; break;
        case 3: value = DE.low;  break;
        case 4: value = HL.high; break;
        case 5: value = HL.low;  break;
        case 6:
            value = m_mmu.read(HL.word);
            cycles = 12; // Reading from (HL) takes 4 extra cycles
            break;
        case 7: value = AF.high; break;
    }

    // 2. Execute the operation
    if (category == 0) { // 0x00 - 0x3F: Rotates & Shifts
        switch (op_or_bit) {
            case 0: { // RLC
                uint8_t bit7 = (value >> 7) & 1;
                value = (value << 1) | bit7;
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit7 == 1);
                break;
            }
            case 1: { // RRC
                uint8_t bit0 = value & 1;
                value = (value >> 1) | (bit0 << 7);
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit0 == 1);
                break;
            }
            case 2: { // RL
                uint8_t bit7 = (value >> 7) & 1;
                uint8_t carry = get_flag_c() ? 1 : 0;
                value = (value << 1) | carry;
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit7 == 1);
                break;
            }
            case 3: { // RR
                uint8_t bit0 = value & 1;
                uint8_t carry = get_flag_c() ? 1 : 0;
                value = (value >> 1) | (carry << 7);
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit0 == 1);
                break;
            }
            case 4: { // SLA
                uint8_t bit7 = (value >> 7) & 1;
                value <<= 1;
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit7 == 1);
                break;
            }
            case 5: { // SRA
                uint8_t bit0 = value & 1;
                value = (value >> 1) | (value & 0x80); // Preserve bit 7
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit0 == 1);
                break;
            }
            case 6: { // SWAP
                value = ((value & 0x0F) << 4) | ((value & 0xF0) >> 4);
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(false);
                break;
            }
            case 7: { // SRL
                uint8_t bit0 = value & 1;
                value >>= 1;
                set_flag_z(value == 0); set_flag_n(false); set_flag_h(false); set_flag_c(bit0 == 1);
                break;
            }
        }
    }
    else if (category == 1) { // 0x40 - 0x7F: BIT
        bool bit_is_zero = (value & (1 << op_or_bit)) == 0;
        set_flag_z(bit_is_zero);
        set_flag_n(false);
        set_flag_h(true);
        // Carry flag remains completely untouched
    }
    else if (category == 2) { // 0x80 - 0xBF: RES
        value &= ~(1 << op_or_bit);
    }
    else if (category == 3) { // 0xC0 - 0xFF: SET
        value |= (1 << op_or_bit);
    }

    // 3. Write the modified value back (if it wasn't a BIT instruction)
    if (category != 1) {
        switch (reg_code) {
            case 0: BC.high = value; break;
            case 1: BC.low  = value; break;
            case 2: DE.high = value; break;
            case 3: DE.low  = value; break;
            case 4: HL.high = value; break;
            case 5: HL.low  = value; break;
            case 6:
                m_mmu.write(HL.word, value);
                cycles = 16; // Memory modification instructions always take 16 cycles
                break;
            case 7: AF.high = value; break;
        }
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