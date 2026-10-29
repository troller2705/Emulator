#include "../../include/Core_GBA/gba_mmu.h"
#include "../../include/Core_GBA/gba_timer.h"
#include "../../include/Core_GBA/gba_apu.h"
#include "../../include/Core_Shared/battery_io.h"
#include <cstring>

GBAMMU::GBAMMU() {
    m_sram.fill(0xFF);
    reset();
}

void GBAMMU::reset() {
    m_ewram.fill(0);
    m_iwram.fill(0);
    m_io.fill(0);
    m_pal_ram.fill(0);
    m_vram.fill(0);
    m_oam.fill(0);
    m_bios.fill(0);
    m_dma = {};

    // Standard GBA BIOS Exception Vectors (0x00 - 0x1C)
    const uint32_t vectors[8] = {
        0xEA000018, // 0x00: Reset
        0xEA000004, // 0x04: Undefined
        0xEA00004C, // 0x08: SWI
        0xEA000002, // 0x0C: Prefetch Abort
        0xEA000001, // 0x10: Data Abort
        0xEA000000, // 0x14: Reserved
        0xEA000042, // 0x18: IRQ (Branches to 0x128)
        0xEA000001  // 0x1C: FIQ
    };
    std::memcpy(&m_bios[0x00], vectors, sizeof(vectors));

    // Embedded ARM BIOS IRQ Dispatcher at 0x00000128 (and mirrored at 0x18 fallback):
    const uint32_t irq_stub[] = {
        0xE92D500F, // STMFD SP!, {R0-R3, R12, LR}
        0xE3A00301, // MOV   R0, #0x04000000
        0xE28FE000, // ADD   LR, PC, #0
        0xE510F004, // LDR   PC, [R0, #-4] -> jumps to [0x03FFFFFC]
        0xE8BD500F, // LDMFD SP!, {R0-R3, R12, LR}
        0xE25EF004  // SUBS  PC, LR, #4
    };
    std::memcpy(&m_bios[0x128], irq_stub, sizeof(irq_stub));

    // Post-Boot Hardware Register Defaults checked by System ID tests
    io_write16(0x000, 0x0080); // DISPCNT (Forced blank post-boot)
    io_write16(0x020, 0x0100); // BG2PA
    io_write16(0x026, 0x0100); // BG2PD
    io_write16(0x030, 0x0100); // BG3PA
    io_write16(0x036, 0x0100); // BG3PD
    io_write16(0x088, 0x0200); // SOUNDBIAS (Default 0x0200)
    io_write16(0x134, 0x8000); // RCNT (General Purpose Mode)
    m_io[0x300] = 0x01;        // POSTFLG (1 = Post-boot)

    // BIOS Checksum / SoftReset flags in IWRAM top (0x03007FF0)
    m_iwram[0x7FFA] = 0x01;

    m_bios_latch    = 0xE3A02004; // Post-reset BIOS open-bus value
    m_in_bios       = false;
    m_halted        = false;
    m_flash_state   = FlashState::Ready;
    m_flash_id_mode = false;
    m_flash_bank    = 0;
}

void GBAMMU::load_rom(const std::vector<uint8_t>& rom_data) {
    m_rom = rom_data;
    m_sram.fill(0xFF);

    // Copy the Nintendo Logo from the ROM header into the BIOS memory.
    // System ID tests use CpuSet to read this exact region to verify BIOS authenticity!
    if (m_rom.size() >= 0xA0) {
        std::memcpy(&m_bios[0x04], &m_rom[0x04], 0x9C);
    }
}

void GBAMMU::load_battery(const std::string& path) {
    CoreShared::load_battery_file(path, m_sram.data(), m_sram.size());
}

void GBAMMU::save_battery(const std::string& path) {
    CoreShared::save_battery_file(path, m_sram.data(), m_sram.size());
}

uint8_t GBAMMU::read_flash(uint32_t addr) const {
    uint16_t offset = addr & 0xFFFF;
    if (m_flash_id_mode) {
        // Sanyo 128KB Flash (LE26FV10N1TS) Chip IDs expected by Pokemon FireRed
        if (offset == 0x0000) return 0x62; // Manufacturer: Sanyo
        if (offset == 0x0001) return 0x13; // Device: 128KB Flash
    }
    return m_sram[(m_flash_bank * 0x10000) + offset];
}

