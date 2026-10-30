#include "Core_SNES/snes_ppu.h"
#include "Core_Shared/color_utils.h"
#include <algorithm>

SNES_PPU::SNES_PPU() {
    reset();
}

void SNES_PPU::reset() {
    m_vram.fill(0);
    m_cgram.fill(0);
    m_oam.fill(0);
    m_framebuffer.fill(0xFF000000);
    m_debug_tile_buffer.fill(0xFF000000);

    m_cycle_counter = 0;
    m_scanline = 0;
    m_in_vblank = false;
    m_forced_blank = true;
    m_brightness = 0;
    m_bg_mode = 0;
    m_bg3_priority = false;
    m_bg_tile_size = 0;
    m_main_screen_mask = 0;
    m_sub_screen_mask = 0;

    m_cgwsel = 0;
    m_cgadsub = 0;
    m_fixed_r = m_fixed_g = m_fixed_b = 0;

    m_vmain = 0;
    m_vram_addr = 0;
    m_vram_prefetch = 0;

    m_cgram_addr = 0;
    m_cgram_latch = 0;
    m_cgram_flipflop = false;

    m_oam_addr = 0;
    m_oam_reload = 0;
    m_oam_latch = 0;
    m_oam_flipflop = false;

    for (int i = 0; i < 4; ++i) {
        m_bg_sc[i] = 0;
        m_bg_hofs[i] = 0;
        m_bg_vofs[i] = 0;
    }
    m_bg_nba[0] = m_bg_nba[1] = 0;
    m_bg_scroll_prev = 0;
    m_obsel = 0;
}

uint16_t SNES_PPU::getVramAddressIncrement() const {
    switch (m_vmain & 0x03) {
        case 0: return 1;
        case 1: return 32;
        case 2:
        case 3: return 128;
        default: return 1;
    }
}

bool SNES_PPU::advance_cycles(int cpu_cycles) {
    m_cycle_counter += cpu_cycles;
    bool entered_vblank = false;

    while (m_cycle_counter >= 227) {
        m_cycle_counter -= 227;
        if (stepScanline()) {
            entered_vblank = true;
        }
    }
    return entered_vblank;
}

bool SNES_PPU::stepScanline() {
    if (m_scanline >= 1 && m_scanline <= SCREEN_HEIGHT) {
        renderScanline(m_scanline - 1);
    }

    m_scanline++;
    bool entered_vblank = false;

    if (m_scanline == 225) {
        m_in_vblank = true;
        entered_vblank = true;
        if (!m_forced_blank) {
            m_oam_addr = m_oam_reload;
            m_oam_flipflop = false;
        }
    } else if (m_scanline >= 262) {
        m_scanline = 0;
        m_in_vblank = false;
    }

    return entered_vblank;
}

uint8_t SNES_PPU::readRegister(uint16_t addr) {
    switch (addr) {
        case 0x2138: { // RDOAM
            uint8_t data = m_oam[m_oam_addr % 0x220];
            m_oam_addr = (m_oam_addr + 1) & 0x03FF;
            return data;
        }
        case 0x2139: { // RDVRAML
            uint8_t val = m_vram_prefetch & 0xFF;
            if ((m_vmain & 0x80) == 0) {
                uint32_t byte_addr = (m_vram_addr << 1) & 0xFFFE;
                m_vram_prefetch = m_vram[byte_addr] | (m_vram[byte_addr + 1] << 8);
                m_vram_addr += getVramAddressIncrement();
            }
            return val;
        }
        case 0x213A: { // RDVRAMH
            uint8_t val = (m_vram_prefetch >> 8) & 0xFF;
            if ((m_vmain & 0x80) != 0) {
                uint32_t byte_addr = (m_vram_addr << 1) & 0xFFFE;
                m_vram_prefetch = m_vram[byte_addr] | (m_vram[byte_addr + 1] << 8);
                m_vram_addr += getVramAddressIncrement();
            }
            return val;
        }
        case 0x213B: { // RDCGRAM
            uint8_t val = 0;
            if (!m_cgram_flipflop) {
                val = m_cgram[m_cgram_addr * 2];
                m_cgram_flipflop = true;
            } else {
                val = m_cgram[m_cgram_addr * 2 + 1] & 0x7F;
                m_cgram_flipflop = false;
                m_cgram_addr++;
            }
            return val;
        }
        case 0x213E: return 0x01;
        case 0x213F: return 0x01;
        default:     return 0x00;
    }
}

