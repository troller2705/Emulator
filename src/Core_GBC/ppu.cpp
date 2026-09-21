#include "ppu.h"

PPU::PPU()
    : m_lcdc(0x91), // Typical post-boot default value
      m_stat(0x85),
      m_scy(0),
      m_scx(0),
      m_ly(0),
      m_lyc(0),
      m_scanline_counter(456)
{
}

uint8_t PPU::read_register(uint16_t address) const {
    switch (address) {
        case 0xFF40: return m_lcdc;
        case 0xFF41: return m_stat;
        case 0xFF42: return m_scy;
        case 0xFF43: return m_scx;
        case 0xFF44: return m_ly;
        case 0xFF45: return m_lyc;
        default: return 0xFF;
    }
}

void PPU::write_register(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF40: {
            bool was_enabled = is_lcd_enabled();
            m_lcdc = value;

            // If LCD was just disabled (Bit 7 transition 1 -> 0)
            if (was_enabled && !is_lcd_enabled()) {
                m_ly = 0;
                m_scanline_counter = 456;
                change_mode(0); // Force Mode 0 (H-Blank) or safe state
            }
            break;
        }
        case 0xFF41:
            // Bits 0-2 are read-only, bits 3-6 control interrupts
            m_stat = (m_stat & 0x07) | (value & 0xF8);
            break;
        case 0xFF42: m_scy = value; break;
        case 0xFF43: m_scx = value; break;
        case 0xFF44:
            // LY is read-only; writing to it resets it on real hardware
            m_ly = 0;
            break;
        case 0xFF45: m_lyc = value; break;
        default: break;
    }
}

void PPU::change_mode(uint8_t mode) {
    // Clear lower 2 bits of STAT and set new mode
    m_stat = (m_stat & ~0x03) | (mode & 0x03);
}

void PPU::step(int cycles) {
    if (!is_lcd_enabled()) {
        return; // PPU is disabled, skip rendering logic entirely
    }

    m_scanline_counter -= cycles;

    if (m_scanline_counter <= 0) {
        // Each scanline takes 456 T-states (cycles)
        m_scanline_counter += 456;
        m_ly++;

        // VBlank starts at scanline 144 up to 153
        if (m_ly == 144) {
            change_mode(1); // Mode 1: VBlank
        } else if (m_ly > 153) {
            m_ly = 0; // Restart frame
        }

        // Handle active scanlines (0-143) modes 2 and 3
        if (m_ly < 144) {
            change_mode(2); // Mode 2: OAM Scan (simplified starting state)
        }

        // Check LY == LYC coincidence flag
        if (m_ly == m_lyc) {
            m_stat |= 0x04; // Set coincidence flag (Bit 2)
        } else {
            m_stat &= ~0x04;
        }
    }
}