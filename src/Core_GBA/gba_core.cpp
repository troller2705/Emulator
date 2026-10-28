#include "gba_core.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

GBACore::GBACore() {
    m_framebuffer.fill(0xFF000000);
    m_debug_tile_buffer.fill(0xFF000000);
    m_audio_buffer.reserve(1024);
}

void GBACore::reset() {
    m_ewram.fill(0);
    m_iwram.fill(0);
    m_io.fill(0);
    m_pal_ram.fill(0);
    m_vram.fill(0);
    m_oam.fill(0);
    m_bios.fill(0);

    // Install Embedded ARM BIOS IRQ Dispatcher at 0x00000018:
    // 0x18: STMFD SP!, {R0-R3, R12, LR} (0xE92D500F)
    // 0x1C: MOV   R0, #0x04000000       (0xE3A00301)
    // 0x20: ADD   LR, PC, #0            (0xE28FE000)
    // 0x24: LDR   PC, [R0, #-4]         (0xE510F004 -> jumps to [0x03FFFFFC])
    // 0x28: LDMFD SP!, {R0-R3, R12, LR} (0xE8BD500F)
    // 0x2C: SUBS  PC, LR, #4            (0xE25EF004)
    const uint32_t irq_stub[] = {
        0xE92D500F, 0xE3A00301, 0xE28FE000,
        0xE510F004, 0xE8BD500F, 0xE25EF004
    };
    std::memcpy(&m_bios[0x18], irq_stub, sizeof(irq_stub));

    m_regs.fill(0);
    m_sp_usr = 0x03007F00;
    m_sp_irq = 0x03007FA0;
    m_sp_svc = 0x03007FE0;
    m_sp_fiq = 0x03007F00;
    m_lr_usr = m_lr_irq = m_lr_svc = m_lr_fiq = 0;
    m_spsr_irq = m_spsr_svc = m_spsr_fiq = 0x0000001F;

    m_regs[13] = m_sp_usr;
    m_regs[14] = 0x08000000;
    m_regs[15] = 0x08000000;
    m_cpsr     = 0x0000001F; // System mode, ARM state
    m_halted   = false;

    // Default SOUNDBIAS
    io_write16(0x088, 0x0200);
}

bool GBACore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0xC0) {
        std::cerr << "[GBACore] Invalid GBA ROM size.\n";
        return false;
    }

    m_rom = rom_data;
    m_sram.fill(0xFF);
    reset();

    char title[13] = {0};
    char code[5]   = {0};
    std::memcpy(title, &m_rom[0xA0], 12);
    std::memcpy(code,  &m_rom[0xAC], 4);

    std::cout << "--- GBA CARTRIDGE LOADED ---\n";
    std::cout << "Title:     " << title << "\n";
    std::cout << "Game Code: " << code << "\n";
    std::cout << "Size:      " << (m_rom.size() / 1024) << " KB\n";
    std::cout << "----------------------------\n";

    return true;
}

void GBACore::load_battery(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(m_sram.data()), m_sram.size());
    }
}

void GBACore::save_battery(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(m_sram.data()), m_sram.size());
    }
}

// ============================================================================
// CPU MODE BANKING & FLAGS
// ============================================================================

void GBACore::switch_mode(uint8_t new_mode) {
    uint8_t old_mode = m_cpsr & 0x1F;
    if (old_mode == new_mode) return;

    // Save current banked SP/LR
    switch (old_mode) {
        case 0x11: m_sp_fiq = m_regs[13]; m_lr_fiq = m_regs[14]; break;
        case 0x12: m_sp_irq = m_regs[13]; m_lr_irq = m_regs[14]; break;
        case 0x13: m_sp_svc = m_regs[13]; m_lr_svc = m_regs[14]; break;
        default:   m_sp_usr = m_regs[13]; m_lr_usr = m_regs[14]; break;
    }

    // Load new banked SP/LR
    switch (new_mode) {
        case 0x11: m_regs[13] = m_sp_fiq; m_regs[14] = m_lr_fiq; break;
        case 0x12: m_regs[13] = m_sp_irq; m_regs[14] = m_lr_irq; break;
        case 0x13: m_regs[13] = m_sp_svc; m_regs[14] = m_lr_svc; break;
        default:   m_regs[13] = m_sp_usr; m_regs[14] = m_lr_usr; break;
    }

    m_cpsr = (m_cpsr & ~0x1FU) | (new_mode & 0x1F);
}

uint32_t GBACore::get_spsr() const {
    switch (m_cpsr & 0x1F) {
        case 0x11: return m_spsr_fiq;
        case 0x12: return m_spsr_irq;
        case 0x13: return m_spsr_svc;
        default:   return m_cpsr;
    }
}

void GBACore::set_spsr(uint32_t val) {
    switch (m_cpsr & 0x1F) {
        case 0x11: m_spsr_fiq = val; break;
        case 0x12: m_spsr_irq = val; break;
        case 0x13: m_spsr_svc = val; break;
        default: break;
    }
}

bool GBACore::check_cond(uint8_t cond) const {
    switch (cond) {
        case 0x0: return flag_z();                          // EQ
        case 0x1: return !flag_z();                         // NE
        case 0x2: return flag_c();                          // CS/HS
        case 0x3: return !flag_c();                         // CC/LO
        case 0x4: return flag_n();                          // MI
        case 0x5: return !flag_n();                         // PL
        case 0x6: return flag_v();                          // VS
        case 0x7: return !flag_v();                         // VC
        case 0x8: return flag_c() && !flag_z();             // HI
        case 0x9: return !flag_c() || flag_z();             // LS
        case 0xA: return flag_n() == flag_v();              // GE
        case 0xB: return flag_n() != flag_v();              // LT
        case 0xC: return !flag_z() && (flag_n() == flag_v());// GT
        case 0xD: return flag_z() || (flag_n() != flag_v()); // LE
        case 0xE: return true;                              // AL
        default:  return true;
    }
}