void SNES_PPU::writeRegister(uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0x2100: // INIDISP
            m_forced_blank = (val & 0x80) != 0;
            m_brightness = val & 0x0F;
            break;

        case 0x2101: // OBSEL
            m_obsel = val;
            break;

        case 0x2102: // OAMADDL
            m_oam_reload = (m_oam_reload & 0x0200) | (static_cast<uint16_t>(val) << 1);
            m_oam_addr = m_oam_reload;
            m_oam_flipflop = false;
            break;

        case 0x2103: // OAMADDH
            m_oam_reload = (m_oam_reload & 0x01FE) | ((val & 0x01) << 9);
            m_oam_addr = m_oam_reload;
            m_oam_flipflop = false;
            break;

        case 0x2104: // OAMDATA
            if (m_oam_addr < 0x200) {
                if (!m_oam_flipflop) {
                    m_oam_latch = val;
                } else {
                    m_oam[m_oam_addr - 1] = m_oam_latch;
                    m_oam[m_oam_addr] = val;
                }
            } else if (m_oam_addr < 0x220) {
                m_oam[m_oam_addr] = val;
            }
            m_oam_addr = (m_oam_addr + 1) & 0x03FF;
            m_oam_flipflop = !m_oam_flipflop;
            break;

        case 0x2105: // BGMODE
            m_bg_mode = val & 0x07;
            m_bg3_priority = (val & 0x08) != 0;
            m_bg_tile_size = (val >> 4) & 0x0F;
            break;

        case 0x2107: case 0x2108: case 0x2109: case 0x210A:
            m_bg_sc[addr - 0x2107] = val;
            break;

        case 0x210B:
            m_bg_nba[0] = val;
            break;

        case 0x210C:
            m_bg_nba[1] = val;
            break;

        case 0x210D: case 0x210F: case 0x2111: case 0x2113: {
            int idx = (addr - 0x210D) / 2;
            m_bg_hofs[idx] = ((val << 8) | (m_bg_scroll_prev & ~7) | ((m_bg_hofs[idx] >> 8) & 7)) & 0x03FF;
            m_bg_scroll_prev = val;
            break;
        }
        case 0x210E: case 0x2110: case 0x2112: case 0x2114: {
            int idx = (addr - 0x210E) / 2;
            m_bg_vofs[idx] = ((val << 8) | m_bg_scroll_prev) & 0x03FF;
            m_bg_scroll_prev = val;
            break;
        }

        case 0x2115:
            m_vmain = val;
            break;

        case 0x2116: {
            m_vram_addr = (m_vram_addr & 0xFF00) | val;
            uint32_t byte_addr = (m_vram_addr << 1) & 0xFFFE;
            m_vram_prefetch = m_vram[byte_addr] | (m_vram[byte_addr + 1] << 8);
            break;
        }
        case 0x2117: {
            m_vram_addr = (m_vram_addr & 0x00FF) | (static_cast<uint16_t>(val) << 8);
            uint32_t byte_addr = (m_vram_addr << 1) & 0xFFFE;
            m_vram_prefetch = m_vram[byte_addr] | (m_vram[byte_addr + 1] << 8);
            break;
        }
        case 0x2118: {
            uint32_t byte_addr = (m_vram_addr << 1) & 0xFFFE;
            m_vram[byte_addr] = val;
            if ((m_vmain & 0x80) == 0) {
                m_vram_addr += getVramAddressIncrement();
            }
            break;
        }
        case 0x2119: {
            uint32_t byte_addr = ((m_vram_addr << 1) & 0xFFFE) + 1;
            m_vram[byte_addr] = val;
            if ((m_vmain & 0x80) != 0) {
                m_vram_addr += getVramAddressIncrement();
            }
            break;
        }

        case 0x2121:
            m_cgram_addr = val;
            m_cgram_flipflop = false;
            break;

        case 0x2122:
            if (!m_cgram_flipflop) {
                m_cgram_latch = val;
                m_cgram_flipflop = true;
            } else {
                m_cgram[m_cgram_addr * 2] = m_cgram_latch;
                m_cgram[m_cgram_addr * 2 + 1] = val & 0x7F;
                m_cgram_flipflop = false;
                m_cgram_addr++;
            }
            break;

        case 0x212C:
            m_main_screen_mask = val;
            break;

        case 0x212D:
            m_sub_screen_mask = val;
            break;

        case 0x2130: // CGWSEL
            m_cgwsel = val;
            break;

        case 0x2131: // CGADSUB
            m_cgadsub = val;
            break;

        case 0x2132: { // COLDATA - Fixed Color Data (R/G/B channels)
            uint8_t intensity = val & 0x1F;
            if (val & 0x20) m_fixed_r = intensity;
            if (val & 0x40) m_fixed_g = intensity;
            if (val & 0x80) m_fixed_b = intensity;
            break;
        }
    }
}

