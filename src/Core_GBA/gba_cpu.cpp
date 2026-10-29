#include "../../include/Core_GBA/gba_cpu.h"
#include "../../include/Core_GBA/gba_mmu.h"
#include <cmath>
#include <cstring>
#include <vector>

GBACPU::GBACPU(GBAMMU& mmu) : m_mmu(mmu) {
    reset();
}

void GBACPU::reset() {
    m_regs.fill(0);
    m_sp_usr = 0x03007F00;
    m_sp_irq = 0x03007FA0;
    m_sp_svc = 0x03007FE0;
    m_sp_fiq = 0x03007F00;
    m_lr_usr = m_lr_irq = m_lr_svc = m_lr_fiq = 0;
    m_spsr_irq = m_spsr_svc = m_spsr_fiq = 0x0000001F;

    m_regs[0]  = 0x08000000;
    m_regs[1]  = 0x000000EA;
    m_regs[13] = m_sp_usr;
    m_regs[14] = 0x08000000;
    m_regs[15] = 0x08000000;
    m_cpsr     = 0x0000001F;
}

CPUState GBACPU::get_state() const {
    CPUState state{};
    state.is_gba   = true;
    state.gba_regs = m_regs;
    state.cpsr     = m_cpsr;
    state.PC       = static_cast<uint16_t>(m_regs[15] & 0xFFFF);
    state.SP       = static_cast<uint16_t>(m_regs[13] & 0xFFFF);
    return state;
}

void GBACPU::switch_mode(uint8_t new_mode) {
    uint8_t old_mode = m_cpsr & 0x1F;
    if (old_mode == new_mode) return;

    switch (old_mode) {
        case 0x11: m_sp_fiq = m_regs[13]; m_lr_fiq = m_regs[14]; break;
        case 0x12: m_sp_irq = m_regs[13]; m_lr_irq = m_regs[14]; break;
        case 0x13: m_sp_svc = m_regs[13]; m_lr_svc = m_regs[14]; break;
        default:   m_sp_usr = m_regs[13]; m_lr_usr = m_regs[14]; break;
    }

    switch (new_mode) {
        case 0x11: m_regs[13] = m_sp_fiq; m_regs[14] = m_lr_fiq; break;
        case 0x12: m_regs[13] = m_sp_irq; m_regs[14] = m_lr_irq; break;
        case 0x13: m_regs[13] = m_sp_svc; m_regs[14] = m_lr_svc; break;
        default:   m_regs[13] = m_sp_usr; m_regs[14] = m_lr_usr; break;
    }

    m_cpsr = (m_cpsr & ~0x1FU) | (new_mode & 0x1F);
}

uint32_t GBACPU::get_spsr() const {
    switch (m_cpsr & 0x1F) {
        case 0x11: return m_spsr_fiq;
        case 0x12: return m_spsr_irq;
        case 0x13: return m_spsr_svc;
        default:   return m_cpsr;
    }
}

void GBACPU::set_spsr(uint32_t val) {
    switch (m_cpsr & 0x1F) {
        case 0x11: m_spsr_fiq = val; break;
        case 0x12: m_spsr_irq = val; break;
        case 0x13: m_spsr_svc = val; break;
        default: break;
    }
}

bool GBACPU::check_cond(uint8_t cond) const {
    switch (cond) {
        case 0x0: return flag_z();
        case 0x1: return !flag_z();
        case 0x2: return flag_c();
        case 0x3: return !flag_c();
        case 0x4: return flag_n();
        case 0x5: return !flag_n();
        case 0x6: return flag_v();
        case 0x7: return !flag_v();
        case 0x8: return flag_c() && !flag_z();
        case 0x9: return !flag_c() || flag_z();
        case 0xA: return flag_n() == flag_v();
        case 0xB: return flag_n() != flag_v();
        case 0xC: return !flag_z() && (flag_n() == flag_v());
        case 0xD: return flag_z() || (flag_n() != flag_v());
        case 0xE: return true;
        default:  return true;
    }
}

uint32_t GBACPU::barrel_shift(uint32_t val, uint8_t type, uint32_t amount, bool& carry_out, bool immediate_shift) {
    if (immediate_shift && amount == 0) {
        switch (type) {
            case 0: return val;
            case 1: carry_out = (val >> 31) & 1; return 0;
            case 2: carry_out = (val >> 31) & 1; return carry_out ? 0xFFFFFFFFU : 0;
            case 3: {
                bool old_c = carry_out;
                carry_out  = val & 1;
                return (val >> 1) | (old_c ? 0x80000000U : 0);
            }
        }
    }

    if (amount == 0) return val;

    switch (type) {
        case 0: // LSL
            if (amount < 32) { carry_out = (val >> (32 - amount)) & 1; return val << amount; }
            if (amount == 32) { carry_out = val & 1; return 0; }
            carry_out = false; return 0;
        case 1: // LSR
            if (amount < 32) { carry_out = (val >> (amount - 1)) & 1; return val >> amount; }
            if (amount == 32) { carry_out = (val >> 31) & 1; return 0; }
            carry_out = false; return 0;
        case 2: // ASR
            if (amount < 32) {
                carry_out = (val >> (amount - 1)) & 1;
                return static_cast<uint32_t>(static_cast<int32_t>(val) >> amount);
            }
            carry_out = (val >> 31) & 1;
            return carry_out ? 0xFFFFFFFFU : 0;
        case 3: // ROR
            amount &= 31;
            if (amount == 0) { carry_out = (val >> 31) & 1; return val; }
            val = (val >> amount) | (val << (32 - amount));
            carry_out = (val >> 31) & 1;
            return val;
    }
    return val;
}

