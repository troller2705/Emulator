#include "Core_Genesis/genesis_cpu.h"
#include "Core_Genesis/genesis_mmu.h"
#include <iostream>
#include <cstdio>

GenesisCPU::GenesisCPU(GenesisMMU& mmu) : m_mmu(mmu) {
    reset();
}

void GenesisCPU::reset() {
    D.fill(0);
    A.fill(0);
    SR = 0x2700; // Supervisor Mode, Interrupt Mask = 7
    m_stopped = false;

    SSP  = m_mmu.read32(0x000000);
    PC   = m_mmu.read32(0x000004);
    A[7] = SSP;

    std::printf("[M68K_CPU] Reset! SSP: 0x%08X | Boot PC: 0x%08X\n", SSP, PC);
}

void GenesisCPU::set_sr(uint16_t new_sr) {
    bool was_super = (SR & 0x2000) != 0;
    bool now_super = (new_sr & 0x2000) != 0;

    if (was_super && !now_super) {
        SSP  = A[7];
        A[7] = USP;
    } else if (!was_super && now_super) {
        USP  = A[7];
        A[7] = SSP;
    }
    SR = new_sr & 0xA71F;
}

uint16_t GenesisCPU::fetch16() {
    uint16_t val = m_mmu.read16(PC);
    PC += 2;
    return val;
}

uint32_t GenesisCPU::fetch32() {
    uint32_t val = m_mmu.read32(PC);
    PC += 4;
    return val;
}

void GenesisCPU::push16(uint16_t val) {
    A[7] -= 2;
    m_mmu.write16(A[7], val);
}

void GenesisCPU::push32(uint32_t val) {
    A[7] -= 4;
    m_mmu.write32(A[7], val);
}

uint16_t GenesisCPU::pop16() {
    uint16_t val = m_mmu.read16(A[7]);
    A[7] += 2;
    return val;
}

uint32_t GenesisCPU::pop32() {
    uint32_t val = m_mmu.read32(A[7]);
    A[7] += 4;
    return val;
}

void GenesisCPU::trigger_interrupt(uint8_t level) {
    uint8_t mask = (SR >> 8) & 0x07;
    if (level > mask || level == 7) {
        m_stopped = false;
        uint16_t old_sr = SR;
        set_sr((SR | 0x2000) & ~0x8700); // Enter Supervisor Mode, clear Trace
        SR |= (level << 8);

        push32(PC);
        push16(old_sr);

        // Autovector N starts at Vector (24 + level) -> Address (24 + level) * 4
        PC = m_mmu.read32((24 + level) * 4);
    }
}

bool GenesisCPU::eval_cond(uint8_t cond) const {
    bool c = flag_c(), v = flag_v(), z = flag_z(), n = flag_n();
    switch (cond & 0x0F) {
        case 0x0: return true;               // T  (True)
        case 0x1: return false;              // F  (False)
        case 0x2: return !c && !z;           // HI (Higher)
        case 0x3: return c || z;             // LS (Lower or Same)
        case 0x4: return !c;                 // CC (Carry Clear)
        case 0x5: return c;                  // CS (Carry Set)
        case 0x6: return !z;                 // NE (Not Equal)
        case 0x7: return z;                  // EQ (Equal)
        case 0x8: return !v;                 // VC (Overflow Clear)
        case 0x9: return v;                  // VS (Overflow Set)
        case 0xA: return !n;                 // PL (Plus)
        case 0xB: return n;                  // MI (Minus)
        case 0xC: return n == v;             // GE (Greater or Equal)
        case 0xD: return n != v;             // LT (Less Than)
        case 0xE: return !z && (n == v);     // GT (Greater Than)
        case 0xF: return z || (n != v);      // LE (Less or Equal)
        default:  return false;
    }
}