uint32_t SNES_PPU::bgr555ToArgb8888(uint16_t bgr555) const {
    uint8_t r5 = bgr555 & 0x1F;
    uint8_t g5 = (bgr555 >> 5) & 0x1F;
    uint8_t b5 = (bgr555 >> 10) & 0x1F;

    uint32_t r = ((r5 << 3) | (r5 >> 2)) * m_brightness / 15;
    uint32_t g = ((g5 << 3) | (g5 >> 2)) * m_brightness / 15;
    uint32_t b = ((b5 << 3) | (b5 >> 2)) * m_brightness / 15;

    return 0xFF000000U | (r << 16) | (g << 8) | b;
}

void SNES_PPU::renderScanline(int line) {
    uint32_t* line_buf = &m_framebuffer[line * SCREEN_WIDTH];

    if (m_forced_blank || m_brightness == 0) {
        std::fill_n(line_buf, SCREEN_WIDTH, 0xFF000000);
        return;
    }

    // Backdrop color: CGRAM[0], combined with Fixed Color ($2132) when Color Math / Subscreen backdrop is used
    uint16_t backdrop_bgr = m_cgram[0] | (m_cgram[1] << 8);
    uint16_t fixed_bgr    = m_fixed_r | (m_fixed_g << 5) | (m_fixed_b << 10);
    if (backdrop_bgr == 0 && fixed_bgr != 0) {
        backdrop_bgr = fixed_bgr;
    }
    uint32_t backdrop_argb = bgr555ToArgb8888(backdrop_bgr);
    std::fill_n(line_buf, SCREEN_WIDTH, backdrop_argb);

    uint8_t layers = m_main_screen_mask | m_sub_screen_mask;

    if (m_bg_mode == 1) {
        // Hardware Mode 1 Priority Order (Back-to-Front):
        if (layers & 0x04) renderBackgroundLayer(line, 2, 2, 0); // 1. BG3 (Priority 0)
        if (layers & 0x10) renderSprites(line, 0);               // 2. OBJ (Priority 0)
        if (!m_bg3_priority && (layers & 0x04)) {
            renderBackgroundLayer(line, 2, 2, 1);                // 3. BG3 (Priority 1 when BG3_PRIO = 0)
        }
        if (layers & 0x10) renderSprites(line, 1);               // 4. OBJ (Priority 1)
        if (layers & 0x02) renderBackgroundLayer(line, 1, 4, 0); // 5. BG2 (Priority 0)
        if (layers & 0x01) renderBackgroundLayer(line, 0, 4, 0); // 6. BG1 (Priority 0)
        if (layers & 0x10) renderSprites(line, 2);               // 7. OBJ (Priority 2)
        if (layers & 0x02) renderBackgroundLayer(line, 1, 4, 1); // 8. BG2 (Priority 1)
        if (layers & 0x01) renderBackgroundLayer(line, 0, 4, 1); // 9. BG1 (Priority 1)
        if (layers & 0x10) renderSprites(line, 3);               // 10. OBJ (Priority 3)
        if (m_bg3_priority && (layers & 0x04)) {
            renderBackgroundLayer(line, 2, 2, 1);                // 11. BG3 (Priority 1 when BG3_PRIO = 1)
        }
    } else if (m_bg_mode == 0) {
        for (int prio = 0; prio <= 1; ++prio) {
            if (layers & 0x08) renderBackgroundLayer(line, 3, 2, prio);
            if (layers & 0x04) renderBackgroundLayer(line, 2, 2, prio);
            if (layers & 0x10) renderSprites(line, prio * 2);
            if (layers & 0x02) renderBackgroundLayer(line, 1, 2, prio);
            if (layers & 0x01) renderBackgroundLayer(line, 0, 2, prio);
            if (layers & 0x10) renderSprites(line, prio * 2 + 1);
        }
    } else {
        for (int prio = 0; prio <= 1; ++prio) {
            if (layers & 0x02) renderBackgroundLayer(line, 1, 4, prio);
            if (layers & 0x10) renderSprites(line, prio * 2);
            if (layers & 0x01) renderBackgroundLayer(line, 0, 4, prio);
            if (layers & 0x10) renderSprites(line, prio * 2 + 1);
        }
    }
}