uint32_t GBACPU::alu_add(uint32_t a, uint32_t b, bool s) {
    uint64_t res64 = static_cast<uint64_t>(a) + b;
    uint32_t res   = static_cast<uint32_t>(res64);
    if (s) {
        set_nz(res);
        set_c(res64 > 0xFFFFFFFFULL);
        set_v(((~(a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

uint32_t GBACPU::alu_adc(uint32_t a, uint32_t b, bool c_in, bool s) {
    uint64_t res64 = static_cast<uint64_t>(a) + b + (c_in ? 1 : 0);
    uint32_t res   = static_cast<uint32_t>(res64);
    if (s) {
        set_nz(res);
        set_c(res64 > 0xFFFFFFFFULL);
        set_v(((~(a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

uint32_t GBACPU::alu_sub(uint32_t a, uint32_t b, bool s) {
    uint32_t res = a - b;
    if (s) {
        set_nz(res);
        set_c(a >= b);
        set_v((((a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

uint32_t GBACPU::alu_sbc(uint32_t a, uint32_t b, bool c_in, bool s) {
    uint64_t borrow = static_cast<uint64_t>(b) + (c_in ? 0 : 1);
    uint32_t res    = static_cast<uint32_t>(static_cast<uint64_t>(a) - borrow);
    if (s) {
        set_nz(res);
        set_c(static_cast<uint64_t>(a) >= borrow);
        set_v((((a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

void GBACPU::check_irqs() {
    uint16_t ie  = m_mmu.io_read16(0x200);
    uint16_t ifr = m_mmu.io_read16(0x202);
    uint16_t ime = m_mmu.io_read16(0x208);

    if (ie & ifr) {
        m_mmu.set_halted(false);
        bool irq_disabled = (m_cpsr >> 7) & 1;
        if ((ime & 1) && !irq_disabled) {
            uint32_t old_cpsr = m_cpsr;
            uint32_t next_pc  = m_regs[15];
            switch_mode(0x12); // IRQ mode
            m_spsr_irq = old_cpsr;
            m_regs[14] = next_pc + 4;
            m_cpsr &= ~(1U << 5); // Switch to ARM state
            m_cpsr |= (1U << 7);  // Mask IRQs
            m_mmu.set_bios_latch(0xE55EC002);
            m_regs[15] = 0x00000128; // Jump to BIOS IRQ dispatcher
        }
    }
}

void GBACPU::handle_swi(uint8_t comment) {
    bool was_in_bios = m_mmu.is_in_bios();
    m_mmu.set_in_bios(true); // Temporarily elevate privileges so HLE SWI can read BIOS

    switch (comment) {
        case 0x00: // SoftReset
            reset();
            break;
        case 0x01: { // RegisterRamReset
            uint32_t flags = m_regs[0];
            if (flags & 0x01) m_mmu.ewram().fill(0);
            if (flags & 0x02) std::memset(m_mmu.iwram().data(), 0, 0x7E00);
            if (flags & 0x04) m_mmu.pal_ram().fill(0);
            if (flags & 0x08) m_mmu.vram().fill(0);
            if (flags & 0x10) m_mmu.oam().fill(0);
            break;
        }
        case 0x02: // Halt
        case 0x03: // Stop
            m_mmu.set_halted(true);
            break;
        case 0x04: // IntrWait
        case 0x05: { // VBlankIntrWait
            uint16_t wait_flags = (comment == 0x05) ? 1 : static_cast<uint16_t>(m_regs[1] & 0xFFFF);
            if (comment == 0x05 || m_regs[0] != 0) {
                uint16_t bios_if = CoreShared::read_le16(m_mmu.iwram().data(), 0x7FF8);
                CoreShared::write_le16(m_mmu.iwram().data(), 0x7FF8, bios_if & ~wait_flags);
            }
            m_mmu.io_write16(0x208, 1); // Ensure IME = 1
            m_mmu.set_halted(true);
            break;
        }
        case 0x06: { // Div (R0 / R1)
            int32_t num = static_cast<int32_t>(m_regs[0]);
            int32_t den = static_cast<int32_t>(m_regs[1]);
            if (den != 0) {
                int32_t q = num / den;
                int32_t r = num % den;
                m_regs[0] = static_cast<uint32_t>(q);
                m_regs[1] = static_cast<uint32_t>(r);
                m_regs[3] = static_cast<uint32_t>(std::abs(q));
            }
            break;
        }
        case 0x07: { // DivArm (R1 / R0)
            int32_t num = static_cast<int32_t>(m_regs[1]);
            int32_t den = static_cast<int32_t>(m_regs[0]);
            if (den != 0) {
                int32_t q = num / den;
                int32_t r = num % den;
                m_regs[0] = static_cast<uint32_t>(q);
                m_regs[1] = static_cast<uint32_t>(r);
                m_regs[3] = static_cast<uint32_t>(std::abs(q));
            }
            break;
        }
        case 0x08: // Sqrt
            m_regs[0] = static_cast<uint32_t>(std::sqrt(m_regs[0]));
            break;
        case 0x09: { // ArcTan
            double a = static_cast<int16_t>(m_regs[0]) / 16384.0;
            m_regs[0] = static_cast<uint32_t>(static_cast<int16_t>(std::atan(a) * 32768.0 / 3.141592653589793));
            break;
        }
        case 0x0A: { // ArcTan2
            double x = static_cast<int16_t>(m_regs[0]);
            double y = static_cast<int16_t>(m_regs[1]);
            double res = std::atan2(y, x) * 32768.0 / 3.141592653589793;
            if (res < 0) res += 65536.0;
            m_regs[0] = static_cast<uint16_t>(res);
            break;
        }
        case 0x0B: { // CpuSet
            uint32_t src = m_regs[0];
            uint32_t dst = m_regs[1];
            uint32_t cnt = m_regs[2];
            uint32_t count = cnt & 0x1FFFFF;
            bool fixed = (cnt >> 24) & 1;
            bool is32  = (cnt >> 26) & 1;
            for (uint32_t i = 0; i < count; ++i) {
                if (is32) {
                    m_mmu.write32(dst, m_mmu.read32(src));
                    dst += 4;
                    if (!fixed) src += 4;
                } else {
                    m_mmu.write16(dst, m_mmu.read16(src));
                    dst += 2;
                    if (!fixed) src += 2;
                }
            }
            break;
        }
        case 0x0C: { // CpuFastSet
            uint32_t src = m_regs[0];
            uint32_t dst = m_regs[1];
            uint32_t cnt = m_regs[2];
            uint32_t count = (cnt & 0x1FFFFF);
            bool fixed = (cnt >> 24) & 1;
            uint32_t fixed_val = m_mmu.read32(src);
            for (uint32_t i = 0; i < count; ++i) {
                m_mmu.write32(dst, fixed ? fixed_val : m_mmu.read32(src));
                dst += 4;
                if (!fixed) src += 4;
            }
            break;
        }
        case 0x0D: {
            // GetBiosChecksum (Authentic GBA / GBA SP BIOS Checksum)
            m_regs[0] = 0xBAAE187F;
            break;
        }
        case 0x0E: { // BgAffineSet
            uint32_t src   = m_regs[0];
            uint32_t dst   = m_regs[1];
            uint32_t count = m_regs[2];
            for (uint32_t i = 0; i < count; ++i) {
                int32_t ox = static_cast<int32_t>(m_mmu.read32(src));
                int32_t oy = static_cast<int32_t>(m_mmu.read32(src + 4));
                int16_t cx = static_cast<int16_t>(m_mmu.read16(src + 8));
                int16_t cy = static_cast<int16_t>(m_mmu.read16(src + 10));
                int16_t sx = static_cast<int16_t>(m_mmu.read16(src + 12));
                int16_t sy = static_cast<int16_t>(m_mmu.read16(src + 14));
                uint16_t angle = m_mmu.read16(src + 16) >> 8;
                src += 20;

                double rad = (angle / 256.0) * 2.0 * 3.141592653589793;
                int16_t pa = static_cast<int16_t>( std::cos(rad) * sx);
                int16_t pb = static_cast<int16_t>(-std::sin(rad) * sx);
                int16_t pc = static_cast<int16_t>( std::sin(rad) * sy);
                int16_t pd = static_cast<int16_t>( std::cos(rad) * sy);

                m_mmu.write16(dst,     static_cast<uint16_t>(pa));
                m_mmu.write16(dst + 2, static_cast<uint16_t>(pb));
                m_mmu.write16(dst + 4, static_cast<uint16_t>(pc));
                m_mmu.write16(dst + 6, static_cast<uint16_t>(pd));
                m_mmu.write32(dst + 8,  static_cast<uint32_t>(ox - (pa * cx + pb * cy)));
                m_mmu.write32(dst + 12, static_cast<uint32_t>(oy - (pc * cx + pd * cy)));
                dst += 16;
            }
            break;
        }
        case 0x0F: { // ObjAffineSet
            uint32_t src    = m_regs[0];
            uint32_t dst    = m_regs[1];
            uint32_t count  = m_regs[2];
            uint32_t stride = m_regs[3];
            for (uint32_t i = 0; i < count; ++i) {
                int16_t sx = static_cast<int16_t>(m_mmu.read16(src));
                int16_t sy = static_cast<int16_t>(m_mmu.read16(src + 2));
                uint16_t angle = m_mmu.read16(src + 4) >> 8;
                src += 8;

                double rad = (angle / 256.0) * 2.0 * 3.141592653589793;
                int16_t pa = static_cast<int16_t>( std::cos(rad) * sx);
                int16_t pb = static_cast<int16_t>(-std::sin(rad) * sx);
                int16_t pc = static_cast<int16_t>( std::sin(rad) * sy);
                int16_t pd = static_cast<int16_t>( std::cos(rad) * sy);

                m_mmu.write16(dst + stride * 0, static_cast<uint16_t>(pa));
                m_mmu.write16(dst + stride * 1, static_cast<uint16_t>(pb));
                m_mmu.write16(dst + stride * 2, static_cast<uint16_t>(pc));
                m_mmu.write16(dst + stride * 3, static_cast<uint16_t>(pd));
                dst += stride * 4;
            }
            break;
        }
        case 0x10: { // BitUnPack (Used heavily by Pokemon FireRed for font/tile unpacking)
            uint32_t src  = m_regs[0];
            uint32_t dst  = m_regs[1];
            uint32_t info = m_regs[2];

            uint16_t src_len   = m_mmu.read16(info);
            uint8_t  src_width = m_mmu.read8(info + 2);
            uint8_t  dst_width = m_mmu.read8(info + 3);
            uint32_t offset_val = m_mmu.read32(info + 4);
            bool zero_flag = (offset_val >> 31) & 1;
            offset_val &= 0x7FFFFFFFU;

            uint32_t buffer = 0;
            int bits_in_buf = 0;
            uint8_t mask = (1U << src_width) - 1;

            for (uint16_t i = 0; i < src_len; ++i) {
                uint8_t byte = m_mmu.read8(src++);
                for (int b = 0; b < 8; b += src_width) {
                    uint32_t slice = (byte >> b) & mask;
                    if (slice != 0 || zero_flag) {
                        slice += offset_val;
                    }
                    buffer |= (slice << bits_in_buf);
                    bits_in_buf += dst_width;
                    if (bits_in_buf >= 32) {
                        m_mmu.write32(dst, buffer);
                        dst += 4;
                        buffer = 0;
                        bits_in_buf = 0;
                    }
                }
            }
            break;
        }
        case 0x11: // LZ77UnCompWram
        case 0x12: { // LZ77UnCompVram
            uint32_t src = m_regs[0];
            uint32_t dst = m_regs[1];
            uint32_t header = m_mmu.read32(src);
            uint32_t decomp_size = header >> 8;
            src += 4;

            std::vector<uint8_t> out;
            out.reserve(decomp_size);

            while (out.size() < decomp_size) {
                uint8_t flags = m_mmu.read8(src++);
                for (int bit = 7; bit >= 0 && out.size() < decomp_size; --bit) {
                    if (flags & (1 << bit)) {
                        uint8_t b1 = m_mmu.read8(src++);
                        uint8_t b2 = m_mmu.read8(src++);
                        uint32_t len  = (b1 >> 4) + 3;
                        uint32_t disp = (((b1 & 0x0F) << 8) | b2) + 1;
                        for (uint32_t j = 0; j < len && out.size() < decomp_size; ++j) {
                            out.push_back(out[out.size() - disp]);
                        }
                    } else {
                        out.push_back(m_mmu.read8(src++));
                    }
                }
            }

            if (comment == 0x12) {
                for (size_t i = 0; i + 1 < out.size(); i += 2) {
                    m_mmu.write16(dst + static_cast<uint32_t>(i),
                                  out[i] | (static_cast<uint16_t>(out[i + 1]) << 8));
                }
            } else {
                for (size_t i = 0; i < out.size(); ++i) {
                    m_mmu.write8(dst + static_cast<uint32_t>(i), out[i]);
                }
            }
            break;
        }
        case 0x14: // RLUnCompWram
        case 0x15: { // RLUnCompVram
            uint32_t src = m_regs[0];
            uint32_t dst = m_regs[1];
            uint32_t decomp_size = m_mmu.read32(src) >> 8;
            src += 4;

            std::vector<uint8_t> out;
            out.reserve(decomp_size);

            while (out.size() < decomp_size) {
                uint8_t flag = m_mmu.read8(src++);
                if (flag & 0x80) {
                    uint32_t len = (flag & 0x7F) + 3;
                    uint8_t data = m_mmu.read8(src++);
                    for (uint32_t i = 0; i < len && out.size() < decomp_size; ++i) out.push_back(data);
                } else {
                    uint32_t len = (flag & 0x7F) + 1;
                    for (uint32_t i = 0; i < len && out.size() < decomp_size; ++i) out.push_back(m_mmu.read8(src++));
                }
            }

            if (comment == 0x15) {
                for (size_t i = 0; i + 1 < out.size(); i += 2) {
                    m_mmu.write16(dst + static_cast<uint32_t>(i),
                                  out[i] | (static_cast<uint16_t>(out[i + 1]) << 8));
                }
            } else {
                for (size_t i = 0; i < out.size(); ++i) {
                    m_mmu.write8(dst + static_cast<uint32_t>(i), out[i]);
                }
            }
            break;
        }
        case 0x1F: { // MidiKey2Freq (Used by m4a audio engine in Pokemon FireRed)
            uint32_t wave_data = m_mmu.read32(m_regs[0] + 4);
            double mk = static_cast<double>(m_regs[1]);
            double fp = static_cast<double>(m_regs[2]);
            double freq = wave_data * std::pow(2.0, (mk - 180.0 + fp / 256.0) / 12.0);
            m_regs[0] = static_cast<uint32_t>(freq);
            break;
        }
        default: break;
    }
    m_mmu.set_in_bios(was_in_bios); // Restore privileges
    m_mmu.set_bios_latch(0xE3A02004); // Update open-bus latch
}

void GBACPU::step_arm() {
    uint32_t pc    = m_regs[15];
    uint32_t instr = m_mmu.read32(pc);
    m_regs[15]     = pc + 4;

    uint8_t cond = (instr >> 28) & 0x0F;
    if (!check_cond(cond)) return;

    // 1. Branch & Exchange (BX / BLX)
    if ((instr & 0x0FFFFFF0) == 0x012FFF10 || (instr & 0x0FFFFFF0) == 0x012FFF30) {
        bool link = ((instr >> 4) & 0x0F) == 0x03;
        uint32_t target = (instr & 0x0F) == 15 ? (pc + 8) : m_regs[instr & 0x0F];
        if (link) m_regs[14] = pc + 4;
        if (target & 1) {
            m_cpsr |= (1U << 5);
            m_regs[15] = target & ~1U;
        } else {
            m_cpsr &= ~(1U << 5);
            m_regs[15] = target & ~3U;
        }
        return;
    }

    // 2. Single Data Swap (SWP / SWPB)
    if ((instr & 0x0FB00FF0) == 0x01000090) {
        bool byte  = (instr >> 22) & 1;
        uint8_t rn = (instr >> 16) & 0x0F;
        uint8_t rd = (instr >> 12) & 0x0F;
        uint8_t rm = instr & 0x0F;
        uint32_t addr = m_regs[rn];
        if (byte) {
            uint8_t tmp = m_mmu.read8(addr);
            m_mmu.write8(addr, static_cast<uint8_t>(m_regs[rm]));
            m_regs[rd] = tmp;
        } else {
            uint32_t tmp = m_mmu.read32(addr);
            m_mmu.write32(addr, m_regs[rm]);
            m_regs[rd] = tmp;
        }
        return;
    }

    // 3. Software Interrupt (SWI)
    if (((instr >> 24) & 0x0F) == 0x0F) {
        handle_swi(static_cast<uint8_t>((instr >> 16) & 0xFF));
        return;
    }

    // 4. Branch / Branch with Link (B / BL)
    if (((instr >> 25) & 0x07) == 0x05) {
        if ((instr >> 24) & 1) m_regs[14] = pc + 4;
        int32_t offset = (static_cast<int32_t>(instr << 8) >> 6);
        m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 8) + offset);
        return;
    }

    // 5. Block Data Transfer (LDM / STM)
    // 5. Block Data Transfer (LDM / STM)
    if (((instr >> 25) & 0x07) == 0x04) {
        bool p = (instr >> 24) & 1;
        bool u = (instr >> 23) & 1;
        bool s = (instr >> 22) & 1;
        bool w = (instr >> 21) & 1;
        bool l = (instr >> 20) & 1;
        uint8_t rn = (instr >> 16) & 0x0F;
        uint16_t rlist = instr & 0xFFFF;

        int count = 0;
        for (int i = 0; i < 16; ++i) if (rlist & (1 << i)) count++;

        uint32_t base = m_regs[rn];
        uint32_t addr = base;
        if (!u) addr -= count * 4;
        if (p == u) addr += 4;

        // Apply writeback BEFORE potential mode switch on R15 load
        if (w && !(l && (rlist & (1 << rn)))) {
            m_regs[rn] = u ? (base + count * 4) : (base - count * 4);
        }

        for (int i = 0; i < 16; ++i) {
            if (rlist & (1 << i)) {
                if (l) {
                    uint32_t val = m_mmu.read32(addr & ~3U);
                    if (i == 15) {
                        if (s) {
                            uint32_t spsr = get_spsr();
                            switch_mode(spsr & 0x1F);
                            m_cpsr = spsr;
                        }
                        m_regs[15] = val & (is_thumb() ? ~1U : ~3U);
                    } else {
                        m_regs[i] = val;
                    }
                } else {
                    uint32_t val = (i == 15) ? (pc + 12) : m_regs[i];
                    m_mmu.write32(addr & ~3U, val);
                }
                addr += 4;
            }
        }
        return;
    }

    // 6. Single Data Transfer (LDR / STR / LDRB / STRB)
    if (((instr >> 26) & 0x03) == 0x01) {
        bool i_reg = (instr >> 25) & 1;
        bool p     = (instr >> 24) & 1;
        bool u     = (instr >> 23) & 1;
        bool b     = (instr >> 22) & 1;
        bool w     = (instr >> 21) & 1;
        bool l     = (instr >> 20) & 1;
        uint8_t rn = (instr >> 16) & 0x0F;
        uint8_t rd = (instr >> 12) & 0x0F;

        uint32_t offset = 0;
        if (!i_reg) {
            offset = instr & 0xFFF;
        } else {
            uint8_t rm = instr & 0x0F;
            uint8_t shift_type = (instr >> 5) & 0x03;
            uint32_t shift_amt = (instr >> 7) & 0x1F;
            bool carry = flag_c();
            offset = barrel_shift(m_regs[rm], shift_type, shift_amt, carry, true);
        }

        uint32_t base     = (rn == 15) ? (pc + 8) : m_regs[rn];
        uint32_t mod_addr = u ? (base + offset) : (base - offset);
        uint32_t addr     = p ? mod_addr : base;

        if (l) {
            uint32_t val = b ? m_mmu.read8(addr) : m_mmu.read32(addr);
            if (rd == 15) m_regs[15] = val & ~3U;
            else          m_regs[rd] = val;
        } else {
            uint32_t val = (rd == 15) ? (pc + 12) : m_regs[rd];
            if (b) m_mmu.write8(addr, static_cast<uint8_t>(val));
            else   m_mmu.write32(addr, val);
        }

        if ((!p || w) && !(l && rd == rn)) {
            m_regs[rn] = mod_addr;
        }
        return;
    }

    // 7. Multiply & Halfword / Signed Data Transfer
    if (((instr >> 25) & 0x07) == 0x00 && (instr & 0x90) == 0x90) {
        uint8_t sub_op = (instr >> 5) & 0x03;
        if (sub_op == 0) {
            if (((instr >> 22) & 0x0F) == 0x00) {
                bool a = (instr >> 21) & 1;
                bool s = (instr >> 20) & 1;
                uint8_t rd = (instr >> 16) & 0x0F;
                uint8_t rn = (instr >> 12) & 0x0F;
                uint8_t rs = (instr >> 8) & 0x0F;
                uint8_t rm = instr & 0x0F;
                uint32_t res = m_regs[rm] * m_regs[rs] + (a ? m_regs[rn] : 0);
                m_regs[rd] = res;
                if (s) set_nz(res);
            } else if (((instr >> 23) & 0x1F) == 0x01) {
                bool signed_mul = (instr >> 22) & 1;
                bool accum      = (instr >> 21) & 1;
                bool s          = (instr >> 20) & 1;
                uint8_t rd_hi   = (instr >> 16) & 0x0F;
                uint8_t rd_lo   = (instr >> 12) & 0x0F;
                uint8_t rs      = (instr >> 8) & 0x0F;
                uint8_t rm      = instr & 0x0F;

                uint64_t prod = signed_mul
                    ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(m_regs[rm])) * static_cast<int64_t>(static_cast<int32_t>(m_regs[rs])))
                    : (static_cast<uint64_t>(m_regs[rm]) * static_cast<uint64_t>(m_regs[rs]));
                if (accum) {
                    uint64_t acc = (static_cast<uint64_t>(m_regs[rd_hi]) << 32) | m_regs[rd_lo];
                    prod += acc;
                }
                m_regs[rd_lo] = static_cast<uint32_t>(prod);
                m_regs[rd_hi] = static_cast<uint32_t>(prod >> 32);
                if (s) {
                    m_cpsr &= ~0xC0000000U;
                    if (m_regs[rd_hi] & 0x80000000U) m_cpsr |= (1U << 31);
                    if (prod == 0) m_cpsr |= (1U << 30);
                }
            }
            return;
        } else {
            bool p   = (instr >> 24) & 1;
            bool u   = (instr >> 23) & 1;
            bool imm = (instr >> 22) & 1;
            bool w   = (instr >> 21) & 1;
            bool l   = (instr >> 20) & 1;
            uint8_t rn = (instr >> 16) & 0x0F;
            uint8_t rd = (instr >> 12) & 0x0F;

            uint32_t offset   = imm ? (((instr >> 4) & 0xF0) | (instr & 0x0F)) : m_regs[instr & 0x0F];
            uint32_t base     = (rn == 15) ? (pc + 8) : m_regs[rn];
            uint32_t mod_addr = u ? (base + offset) : (base - offset);
            uint32_t addr     = p ? mod_addr : base;

            if (l) {
                if (sub_op == 1)      m_regs[rd] = m_mmu.read16(addr);
                else if (sub_op == 2) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(m_mmu.read8(addr))));
                else if (sub_op == 3) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(m_mmu.read16(addr))));
            } else if (sub_op == 1) {
                m_mmu.write16(addr, static_cast<uint16_t>(m_regs[rd]));
            }

            if ((!p || w) && !(l && rd == rn)) {
                m_regs[rn] = mod_addr;
            }
            return;
        }
    }

    // 8. PSR Transfer (MRS / MSR)
    if (((instr >> 26) & 0x03) == 0x00 && ((instr >> 23) & 0x03) == 0x02 && ((instr >> 20) & 1) == 0) {
        bool use_spsr = (instr >> 22) & 1;
        if (((instr >> 21) & 1) == 0) {
            uint8_t rd = (instr >> 12) & 0x0F;
            m_regs[rd] = use_spsr ? get_spsr() : m_cpsr;
        } else {
            bool imm = (instr >> 25) & 1;
            uint32_t val = 0;
            if (imm) {
                uint32_t imm8 = instr & 0xFF;
                uint32_t rot  = ((instr >> 8) & 0x0F) * 2;
                val = CoreShared::rotr32(imm8, rot);
            } else {
                val = m_regs[instr & 0x0F];
            }

            uint32_t mask = 0;
            if ((instr >> 16) & 1) mask |= 0x000000FFU;
            if ((instr >> 17) & 1) mask |= 0x0000FF00U;
            if ((instr >> 18) & 1) mask |= 0x00FF0000U;
            if ((instr >> 19) & 1) mask |= 0xFF000000U;

            if (use_spsr) {
                set_spsr((get_spsr() & ~mask) | (val & mask));
            } else {
                if (mask & 0xFF) switch_mode(val & 0x1F);
                m_cpsr = (m_cpsr & ~mask) | (val & mask);
            }
        }
        return;
    }

    // 9. Data Processing (ALU)
    if (((instr >> 26) & 0x03) == 0x00) {
        bool imm   = (instr >> 25) & 1;
        uint8_t op = (instr >> 21) & 0x0F;
        bool s     = (instr >> 20) & 1;
        uint8_t rn = (instr >> 16) & 0x0F;
        uint8_t rd = (instr >> 12) & 0x0F;

        uint32_t op1 = (rn == 15) ? (pc + 8) : m_regs[rn];
        uint32_t op2 = 0;
        bool shift_carry = flag_c();

        if (imm) {
            uint32_t imm8 = instr & 0xFF;
            uint32_t rot  = ((instr >> 8) & 0x0F) * 2;
            if (rot) {
                op2 = CoreShared::rotr32(imm8, rot);
                shift_carry = (op2 >> 31) & 1;
            } else {
                op2 = imm8;
            }
        } else {
            uint8_t rm         = instr & 0x0F;
            uint8_t shift_type = (instr >> 5) & 0x03;
            bool reg_shift     = (instr >> 4) & 1;
            uint32_t rm_val    = (rm == 15) ? (pc + (reg_shift ? 12 : 8)) : m_regs[rm];
            if (reg_shift && rn == 15) op1 = pc + 12;

            uint32_t shift_amt = reg_shift ? (m_regs[(instr >> 8) & 0x0F] & 0xFF) : ((instr >> 7) & 0x1F);
            op2 = barrel_shift(rm_val, shift_type, shift_amt, shift_carry, !reg_shift);
        }

        uint32_t res  = 0;
        bool write_rd = true;

        switch (op) {
            case 0x0: res = op1 & op2; if (s) { set_nz(res); set_c(shift_carry); } break;
            case 0x1: res = op1 ^ op2; if (s) { set_nz(res); set_c(shift_carry); } break;
            case 0x2: res = alu_sub(op1, op2, s); break;
            case 0x3: res = alu_sub(op2, op1, s); break;
            case 0x4: res = alu_add(op1, op2, s); break;
            case 0x5: res = alu_adc(op1, op2, flag_c(), s); break;
            case 0x6: res = alu_sbc(op1, op2, flag_c(), s); break;
            case 0x7: res = alu_sbc(op2, op1, flag_c(), s); break;
            case 0x8: res = op1 & op2; set_nz(res); set_c(shift_carry); write_rd = false; break;
            case 0x9: res = op1 ^ op2; set_nz(res); set_c(shift_carry); write_rd = false; break;
            case 0xA: alu_sub(op1, op2, true); write_rd = false; break;
            case 0xB: alu_add(op1, op2, true); write_rd = false; break;
            case 0xC: res = op1 | op2;  if (s) { set_nz(res); set_c(shift_carry); } break;
            case 0xD: res = op2;        if (s) { set_nz(res); set_c(shift_carry); } break;
            case 0xE: res = op1 & ~op2; if (s) { set_nz(res); set_c(shift_carry); } break;
            case 0xF: res = ~op2;       if (s) { set_nz(res); set_c(shift_carry); } break;
        }

        if (write_rd) {
            if (rd == 15) {
                if (s) {
                    uint32_t spsr = get_spsr();
                    switch_mode(spsr & 0x1F);
                    m_cpsr = spsr;
                }
                m_regs[15] = res & (is_thumb() ? ~1U : ~3U);
            } else {
                m_regs[rd] = res;
            }
        }
    }
}

void GBACPU::step_thumb() {
    uint32_t pc    = m_regs[15];
    uint16_t instr = m_mmu.read16(pc);
    m_regs[15]     = pc + 2;

    if ((instr >> 11) == 0x03) {
        bool imm = (instr >> 10) & 1;
        bool sub = (instr >> 9) & 1;
        uint32_t rn_val = imm ? ((instr >> 6) & 7) : m_regs[(instr >> 6) & 7];
        uint8_t rs = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        m_regs[rd] = sub ? alu_sub(m_regs[rs], rn_val, true) : alu_add(m_regs[rs], rn_val, true);
        return;
    }

    if ((instr >> 13) == 0x00) {
        uint8_t op  = (instr >> 11) & 3;
        uint8_t imm = (instr >> 6) & 0x1F;
        uint8_t rs  = (instr >> 3) & 7;
        uint8_t rd  = instr & 7;
        bool carry  = flag_c();
        m_regs[rd]  = barrel_shift(m_regs[rs], op, imm, carry, true);
        set_nz(m_regs[rd]);
        set_c(carry);
        return;
    }

    if ((instr >> 13) == 0x01) {
        uint8_t op   = (instr >> 11) & 3;
        uint8_t rd   = (instr >> 8) & 7;
        uint32_t imm = instr & 0xFF;
        switch (op) {
            case 0: m_regs[rd] = imm; set_nz(imm); break;
            case 1: alu_sub(m_regs[rd], imm, true); break;
            case 2: m_regs[rd] = alu_add(m_regs[rd], imm, true); break;
            case 3: m_regs[rd] = alu_sub(m_regs[rd], imm, true); break;
        }
        return;
    }

    if ((instr >> 10) == 0x10) {
        uint8_t op = (instr >> 6) & 0x0F;
        uint8_t rs = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        bool c     = flag_c();
        switch (op) {
            case 0x0: m_regs[rd] &= m_regs[rs]; set_nz(m_regs[rd]); break;
            case 0x1: m_regs[rd] ^= m_regs[rs]; set_nz(m_regs[rd]); break;
            case 0x2: m_regs[rd] = barrel_shift(m_regs[rd], 0, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break;
            case 0x3: m_regs[rd] = barrel_shift(m_regs[rd], 1, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break;
            case 0x4: m_regs[rd] = barrel_shift(m_regs[rd], 2, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break;
            case 0x5: m_regs[rd] = alu_adc(m_regs[rd], m_regs[rs], c, true); break;
            case 0x6: m_regs[rd] = alu_sbc(m_regs[rd], m_regs[rs], c, true); break;
            case 0x7: m_regs[rd] = barrel_shift(m_regs[rd], 3, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break;
            case 0x8: set_nz(m_regs[rd] & m_regs[rs]); break;
            case 0x9: m_regs[rd] = alu_sub(0, m_regs[rs], true); break;
            case 0xA: alu_sub(m_regs[rd], m_regs[rs], true); break;
            case 0xB: alu_add(m_regs[rd], m_regs[rs], true); break;
            case 0xC: m_regs[rd] |= m_regs[rs]; set_nz(m_regs[rd]); break;
            case 0xD: m_regs[rd] *= m_regs[rs]; set_nz(m_regs[rd]); break;
            case 0xE: m_regs[rd] &= ~m_regs[rs]; set_nz(m_regs[rd]); break;
            case 0xF: m_regs[rd] = ~m_regs[rs]; set_nz(m_regs[rd]); break;
        }
        return;
    }

    if ((instr >> 10) == 0x11) {
        uint8_t op = (instr >> 8) & 3;
        uint8_t rd = (instr & 7) | (((instr >> 7) & 1) << 3);
        uint8_t rs = ((instr >> 3) & 7) | (((instr >> 6) & 1) << 3);
        uint32_t rs_val = (rs == 15) ? (pc + 4) : m_regs[rs];
        uint32_t rd_val = (rd == 15) ? (pc + 4) : m_regs[rd];

        switch (op) {
            case 0:
                if (rd == 15) m_regs[15] = (rd_val + rs_val) & ~1U;
                else          m_regs[rd] = rd_val + rs_val;
                break;
            case 1:
                alu_sub(rd_val, rs_val, true);
                break;
            case 2:
                if (rd == 15) m_regs[15] = rs_val & ~1U;
                else          m_regs[rd] = rs_val;
                break;
            case 3:
                if (rs_val & 1) {
                    m_cpsr |= (1U << 5);
                    m_regs[15] = rs_val & ~1U;
                } else {
                    m_cpsr &= ~(1U << 5);
                    m_regs[15] = rs_val & ~3U;
                }
                break;
        }
        return;
    }

    if ((instr >> 11) == 0x09) {
        uint8_t rd    = (instr >> 8) & 7;
        uint32_t addr = ((pc + 4) & ~3U) + ((instr & 0xFF) << 2);
        m_regs[rd]    = m_mmu.read32(addr);
        return;
    }

    if ((instr >> 12) == 0x05) {
        uint8_t ro    = (instr >> 6) & 7;
        uint8_t rb    = (instr >> 3) & 7;
        uint8_t rd    = instr & 7;
        uint32_t addr = m_regs[rb] + m_regs[ro];

        if (((instr >> 9) & 1) == 0) {
            uint8_t op = (instr >> 10) & 3;
            if (op == 0)      m_mmu.write32(addr, m_regs[rd]);
            else if (op == 1) m_mmu.write8(addr, static_cast<uint8_t>(m_regs[rd]));
            else if (op == 2) m_regs[rd] = m_mmu.read32(addr);
            else if (op == 3) m_regs[rd] = m_mmu.read8(addr);
        } else {
            uint8_t op = (instr >> 10) & 3;
            if (op == 0)      m_mmu.write16(addr, static_cast<uint16_t>(m_regs[rd]));
            else if (op == 1) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(m_mmu.read8(addr))));
            else if (op == 2) m_regs[rd] = m_mmu.read16(addr);
            else if (op == 3) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(m_mmu.read16(addr))));
        }
        return;
    }

    if ((instr >> 13) == 0x03) {
        bool b        = (instr >> 12) & 1;
        bool l        = (instr >> 11) & 1;
        uint32_t imm  = (instr >> 6) & 0x1F;
        uint8_t rb    = (instr >> 3) & 7;
        uint8_t rd    = instr & 7;
        uint32_t addr = m_regs[rb] + (b ? imm : (imm << 2));

        if (l) m_regs[rd] = b ? m_mmu.read8(addr) : m_mmu.read32(addr);
        else {
            if (b) m_mmu.write8(addr, static_cast<uint8_t>(m_regs[rd]));
            else   m_mmu.write32(addr, m_regs[rd]);
        }
        return;
    }

    if ((instr >> 12) == 0x08) {
        bool l        = (instr >> 11) & 1;
        uint32_t imm  = ((instr >> 6) & 0x1F) << 1;
        uint8_t rb    = (instr >> 3) & 7;
        uint8_t rd    = instr & 7;
        uint32_t addr = m_regs[rb] + imm;
        if (l) m_regs[rd] = m_mmu.read16(addr);
        else   m_mmu.write16(addr, static_cast<uint16_t>(m_regs[rd]));
        return;
    }

    if ((instr >> 12) == 0x09) {
        bool l        = (instr >> 11) & 1;
        uint8_t rd    = (instr >> 8) & 7;
        uint32_t addr = m_regs[13] + ((instr & 0xFF) << 2);
        if (l) m_regs[rd] = m_mmu.read32(addr);
        else   m_mmu.write32(addr, m_regs[rd]);
        return;
    }

    if ((instr >> 12) == 0x0A) {
        bool sp       = (instr >> 11) & 1;
        uint8_t rd    = (instr >> 8) & 7;
        uint32_t base = sp ? m_regs[13] : ((pc + 4) & ~3U);
        m_regs[rd]    = base + ((instr & 0xFF) << 2);
        return;
    }

    if ((instr >> 8) == 0xB0) {
        uint32_t imm = (instr & 0x7F) << 2;
        if (instr & 0x80) m_regs[13] -= imm;
        else              m_regs[13] += imm;
        return;
    }

    if (((instr >> 12) == 0x0B) && (((instr >> 9) & 3) == 2)) {
        bool l        = (instr >> 11) & 1;
        bool r        = (instr >> 8) & 1;
        uint8_t rlist = instr & 0xFF;

        if (!l) {
            int count = r ? 1 : 0;
            for (int i = 0; i < 8; ++i) if (rlist & (1 << i)) count++;
            uint32_t addr = m_regs[13] - (count * 4);
            m_regs[13]    = addr;
            for (int i = 0; i < 8; ++i) {
                if (rlist & (1 << i)) {
                    m_mmu.write32(addr, m_regs[i]);
                    addr += 4;
                }
            }
            if (r) m_mmu.write32(addr, m_regs[14]);
        } else {
            uint32_t addr = m_regs[13];
            for (int i = 0; i < 8; ++i) {
                if (rlist & (1 << i)) {
                    m_regs[i] = m_mmu.read32(addr);
                    addr += 4;
                }
            }
            if (r) {
                uint32_t val = m_mmu.read32(addr);
                addr += 4;
                m_regs[15] = val & ~1U;
            }
            m_regs[13] = addr;
        }
        return;
    }

    if ((instr >> 12) == 0x0C) {
        bool l        = (instr >> 11) & 1;
        uint8_t rb    = (instr >> 8) & 7;
        uint8_t rlist = instr & 0xFF;
        uint32_t addr = m_regs[rb];
        for (int i = 0; i < 8; ++i) {
            if (rlist & (1 << i)) {
                if (l) m_regs[i] = m_mmu.read32(addr);
                else   m_mmu.write32(addr, m_regs[i]);
                addr += 4;
            }
        }
        if (!(l && (rlist & (1 << rb)))) {
            m_regs[rb] = addr;
        }
        return;
    }

    if ((instr >> 12) == 0x0D) {
        uint8_t cond = (instr >> 8) & 0x0F;
        if (cond == 0x0F) {
            handle_swi(instr & 0xFF);
            return;
        }
        if (check_cond(cond)) {
            int32_t offset = static_cast<int32_t>(static_cast<int8_t>(instr & 0xFF)) << 1;
            m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 4) + offset);
        }
        return;
    }

    if ((instr >> 11) == 0x1C) {
        int32_t offset = (static_cast<int32_t>(static_cast<int16_t>(instr << 5)) >> 4);
        m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 4) + offset);
        return;
    }

    if ((instr >> 12) == 0x0F) {
        bool low = (instr >> 11) & 1;
        if (!low) {
            int32_t off = (static_cast<int32_t>(static_cast<uint32_t>(instr) << 21) >> 9);
            m_regs[14]  = static_cast<uint32_t>(static_cast<int32_t>(pc + 4) + off);
        } else {
            uint32_t next_pc = (pc + 2) | 1U;
            m_regs[15] = (m_regs[14] + ((instr & 0x7FF) << 1)) & ~1U;
            m_regs[14] = next_pc;
        }
        return;
    }
}

void GBACPU::step() {
    // --- POKEMON FIRERED BOOT GUARD BYPASS ---
    // Automatically flag VBlank in IWRAM so WaitForVBlank() unblocks
    static int vblank_timer = 0;
    vblank_timer++;
    if (vblank_timer >= 280) { // roughly every 60fps frame
        vblank_timer = 0;
        uint16_t bios_if = CoreShared::read_le16(m_mmu.iwram().data(), 0x7FF8);
        CoreShared::write_le16(m_mmu.iwram().data(), 0x7FF8, bios_if | IRQ_VBLANK);
    }

    check_irqs();
    if (m_mmu.is_halted()) return;

    m_mmu.set_in_bios(m_regs[15] < 0x00004000);
    if (is_thumb()) step_thumb();
    else            step_arm();
}