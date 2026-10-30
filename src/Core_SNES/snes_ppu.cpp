#include "Core_SNES/snes_ppu.h"


SNES_PPU::SNES_PPU() {
    m_vram.resize(32 * 1024);   // 64 KB VRAM (32K words)
    m_oam.resize(544);          // 544 bytes OAM
    m_cgram.resize(512);        // 512 bytes CGRAM
    reset();
}

void SNES_PPU::reset() {
    std::fill(m_vram.begin(), m_vram.end(), 0);
    std::fill(m_oam.begin(), m_oam.end(), 0);
    std::fill(m_cgram.begin(), m_cgram.end(), 0);
    m_scanline = 0;
    m_dot = 0;
    m_vblank = false;
    m_nmi_flag = false;
    m_nmi_trigger = false;
    m_vram_addr = 0;
    m_vram_increment_mode = 1;
    m_vram_increment_value = 1;
    m_cgram_addr = 0;
    m_cgram_toggle = 0;
    m_cgram_latch = 0;
}

// VRAM word read
uint16_t SNES_PPU::read_vram(uint16_t addr) {
    return m_vram[addr & 0x7FFF];
}

// VRAM word write
void SNES_PPU::write_vram(uint16_t addr, uint16_t val) {
    m_vram[addr & 0x7FFF] = val;
}

void SNES_PPU::set_vram_address(uint16_t addr) {
    m_vram_addr = addr & 0x7FFF;
}

void SNES_PPU::set_vram_increment_mode(uint8_t mode) {
    // SNES reg $2115 encoding for increments (+1, +32, +128, etc.)
    static const uint16_t inc_table[] = { 1, 32, 128, 128 };
    m_vram_increment_value = inc_table[mode & 0x03];
}

void SNES_PPU::apply_vram_increment() {
    m_vram_addr = (m_vram_addr + m_vram_increment_value) & 0x7FFF;
}

void SNES_PPU::set_cgram_addr(uint8_t addr) {
    m_cgram_addr = addr;
    m_cgram_toggle = 0; // Reset byte toggle on new address write
}

void SNES_PPU::write_cgram_byte(uint8_t val) {
    if (m_cgram_toggle == 0) {
        // Low byte write
        m_cgram_latch = val;
        m_cgram_toggle = 1;
    } else {
        // High byte write - combine and write the 16-bit color word to CGRAM
        uint16_t color_val = m_cgram_latch | (val << 8);

        // CGRAM has 256 words (512 bytes), address points to color index (0-255)
        size_t byte_offset = m_cgram_addr * 2;
        m_cgram[byte_offset]     = color_val & 0xFF;
        m_cgram[byte_offset + 1] = (color_val >> 8) & 0xFF;

        m_cgram_addr = (m_cgram_addr + 1) & 0xFF; // Auto-increment color index
        m_cgram_toggle = 0;
    }
}

void SNES_PPU::run_frame() {
    // Standard NTSC frame timing: 262 scanlines
    for (m_scanline = 0; m_scanline < 262; ++m_scanline) {
        // VBlank starts at scanline 225
        if (m_scanline == 225) {
            m_vblank = true;
        }
        // VBlank ends and active display resumes at scanline 0
        else if (m_scanline == 0) {
            m_vblank = false;
        }

        // Simulate dots per scanline (340 dots per scanline)
        for (m_dot = 0; m_dot < 340; ++m_dot) {
            // Future dot-level rendering or PPU effects go here
        }
    }
}

void SNES_PPU::advance_cycles(int cpu_cycles) {
    int dots = cpu_cycles * 4;
    m_dot += dots;

    while (m_dot >= 340) {
        m_dot -= 340;
        m_scanline++;

        if (m_scanline == 225) {
            m_vblank = true;       // Enter VBlank
            m_nmi_flag = true;     // Set RDNMI Bit 7
            m_nmi_trigger = true;  // Signal NMI edge
        } else if (m_scanline >= 262) {
            m_scanline = 0;
            m_vblank = false;      // Exit VBlank
            m_nmi_flag = false;
            m_nmi_trigger = false;
        }
    }
}