void SNES_PPU::renderBackgroundLayer(int line, int bg_index, int bpp, int target_priority) {
    uint32_t* line_buf = &m_framebuffer[line * SCREEN_WIDTH];

    uint32_t map_base = (m_bg_sc[bg_index] & 0xFC) << 9;
    uint8_t nba_reg   = (bg_index < 2) ? m_bg_nba[0] : m_bg_nba[1];
    uint8_t shift     = (bg_index & 1) ? 4 : 0;
    uint32_t tile_base = ((nba_reg >> shift) & 0x0F) << 13;

    bool tile_16x16 = (m_bg_tile_size & (1 << bg_index)) != 0;
    int tile_dim    = tile_16x16 ? 16 : 8;

    int scroll_y = (line + m_bg_vofs[bg_index]) & 0x3FF;
    int map_y    = (scroll_y / tile_dim);
    int tile_y   = map_y & 0x1F;
    int sub_y    = (scroll_y & 8) >> 3;
    int fine_y   = scroll_y & 7;

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        int scroll_x = (x + m_bg_hofs[bg_index]) & 0x3FF;
        int map_x    = (scroll_x / tile_dim);
        int tile_x   = map_x & 0x1F;
        int sub_x    = (scroll_x & 8) >> 3;
        int fine_x   = scroll_x & 7;

        uint32_t screen_offset = 0;
        if ((m_bg_sc[bg_index] & 0x01) && (map_x & 0x20)) screen_offset += 0x800;
        if ((m_bg_sc[bg_index] & 0x02) && (map_y & 0x20)) {
            screen_offset += (m_bg_sc[bg_index] & 0x01) ? 0x1000 : 0x800;
        }

        uint32_t entry_addr = (map_base + screen_offset + ((tile_y * 32 + tile_x) * 2)) & 0xFFFE;
        uint16_t entry = m_vram[entry_addr] | (m_vram[entry_addr + 1] << 8);

        int entry_prio = (entry >> 13) & 1;
        if (entry_prio != target_priority) continue;

        uint16_t tile_num = entry & 0x03FF;
        uint8_t  palette  = (entry >> 10) & 0x07;
        bool     flip_x   = (entry & 0x4000) != 0;
        bool     flip_y   = (entry & 0x8000) != 0;

        if (tile_16x16) {
            int eff_sub_x = flip_x ? (1 - sub_x) : sub_x;
            int eff_sub_y = flip_y ? (1 - sub_y) : sub_y;
            tile_num = (tile_num + eff_sub_x + (eff_sub_y * 16)) & 0x03FF;
        }

        int row = flip_y ? (7 - fine_y) : fine_y;
        int bit = flip_x ? fine_x : (7 - fine_x);

        if (bpp == 2) {
            uint32_t chr_addr = (tile_base + tile_num * 16 + row * 2) & 0xFFFE;
            uint8_t bp0 = (m_vram[chr_addr] >> bit) & 1;
            uint8_t bp1 = (m_vram[chr_addr + 1] >> bit) & 1;
            uint8_t color_idx = bp0 | (bp1 << 1);
            if (color_idx != 0) {
                uint8_t cgram_base = (m_bg_mode == 0) ? (bg_index * 32) : 0;
                uint8_t cgram_index = cgram_base + (palette * 4) + color_idx;
                uint16_t bgr = m_cgram[cgram_index * 2] | (m_cgram[cgram_index * 2 + 1] << 8);
                line_buf[x] = bgr555ToArgb8888(bgr);
            }
        } else if (bpp == 4) {
            uint32_t chr_addr = (tile_base + tile_num * 32 + row * 2) & 0xFFFE;
            uint8_t bp0 = (m_vram[chr_addr] >> bit) & 1;
            uint8_t bp1 = (m_vram[chr_addr + 1] >> bit) & 1;
            uint8_t bp2 = (m_vram[(chr_addr + 16) & 0xFFFF] >> bit) & 1;
            uint8_t bp3 = (m_vram[(chr_addr + 17) & 0xFFFF] >> bit) & 1;
            uint8_t color_idx = bp0 | (bp1 << 1) | (bp2 << 2) | (bp3 << 3);
            if (color_idx != 0) {
                uint8_t cgram_index = (palette * 16) + color_idx;
                uint16_t bgr = m_cgram[cgram_index * 2] | (m_cgram[cgram_index * 2 + 1] << 8);
                line_buf[x] = bgr555ToArgb8888(bgr);
            }
        }
    }
}