uint32_t GenesisCPU::calc_ea_addr(uint8_t mode, uint8_t reg, int size) {
    switch (mode) {
        case 2: return A[reg]; // (An)
        case 3: {              // (An)+
            uint32_t step = (size == 1 && reg == 7) ? 2 : size;
            uint32_t addr = A[reg];
            A[reg] += step;
            return addr;
        }
        case 4: {              // -(An)
            uint32_t step = (size == 1 && reg == 7) ? 2 : size;
            A[reg] -= step;
            return A[reg];
        }
        case 5: {              // (d16, An)
            int16_t disp = static_cast<int16_t>(fetch16());
            return A[reg] + disp;
        }
        case 6: {              // (d8, An, Xn)
            uint16_t ext = fetch16();
            int8_t disp  = static_cast<int8_t>(ext & 0xFF);
            uint8_t idx_reg = (ext >> 12) & 0x07;
            uint32_t idx_val = (ext & 0x8000) ? A[idx_reg] : D[idx_reg];
            if ((ext & 0x0800) == 0) {
                idx_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(idx_val & 0xFFFF)));
            }
            return A[reg] + disp + idx_val;
        }
        case 7: {
            switch (reg) {
                case 0: return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(fetch16()))); // (xxx).W
                case 1: return fetch32(); // (xxx).L
                case 2: {                 // (d16, PC)
                    uint32_t base_pc = PC;
                    int16_t disp = static_cast<int16_t>(fetch16());
                    return base_pc + disp;
                }
                case 3: {                 // (d8, PC, Xn)
                    uint32_t base_pc = PC;
                    uint16_t ext = fetch16();
                    int8_t disp  = static_cast<int8_t>(ext & 0xFF);
                    uint8_t idx_reg = (ext >> 12) & 0x07;
                    uint32_t idx_val = (ext & 0x8000) ? A[idx_reg] : D[idx_reg];
                    if ((ext & 0x0800) == 0) {
                        idx_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(idx_val & 0xFFFF)));
                    }
                    return base_pc + disp + idx_val;
                }
            }
        }
    }
    return 0;
}

uint32_t GenesisCPU::read_ea(uint8_t mode, uint8_t reg, int size, uint32_t& out_addr) {
    if (mode == 0) {
        if (size == 1) return D[reg] & 0xFF;
        if (size == 2) return D[reg] & 0xFFFF;
        return D[reg];
    }
    if (mode == 1) {
        if (size == 2) return A[reg] & 0xFFFF;
        return A[reg];
    }
    if (mode == 7 && reg == 4) { // #imm
        if (size == 1) return fetch16() & 0xFF;
        if (size == 2) return fetch16();
        return fetch32();
    }

    out_addr = calc_ea_addr(mode, reg, size);
    if (size == 1) return m_mmu.read8(out_addr);
    if (size == 2) return m_mmu.read16(out_addr);
    return m_mmu.read32(out_addr);
}

void GenesisCPU::write_ea(uint8_t mode, uint8_t reg, int size, uint32_t addr, uint32_t val) {
    if (mode == 0) {
        if (size == 1)      D[reg] = (D[reg] & 0xFFFFFF00U) | (val & 0xFFU);
        else if (size == 2) D[reg] = (D[reg] & 0xFFFF0000U) | (val & 0xFFFFU);
        else                D[reg] = val;
        return;
    }
    if (mode == 1) {
        // Writing a 16-bit word to an Address Register always sign-extends to 32 bits!
        if (size == 2) A[reg] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(val & 0xFFFF)));
        else           A[reg] = val;
        return;
    }

    if (size == 1)      m_mmu.write8(addr, static_cast<uint8_t>(val & 0xFF));
    else if (size == 2) m_mmu.write16(addr, static_cast<uint16_t>(val & 0xFFFF));
    else                m_mmu.write32(addr, val);
}

static uint32_t sign_mask(int size) {
    return (size == 1) ? 0x80U : ((size == 2) ? 0x8000U : 0x80000000U);
}

static uint32_t val_mask(int size) {
    return (size == 1) ? 0xFFU : ((size == 2) ? 0xFFFFU : 0xFFFFFFFFU);
}

