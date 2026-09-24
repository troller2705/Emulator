#include "ppu.h"
#include "mmu.h"
#include <cstdlib>

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
        case 0xFF47: return m_bgp;
        case 0xFF48: return m_obp0;
        case 0xFF49: return m_obp1;
        case 0xFF4A: return m_wy;
        case 0xFF4B: return m_wx;
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
        case 0xFF47: m_bgp = value; break;
        case 0xFF48: m_obp0 = value; break;
        case 0xFF49: m_obp1 = value; break;
        case 0xFF4A: m_wy = value; break;
        case 0xFF4B: m_wx = value; break;
        default: break;
    }
}

void PPU::change_mode(uint8_t mode) {
    // Clear lower 2 bits of STAT and set new mode
    m_stat = (m_stat & ~0x03) | (mode & 0x03);
}

void PPU::render_scanline(MMU& mmu) {
    uint16_t bg_map_base = get_bg_tile_map_address();
    uint16_t tile_base = get_tile_data_address();
    bool is_signed = (tile_base == 0x8800);

    uint8_t scy = mmu.read(0xFF42);
    uint8_t scx = mmu.read(0xFF43);
    uint8_t bgp = mmu.read(0xFF47);

    uint32_t colors[4] = {0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000};

    // Track the BG color drawn at each pixel for sprite priority checks
    uint8_t scanline_bg_colors[160] = {0};

    // --- 1. RENDER BACKGROUND ---
    if (is_bg_window_enabled()) {
        uint8_t y = (m_ly + scy) & 0xFF;
        uint8_t tile_y = y / 8;
        uint8_t pixel_y = y % 8;

        for (int p_x = 0; p_x < 160; p_x++) {
            uint8_t x = (p_x + scx) & 0xFF;
            uint8_t tile_x = x / 8;

            uint16_t tile_address = bg_map_base + (tile_y * 32) + tile_x;
            uint8_t tile_id = mmu.read(tile_address);

            uint16_t tile_data_loc = tile_base;
            if (is_signed) {
                int8_t signed_id = static_cast<int8_t>(tile_id);
                tile_data_loc += (signed_id + 128) * 16;
            } else {
                tile_data_loc += (tile_id * 16);
            }

            uint8_t byte1 = mmu.read(tile_data_loc + (pixel_y * 2));
            uint8_t byte2 = mmu.read(tile_data_loc + (pixel_y * 2) + 1);

            int bit_index = 7 - (x % 8);
            uint8_t color_bit1 = (byte1 >> bit_index) & 1;
            uint8_t color_bit2 = (byte2 >> bit_index) & 1;
            uint8_t color_id = (color_bit2 << 1) | color_bit1;

            scanline_bg_colors[p_x] = color_id; // Save for sprites
            uint8_t actual_color = (bgp >> (color_id * 2)) & 0x03;
            m_framebuffer[m_ly * 160 + p_x] = colors[actual_color];
        }
    } else {
        for (int p_x = 0; p_x < 160; p_x++) {
            m_framebuffer[m_ly * 160 + p_x] = colors[0];
            scanline_bg_colors[p_x] = 0;
        }
    }

    // --- 2. RENDER WINDOW LAYER ---
    if (is_window_enabled() && is_bg_window_enabled() && m_ly >= m_wy) {
        uint16_t win_map_base = get_window_tile_map_address();
        uint8_t win_y = m_ly - m_wy;
        uint8_t tile_y = win_y / 8;
        uint8_t pixel_y = win_y % 8;

        for (int p_x = 0; p_x < 160; p_x++) {
            if (p_x < m_wx - 7) continue; // Window X is offset by 7 internally

            uint8_t win_x = p_x - (m_wx - 7);
            uint8_t tile_x = win_x / 8;

            uint16_t tile_address = win_map_base + (tile_y * 32) + tile_x;
            uint8_t tile_id = mmu.read(tile_address);

            uint16_t tile_data_loc = tile_base;
            if (is_signed) {
                int8_t signed_id = static_cast<int8_t>(tile_id);
                tile_data_loc += (signed_id + 128) * 16;
            } else {
                tile_data_loc += (tile_id * 16);
            }

            uint8_t byte1 = mmu.read(tile_data_loc + (pixel_y * 2));
            uint8_t byte2 = mmu.read(tile_data_loc + (pixel_y * 2) + 1);

            int bit_index = 7 - (win_x % 8);
            uint8_t color_bit1 = (byte1 >> bit_index) & 1;
            uint8_t color_bit2 = (byte2 >> bit_index) & 1;
            uint8_t color_id = (color_bit2 << 1) | color_bit1;

            scanline_bg_colors[p_x] = color_id; // Overwrite BG color tracking
            uint8_t actual_color = (bgp >> (color_id * 2)) & 0x03;
            m_framebuffer[m_ly * 160 + p_x] = colors[actual_color];
        }
    }

    // --- 3. RENDER SPRITES (OAM) ---
    if (!are_sprites_enabled()) return;

    bool tall_sprites = get_sprite_height() == 16;
    int sprite_height = tall_sprites ? 16 : 8;

    // Draw backwards (39 to 0) so earlier sprites draw on top
    for (int i = 39; i >= 0; i--) {
        uint16_t oam_addr = 0xFE00 + (i * 4);
        int sprite_y = mmu.read(oam_addr) - 16;
        int sprite_x = mmu.read(oam_addr + 1) - 8;
        uint8_t tile_index = mmu.read(oam_addr + 2);
        uint8_t attributes = mmu.read(oam_addr + 3);

        if (m_ly < sprite_y || m_ly >= sprite_y + sprite_height) {
            continue;
        }

        bool bg_priority = (attributes & 0x80) != 0; // Bit 7!
        bool y_flip      = (attributes & 0x40) != 0;
        bool x_flip      = (attributes & 0x20) != 0;
        bool use_obp1    = (attributes & 0x10) != 0;

        uint8_t obp = mmu.read(use_obp1 ? 0xFF49 : 0xFF48);

        int line = m_ly - sprite_y;
        if (y_flip) {
            line = (sprite_height - 1) - line;
        }

        if (tall_sprites) {
            if (line < 8) {
                tile_index &= 0xFE;
            } else {
                tile_index |= 0x01;
                line -= 8;
            }
        }

        uint16_t tile_data_loc = 0x8000 + (tile_index * 16);
        uint8_t byte1 = mmu.read(tile_data_loc + (line * 2));
        uint8_t byte2 = mmu.read(tile_data_loc + (line * 2) + 1);

        for (int x = 0; x < 8; x++) {
            int px = sprite_x + x;
            if (px < 0 || px >= 160) continue;

            int bit_index = x_flip ? x : (7 - x);
            uint8_t color_bit1 = (byte1 >> bit_index) & 1;
            uint8_t color_bit2 = (byte2 >> bit_index) & 1;
            uint8_t color_id = (color_bit2 << 1) | color_bit1;

            if (color_id == 0) continue; // Transparent pixel

            // PRIORITY CHECK: If Bit 7 is set, sprite hides behind non-white BG pixels
            if (bg_priority && scanline_bg_colors[px] != 0) continue;

            uint8_t actual_color = (obp >> (color_id * 2)) & 0x03;
            m_framebuffer[m_ly * 160 + px] = colors[actual_color];
        }
    }
}

void PPU::step(int cycles, MMU& mmu) {
    if (!is_lcd_enabled()) {
        m_scanline_counter -= cycles;
        if (m_scanline_counter <= 0) {
            m_scanline_counter += 70224;
            for (int i = 0; i < 160 * 144; i++) {
                m_framebuffer[i] = 0xFFFFFFFF; // Blank white screen
            }
        }
        return;
    }

    m_scanline_counter -= cycles;
    if (m_scanline_counter <= 0) {
        m_scanline_counter += 456;

        // Call our new render function!
        if (m_ly < 144) {
            render_scanline(mmu);
        }

        m_ly++;

        if (m_ly == 144) {
            change_mode(1); // VBlank
            frame_ready = true;

            // --- NEW: Request VBlank Interrupt! ---
            uint8_t current_if = mmu.read(0xFF0F);
            mmu.write(0xFF0F, current_if | 0x01); // Set bit 0

        } else if (m_ly > 153) {
            m_ly = 0; // Restart frame
        }

        if (m_ly == m_lyc) m_stat |= 0x04;
        else               m_stat &= ~0x04;
    }
}