uint32_t GBACore::barrel_shift(uint32_t val, uint8_t type, uint32_t amount, bool& carry_out, bool immediate_shift) {
    if (immediate_shift && amount == 0) {
        switch (type) {
            case 0: return val; // LSL #0
            case 1: carry_out = (val >> 31) & 1; return 0; // LSR #32
            case 2: carry_out = (val >> 31) & 1; return carry_out ? 0xFFFFFFFFU : 0; // ASR #32
            case 3: { // RRX #1
                bool old_c = carry_out;
                carry_out = val & 1;
                return (val >> 1) | (old_c ? 0x80000000U : 0);
            }
        }
    }

    if (amount == 0) return val;

    switch (type) {
        case 0: // LSL
            if (amount < 32) {
                carry_out = (val >> (32 - amount)) & 1;
                return val << amount;
            } else if (amount == 32) {
                carry_out = val & 1;
                return 0;
            } else {
                carry_out = false;
                return 0;
            }
        case 1: // LSR
            if (amount < 32) {
                carry_out = (val >> (amount - 1)) & 1;
                return val >> amount;
            } else if (amount == 32) {
                carry_out = (val >> 31) & 1;
                return 0;
            } else {
                carry_out = false;
                return 0;
            }
        case 2: // ASR
            if (amount < 32) {
                carry_out = (val >> (amount - 1)) & 1;
                return static_cast<uint32_t>(static_cast<int32_t>(val) >> amount);
            } else {
                carry_out = (val >> 31) & 1;
                return carry_out ? 0xFFFFFFFFU : 0;
            }
        case 3: // ROR
            amount &= 31;
            if (amount == 0) {
                carry_out = (val >> 31) & 1;
                return val;
            }
            val = (val >> amount) | (val << (32 - amount));
            carry_out = (val >> 31) & 1;
            return val;
    }
    return val;
}

