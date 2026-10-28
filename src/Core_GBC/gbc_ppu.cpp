#include "../../include/Core_GBC/gbc_ppu.h"
#include "../../include/Core_GBC/gbc_mmu.h"
#include "../../include/Core_Shared/color_utils.h"
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
        case 0xFF68: return m_bcps | 0x40; // Bit 6 is always 1
        case 0xFF69: return m_bg_palette_ram[m_bcps & 0x3F];
        case 0xFF6A: return m_ocps | 0x40; // Bit 6 is always 1
        case 0xFF6B: return m_sprite_palette_ram[m_ocps & 0x3F];
        default: return 0xFF;
    }
}

void PPU::write_register(uint16_t address, uint8_t value) {
    switch (address) {
        case 0xFF40: {
            bool was_enabled = is_lcd_enabled();
            m_lcdc = value;

            // If LCD was just disabled (Bit 7 transition 1 -> 0)
            // If LCD was just disabled (Bit 7 transition 1 -> 0)
            if (was_enabled && !is_lcd_enabled()) {
                m_ly = 0;
                m_window_line = 0;
                m_scanline_counter = 456;
                m_stat = (m_stat & 0xFC) | 0; // Directly force Mode 0
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
        case 0xFF68:
            m_bcps = value;
            break;
        case 0xFF69: {
            uint8_t index = m_bcps & 0x3F;
            m_bg_palette_ram[index] = value;
            // Auto-increment if Bit 7 is set
            if (m_bcps & 0x80) {
                uint8_t next_index = (index + 1) & 0x3F;
                m_bcps = 0x80 | next_index;
            }
            break;
        }
        case 0xFF6A:
            m_ocps = value;
            break;
        case 0xFF6B: {
            uint8_t index = m_ocps & 0x3F;
            m_sprite_palette_ram[index] = value;
            // Auto-increment if Bit 7 is set
            if (m_ocps & 0x80) {
                uint8_t next_index = (index + 1) & 0x3F;
                m_ocps = 0x80 | next_index;
            }
            break;
        }
        default: break;
    }
}

void PPU::change_mode(uint8_t mode, MMU& mmu) {
    // Clear lower 2 bits of STAT and set new mode
    m_stat = (m_stat & 0xFC) | (mode & 0x03);

    // Check STAT Interrupts
    bool request_interrupt = false;
    if (mode == 0 && (m_stat & (1 << 3))) request_interrupt = true;
    if (mode == 1 && (m_stat & (1 << 4))) request_interrupt = true;
    if (mode == 2 && (m_stat & (1 << 5))) request_interrupt = true;

    if (request_interrupt) {
        uint8_t if_reg = mmu.read(0xFF0F);
        mmu.write(0xFF0F, if_reg | (1 << 1));
    }
}

void PPU::render_scanline(MMU& mmu) {
    uint16_t bg_map_base = get_bg_tile_map_address();
    uint16_t tile_base = get_tile_data_address();
    bool is_signed = (tile_base == 0x8800);

    uint8_t scy = mmu.read(0xFF42);
    uint8_t scx = mmu.read(0xFF43);
    uint8_t bgp = mmu.read(0xFF47);

    const uint32_t dmg_colors[4] = {0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000};

    // Track the BG color ID (0-3) drawn at each pixel for sprite priority checks
    uint8_t scanline_bg_colors[160] = {0};

    // --- 1. RENDER BACKGROUND (DMG & CGB) ---
    if (is_bg_window_enabled() || mmu.m_is_cgb) {
        uint8_t y = (m_ly + scy) & 0xFF;
        uint8_t tile_y = y / 8;
        uint8_t render_pixel_y = y % 8;

        for (int p_x = 0; p_x < 160; p_x++) {
            uint8_t x = (p_x + scx) & 0xFF;
            uint8_t tile_x = x / 8;

            uint16_t map_address = bg_map_base + (tile_y * 32) + tile_x;
            uint8_t tile_id = mmu.read_vram(0, map_address);

            uint8_t palette_num = 0;
            uint8_t tile_bank   = 0;
            bool x_flip         = false;
            bool y_flip         = false;

            if (mmu.m_is_cgb) {
                uint8_t attributes = mmu.read_vram(1, map_address);
                palette_num = attributes & 0x07;
                tile_bank   = (attributes >> 3) & 0x01;
                x_flip      = (attributes >> 5) & 0x01;
                y_flip      = (attributes >> 6) & 0x01;
            }

            uint8_t active_pixel_y = y_flip ? (7 - render_pixel_y) : render_pixel_y;

            uint16_t tile_data_loc = tile_base;
            if (is_signed) {
                int8_t signed_id = static_cast<int8_t>(tile_id);
                tile_data_loc += (signed_id + 128) * 16;
            } else {
                tile_data_loc += (tile_id * 16);
            }

            uint8_t byte1 = mmu.read_vram(tile_bank, tile_data_loc + (active_pixel_y * 2));
            uint8_t byte2 = mmu.read_vram(tile_bank, tile_data_loc + (active_pixel_y * 2) + 1);

            uint8_t active_pixel_x = x_flip ? (x % 8) : (7 - (x % 8));

            uint8_t color_bit1 = (byte1 >> active_pixel_x) & 1;
            uint8_t color_bit2 = (byte2 >> active_pixel_x) & 1;
            uint8_t color_id = (color_bit2 << 1) | color_bit1;

            scanline_bg_colors[p_x] = color_id;

            if (mmu.m_is_cgb) {
                uint8_t color_address = (palette_num * 8) + (color_id * 2);
                uint8_t pal_byte1 = m_bg_palette_ram[color_address];
                uint8_t pal_byte2 = m_bg_palette_ram[color_address + 1];
                m_framebuffer[m_ly * 160 + p_x] = color_correct_rgb555(pal_byte1, pal_byte2);
            } else {
                uint8_t shade = (bgp >> (color_id * 2)) & 0x03;
                m_framebuffer[m_ly * 160 + p_x] = dmg_colors[shade];
            }
        }
    } else {
        for (int p_x = 0; p_x < 160; p_x++) {
            m_framebuffer[m_ly * 160 + p_x] = 0xFFFFFFFF;
            scanline_bg_colors[p_x] = 0;
        }
    }

    // --- 2. RENDER WINDOW LAYER (DMG & CGB) ---
    if (is_window_enabled() && (is_bg_window_enabled() || mmu.m_is_cgb) && m_ly >= m_wy) {
        uint16_t win_map_base = get_window_tile_map_address();
        uint8_t win_y = m_window_line;
        uint8_t tile_y = win_y / 8;
        uint8_t render_pixel_y = win_y % 8;

        bool window_drawn_on_scanline = false;

        for (int p_x = 0; p_x < 160; p_x++) {
            if (p_x < m_wx - 7) continue;

            window_drawn_on_scanline = true;

            uint8_t win_x = p_x - (m_wx - 7);
            uint8_t tile_x = win_x / 8;

            uint16_t map_address = win_map_base + (tile_y * 32) + tile_x;
            uint8_t tile_id = mmu.read_vram(0, map_address);

            uint8_t palette_num = 0;
            uint8_t tile_bank   = 0;
            bool x_flip         = false;
            bool y_flip         = false;

            if (mmu.m_is_cgb) {
                uint8_t attributes = mmu.read_vram(1, map_address);
                palette_num = attributes & 0x07;
                tile_bank   = (attributes >> 3) & 0x01;
                x_flip      = (attributes >> 5) & 0x01;
                y_flip      = (attributes >> 6) & 0x01;
            }

            uint8_t active_pixel_y = y_flip ? (7 - render_pixel_y) : render_pixel_y;

            uint16_t tile_data_loc = tile_base;
            if (is_signed) {
                int8_t signed_id = static_cast<int8_t>(tile_id);
                tile_data_loc += (signed_id + 128) * 16;
            } else {
                tile_data_loc += (tile_id * 16);
            }

            uint8_t byte1 = mmu.read_vram(tile_bank, tile_data_loc + (active_pixel_y * 2));
            uint8_t byte2 = mmu.read_vram(tile_bank, tile_data_loc + (active_pixel_y * 2) + 1);

            uint8_t active_pixel_x = x_flip ? (win_x % 8) : (7 - (win_x % 8));

            uint8_t color_bit1 = (byte1 >> active_pixel_x) & 1;
            uint8_t color_bit2 = (byte2 >> active_pixel_x) & 1;
            uint8_t color_id = (color_bit2 << 1) | color_bit1;

            scanline_bg_colors[p_x] = color_id;

            if (mmu.m_is_cgb) {
                uint8_t color_address = (palette_num * 8) + (color_id * 2);
                uint8_t pal_byte1 = m_bg_palette_ram[color_address];
                uint8_t pal_byte2 = m_bg_palette_ram[color_address + 1];
                m_framebuffer[m_ly * 160 + p_x] = color_correct_rgb555(pal_byte1, pal_byte2);
            } else {
                uint8_t shade = (bgp >> (color_id * 2)) & 0x03;
                m_framebuffer[m_ly * 160 + p_x] = dmg_colors[shade];
            }
        }

        if (window_drawn_on_scanline) {
            m_window_line++;
        }
    }

    // --- 3. RENDER SPRITES (OAM) (DMG & CGB) ---
    if (!are_sprites_enabled()) return;

    bool tall_sprites = get_sprite_height() == 16;
    int sprite_height = tall_sprites ? 16 : 8;

    for (int i = 39; i >= 0; i--) {
        uint16_t oam_addr = 0xFE00 + (i * 4);
        int sprite_y = mmu.read(oam_addr) - 16;
        int sprite_x = mmu.read(oam_addr + 1) - 8;
        uint8_t tile_index = mmu.read(oam_addr + 2);
        uint8_t attributes = mmu.read(oam_addr + 3);

        if (m_ly < sprite_y || m_ly >= sprite_y + sprite_height) {
            continue;
        }

        bool bg_priority = (attributes & 0x80) != 0;
        bool y_flip      = (attributes & 0x40) != 0;
        bool x_flip      = (attributes & 0x20) != 0;
        bool use_obp1    = (attributes & 0x10) != 0;

        uint8_t cgb_palette = attributes & 0x07;
        uint8_t tile_bank   = mmu.m_is_cgb ? ((attributes >> 3) & 0x01) : 0;

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
        uint8_t byte1 = mmu.read_vram(tile_bank, tile_data_loc + (line * 2));
        uint8_t byte2 = mmu.read_vram(tile_bank, tile_data_loc + (line * 2) + 1);

        uint8_t obp = mmu.read(use_obp1 ? 0xFF49 : 0xFF48);

        for (int x = 0; x < 8; x++) {
            int px = sprite_x + x;
            if (px < 0 || px >= 160) continue;

            int bit_index = x_flip ? x : (7 - x);
            uint8_t color_bit1 = (byte1 >> bit_index) & 1;
            uint8_t color_bit2 = (byte2 >> bit_index) & 1;
            uint8_t color_id = (color_bit2 << 1) | color_bit1;

            if (color_id == 0) continue; // Transparent pixel

            if (bg_priority && scanline_bg_colors[px] != 0) continue;

            if (mmu.m_is_cgb) {
                uint8_t color_address = (cgb_palette * 8) + (color_id * 2);
                uint8_t pal_byte1 = m_sprite_palette_ram[color_address];
                uint8_t pal_byte2 = m_sprite_palette_ram[color_address + 1];
                m_framebuffer[m_ly * 160 + px] = color_correct_rgb555(pal_byte1, pal_byte2);
            } else {
                uint8_t shade = (obp >> (color_id * 2)) & 0x03;
                m_framebuffer[m_ly * 160 + px] = dmg_colors[shade];
            }
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

    // --- THE MISSING STAT MODE STATE MACHINE ---
    // The PPU must cycle through Mode 2, Mode 3, and Mode 0 during visible lines.
    // Crystal relies on Mode 0 (H-Blank) to safely push graphics to VRAM.
    if (m_ly < 144) {
        if (m_scanline_counter >= 376) {
            // Mode 2: OAM Search (First 80 cycles)
            if ((m_stat & 0x03) != 2) change_mode(2, mmu);
        } else if (m_scanline_counter >= 204) {
            // Mode 3: Pixel Transfer (Next ~172 cycles)
            if ((m_stat & 0x03) != 3) change_mode(3, mmu);
        } else {
            // Mode 0: H-Blank (Remaining ~204 cycles)
            if ((m_stat & 0x03) != 0) change_mode(0, mmu);
        }
    }

    if (m_scanline_counter <= 0) {
        m_scanline_counter += 456;

        // Render the line right as H-Blank concludes
        if (m_ly < 144) {
            render_scanline(mmu);
            mmu.step_hdma();
        }

        m_ly++;

        if (m_ly == 144) {
            change_mode(1, mmu); // VBlank
            frame_ready = true;

            // Request VBlank Interrupt
            uint8_t current_if = mmu.read(0xFF0F);
            mmu.write(0xFF0F, current_if | 0x01);

        } else if (m_ly > 153) {
            m_ly = 0; // Restart frame
            m_window_line = 0;
        }

        // --- LY == LYC Check & Interrupt ---
        if (m_ly == m_lyc) {
            m_stat |= 0x04; // Set LY Coincidence Flag
            if (m_stat & (1 << 6)) {
                // Request STAT Interrupt
                uint8_t current_if = mmu.read(0xFF0F);
                mmu.write(0xFF0F, current_if | 0x02);
            }
        } else {
            m_stat &= ~0x04; // Clear LY Coincidence Flag
        }
    }
}

void PPU::check_lyc(MMU& mmu) {
    if (m_ly == m_lyc) {
        m_stat |= 0x04; // Set the LY Coincidence Flag (Bit 2)

        // If the LYC Interrupt Enable flag is set (Bit 6), fire the STAT interrupt!
        if (m_stat & (1 << 6)) {
            uint8_t if_reg = mmu.read(0xFF0F);
            mmu.write(0xFF0F, if_reg | (1 << 1));
        }
    } else {
        m_stat &= ~0x04; // Clear the LY Coincidence Flag
    }
}

uint32_t PPU::color_correct_rgb555(uint8_t byte1, uint8_t byte2) {
    return CoreShared::bgr555_bytes_to_argb8888(byte1, byte2);
}