void SNES_PPU::renderSprites(int line, int target_priority) {
    static const int small_widths[8]  = { 8,  8,  8, 16, 16, 32, 16, 16};
    static const int small_heights[8] = { 8,  8,  8, 16, 16, 32, 32, 32};
    static const int large_widths[8]  = {16, 32, 64, 32, 64, 64, 32, 32};
    static const int large_heights[8] = {16, 32, 64, 32, 64, 64, 64, 32};

    uint32_t* line_buf  = &m_framebuffer[line * SCREEN_WIDTH];
    uint32_t obj_base   = (m_obsel & 0x07) << 14;
    uint32_t obj_select = ((m_obsel >> 3) & 0x03) << 13;
    uint8_t  size_mode  = (m_obsel >> 5) & 0x07;

    for (int i = 127; i >= 0; --i) {
        uint8_t x_low = m_oam[i * 4];
        uint8_t y_pos = (m_oam[i * 4 + 1] + 1) & 0xFF;
        uint8_t tile  = m_oam[i * 4 + 2];
        uint8_t attr  = m_oam[i * 4 + 3];

        int spr_prio = (attr >> 4) & 0x03;
        if (spr_prio != target_priority) continue;

        uint8_t high_bits = (m_oam[0x200 + (i / 4)] >> ((i % 4) * 2)) & 0x03;
        int x_pos = x_low;
        if (high_bits & 1) x_pos -= 256;

        bool is_large = (high_bits & 2) != 0;
        int width     = is_large ? large_widths[size_mode]  : small_widths[size_mode];
        int height    = is_large ? large_heights[size_mode] : small_heights[size_mode];

        int rel_y = (line - y_pos) & 0xFF;
        if (rel_y >= height) continue;

        bool flip_x = (attr & 0x40) != 0;
        bool flip_y = (attr & 0x80) != 0;
        uint8_t palette = (attr >> 1) & 0x07;
        uint32_t table_base = obj_base + ((attr & 0x01) ? obj_select : 0);

        int eff_y    = flip_y ? (height - 1 - rel_y) : rel_y;
        int tile_row = eff_y >> 3;
        int fine_y   = eff_y & 7;

        for (int px = 0; px < width; ++px) {
            int screen_x = x_pos + px;
            if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;

            int eff_x    = flip_x ? (width - 1 - px) : px;
            int tile_col = eff_x >> 3;
            int fine_x   = eff_x & 7;

            uint8_t sub_tile = (((tile >> 4) + tile_row) & 0x0F) << 4 |
                               (((tile & 0x0F) + tile_col) & 0x0F);

            uint32_t chr_addr = (table_base + sub_tile * 32 + fine_y * 2) & 0xFFFE;
            int bit = 7 - fine_x;

            uint8_t bp0 = (m_vram[chr_addr] >> bit) & 1;
            uint8_t bp1 = (m_vram[chr_addr + 1] >> bit) & 1;
            uint8_t bp2 = (m_vram[(chr_addr + 16) & 0xFFFF] >> bit) & 1;
            uint8_t bp3 = (m_vram[(chr_addr + 17) & 0xFFFF] >> bit) & 1;
            uint8_t color_idx = bp0 | (bp1 << 1) | (bp2 << 2) | (bp3 << 3);

            if (color_idx != 0) {
                uint8_t cgram_index = 128 + (palette * 16) + color_idx;
                uint16_t bgr = m_cgram[cgram_index * 2] | (m_cgram[cgram_index * 2 + 1] << 8);
                line_buf[screen_x] = bgr555ToArgb8888(bgr);
            }
        }
    }
}

void SNES_PPU::debug_update_tile_buffer() {
    for (int tile = 0; tile < 384; ++tile) {
        int tile_x = (tile % 16) * 8;
        int tile_y = (tile / 16) * 8;
        uint32_t chr_base = (tile * 32) & 0xFFFF;

        for (int row = 0; row < 8; ++row) {
            uint32_t addr = (chr_base + row * 2) & 0xFFFE;
            uint8_t bp0 = m_vram[addr];
            uint8_t bp1 = m_vram[addr + 1];
            uint8_t bp2 = m_vram[(addr + 16) & 0xFFFF];
            uint8_t bp3 = m_vram[(addr + 17) & 0xFFFF];

            for (int col = 0; col < 8; ++col) {
                int bit = 7 - col;
                uint8_t c_idx = ((bp0 >> bit) & 1) |
                                (((bp1 >> bit) & 1) << 1) |
                                (((bp2 >> bit) & 1) << 2) |
                                (((bp3 >> bit) & 1) << 3);

                uint16_t bgr = m_cgram[c_idx * 2] | (m_cgram[c_idx * 2 + 1] << 8);
                uint32_t color = (bgr != 0 || c_idx == 0)
                    ? CoreShared::bgr555_to_argb8888(bgr)
                    : (0xFF000000U | ((c_idx * 17) << 16) | ((c_idx * 17) << 8) | (c_idx * 17));

                m_debug_tile_buffer[(tile_y + row) * 128 + (tile_x + col)] = color;
            }
        }
    }
}