uint32_t GBACore::alu_add(uint32_t a, uint32_t b, bool s) {
    uint64_t res64 = static_cast<uint64_t>(a) + b;
    uint32_t res = static_cast<uint32_t>(res64);
    if (s) {
        set_nz(res);
        set_c(res64 > 0xFFFFFFFFULL);
        set_v(((~(a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

uint32_t GBACore::alu_adc(uint32_t a, uint32_t b, bool c_in, bool s) {
    uint64_t res64 = static_cast<uint64_t>(a) + b + (c_in ? 1 : 0);
    uint32_t res = static_cast<uint32_t>(res64);
    if (s) {
        set_nz(res);
        set_c(res64 > 0xFFFFFFFFULL);
        set_v(((~(a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

uint32_t GBACore::alu_sub(uint32_t a, uint32_t b, bool s) {
    uint32_t res = a - b;
    if (s) {
        set_nz(res);
        set_c(a >= b);
        set_v((((a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

uint32_t GBACore::alu_sbc(uint32_t a, uint32_t b, bool c_in, bool s) {
    uint64_t borrow = static_cast<uint64_t>(b) + (c_in ? 0 : 1);
    uint32_t res = static_cast<uint32_t>(static_cast<uint64_t>(a) - borrow);
    if (s) {
        set_nz(res);
        set_c(static_cast<uint64_t>(a) >= borrow);
        set_v((((a ^ b) & (a ^ res)) >> 31) & 1);
    }
    return res;
}

// ============================================================================
// MEMORY BUS & DMA
// ============================================================================

uint8_t GBACore::read8(uint32_t addr) const {
    switch (addr >> 24) {
        case 0x00: return (addr < 0x4000) ? m_bios[addr] : 0;
        case 0x02: return m_ewram[addr & 0x3FFFF];
        case 0x03: return m_iwram[addr & 0x7FFF];
        case 0x04: {
            uint32_t offset = addr & 0x3FF;
            if (offset == 0x130) return m_keyinput & 0xFF;
            if (offset == 0x131) return (m_keyinput >> 8) & 0xFF;
            return m_io[offset];
        }
        case 0x05: return m_pal_ram[addr & 0x3FF];
        case 0x06: {
            uint32_t offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            return m_vram[offset];
        }
        case 0x07: return m_oam[addr & 0x3FF];
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: {
            uint32_t offset = addr & 0x1FFFFFF;
            return (offset < m_rom.size()) ? m_rom[offset] : 0xFF;
        }
        case 0x0E: case 0x0F:
            return m_sram[addr & 0xFFFF];
        default: return 0;
    }
}

uint16_t GBACore::read16(uint32_t addr) const {
    addr &= ~1U;
    return read8(addr) | (static_cast<uint16_t>(read8(addr + 1)) << 8);
}

uint32_t GBACore::read32(uint32_t addr) const {
    uint32_t aligned = addr & ~3U;
    uint32_t val = read16(aligned) | (static_cast<uint32_t>(read16(aligned + 2)) << 16);
    // Unaligned 32-bit reads rotate right by (addr & 3) * 8 on ARM7TDMI
    uint32_t rot = (addr & 3) * 8;
    if (rot) val = (val >> rot) | (val << (32 - rot));
    return val;
}

void GBACore::write8(uint32_t addr, uint8_t val) {
    switch (addr >> 24) {
        case 0x02: m_ewram[addr & 0x3FFFF] = val; break;
        case 0x03: m_iwram[addr & 0x7FFF]  = val; break;
        case 0x04: {
            uint32_t offset = addr & 0x3FF;
            // Writing 1s to IF (0x202/0x203) acknowledges and clears interrupt flags!
            if (offset == 0x202 || offset == 0x203) {
                m_io[offset] &= ~val;
                return;
            }
            if (offset == 0x301) { // HALTCNT
                m_halted = true;
                return;
            }
            m_io[offset] = val;
            // Check if DMA0-3 Control High byte was enabled
            if (offset == 0x0BB && (val & 0x80)) trigger_dma(0);
            if (offset == 0x0C7 && (val & 0x80)) trigger_dma(1);
            if (offset == 0x0D3 && (val & 0x80)) trigger_dma(2);
            if (offset == 0x0DF && (val & 0x80)) trigger_dma(3);
            break;
        }
        case 0x05: {
            // 8-bit writes to Palette RAM duplicate the byte across the 16-bit halfword
            uint32_t offset = addr & 0x3FE;
            m_pal_ram[offset] = m_pal_ram[offset + 1] = val;
            break;
        }
        case 0x06: {
            uint32_t offset = addr & 0x1FFFE;
            if (offset >= 0x18000) offset -= 0x8000;
            m_vram[offset] = m_vram[offset + 1] = val;
            break;
        }
        case 0x07: break; // 8-bit writes to OAM are ignored by GBA hardware
        case 0x0E: case 0x0F:
            m_sram[addr & 0xFFFF] = val;
            break;
        default: break;
    }
}

void GBACore::write16(uint32_t addr, uint16_t val) {
    addr &= ~1U;
    uint32_t region = addr >> 24;
    if (region == 0x05) {
        uint32_t offset = addr & 0x3FE;
        m_pal_ram[offset]     = val & 0xFF;
        m_pal_ram[offset + 1] = (val >> 8) & 0xFF;
        return;
    }
    if (region == 0x06) {
        uint32_t offset = addr & 0x1FFFE;
        if (offset >= 0x18000) offset -= 0x8000;
        m_vram[offset]     = val & 0xFF;
        m_vram[offset + 1] = (val >> 8) & 0xFF;
        return;
    }
    if (region == 0x07) {
        uint32_t offset = addr & 0x3FE;
        m_oam[offset]     = val & 0xFF;
        m_oam[offset + 1] = (val >> 8) & 0xFF;
        return;
    }
    write8(addr, val & 0xFF);
    write8(addr + 1, (val >> 8) & 0xFF);
}

void GBACore::write32(uint32_t addr, uint32_t val) {
    addr &= ~3U;
    write16(addr, val & 0xFFFF);
    write16(addr + 2, (val >> 16) & 0xFFFF);
}

void GBACore::trigger_dma(int ch) {
    uint32_t base = 0x0B0 + (ch * 12);
    uint16_t cnt_h = io_read16(base + 10);
    if (!(cnt_h & 0x8000)) return;

    // Only execute Immediate DMAs immediately (Start timing = bits 12-13 == 0)
    uint8_t timing = (cnt_h >> 12) & 0x03;
    if (timing != 0) return;

    uint32_t src = io_read16(base) | (static_cast<uint32_t>(io_read16(base + 2)) << 16);
    uint32_t dst = io_read16(base + 4) | (static_cast<uint32_t>(io_read16(base + 6)) << 16);
    uint32_t count = io_read16(base + 8);
    if (count == 0) count = (ch == 3) ? 0x10000 : 0x4000;

    bool is_32bit = (cnt_h >> 10) & 1;
    uint32_t step = is_32bit ? 4 : 2;

    int dst_ctrl = (cnt_h >> 5) & 3;
    int src_ctrl = (cnt_h >> 7) & 3;

    for (uint32_t i = 0; i < count; ++i) {
        if (is_32bit) {
            write32(dst, read32(src));
        } else {
            write16(dst, read16(src));
        }

        if (src_ctrl == 0 || src_ctrl == 3) src += step;
        else if (src_ctrl == 1)             src -= step;

        if (dst_ctrl == 0 || dst_ctrl == 3) dst += step;
        else if (dst_ctrl == 1)             dst -= step;
    }

    // Clear DMA Enable bit if not repeating
    if (!(cnt_h & (1 << 9))) {
        io_write16(base + 10, cnt_h & ~0x8000U);
    }

    // Fire DMA IRQ if requested
    if (cnt_h & (1 << 14)) {
        request_irq(1 << (8 + ch));
    }
}

void GBACore::request_irq(uint16_t flag) {
    uint16_t if_reg = io_read16(0x202) | flag;
    io_write16(0x202, if_reg);

    // Also update BIOS IF mirror at 0x03007FF8 for VBlankIntrWait compatibility
    uint16_t bios_if = m_iwram[0x7FF8] | (static_cast<uint16_t>(m_iwram[0x7FF9]) << 8);
    bios_if |= flag;
    m_iwram[0x7FF8] = bios_if & 0xFF;
    m_iwram[0x7FF9] = (bios_if >> 8) & 0xFF;
}

void GBACore::check_irqs() {
    uint16_t ie  = io_read16(0x200);
    uint16_t ifr = io_read16(0x202);
    if (ie & ifr) {
        m_halted = false;
        uint16_t ime = io_read16(0x208);
        bool irq_disabled = (m_cpsr >> 7) & 1;
        if ((ime & 1) && !irq_disabled) {
            uint32_t old_cpsr = m_cpsr;
            uint32_t next_pc  = m_regs[15];
            switch_mode(0x12); // Switch to IRQ Mode
            m_spsr_irq = old_cpsr;
            m_regs[14] = next_pc + 4;
            m_cpsr &= ~(1U << 5); // Switch to ARM state
            m_cpsr |= (1U << 7);  // Disable further IRQs
            m_regs[15] = 0x00000018; // Jump to Embedded BIOS IRQ Stub
        }
    }
}

// ============================================================================
// HLE BIOS SWI HANDLER
// ============================================================================

void GBACore::handle_swi(uint8_t comment) {
    switch (comment) {
        case 0x00: // SoftReset
            reset();
            break;
        case 0x01: { // RegisterRamReset
            uint32_t flags = m_regs[0];
            if (flags & 0x01) m_ewram.fill(0);
            if (flags & 0x02) std::memset(m_iwram.data(), 0, 0x7E00);
            if (flags & 0x04) m_pal_ram.fill(0);
            if (flags & 0x08) m_vram.fill(0);
            if (flags & 0x10) m_oam.fill(0);
            break;
        }
        case 0x02: // Halt
        case 0x04: // IntrWait
        case 0x05: // VBlankIntrWait
            m_halted = true;
            break;
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
        case 0x08: // Sqrt
            m_regs[0] = static_cast<uint32_t>(std::sqrt(m_regs[0]));
            break;
        case 0x0B: { // CpuSet
            uint32_t src = m_regs[0];
            uint32_t dst = m_regs[1];
            uint32_t cnt = m_regs[2];
            uint32_t count = cnt & 0x1FFFFF;
            bool fixed = (cnt >> 24) & 1;
            bool is32  = (cnt >> 26) & 1;
            for (uint32_t i = 0; i < count; ++i) {
                if (is32) {
                    write32(dst, read32(src));
                    dst += 4;
                    if (!fixed) src += 4;
                } else {
                    write16(dst, read16(src));
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
            uint32_t count = cnt & 0x1FFFFF;
            bool fixed = (cnt >> 24) & 1;
            for (uint32_t i = 0; i < count; ++i) {
                write32(dst, read32(src));
                dst += 4;
                if (!fixed) src += 4;
            }
            break;
        }
        case 0x11: // LZ77UnCompWram
        case 0x12: { // LZ77UnCompVram
            uint32_t src = m_regs[0];
            uint32_t dst = m_regs[1];
            uint32_t header = read32(src);
            uint32_t decomp_size = header >> 8;
            src += 4;

            std::vector<uint8_t> out;
            out.reserve(decomp_size);

            while (out.size() < decomp_size) {
                uint8_t flags = read8(src++);
                for (int bit = 7; bit >= 0 && out.size() < decomp_size; --bit) {
                    if (flags & (1 << bit)) {
                        uint8_t b1 = read8(src++);
                        uint8_t b2 = read8(src++);
                        uint32_t len = (b1 >> 4) + 3;
                        uint32_t disp = (((b1 & 0x0F) << 8) | b2) + 1;
                        for (uint32_t j = 0; j < len && out.size() < decomp_size; ++j) {
                            out.push_back(out[out.size() - disp]);
                        }
                    } else {
                        out.push_back(read8(src++));
                    }
                }
            }

            if (comment == 0x12) {
                for (size_t i = 0; i + 1 < out.size(); i += 2) {
                    write16(dst + static_cast<uint32_t>(i), out[i] | (static_cast<uint16_t>(out[i + 1]) << 8));
                }
            } else {
                for (size_t i = 0; i < out.size(); ++i) {
                    write8(dst + static_cast<uint32_t>(i), out[i]);
                }
            }
            break;
        }
        default: break;
    }
}

// ============================================================================
// ARM & THUMB INSTRUCTION DECODERS
// ============================================================================

void GBACore::step_arm() {
    uint32_t pc = m_regs[15];
    uint32_t instr = read32(pc);
    m_regs[15] = pc + 4;

    uint8_t cond = (instr >> 28) & 0x0F;
    if (!check_cond(cond)) return;

    // 1. Branch & Exchange (BX / BLX)
    if ((instr & 0x0FFFFFF0) == 0x012FFF10 || (instr & 0x0FFFFFF0) == 0x012FFF30) {
        bool link = ((instr >> 4) & 0x0F) == 0x03;
        uint32_t target = m_regs[instr & 0x0F];
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

    // 2. Software Interrupt (SWI)
    if (((instr >> 24) & 0x0F) == 0x0F) {
        handle_swi(static_cast<uint8_t>((instr >> 16) & 0xFF));
        return;
    }

    // 3. Branch / Branch with Link (B / BL)
    if (((instr >> 25) & 0x07) == 0x05) {
        if ((instr >> 24) & 1) m_regs[14] = pc + 4;
        int32_t offset = (static_cast<int32_t>(instr << 8) >> 6);
        m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 8) + offset);
        return;
    }

    // 4. Block Data Transfer (LDM / STM)
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

        for (int i = 0; i < 16; ++i) {
            if (rlist & (1 << i)) {
                if (l) {
                    uint32_t val = read32(addr);
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
                    write32(addr, val);
                }
                addr += 4;
            }
        }

        if (w && !(l && (rlist & (1 << rn)))) {
            m_regs[rn] = u ? (base + count * 4) : (base - count * 4);
        }
        return;
    }

    // 5. Single Data Transfer (LDR / STR / LDRB / STRB)
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

        uint32_t base = (rn == 15) ? (pc + 8) : m_regs[rn];
        uint32_t mod_addr = u ? (base + offset) : (base - offset);
        uint32_t addr = p ? mod_addr : base;

        if (l) {
            uint32_t val = b ? read8(addr) : read32(addr);
            if (rd == 15) {
                m_regs[15] = val & ~3U;
            } else {
                m_regs[rd] = val;
            }
        } else {
            uint32_t val = (rd == 15) ? (pc + 12) : m_regs[rd];
            if (b) write8(addr, static_cast<uint8_t>(val));
            else   write32(addr, val);
        }

        if ((!p || w) && !(l && rd == rn)) {
            m_regs[rn] = mod_addr;
        }
        return;
    }

    // 6. Multiply & Halfword / Signed Data Transfer (bits 27:25 == 000, bits 7 & 4 == 1)
    if (((instr >> 25) & 0x07) == 0x00 && (instr & 0x90) == 0x90) {
        uint8_t sub_op = (instr >> 5) & 0x03;
        if (sub_op == 0) {
            // Multiply (MUL / MLA) or Long Multiply (UMULL / SMULL / UMLAL / SMLAL)
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
            // Halfword & Signed Data Transfer (LDRH / STRH / LDRSB / LDRSH)
            bool p   = (instr >> 24) & 1;
            bool u   = (instr >> 23) & 1;
            bool imm = (instr >> 22) & 1;
            bool w   = (instr >> 21) & 1;
            bool l   = (instr >> 20) & 1;
            uint8_t rn = (instr >> 16) & 0x0F;
            uint8_t rd = (instr >> 12) & 0x0F;

            uint32_t offset = imm ? (((instr >> 4) & 0xF0) | (instr & 0x0F)) : m_regs[instr & 0x0F];
            uint32_t base = (rn == 15) ? (pc + 8) : m_regs[rn];
            uint32_t mod_addr = u ? (base + offset) : (base - offset);
            uint32_t addr = p ? mod_addr : base;

            if (l) {
                if (sub_op == 1)      m_regs[rd] = read16(addr); // LDRH
                else if (sub_op == 2) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(read8(addr)))); // LDRSB
                else if (sub_op == 3) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(read16(addr)))); // LDRSH
            } else if (sub_op == 1) {
                write16(addr, static_cast<uint16_t>(m_regs[rd])); // STRH
            }

            if ((!p || w) && !(l && rd == rn)) {
                m_regs[rn] = mod_addr;
            }
            return;
        }
    }

    // 7. PSR Transfer (MRS / MSR)
    if (((instr >> 26) & 0x03) == 0x00 && ((instr >> 23) & 0x03) == 0x02 && ((instr >> 20) & 1) == 0) {
        bool use_spsr = (instr >> 22) & 1;
        if (((instr >> 21) & 1) == 0) {
            // MRS Rd, CPSR/SPSR
            uint8_t rd = (instr >> 12) & 0x0F;
            m_regs[rd] = use_spsr ? get_spsr() : m_cpsr;
        } else {
            // MSR CPSR/SPSR, Op2
            bool imm = (instr >> 25) & 1;
            uint32_t val = 0;
            if (imm) {
                uint32_t imm8 = instr & 0xFF;
                uint32_t rot  = ((instr >> 8) & 0x0F) * 2;
                val = rot ? ((imm8 >> rot) | (imm8 << (32 - rot))) : imm8;
            } else {
                val = m_regs[instr & 0x0F];
            }

            uint32_t mask = 0;
            if ((instr >> 16) & 1) mask |= 0x000000FFU;
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

    // 8. Data Processing (ALU)
    if (((instr >> 26) & 0x03) == 0x00) {
        bool imm     = (instr >> 25) & 1;
        uint8_t op   = (instr >> 21) & 0x0F;
        bool s       = (instr >> 20) & 1;
        uint8_t rn   = (instr >> 16) & 0x0F;
        uint8_t rd   = (instr >> 12) & 0x0F;

        uint32_t op1 = (rn == 15) ? (pc + 8) : m_regs[rn];
        uint32_t op2 = 0;
        bool shift_carry = flag_c();

        if (imm) {
            uint32_t imm8 = instr & 0xFF;
            uint32_t rot  = ((instr >> 8) & 0x0F) * 2;
            if (rot) {
                op2 = (imm8 >> rot) | (imm8 << (32 - rot));
                shift_carry = (op2 >> 31) & 1;
            } else {
                op2 = imm8;
            }
        } else {
            uint8_t rm = instr & 0x0F;
            uint8_t shift_type = (instr >> 5) & 0x03;
            bool reg_shift = (instr >> 4) & 1;
            uint32_t rm_val = (rm == 15) ? (pc + (reg_shift ? 12 : 8)) : m_regs[rm];
            if (reg_shift && rn == 15) op1 = pc + 12;

            uint32_t shift_amt = reg_shift ? (m_regs[(instr >> 8) & 0x0F] & 0xFF) : ((instr >> 7) & 0x1F);
            op2 = barrel_shift(rm_val, shift_type, shift_amt, shift_carry, !reg_shift);
        }

        uint32_t res = 0;
        bool write_rd = true;

        switch (op) {
            case 0x0: res = op1 & op2; if (s) { set_nz(res); set_c(shift_carry); } break; // AND
            case 0x1: res = op1 ^ op2; if (s) { set_nz(res); set_c(shift_carry); } break; // EOR
            case 0x2: res = alu_sub(op1, op2, s); break;                                  // SUB
            case 0x3: res = alu_sub(op2, op1, s); break;                                  // RSB
            case 0x4: res = alu_add(op1, op2, s); break;                                  // ADD
            case 0x5: res = alu_adc(op1, op2, flag_c(), s); break;                        // ADC
            case 0x6: res = alu_sbc(op1, op2, flag_c(), s); break;                        // SBC
            case 0x7: res = alu_sbc(op2, op1, flag_c(), s); break;                        // RSC
            case 0x8: res = op1 & op2; set_nz(res); set_c(shift_carry); write_rd = false; break; // TST
            case 0x9: res = op1 ^ op2; set_nz(res); set_c(shift_carry); write_rd = false; break; // TEQ
            case 0xA: alu_sub(op1, op2, true); write_rd = false; break;                   // CMP
            case 0xB: alu_add(op1, op2, true); write_rd = false; break;                   // CMN
            case 0xC: res = op1 | op2; if (s) { set_nz(res); set_c(shift_carry); } break; // ORR
            case 0xD: res = op2;       if (s) { set_nz(res); set_c(shift_carry); } break; // MOV
            case 0xE: res = op1 & ~op2;if (s) { set_nz(res); set_c(shift_carry); } break; // BIC
            case 0xF: res = ~op2;      if (s) { set_nz(res); set_c(shift_carry); } break; // MVN
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

void GBACore::step_thumb() {
    uint32_t pc = m_regs[15];
    uint16_t instr = read16(pc);
    m_regs[15] = pc + 2;

    // Format 2: Add/Subtract (00011...)
    if ((instr >> 11) == 0x03) {
        bool imm = (instr >> 10) & 1;
        bool sub = (instr >> 9) & 1;
        uint32_t rn_val = imm ? ((instr >> 6) & 7) : m_regs[(instr >> 6) & 7];
        uint8_t rs = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        m_regs[rd] = sub ? alu_sub(m_regs[rs], rn_val, true) : alu_add(m_regs[rs], rn_val, true);
        return;
    }

    // Format 1: Move Shifted Register (000...)
    if ((instr >> 13) == 0x00) {
        uint8_t op  = (instr >> 11) & 3;
        uint8_t imm = (instr >> 6) & 0x1F;
        uint8_t rs  = (instr >> 3) & 7;
        uint8_t rd  = instr & 7;
        bool carry = flag_c();
        m_regs[rd] = barrel_shift(m_regs[rs], op, imm, carry, true);
        set_nz(m_regs[rd]);
        set_c(carry);
        return;
    }

    // Format 3: MOV / CMP / ADD / SUB Immediate (001...)
    if ((instr >> 13) == 0x01) {
        uint8_t op  = (instr >> 11) & 3;
        uint8_t rd  = (instr >> 8) & 7;
        uint32_t imm = instr & 0xFF;
        switch (op) {
            case 0: m_regs[rd] = imm; set_nz(imm); break;                  // MOV
            case 1: alu_sub(m_regs[rd], imm, true); break;                 // CMP
            case 2: m_regs[rd] = alu_add(m_regs[rd], imm, true); break;    // ADD
            case 3: m_regs[rd] = alu_sub(m_regs[rd], imm, true); break;    // SUB
        }
        return;
    }

    // Format 4: ALU Operations (010000...)
    if ((instr >> 10) == 0x10) {
        uint8_t op = (instr >> 6) & 0x0F;
        uint8_t rs = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        bool c = flag_c();
        switch (op) {
            case 0x0: m_regs[rd] &= m_regs[rs]; set_nz(m_regs[rd]); break; // AND
            case 0x1: m_regs[rd] ^= m_regs[rs]; set_nz(m_regs[rd]); break; // EOR
            case 0x2: m_regs[rd] = barrel_shift(m_regs[rd], 0, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break; // LSL
            case 0x3: m_regs[rd] = barrel_shift(m_regs[rd], 1, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break; // LSR
            case 0x4: m_regs[rd] = barrel_shift(m_regs[rd], 2, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break; // ASR
            case 0x5: m_regs[rd] = alu_adc(m_regs[rd], m_regs[rs], c, true); break; // ADC
            case 0x6: m_regs[rd] = alu_sbc(m_regs[rd], m_regs[rs], c, true); break; // SBC
            case 0x7: m_regs[rd] = barrel_shift(m_regs[rd], 3, m_regs[rs] & 0xFF, c, false); set_nz(m_regs[rd]); set_c(c); break; // ROR
            case 0x8: set_nz(m_regs[rd] & m_regs[rs]); break;              // TST
            case 0x9: m_regs[rd] = alu_sub(0, m_regs[rs], true); break;    // NEG
            case 0xA: alu_sub(m_regs[rd], m_regs[rs], true); break;        // CMP
            case 0xB: alu_add(m_regs[rd], m_regs[rs], true); break;        // CMN
            case 0xC: m_regs[rd] |= m_regs[rs]; set_nz(m_regs[rd]); break; // ORR
            case 0xD: m_regs[rd] *= m_regs[rs]; set_nz(m_regs[rd]); break; // MUL
            case 0xE: m_regs[rd] &= ~m_regs[rs]; set_nz(m_regs[rd]); break;// BIC
            case 0xF: m_regs[rd] = ~m_regs[rs]; set_nz(m_regs[rd]); break; // MVN
        }
        return;
    }

    // Format 5: Hi Register Operations / BX (010001...)
    if ((instr >> 10) == 0x11) {
        uint8_t op = (instr >> 8) & 3;
        uint8_t rd = (instr & 7) | (((instr >> 7) & 1) << 3);
        uint8_t rs = ((instr >> 3) & 7) | (((instr >> 6) & 1) << 3);
        uint32_t rs_val = (rs == 15) ? (pc + 4) : m_regs[rs];
        uint32_t rd_val = (rd == 15) ? (pc + 4) : m_regs[rd];

        switch (op) {
            case 0: // ADD
                if (rd == 15) m_regs[15] = (rd_val + rs_val) & ~1U;
                else          m_regs[rd] = rd_val + rs_val;
                break;
            case 1: // CMP
                alu_sub(rd_val, rs_val, true);
                break;
            case 2: // MOV
                if (rd == 15) m_regs[15] = rs_val & ~1U;
                else          m_regs[rd] = rs_val;
                break;
            case 3: // BX / BLX
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

    // Format 6: PC-Relative Load (01001...)
    if ((instr >> 11) == 0x09) {
        uint8_t rd = (instr >> 8) & 7;
        uint32_t addr = ((pc + 4) & ~3U) + ((instr & 0xFF) << 2);
        m_regs[rd] = read32(addr);
        return;
    }

    // Format 7 & 8: Load/Store with Register Offset (0101...)
    if ((instr >> 12) == 0x05) {
        uint8_t ro = (instr >> 6) & 7;
        uint8_t rb = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        uint32_t addr = m_regs[rb] + m_regs[ro];

        if (((instr >> 9) & 1) == 0) {
            uint8_t op = (instr >> 10) & 3;
            if (op == 0)      write32(addr, m_regs[rd]);
            else if (op == 1) write8(addr, static_cast<uint8_t>(m_regs[rd]));
            else if (op == 2) m_regs[rd] = read32(addr);
            else if (op == 3) m_regs[rd] = read8(addr);
        } else {
            uint8_t op = (instr >> 10) & 3;
            if (op == 0)      write16(addr, static_cast<uint16_t>(m_regs[rd]));
            else if (op == 1) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(read8(addr))));
            else if (op == 2) m_regs[rd] = read16(addr);
            else if (op == 3) m_regs[rd] = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(read16(addr))));
        }
        return;
    }

    // Format 9: Load/Store with Immediate Offset (011...)
    if ((instr >> 13) == 0x03) {
        bool b = (instr >> 12) & 1;
        bool l = (instr >> 11) & 1;
        uint32_t imm = (instr >> 6) & 0x1F;
        uint8_t rb = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        uint32_t addr = m_regs[rb] + (b ? imm : (imm << 2));

        if (l) m_regs[rd] = b ? read8(addr) : read32(addr);
        else {
            if (b) write8(addr, static_cast<uint8_t>(m_regs[rd]));
            else   write32(addr, m_regs[rd]);
        }
        return;
    }

    // Format 10: Load/Store Halfword (1000...)
    if ((instr >> 12) == 0x08) {
        bool l = (instr >> 11) & 1;
        uint32_t imm = ((instr >> 6) & 0x1F) << 1;
        uint8_t rb = (instr >> 3) & 7;
        uint8_t rd = instr & 7;
        uint32_t addr = m_regs[rb] + imm;
        if (l) m_regs[rd] = read16(addr);
        else   write16(addr, static_cast<uint16_t>(m_regs[rd]));
        return;
    }

    // Format 11: SP-Relative Load/Store (1001...)
    if ((instr >> 12) == 0x09) {
        bool l = (instr >> 11) & 1;
        uint8_t rd = (instr >> 8) & 7;
        uint32_t addr = m_regs[13] + ((instr & 0xFF) << 2);
        if (l) m_regs[rd] = read32(addr);
        else   write32(addr, m_regs[rd]);
        return;
    }

    // Format 12: Load Address (1010...)
    if ((instr >> 12) == 0x0A) {
        bool sp = (instr >> 11) & 1;
        uint8_t rd = (instr >> 8) & 7;
        uint32_t base = sp ? m_regs[13] : ((pc + 4) & ~3U);
        m_regs[rd] = base + ((instr & 0xFF) << 2);
        return;
    }

    // Format 13: Add Offset to Stack Pointer (10110000...)
    if ((instr >> 8) == 0xB0) {
        uint32_t imm = (instr & 0x7F) << 2;
        if (instr & 0x80) m_regs[13] -= imm;
        else              m_regs[13] += imm;
        return;
    }

    // Format 14: Push / Pop Registers (1011x10...)
    if (((instr >> 12) == 0x0B) && (((instr >> 9) & 3) == 2)) {
        bool l = (instr >> 11) & 1;
        bool r = (instr >> 8) & 1;
        uint8_t rlist = instr & 0xFF;

        if (!l) { // PUSH
            int count = r ? 1 : 0;
            for (int i = 0; i < 8; ++i) if (rlist & (1 << i)) count++;
            uint32_t addr = m_regs[13] - (count * 4);
            m_regs[13] = addr;
            for (int i = 0; i < 8; ++i) {
                if (rlist & (1 << i)) {
                    write32(addr, m_regs[i]);
                    addr += 4;
                }
            }
            if (r) write32(addr, m_regs[14]);
        } else { // POP
            uint32_t addr = m_regs[13];
            for (int i = 0; i < 8; ++i) {
                if (rlist & (1 << i)) {
                    m_regs[i] = read32(addr);
                    addr += 4;
                }
            }
            if (r) {
                uint32_t val = read32(addr);
                addr += 4;
                m_regs[15] = val & ~1U;
            }
            m_regs[13] = addr;
        }
        return;
    }

    // Format 15: Multiple Load/Store (STMIA / LDMIA) (1100...)
    if ((instr >> 12) == 0x0C) {
        bool l = (instr >> 11) & 1;
        uint8_t rb = (instr >> 8) & 7;
        uint8_t rlist = instr & 0xFF;
        uint32_t addr = m_regs[rb];
        for (int i = 0; i < 8; ++i) {
            if (rlist & (1 << i)) {
                if (l) m_regs[i] = read32(addr);
                else   write32(addr, m_regs[i]);
                addr += 4;
            }
        }
        if (!(l && (rlist & (1 << rb)))) {
            m_regs[rb] = addr;
        }
        return;
    }

    // Format 16 & 17: Conditional Branch / SWI (1101...)
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

    // Format 18: Unconditional Branch (11100...)
    if ((instr >> 11) == 0x1C) {
        int32_t offset = (static_cast<int32_t>(static_cast<int16_t>(instr << 5)) >> 4);
        m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 4) + offset);
        return;
    }

    // Format 19: Long Branch with Link (BL) (1111...)
    if ((instr >> 12) == 0x0F) {
        bool low = (instr >> 11) & 1;
        if (!low) {
            int32_t off = (static_cast<int32_t>(instr << 21) >> 9);
            m_regs[14] = static_cast<uint32_t>(static_cast<int32_t>(pc + 4) + off);
        } else {
            uint32_t next_pc = (pc + 2) | 1U;
            m_regs[15] = (m_regs[14] + ((instr & 0x7FF) << 1)) & ~1U;
            m_regs[14] = next_pc;
        }
        return;
    }
}

void GBACore::step_instruction() {
    check_irqs();
    if (m_halted) return;

    if (is_thumb()) step_thumb();
    else            step_arm();
}

// ============================================================================
// PPU SCANLINE RENDERER (MODES 0-5 + SPRITES)
// ============================================================================

uint32_t GBACore::bgr555_to_argb8888(uint16_t bgr) {
    uint8_t r5 = bgr & 0x1F;
    uint8_t g5 = (bgr >> 5) & 0x1F;
    uint8_t b5 = (bgr >> 10) & 0x1F;

    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g5 << 3) | (g5 >> 2);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    return 0xFF000000U | (r8 << 16) | (g8 << 8) | b8;
}

void GBACore::render_scanline(int ly) {
    uint16_t dispcnt = io_read16(0x000);
    uint8_t mode = dispcnt & 0x07;

    uint16_t backdrop_bgr = m_pal_ram[0] | (static_cast<uint16_t>(m_pal_ram[1]) << 8);
    uint32_t backdrop = bgr555_to_argb8888(backdrop_bgr);
    for (int x = 0; x < 240; ++x) {
        m_framebuffer[ly * 240 + x] = backdrop;
    }

    if (mode == 3) {
        // Mode 3: 240x160 16-bit BGR555 Bitmap
        if (dispcnt & (1 << 10)) {
            for (int x = 0; x < 240; ++x) {
                uint32_t idx = (ly * 240 + x) * 2;
                uint16_t bgr = m_vram[idx] | (static_cast<uint16_t>(m_vram[idx + 1]) << 8);
                m_framebuffer[ly * 240 + x] = bgr555_to_argb8888(bgr);
            }
        }
    } else if (mode == 4) {
        // Mode 4: 240x160 8-bit Paletted Bitmap
        if (dispcnt & (1 << 10)) {
            uint32_t base = (dispcnt & (1 << 4)) ? 0xA000 : 0x0000;
            for (int x = 0; x < 240; ++x) {
                uint8_t pal_idx = m_vram[base + ly * 240 + x];
                if (pal_idx != 0) {
                    uint16_t bgr = m_pal_ram[pal_idx * 2] | (static_cast<uint16_t>(m_pal_ram[pal_idx * 2 + 1]) << 8);
                    m_framebuffer[ly * 240 + x] = bgr555_to_argb8888(bgr);
                }
            }
        }
    } else if (mode <= 2) {
        // Mode 0/1/2: Render Tiled Backgrounds (Priority 3 down to 0)
        for (int prio = 3; prio >= 0; --prio) {
            for (int bg = 3; bg >= 0; --bg) {
                if (!(dispcnt & (1 << (8 + bg)))) continue;
                uint16_t bgcnt = io_read16(0x008 + bg * 2);
                if ((bgcnt & 3) != prio) continue;

                uint32_t char_base   = ((bgcnt >> 2) & 0x03) * 0x4000;
                bool is_8bpp         = (bgcnt >> 7) & 1;
                uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x0800;
                uint8_t screen_size  = (bgcnt >> 14) & 0x03;

                uint16_t hofs = io_read16(0x010 + bg * 4) & 0x1FF;
                uint16_t vofs = io_read16(0x012 + bg * 4) & 0x1FF;

                int y = ly + vofs;
                int tile_y = (y / 8);
                int fine_y = y & 7;

                for (int x = 0; x < 240; ++x) {
                    int sx = x + hofs;
                    int tile_x = (sx / 8);
                    int fine_x = sx & 7;

                    int block = 0;
                    if (screen_size == 1)      block = (tile_x >> 5) & 1;
                    else if (screen_size == 2) block = (tile_y >> 5) & 1;
                    else if (screen_size == 3) block = ((tile_x >> 5) & 1) + (((tile_y >> 5) & 1) * 2);

                    uint32_t map_addr = (screen_base + block * 0x800 + ((tile_y & 31) * 32 + (tile_x & 31)) * 2) & 0xFFFF;
                    uint16_t entry = m_vram[map_addr] | (static_cast<uint16_t>(m_vram[map_addr + 1]) << 8);

                    uint16_t tile_id = entry & 0x3FF;
                    bool hflip       = (entry >> 10) & 1;
                    bool vflip       = (entry >> 11) & 1;
                    uint8_t pal_bank = (entry >> 12) & 0x0F;

                    int px = hflip ? (7 - fine_x) : fine_x;
                    int py = vflip ? (7 - fine_y) : fine_y;

                    uint8_t color_idx = 0;
                    if (is_8bpp) {
                        uint32_t t_addr = (char_base + tile_id * 64 + py * 8 + px) & 0xFFFF;
                        color_idx = m_vram[t_addr];
                    } else {
                        uint32_t t_addr = (char_base + tile_id * 32 + py * 4 + (px >> 1)) & 0xFFFF;
                        uint8_t byte = m_vram[t_addr];
                        uint8_t nibble = (px & 1) ? (byte >> 4) : (byte & 0x0F);
                        if (nibble != 0) color_idx = (pal_bank * 16) + nibble;
                    }

                    if (color_idx != 0) {
                        uint16_t bgr = m_pal_ram[color_idx * 2] | (static_cast<uint16_t>(m_pal_ram[color_idx * 2 + 1]) << 8);
                        m_framebuffer[ly * 240 + x] = bgr555_to_argb8888(bgr);
                    }
                }
            }
        }
    }

    // Render OAM Sprites (OBJ) if enabled (Bit 12 of DISPCNT)
    if (dispcnt & (1 << 12)) {
        render_sprites(ly, dispcnt);
    }
}

void GBACore::render_sprites(int ly, uint16_t dispcnt) {
    static const int OBJ_WIDTHS[4][4]  = {{8,16,32,64}, {16,32,32,64}, {8,8,16,32}, {8,8,8,8}};
    static const int OBJ_HEIGHTS[4][4] = {{8,16,32,64}, {8,8,16,32},   {16,32,32,64},{8,8,8,8}};

    bool mapping_1d = (dispcnt >> 6) & 1;

    for (int i = 127; i >= 0; --i) {
        uint32_t base = i * 8;
        uint16_t attr0 = m_oam[base]     | (static_cast<uint16_t>(m_oam[base + 1]) << 8);
        uint16_t attr1 = m_oam[base + 2] | (static_cast<uint16_t>(m_oam[base + 3]) << 8);
        uint16_t attr2 = m_oam[base + 4] | (static_cast<uint16_t>(m_oam[base + 5]) << 8);

        // Skip disabled or affine-double sprites for fast scanline rendering
        if (((attr0 >> 8) & 3) == 2) continue;

        uint8_t shape = (attr0 >> 14) & 3;
        uint8_t size  = (attr1 >> 14) & 3;
        int w = OBJ_WIDTHS[shape][size];
        int h = OBJ_HEIGHTS[shape][size];

        int sy = attr0 & 0xFF;
        if (sy >= 160) sy -= 256;
        if (ly < sy || ly >= sy + h) continue;

        int sx = attr1 & 0x1FF;
        if (sx >= 240) sx -= 512;

        bool is_8bpp     = (attr0 >> 13) & 1;
        bool hflip       = (attr1 >> 12) & 1;
        bool vflip       = (attr1 >> 13) & 1;
        uint16_t tile_id = attr2 & 0x3FF;
        uint8_t pal_bank = (attr2 >> 12) & 0x0F;

        int py = vflip ? (h - 1 - (ly - sy)) : (ly - sy);
        int tile_row = py / 8;
        int fine_y   = py & 7;

        for (int x = 0; x < w; ++x) {
            int screen_x = sx + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int px = hflip ? (w - 1 - x) : x;
            int tile_col = px / 8;
            int fine_x   = px & 7;

            uint32_t cur_tile = 0;
            if (mapping_1d) {
                int tiles_per_row = w / 8;
                cur_tile = tile_id + (tile_row * tiles_per_row + tile_col) * (is_8bpp ? 2 : 1);
            } else {
                cur_tile = (tile_id + tile_row * 32 + tile_col * (is_8bpp ? 2 : 1)) & 0x3FF;
            }

            uint8_t color_idx = 0;
            if (is_8bpp) {
                uint32_t addr = 0x10000 + (cur_tile * 32) + fine_y * 8 + fine_x;
                if (addr < m_vram.size()) color_idx = m_vram[addr];
            } else {
                uint32_t addr = 0x10000 + (cur_tile * 32) + fine_y * 4 + (fine_x >> 1);
                if (addr < m_vram.size()) {
                    uint8_t b = m_vram[addr];
                    uint8_t nib = (fine_x & 1) ? (b >> 4) : (b & 0x0F);
                    if (nib != 0) color_idx = pal_bank * 16 + nib;
                }
            }

            if (color_idx != 0) {
                uint32_t pal_addr = 0x200 + color_idx * 2;
                uint16_t bgr = m_pal_ram[pal_addr] | (static_cast<uint16_t>(m_pal_ram[pal_addr + 1]) << 8);
                m_framebuffer[ly * 240 + screen_x] = bgr555_to_argb8888(bgr);
            }
        }
    }
}

void GBACore::run_frame() {
    m_audio_buffer.clear();

    for (int ly = 0; ly < 228; ++ly) {
        io_write16(0x006, static_cast<uint16_t>(ly)); // VCOUNT

        uint16_t dispstat = io_read16(0x004);
        uint8_t lyc = (dispstat >> 8) & 0xFF;

        if (ly == lyc) {
            dispstat |= (1 << 2);
            if (dispstat & (1 << 5)) request_irq(1 << 2); // VCounter IRQ
        } else {
            dispstat &= ~(1U << 2);
        }

        if (ly < 160) {
            dispstat &= ~0x03U;
            io_write16(0x004, dispstat);

            for (int i = 0; i < 300; ++i) {
                step_instruction();
                if (m_halted) break;
            }

            render_scanline(ly);

            // HBlank
            dispstat |= (1 << 1);
            io_write16(0x004, dispstat);
            if (dispstat & (1 << 4)) request_irq(1 << 1); // HBlank IRQ
        } else {
            if (ly == 160) {
                dispstat |= (1 << 0);
                io_write16(0x004, dispstat);
                if (dispstat & (1 << 3)) request_irq(1 << 0); // VBlank IRQ
            }
            if (ly == 227) {
                dispstat &= ~(1U << 0);
                io_write16(0x004, dispstat);
            }

            for (int i = 0; i < 300; ++i) {
                step_instruction();
                if (m_halted) break;
            }
        }
    }
}

// ============================================================================
// INPUT & DEBUGGER HOOKS
// ============================================================================

void GBACore::set_input(uint32_t input_state) {
    uint16_t keys = 0x03FF;
    if (input_state & (1 << 0)) keys &= ~(1 << 0); // A
    if (input_state & (1 << 1)) keys &= ~(1 << 1); // B
    if (input_state & (1 << 2)) keys &= ~(1 << 2); // Select
    if (input_state & (1 << 3)) keys &= ~(1 << 3); // Start
    if (input_state & (1 << 4)) keys &= ~(1 << 4); // Right
    if (input_state & (1 << 5)) keys &= ~(1 << 5); // Left
    if (input_state & (1 << 6)) keys &= ~(1 << 6); // Up
    if (input_state & (1 << 7)) keys &= ~(1 << 7); // Down
    m_keyinput = keys;
}

CPUState GBACore::get_cpu_state() const {
    CPUState state{};
    state.A  = static_cast<uint8_t>(m_regs[0] & 0xFF);
    state.F  = static_cast<uint8_t>((m_cpsr >> 24) & 0xFF);
    state.B  = static_cast<uint8_t>(m_regs[1] & 0xFF);
    state.C  = static_cast<uint8_t>(m_regs[2] & 0xFF);
    state.D  = static_cast<uint8_t>(m_regs[3] & 0xFF);
    state.E  = static_cast<uint8_t>(m_regs[4] & 0xFF);
    state.H  = static_cast<uint8_t>((m_regs[15] >> 24) & 0xFF);
    state.L  = static_cast<uint8_t>((m_regs[15] >> 16) & 0xFF);
    state.PC = static_cast<uint16_t>(m_regs[15] & 0xFFFF);
    state.SP = static_cast<uint16_t>(m_regs[13] & 0xFFFF);
    return state;
}

uint8_t GBACore::debug_read_memory(uint16_t address) {
    return read8(0x02000000 + address);
}

void GBACore::debug_write_memory(uint16_t address, uint8_t value) {
    write8(0x02000000 + address, value);
}

void GBACore::debug_update_tile_buffer() {
    const uint32_t palette[4] = { 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 };
    for (int tile = 0; tile < 384; ++tile) {
        int tile_x = (tile % 16) * 8;
        int tile_y = (tile / 16) * 8;
        uint32_t base = tile * 32;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint8_t byte = m_vram[base + row * 4 + (col / 2)];
                uint8_t nibble = (col & 1) ? (byte >> 4) : (byte & 0x0F);
                m_debug_tile_buffer[(tile_y + row) * 128 + (tile_x + col)] = palette[nibble & 0x03];
            }
        }
    }
}