int GenesisCPU::step() {
    if (m_stopped) return 4;

    uint32_t instr_pc = PC;
    uint16_t op = fetch16();
    uint8_t  group = (op >> 12) & 0x0F;

    auto set_nz = [&](uint32_t res, int size) {
        uint32_t m = val_mask(size);
        uint32_t s = sign_mask(size);
        set_z((res & m) == 0);
        set_n((res & s) != 0);
    };

    auto do_add = [&](uint32_t src, uint32_t dst, int size, bool update_x) -> uint32_t {
        uint64_t m = val_mask(size);
        uint32_t s = sign_mask(size);
        uint64_t s_m = src & m;
        uint64_t d_m = dst & m;
        uint64_t res = s_m + d_m;
        bool carry = res > m;
        bool ovf = ((~(s_m ^ d_m) & (s_m ^ res)) & s) != 0;
        set_c(carry);
        if (update_x) set_x(carry);
        set_v(ovf);
        set_nz(static_cast<uint32_t>(res), size);
        return static_cast<uint32_t>(res & m);
    };

    auto do_sub = [&](uint32_t src, uint32_t dst, int size, bool update_x, bool is_cmp) -> uint32_t {
        uint64_t m = val_mask(size);
        uint32_t s = sign_mask(size);
        uint64_t s_m = src & m;
        uint64_t d_m = dst & m;
        uint64_t res = d_m - s_m;
        bool borrow = s_m > d_m;
        bool ovf = (((d_m ^ s_m) & (d_m ^ res)) & s) != 0;
        set_c(borrow);
        if (update_x && !is_cmp) set_x(borrow);
        set_v(ovf);
        set_nz(static_cast<uint32_t>(res), size);
        return static_cast<uint32_t>(res & m);
    };

    switch (group) {
        // --- GROUP 0: Immediate ALU & Bit Manipulation (ORI, ANDI, SUBI, ADDI, EORI, CMPI, BTST/BSET/BCLR/BCHG) ---
        case 0x0: {
            uint8_t mode = (op >> 3) & 0x07;
            uint8_t reg  = op & 0x07;

            // Dynamic & Static Bit Operations: BTST (00), BCHG (01), BCLR (10), BSET (11)
            if ((op & 0x0100) || ((op & 0xFF00) == 0x0800)) {
                uint32_t bit_num = (op & 0x0100) ? D[(op >> 9) & 7] : (fetch16() & 0xFF);
                uint8_t  bit_op  = (op >> 6) & 0x03;
                int      size    = (mode == 0) ? 4 : 1;
                bit_num &= (size == 4) ? 31 : 7;

                uint32_t addr = 0;
                uint32_t val  = read_ea(mode, reg, size, addr);
                set_z((val & (1U << bit_num)) == 0);

                if (bit_op == 1)      val ^=  (1U << bit_num); // BCHG
                else if (bit_op == 2) val &= ~(1U << bit_num); // BCLR
                else if (bit_op == 3) val |=  (1U << bit_num); // BSET

                if (bit_op != 0) write_ea(mode, reg, size, addr, val);
                return 8;
            }

            // Immediate Operations: ORI (000), ANDI (001), SUBI (010), ADDI (011), EORI (101), CMPI (110)
            uint8_t sub_op  = (op >> 9) & 0x07;
            uint8_t sz_bits = (op >> 6) & 0x03;
            int size = (sz_bits == 0) ? 1 : ((sz_bits == 1) ? 2 : 4);

            // ORI / ANDI / EORI to CCR or SR
            if (mode == 7 && reg == 4) {
                uint16_t imm = fetch16();
                if (sub_op == 0)      set_sr(SR | imm);
                else if (sub_op == 1) set_sr(SR & (sz_bits == 0 ? (0xFF00 | (imm & 0xFF)) : imm));
                else if (sub_op == 5) set_sr(SR ^ imm);
                return 16;
            }

            uint32_t imm = (size == 4) ? fetch32() : (fetch16() & val_mask(size));
            uint32_t addr = 0;
            uint32_t dst = read_ea(mode, reg, size, addr);
            uint32_t res = dst;

            if (sub_op == 0)      { res = dst | imm; set_nz(res, size); set_c(false); set_v(false); } // ORI
            else if (sub_op == 1) { res = dst & imm; set_nz(res, size); set_c(false); set_v(false); } // ANDI
            else if (sub_op == 2) { res = do_sub(imm, dst, size, true, false); }                      // SUBI
            else if (sub_op == 3) { res = do_add(imm, dst, size, true); }                             // ADDI
            else if (sub_op == 5) { res = dst ^ imm; set_nz(res, size); set_c(false); set_v(false); } // EORI
            else if (sub_op == 6) { do_sub(imm, dst, size, false, true); return 8; }                  // CMPI

            write_ea(mode, reg, size, addr, res);
            return 12;
        }

        // --- GROUPS 1, 2, 3: MOVE.B (01), MOVE.L (10), MOVE.W (11) & MOVEA ---
        case 0x1:
        case 0x2:
        case 0x3: {
            int size = (group == 1) ? 1 : ((group == 3) ? 2 : 4);
            uint8_t src_mode = (op >> 3) & 0x07;
            uint8_t src_reg  = op & 0x07;
            uint8_t dst_mode = (op >> 6) & 0x07;
            uint8_t dst_reg  = (op >> 9) & 0x07;

            uint32_t src_addr = 0;
            uint32_t val = read_ea(src_mode, src_reg, size, src_addr);

            if (dst_mode != 1) { // MOVEA does not affect flags!
                set_nz(val, size);
                set_c(false);
                set_v(false);
            }

            uint32_t dst_addr = (dst_mode >= 2) ? calc_ea_addr(dst_mode, dst_reg, size) : 0;
            write_ea(dst_mode, dst_reg, size, dst_addr, val);
            return 8;
        }

        // --- GROUP 4: Miscellaneous (LEA, PEA, CLR, TST, NEG, NOT, EXT, SWAP, MOVEM, JSR, JMP, RTS, RTE, TRAP) ---
        case 0x4: {
            uint8_t mode = (op >> 3) & 0x07;
            uint8_t reg  = op & 0x07;

            if (op == 0x4E71) return 4; // NOP
            if (op == 0x4E75) { PC = pop32(); return 16; } // RTS
            if (op == 0x4E73) { uint16_t s = pop16(); PC = pop32(); set_sr(s); return 20; } // RTE
            if (op == 0x4E70) return 4; // RESET
            if (op == 0x4E72) { uint16_t imm = fetch16(); set_sr(imm); m_stopped = true; return 4; } // STOP

            // SWAP Dn
            if ((op & 0xFFF8) == 0x4840) {
                D[reg] = (D[reg] >> 16) | (D[reg] << 16);
                set_nz(D[reg], 4); set_c(false); set_v(false);
                return 4;
            }
            // EXT.W / EXT.L Dn
            if ((op & 0xFFB8) == 0x4880) {
                if ((op & 0x0040) == 0) { // EXT.W
                    uint16_t w = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(D[reg] & 0xFF)));
                    D[reg] = (D[reg] & 0xFFFF0000U) | w;
                    set_nz(w, 2);
                } else {                  // EXT.L
                    D[reg] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(D[reg] & 0xFFFF)));
                    set_nz(D[reg], 4);
                }
                set_c(false); set_v(false);
                return 4;
            }
            // LEA <ea>, An
            if ((op & 0xF1C0) == 0x41C0) {
                A[(op >> 9) & 7] = calc_ea_addr(mode, reg, 4);
                return 8;
            }
            // PEA <ea>
            if ((op & 0xFFC0) == 0x4840 && mode != 0) {
                push32(calc_ea_addr(mode, reg, 4));
                return 12;
            }
            // JSR <ea>
            if ((op & 0xFFC0) == 0x4E80) {
                uint32_t target = calc_ea_addr(mode, reg, 4);
                push32(PC);
                PC = target;
                return 16;
            }
            // JMP <ea>
            if ((op & 0xFFC0) == 0x4EC0) {
                PC = calc_ea_addr(mode, reg, 4);
                return 8;
            }
            // MOVE from SR / MOVE to SR / MOVE to CCR /MOVE USP
            if ((op & 0xFFC0) == 0x40C0) {
                uint32_t addr = (mode >= 2) ? calc_ea_addr(mode, reg, 2) : 0;
                write_ea(mode, reg, 2, addr, SR);
                return 6;
            }
            if ((op & 0xFFC0) == 0x44C0) {
                uint32_t addr = 0;
                uint16_t v = static_cast<uint16_t>(read_ea(mode, reg, 2, addr));
                set_sr((SR & 0xFF00) | (v & 0x1F));
                return 12;
            }
            if ((op & 0xFFC0) == 0x46C0) {
                uint32_t addr = 0;
                set_sr(static_cast<uint16_t>(read_ea(mode, reg, 2, addr)));
                return 12;
            }
            if ((op & 0xFFF0) == 0x4E60) {
                if (op & 8) A[reg] = USP;
                else        USP = A[reg];
                return 4;
            }
            // TRAP #vector
            if ((op & 0xFFF0) == 0x4E40) {
                uint8_t vec = 32 + (op & 0x0F);
                uint16_t old_sr = SR;
                set_sr(SR | 0x2000);
                push32(PC);
                push16(old_sr);
                PC = m_mmu.read32(vec * 4);
                return 34;
            }
            // MOVEM (Registers <-> Memory)
            if ((op & 0xFB80) == 0x4880 && mode >= 2) {
                bool to_reg = (op & 0x0400) != 0;
                int size = (op & 0x0040) ? 4 : 2;
                uint16_t mask = fetch16();

                if (!to_reg && mode == 4) { // Pre-decrement -(An): bit 0 = A7, bit 15 = D0
                    for (int i = 0; i < 16; ++i) {
                        if (mask & (1 << i)) {
                            int r = 15 - i;
                            uint32_t val = (r >= 8) ? A[r - 8] : D[r];
                            A[reg] -= size;
                            if (size == 2) m_mmu.write16(A[reg], static_cast<uint16_t>(val));
                            else           m_mmu.write32(A[reg], val);
                        }
                    }
                } else {
                    uint32_t addr = (mode == 3) ? A[reg] : calc_ea_addr(mode, reg, size);
                    for (int i = 0; i < 16; ++i) {
                        if (mask & (1 << i)) {
                            if (to_reg) {
                                uint32_t val = (size == 2)
                                    ? static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(m_mmu.read16(addr))))
                                    : m_mmu.read32(addr);
                                if (i < 8) D[i] = val;
                                else       A[i - 8] = val;
                            } else {
                                uint32_t val = (i < 8) ? D[i] : A[i - 8];
                                if (size == 2) m_mmu.write16(addr, static_cast<uint16_t>(val));
                                else           m_mmu.write32(addr, val);
                            }
                            addr += size;
                        }
                    }
                    if (mode == 3) A[reg] = addr;
                }
                return 24;
            }

            // CLR (42xx), NEG (44xx), NOT (46xx), TST (4Axx)
            uint8_t sub_op  = (op >> 9) & 0x07;
            uint8_t sz_bits = (op >> 6) & 0x03;
            if (sz_bits < 3) {
                int size = (sz_bits == 0) ? 1 : ((sz_bits == 1) ? 2 : 4);
                if (sub_op == 1) { // CLR
                    uint32_t addr = (mode >= 2) ? calc_ea_addr(mode, reg, size) : 0;
                    write_ea(mode, reg, size, addr, 0);
                    set_z(true); set_n(false); set_c(false); set_v(false);
                    return 6;
                }
                uint32_t addr = 0;
                uint32_t val  = read_ea(mode, reg, size, addr);
                if (sub_op == 2) { // NEG
                    uint32_t res = do_sub(val, 0, size, true, false);
                    write_ea(mode, reg, size, addr, res);
                    return 6;
                }
                if (sub_op == 3) { // NOT
                    uint32_t res = ~val & val_mask(size);
                    write_ea(mode, reg, size, addr, res);
                    set_nz(res, size); set_c(false); set_v(false);
                    return 6;
                }
                if (sub_op == 5) { // TST
                    set_nz(val, size); set_c(false); set_v(false);
                    return 4;
                }
            }
            break;
        }

        // --- GROUP 5: ADDQ, SUBQ, Scc, DBcc ---
        case 0x5: {
            uint8_t sz_bits = (op >> 6) & 0x03;
            uint8_t mode    = (op >> 3) & 0x07;
            uint8_t reg     = op & 0x07;

            if (sz_bits == 3) {
                uint8_t cond = (op >> 8) & 0x0F;
                if (mode == 1) { // DBcc Dn, disp16
                    int16_t disp = static_cast<int16_t>(fetch16());
                    if (!eval_cond(cond)) {
                        uint16_t cnt = (D[reg] & 0xFFFF) - 1;
                        D[reg] = (D[reg] & 0xFFFF0000U) | cnt;
                        if (cnt != 0xFFFF) {
                            PC = (PC - 2) + disp;
                        }
                    }
                    return 10;
                } else { // Scc <ea>
                    uint32_t addr = (mode >= 2) ? calc_ea_addr(mode, reg, 1) : 0;
                    write_ea(mode, reg, 1, addr, eval_cond(cond) ? 0xFF : 0x00);
                    return 6;
                }
            }

            // ADDQ / SUBQ
            uint32_t imm = (op >> 9) & 0x07;
            if (imm == 0) imm = 8;
            int size = (sz_bits == 0) ? 1 : ((sz_bits == 1) ? 2 : 4);
            bool is_sub = (op & 0x0100) != 0;

            if (mode == 1) { // ADDQ/SUBQ to An operates on entire 32-bit address register without flags
                A[reg] = is_sub ? (A[reg] - imm) : (A[reg] + imm);
                return 8;
            }

            uint32_t addr = 0;
            uint32_t dst  = read_ea(mode, reg, size, addr);
            uint32_t res  = is_sub ? do_sub(imm, dst, size, true, false) : do_add(imm, dst, size, true);
            write_ea(mode, reg, size, addr, res);
            return 8;
        }

        // --- GROUP 6: Bcc, BRA, BSR ---
        case 0x6: {
            uint8_t cond = (op >> 8) & 0x0F;
            int8_t  d8   = static_cast<int8_t>(op & 0xFF);
            uint32_t base_pc = PC;
            int32_t disp = (d8 == 0) ? static_cast<int16_t>(fetch16()) : d8;

            if (cond == 1) { // BSR (Branch to Subroutine)
                push32(PC);
                PC = base_pc + disp;
                return 18;
            }
            if (eval_cond(cond)) {
                PC = base_pc + disp;
                return 10;
            }
            return 8;
        }

        // --- GROUP 7: MOVEQ #imm8, Dn ---
        case 0x7: {
            uint8_t reg = (op >> 9) & 0x07;
            int32_t val = static_cast<int8_t>(op & 0xFF);
            D[reg] = static_cast<uint32_t>(val);
            set_nz(D[reg], 4);
            set_c(false);
            set_v(false);
            return 4;
        }

        // --- GROUP 8: OR, DIVU, DIVS ---
        case 0x8: {
            uint8_t reg  = (op >> 9) & 0x07;
            uint8_t mode = (op >> 3) & 0x07;
            uint8_t ea_r = op & 0x07;
            uint8_t sz   = (op >> 6) & 0x03;

            if (sz == 3) { // DIVU (0) / DIVS (1)
                uint32_t addr = 0;
                uint16_t divisor = static_cast<uint16_t>(read_ea(mode, ea_r, 2, addr));
                if (divisor == 0) return 38;
                if (op & 0x0100) {
                    int32_t q = static_cast<int32_t>(D[reg]) / static_cast<int16_t>(divisor);
                    int16_t r = static_cast<int32_t>(D[reg]) % static_cast<int16_t>(divisor);
                    D[reg] = (static_cast<uint32_t>(static_cast<uint16_t>(r)) << 16) | static_cast<uint16_t>(q);
                    set_nz(static_cast<uint16_t>(q), 2);
                } else {
                    uint32_t q = D[reg] / divisor;
                    uint16_t r = D[reg] % divisor;
                    D[reg] = (static_cast<uint32_t>(r) << 16) | (q & 0xFFFF);
                    set_nz(q & 0xFFFF, 2);
                }
                set_c(false); set_v(false);
                return 140;
            }

            int size = (sz == 0) ? 1 : ((sz == 1) ? 2 : 4);
            uint32_t addr = 0;
            uint32_t val  = read_ea(mode, ea_r, size, addr);
            if (op & 0x0100) {
                uint32_t res = (val | D[reg]) & val_mask(size);
                write_ea(mode, ea_r, size, addr, res);
                set_nz(res, size);
            } else {
                uint32_t res = ((D[reg] & val_mask(size)) | val) & val_mask(size);
                write_ea(0, reg, size, 0, res);
                set_nz(res, size);
            }
            set_c(false); set_v(false);
            return 8;
        }

        // --- GROUPS 9 & D: SUB / SUBA (9) and ADD / ADDA (D) ---
        case 0x9:
        case 0xD: {
            bool is_sub  = (group == 0x9);
            uint8_t reg  = (op >> 9) & 0x07;
            uint8_t mode = (op >> 3) & 0x07;
            uint8_t ea_r = op & 0x07;
            uint8_t sz   = (op >> 6) & 0x03;

            if (sz == 3) { // ADDA / SUBA (.W or .L)
                int size = (op & 0x0100) ? 4 : 2;
                uint32_t addr = 0;
                uint32_t val  = read_ea(mode, ea_r, size, addr);
                if (size == 2) val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(val)));
                A[reg] = is_sub ? (A[reg] - val) : (A[reg] + val);
                return 8;
            }

            int size = (sz == 0) ? 1 : ((sz == 1) ? 2 : 4);
            uint32_t addr = 0;
            uint32_t ea_v = read_ea(mode, ea_r, size, addr);

            if (op & 0x0100) { // Dn -> <ea>
                uint32_t res = is_sub
                    ? do_sub(D[reg], ea_v, size, true, false)
                    : do_add(D[reg], ea_v, size, true);
                write_ea(mode, ea_r, size, addr, res);
            } else {           // <ea> -> Dn
                uint32_t res = is_sub
                    ? do_sub(ea_v, D[reg], size, true, false)
                    : do_add(ea_v, D[reg], size, true);
                write_ea(0, reg, size, 0, res);
            }
            return 8;
        }

        // --- GROUP B: CMP, CMPA, CMPM, EOR ---
        case 0xB: {
            uint8_t reg  = (op >> 9) & 0x07;
            uint8_t mode = (op >> 3) & 0x07;
            uint8_t ea_r = op & 0x07;
            uint8_t sz   = (op >> 6) & 0x03;

            if (sz == 3) { // CMPA (.W or .L)
                int size = (op & 0x0100) ? 4 : 2;
                uint32_t addr = 0;
                uint32_t val  = read_ea(mode, ea_r, size, addr);
                if (size == 2) val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(val)));
                do_sub(val, A[reg], 4, false, true);
                return 6;
            }

            int size = (sz == 0) ? 1 : ((sz == 1) ? 2 : 4);
            if ((op & 0x0100) && mode == 1) { // CMPM (Ay)+, (Ax)+
                uint32_t a1 = 0, a2 = 0;
                uint32_t v1 = read_ea(3, ea_r, size, a1);
                uint32_t v2 = read_ea(3, reg,  size, a2);
                do_sub(v1, v2, size, false, true);
                return 12;
            }
            uint32_t addr = 0;
            uint32_t val  = read_ea(mode, ea_r, size, addr);
            if (op & 0x0100) { // EOR Dn, <ea>
                uint32_t res = (val ^ D[reg]) & val_mask(size);
                write_ea(mode, ea_r, size, addr, res);
                set_nz(res, size); set_c(false); set_v(false);
            } else {           // CMP <ea>, Dn
                do_sub(val, D[reg], size, false, true);
            }
            return 6;
        }

        // --- GROUP C: AND, MULU, MULS, EXG ---
        case 0xC: {
            uint8_t reg  = (op >> 9) & 0x07;
            uint8_t mode = (op >> 3) & 0x07;
            uint8_t ea_r = op & 0x07;
            uint8_t sz   = (op >> 6) & 0x03;

            if (sz == 3) { // MULU (0) / MULS (1)
                uint32_t addr = 0;
                uint16_t v = static_cast<uint16_t>(read_ea(mode, ea_r, 2, addr));
                if (op & 0x0100) {
                    D[reg] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(D[reg] & 0xFFFF)) * static_cast<int16_t>(v));
                } else {
                    D[reg] = static_cast<uint32_t>(D[reg] & 0xFFFF) * v;
                }
                set_nz(D[reg], 4); set_c(false); set_v(false);
                return 70;
            }
            // EXG
            if ((op & 0x0130) == 0x0100) {
                uint8_t exg_mode = (op >> 3) & 0x1F;
                if (exg_mode == 0x08) std::swap(D[reg], D[ea_r]);
                else if (exg_mode == 0x09) std::swap(A[reg], A[ea_r]);
                else if (exg_mode == 0x11) std::swap(D[reg], A[ea_r]);
                return 6;
            }

            int size = (sz == 0) ? 1 : ((sz == 1) ? 2 : 4);
            uint32_t addr = 0;
            uint32_t val  = read_ea(mode, ea_r, size, addr);
            if (op & 0x0100) {
                uint32_t res = (val & D[reg]) & val_mask(size);
                write_ea(mode, ea_r, size, addr, res);
                set_nz(res, size);
            } else {
                uint32_t res = ((D[reg] & val_mask(size)) & val) & val_mask(size);
                write_ea(0, reg, size, 0, res);
                set_nz(res, size);
            }
            set_c(false); set_v(false);
            return 8;
        }

        // --- GROUP E: Shifts & Rotates (ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR) ---
        case 0xE: {
            uint8_t sz   = (op >> 6) & 0x03;
            bool left    = (op & 0x0100) != 0;

            auto shift_val = [&](uint32_t v, int size, uint8_t type, uint32_t cnt) -> uint32_t {
                uint32_t m = val_mask(size);
                uint32_t s = sign_mask(size);
                v &= m;
                if (cnt == 0) {
                    set_c(false); set_v(false); set_nz(v, size);
                    return v;
                }
                bool last_bit = false;
                for (uint32_t i = 0; i < cnt; ++i) {
                    if (left) {
                        last_bit = (v & s) != 0;
                        v = (v << 1) & m;
                        if (type == 3 && last_bit) v |= 1; // ROL
                        else if (type == 2) { if (flag_x()) v |= 1; set_x(last_bit); } // ROXL
                    } else {
                        last_bit = (v & 1) != 0;
                        uint32_t msb = (type == 0) ? (v & s) : 0; // ASR preserves sign
                        v = (v >> 1) | msb;
                        if (type == 3 && last_bit) v |= s; // ROR
                        else if (type == 2) { if (flag_x()) v |= s; set_x(last_bit); } // ROXR
                    }
                }
                set_c(last_bit);
                if (type != 3) set_x(last_bit);
                set_v(false);
                set_nz(v, size);
                return v;
            };

            if (sz == 3) { // Memory Shift (1-bit Word)
                uint8_t type = (op >> 9) & 0x03;
                uint8_t mode = (op >> 3) & 0x07;
                uint8_t reg  = op & 0x07;
                uint32_t addr = 0;
                uint32_t val  = read_ea(mode, reg, 2, addr);
                uint32_t res  = shift_val(val, 2, type, 1);
                write_ea(mode, reg, 2, addr, res);
                return 8;
            } else { // Register Shift
                int size = (sz == 0) ? 1 : ((sz == 1) ? 2 : 4);
                uint8_t type = (op >> 3) & 0x03;
                uint8_t reg  = op & 0x07;
                uint8_t cnt_src = (op >> 9) & 0x07;
                uint32_t cnt = (op & 0x0020) ? (D[cnt_src] & 63) : (cnt_src == 0 ? 8 : cnt_src);
                uint32_t res = shift_val(D[reg], size, type, cnt);
                write_ea(0, reg, size, 0, res);
                return 8;
            }
        }
    }

    std::printf("\n--- CRASH: Unimplemented M68K Opcode 0x%04X ---\n", op);
    std::printf("At Address: 0x%06X | SR: 0x%04X\n", instr_pc, SR);
    std::printf("D0:%08X D1:%08X D2:%08X D3:%08X\n", D[0], D[1], D[2], D[3]);
    std::printf("A0:%08X A1:%08X A6:%08X A7:%08X\n", A[0], A[1], A[6], A[7]);
    exit(1);
}