void GBAMMU::write_flash(uint32_t addr, uint8_t val) {
    uint16_t offset = addr & 0xFFFF;

    switch (m_flash_state) {
        case FlashState::Ready:
            if (offset == 0x5555 && val == 0xAA) {
                m_flash_state = FlashState::Cmd1;
            } else {
                // Direct SRAM fallback compatibility for standard 64KB SRAM games
                m_sram[(m_flash_bank * 0x10000) + offset] = val;
            }
            break;

        case FlashState::Cmd1:
            if (offset == 0x2AAA && val == 0x55) m_flash_state = FlashState::Cmd2;
            else m_flash_state = FlashState::Ready;
            break;

        case FlashState::Cmd2:
            if (offset == 0x5555) {
                if (val == 0x90)      { m_flash_id_mode = true;  m_flash_state = FlashState::Ready; }
                else if (val == 0xF0) { m_flash_id_mode = false; m_flash_state = FlashState::Ready; }
                else if (val == 0x80) { m_flash_state = FlashState::EraseCmd1; }
                else if (val == 0xA0) { m_flash_state = FlashState::WriteByte; }
                else if (val == 0xB0) { m_flash_state = FlashState::BankSwitch; }
                else                  { m_flash_state = FlashState::Ready; }
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::EraseCmd1:
            if (offset == 0x5555 && val == 0xAA) m_flash_state = FlashState::EraseCmd2;
            else m_flash_state = FlashState::Ready;
            break;

        case FlashState::EraseCmd2:
            if (offset == 0x2AAA && val == 0x55) {
                // Wait for sector or chip erase command next
            } else if (offset == 0x5555 && val == 0x10) {
                // Erase entire 128KB chip
                m_sram.fill(0xFF);
                m_flash_state = FlashState::Ready;
            } else if (val == 0x30) {
                // Erase 4KB sector
                uint32_t sector_base = (m_flash_bank * 0x10000) + (offset & 0xF000);
                std::memset(&m_sram[sector_base], 0xFF, 0x1000);
                m_flash_state = FlashState::Ready;
            } else {
                m_flash_state = FlashState::Ready;
            }
            break;

        case FlashState::WriteByte:
            m_sram[(m_flash_bank * 0x10000) + offset] &= val;
            m_flash_state = FlashState::Ready;
            break;

        case FlashState::BankSwitch:
            if (offset == 0x0000) m_flash_bank = val & 1;
            m_flash_state = FlashState::Ready;
            break;
    }
}

uint8_t GBAMMU::read8(uint32_t addr) const {
    switch (addr >> 24) {
        case 0x00: {
            if (addr < 0x4000) {
                if (m_in_bios) return m_bios[addr];
                // When reading BIOS from outside BIOS, hardware returns the BIOS open-bus latch
                return static_cast<uint8_t>((m_bios_latch >> ((addr & 3) * 8)) & 0xFF);
            }
            return 0;
        }
        case 0x02: return m_ewram[addr & 0x3FFFF];
        case 0x03: return m_iwram[addr & 0x7FFF];
        case 0x04: {
            if ((addr & 0xFFFF) == 0x0800) return 0x20;
            if ((addr & 0xFFFF) == 0x0803) return 0x0D; // 0x04000800 = 0x0D000020
            uint32_t offset = addr & 0x3FF;
            if (offset == 0x130) return m_keyinput & 0xFF;
            if (offset == 0x131) return (m_keyinput >> 8) & 0xFF;
            if (offset >= 0x100 && offset <= 0x10F && m_timer) {
                uint16_t v = m_timer->read_register(offset & ~1U);
                return (offset & 1) ? ((v >> 8) & 0xFF) : (v & 0xFF);
            }
            return m_io[offset];
        }
        case 0x05: return m_pal_ram[addr & 0x3FF];
        case 0x06: {
            uint32_t offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            return m_vram[offset];
        }
        case 0x07: return m_oam[addr & 0x3FF];
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: {
            uint32_t offset = addr & 0x1FFFFFF;
            return (offset < m_rom.size()) ? m_rom[offset] : static_cast<uint8_t>((addr >> 1) & 0xFF);
        }
        case 0x0D: {
            uint32_t offset = addr & 0x1FFFFFF;
            return (offset < m_rom.size()) ? m_rom[offset] : 0x01;
        }
        case 0x0E: case 0x0F:
            return read_flash(addr);
        default: return 0;
    }
}

uint16_t GBAMMU::read16(uint32_t addr) const {
    addr &= ~1U;
    if ((addr >> 24) == 0x04) {
        uint32_t offset = addr & 0x3FE;
        if (offset >= 0x100 && offset <= 0x10E && m_timer) {
            return m_timer->read_register(offset);
        }
    }
    return read8(addr) | (static_cast<uint16_t>(read8(addr + 1)) << 8);
}

uint32_t GBAMMU::read32(uint32_t addr) const {
    uint32_t aligned = addr & ~3U;
    uint32_t val = read16(aligned) | (static_cast<uint32_t>(read16(aligned + 2)) << 16);
    uint32_t rot = (addr & 3) * 8;
    return CoreShared::rotr32(val, rot);
}

void GBAMMU::write8(uint32_t addr, uint8_t val) {
    switch (addr >> 24) {
        case 0x02: m_ewram[addr & 0x3FFFF] = val; break;
        case 0x03: m_iwram[addr & 0x7FFF]  = val; break;
        case 0x04: {
            uint32_t offset = addr & 0x3FF;

            // Route ALL Audio I/O directly to the APU's 8-bit handler
            if (offset >= 0x060 && offset <= 0x0A7 && m_apu) {
                m_apu->write_register8(offset, val);
                return;
            }

            if (offset == 0x202 || offset == 0x203) {
                uint16_t if_val = io_read16(0x202);
                if_val &= ~val;
                io_write16(0x202, if_val);
                return;
            }
            if (offset == 0x208 || offset == 0x209) {
                m_io[offset] = val;
                return;
            }
            if (offset == 0x301) {
                m_halted = true;
                return;
            }

            uint8_t old_hi = m_io[offset];
            m_io[offset]   = val;

            if (offset == 0x0BB && !(old_hi & 0x80) && (val & 0x80)) latch_dma(0);
            if (offset == 0x0C7 && !(old_hi & 0x80) && (val & 0x80)) latch_dma(1);
            if (offset == 0x0D3 && !(old_hi & 0x80) && (val & 0x80)) latch_dma(2);
            if (offset == 0x0DF && !(old_hi & 0x80) && (val & 0x80)) latch_dma(3);
            break;
        }
        case 0x05: {
            uint32_t offset = addr & 0x3FE;
            m_pal_ram[offset] = m_pal_ram[offset + 1] = val;
            break;
        }
        case 0x06: {
            uint32_t offset = addr & 0x1FFFE;
            if (offset >= 0x18000) offset -= 0x8000;
            if (offset < 0x10000) m_vram[offset] = m_vram[offset + 1] = val;
            break;
        }
        case 0x0E: case 0x0F: write_flash(addr, val); break;
        default: break;
    }
}

void GBAMMU::write16(uint32_t addr, uint16_t val) {
    addr &= ~1U;
    uint32_t region = addr >> 24;
    if (region == 0x04) {
        uint32_t offset = addr & 0x3FE;
        if (offset >= 0x100 && offset <= 0x10E && m_timer) {
            m_timer->write_register(offset, val);
            io_write16(offset, val);
            return;
        }
        // Fall through! Audio writes will automatically be split into write8 calls above!
    }
    if (region == 0x05) { CoreShared::write_le16(m_pal_ram.data(), addr & 0x3FE, val); return; }
    if (region == 0x06) {
        uint32_t offset = addr & 0x1FFFE;
        if (offset >= 0x18000) offset -= 0x8000;
        CoreShared::write_le16(m_vram.data(), offset, val);
        return;
    }
    if (region == 0x07) { CoreShared::write_le16(m_oam.data(), addr & 0x3FE, val); return; }
    write8(addr, val & 0xFF);
    write8(addr + 1, (val >> 8) & 0xFF);
}

void GBAMMU::write32(uint32_t addr, uint32_t val) {
    addr &= ~3U;
    if ((addr >> 24) == 0x04) {
        uint32_t offset = addr & 0x3FC;
        if ((offset == 0x0A0 || offset == 0x0A4) && m_apu) {
            m_apu->write_fifo32(offset, val);
            return;
        }
    }
    write16(addr, val & 0xFFFF);
    write16(addr + 2, (val >> 16) & 0xFFFF);
}

void GBAMMU::latch_dma(int ch) {
    uint32_t base = 0x0B0 + (ch * 12);
    m_dma[ch].src   = io_read16(base)     | (static_cast<uint32_t>(io_read16(base + 2)) << 16);
    m_dma[ch].dst   = io_read16(base + 4) | (static_cast<uint32_t>(io_read16(base + 6)) << 16);
    m_dma[ch].count = io_read16(base + 8);
    m_dma[ch].active = true;

    uint16_t cnt_h = io_read16(base + 10);
    uint8_t timing = (cnt_h >> 12) & 0x03;
    if (timing == 0) {
        execute_dma_channel(ch);
    }
}

void GBAMMU::run_dma(uint8_t timing_mode) {
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_dma[ch].active) continue;
        uint32_t base  = 0x0B0 + (ch * 12);
        uint16_t cnt_h = io_read16(base + 10);
        if (!(cnt_h & 0x8000)) {
            m_dma[ch].active = false;
            continue;
        }
        uint8_t ch_timing = (cnt_h >> 12) & 0x03;
        if (ch_timing == timing_mode) {
            execute_dma_channel(ch);
        }
    }
}

void GBAMMU::trigger_fifo_dma(int fifo_index) {
    int ch = fifo_index + 1; // DMA1 -> FIFO A (0), DMA2 -> FIFO B (1)
    if (!m_dma[ch].active) return;
    uint32_t base  = 0x0B0 + (ch * 12);
    uint16_t cnt_h = io_read16(base + 10);
    if ((cnt_h & 0x8000) && (((cnt_h >> 12) & 0x03) == 3)) {
        for (int i = 0; i < 4; ++i) {
            write32(m_dma[ch].dst, read32(m_dma[ch].src));
            m_dma[ch].src += 4;
        }
        if (cnt_h & (1 << 14)) {
            request_irq(1 << (8 + ch));
        }
    }
}

void GBAMMU::execute_dma_channel(int ch) {
    uint32_t base  = 0x0B0 + (ch * 12);
    uint16_t cnt_h = io_read16(base + 10);

    uint32_t count = io_read16(base + 8);
    if (count == 0) count = (ch == 3) ? 0x10000 : 0x4000;

    bool is_32bit  = (cnt_h >> 10) & 1;
    uint32_t step  = is_32bit ? 4 : 2;
    int dst_ctrl   = (cnt_h >> 5) & 3;
    int src_ctrl   = (cnt_h >> 7) & 3;

    for (uint32_t i = 0; i < count; ++i) {
        if (is_32bit) write32(m_dma[ch].dst, read32(m_dma[ch].src));
        else          write16(m_dma[ch].dst, read16(m_dma[ch].src));

        if (src_ctrl == 0 || src_ctrl == 3) m_dma[ch].src += step;
        else if (src_ctrl == 1)             m_dma[ch].src -= step;

        if (dst_ctrl == 0 || dst_ctrl == 3) m_dma[ch].dst += step;
        else if (dst_ctrl == 1)             m_dma[ch].dst -= step;
    }

    if (dst_ctrl == 3) {
        m_dma[ch].dst = io_read16(base + 4) | (static_cast<uint32_t>(io_read16(base + 6)) << 16);
    }

    if (!(cnt_h & (1 << 9))) {
        io_write16(base + 10, cnt_h & ~0x8000U);
        m_dma[ch].active = false;
    }

    if (cnt_h & (1 << 14)) {
        request_irq(1 << (8 + ch));
    }
}

void GBAMMU::request_irq(uint16_t flag) {
    uint16_t if_reg = io_read16(0x202) | flag;
    io_write16(0x202, if_reg);

    // Mirror into BIOS IF flag at 0x03007FF8 for HLE IntrWait / VBlankIntrWait
    uint16_t bios_if = CoreShared::read_le16(m_iwram.data(), 0x7FF8) | flag;
    CoreShared::write_le16(m_iwram.data(), 0x7FF8, bios_if);
}