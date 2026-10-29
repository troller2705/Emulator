#include "../../include/Core_GBA/gba_ppu.h"
#include "../../include/Core_GBA/gba_mmu.h"
#include "../../include/Core_Shared/color_utils.h"
#include "../../include/Core_Shared/bit_utils.h"
#include <algorithm>

GBAPPU::GBAPPU(GBAMMU& mmu) : m_mmu(mmu) {
    reset();
}

void GBAPPU::reset() {
    m_framebuffer.fill(0xFF000000);
    m_debug_tile_buffer.fill(0xFF000000);
}

void GBAPPU::render_bg_regular(int ly, int bg, uint16_t bgcnt) {
    const uint8_t* vram    = m_mmu.vram().data();
    const uint8_t* pal_ram = m_mmu.pal_ram().data();

    uint32_t char_base   = ((bgcnt >> 2) & 0x03) * 0x4000;
    bool is_8bpp         = (bgcnt >> 7) & 1;
    uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x0800;
    uint8_t screen_size  = (bgcnt >> 14) & 0x03;

    uint16_t hofs = m_mmu.io_read16(0x010 + bg * 4) & 0x1FF;
    uint16_t vofs = m_mmu.io_read16(0x012 + bg * 4) & 0x1FF;

    int y      = ly + vofs;
    int tile_y = y >> 3;
    int fine_y = y & 7;

    uint32_t* line_dst = &m_framebuffer[ly * 240];

    for (int x = 0; x < 240; ++x) {
        int sx     = x + hofs;
        int tile_x = sx >> 3;
        int fine_x = sx & 7;

        int block = 0;
        if (screen_size == 1)      block = (tile_x >> 5) & 1;
        else if (screen_size == 2) block = (tile_y >> 5) & 1;
        else if (screen_size == 3) block = ((tile_x >> 5) & 1) + (((tile_y >> 5) & 1) * 2);

        uint32_t map_addr = (screen_base + block * 0x800 + ((tile_y & 31) * 32 + (tile_x & 31)) * 2) & 0xFFFF;
        uint16_t entry    = CoreShared::read_le16(vram, map_addr);

        uint16_t tile_id = entry & 0x3FF;
        bool hflip       = (entry >> 10) & 1;
        bool vflip       = (entry >> 11) & 1;
        uint8_t pal_bank = (entry >> 12) & 0x0F;

        int px = hflip ? (7 - fine_x) : fine_x;
        int py = vflip ? (7 - fine_y) : fine_y;

        uint8_t color_idx = 0;
        if (is_8bpp) {
            uint32_t t_addr = char_base + tile_id * 64 + py * 8 + px;
            if (t_addr < 0x10000) color_idx = vram[t_addr];
        } else {
            uint32_t t_addr = char_base + tile_id * 32 + py * 4 + (px >> 1);
            if (t_addr < 0x10000) {
                uint8_t byte   = vram[t_addr];
                uint8_t nibble = (px & 1) ? (byte >> 4) : (byte & 0x0F);
                if (nibble != 0) color_idx = (pal_bank * 16) + nibble;
            }
        }

        if (color_idx != 0) {
            uint16_t bgr = CoreShared::read_le16(pal_ram, color_idx * 2);
            line_dst[x]  = CoreShared::bgr555_to_argb8888(bgr);
        }
    }
}

void GBAPPU::render_bg_affine(int ly, int bg, uint16_t bgcnt) {
    const uint8_t* vram    = m_mmu.vram().data();
    const uint8_t* pal_ram = m_mmu.pal_ram().data();
    const uint8_t* io      = m_mmu.io().data();

    uint32_t char_base   = ((bgcnt >> 2) & 0x03) * 0x4000;
    uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x0800;
    bool wraparound      = (bgcnt >> 13) & 1;
    uint8_t screen_size  = (bgcnt >> 14) & 0x03;

    int map_tiles  = 16 << screen_size; // 16, 32, 64, or 128 tiles
    int map_pixels = map_tiles * 8;     // 128, 256, 512, or 1024 pixels

    uint32_t io_base = (bg == 2) ? 0x020 : 0x030;
    int16_t pa = static_cast<int16_t>(CoreShared::read_le16(io, io_base + 0x00));
    int16_t pb = static_cast<int16_t>(CoreShared::read_le16(io, io_base + 0x02));
    int16_t pc = static_cast<int16_t>(CoreShared::read_le16(io, io_base + 0x04));
    int16_t pd = static_cast<int16_t>(CoreShared::read_le16(io, io_base + 0x06));

    // If identity matrix hasn't been initialized yet, default to 1.0x scale
    if (pa == 0 && pb == 0 && pc == 0 && pd == 0) {
        pa = 0x0100;
        pd = 0x0100;
    }

    uint32_t raw_x = CoreShared::read_le32(io, io_base + 0x08);
    uint32_t raw_y = CoreShared::read_le32(io, io_base + 0x0C);
    // Sign-extend 28-bit signed fixed-point reference coordinates
    int32_t ref_x = (static_cast<int32_t>(raw_x << 4)) >> 4;
    int32_t ref_y = (static_cast<int32_t>(raw_y << 4)) >> 4;

    int32_t cur_x = ref_x + ly * pb;
    int32_t cur_y = ref_y + ly * pd;

    uint32_t* line_dst = &m_framebuffer[ly * 240];

    for (int x = 0; x < 240; ++x) {
        int tex_x = cur_x >> 8;
        int tex_y = cur_y >> 8;
        cur_x += pa;
        cur_y += pc;

        if (wraparound) {
            tex_x &= (map_pixels - 1);
            tex_y &= (map_pixels - 1);
        } else if (tex_x < 0 || tex_x >= map_pixels || tex_y < 0 || tex_y >= map_pixels) {
            continue;
        }

        int tile_x = tex_x >> 3;
        int tile_y = tex_y >> 3;
        int fine_x = tex_x & 7;
        int fine_y = tex_y & 7;

        uint32_t map_addr = (screen_base + tile_y * map_tiles + tile_x) & 0xFFFF;
        uint8_t tile_id   = vram[map_addr];

        uint32_t t_addr = char_base + tile_id * 64 + fine_y * 8 + fine_x;
        if (t_addr < 0x10000) {
            uint8_t color_idx = vram[t_addr];
            if (color_idx != 0) {
                uint16_t bgr = CoreShared::read_le16(pal_ram, color_idx * 2);
                line_dst[x]  = CoreShared::bgr555_to_argb8888(bgr);
            }
        }
    }
}

void GBAPPU::render_sprites(int ly, uint16_t dispcnt, int target_prio) {
    static const int OBJ_WIDTHS[4][4]  = {{8,16,32,64}, {16,32,32,64}, {8,8,16,32}, {8,8,8,8}};
    static const int OBJ_HEIGHTS[4][4] = {{8,16,32,64}, {8,8,16,32},   {16,32,32,64},{8,8,8,8}};

    const uint8_t* oam     = m_mmu.oam().data();
    const uint8_t* vram    = m_mmu.vram().data();
    const uint8_t* pal_ram = m_mmu.pal_ram().data();
    bool mapping_1d        = (dispcnt >> 6) & 1;
    uint32_t* line_dst     = &m_framebuffer[ly * 240];

    for (int i = 127; i >= 0; --i) {
        uint32_t base  = i * 8;
        uint16_t attr0 = CoreShared::read_le16(oam, base);
        uint16_t attr1 = CoreShared::read_le16(oam, base + 2);
        uint16_t attr2 = CoreShared::read_le16(oam, base + 4);

        uint8_t obj_mode = (attr0 >> 8) & 3;
        if (obj_mode == 2) continue; // Disabled sprite
        if (((attr2 >> 10) & 3) != target_prio) continue;

        uint8_t shape = (attr0 >> 14) & 3;
        uint8_t size  = (attr1 >> 14) & 3;
        int w = OBJ_WIDTHS[shape][size];
        int h = OBJ_HEIGHTS[shape][size];

        bool is_affine   = (obj_mode & 1) != 0;
        bool double_size = (obj_mode == 3);
        int bbox_w = double_size ? (w * 2) : w;
        int bbox_h = double_size ? (h * 2) : h;

        int sy = attr0 & 0xFF;
        if (sy >= 160) sy -= 256;
        if (ly < sy || ly >= sy + bbox_h) continue;

        int sx = attr1 & 0x1FF;
        if (sx >= 240) sx -= 512;

        bool is_8bpp     = (attr0 >> 13) & 1;
        bool hflip       = !is_affine && ((attr1 >> 12) & 1);
        bool vflip       = !is_affine && ((attr1 >> 13) & 1);
        uint16_t tile_id = attr2 & 0x3FF;
        uint8_t pal_bank = (attr2 >> 12) & 0x0F;

        int16_t pa = 0x0100, pb = 0, pc = 0, pd = 0x0100;
        if (is_affine) {
            uint32_t p_base = ((attr1 >> 9) & 0x1F) * 32;
            pa = static_cast<int16_t>(CoreShared::read_le16(oam, p_base + 0x06));
            pb = static_cast<int16_t>(CoreShared::read_le16(oam, p_base + 0x0E));
            pc = static_cast<int16_t>(CoreShared::read_le16(oam, p_base + 0x16));
            pd = static_cast<int16_t>(CoreShared::read_le16(oam, p_base + 0x1E));
        }

        int iy = (ly - sy) - (bbox_h / 2);

        for (int x = 0; x < bbox_w; ++x) {
            int screen_x = sx + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int px, py;
            if (is_affine) {
                int ix = x - (bbox_w / 2);
                px = ((pa * ix + pb * iy) >> 8) + (w / 2);
                py = ((pc * ix + pd * iy) >> 8) + (h / 2);
                if (px < 0 || px >= w || py < 0 || py >= h) continue;
            } else {
                px = hflip ? (w - 1 - x) : x;
                py = vflip ? (h - 1 - (ly - sy)) : (ly - sy);
            }

            int tile_row = py >> 3;
            int fine_y   = py & 7;
            int tile_col = px >> 3;
            int fine_x   = px & 7;

            uint32_t cur_tile = 0;
            if (mapping_1d) {
                int tiles_per_row = w >> 3;
                cur_tile = tile_id + (tile_row * tiles_per_row + tile_col) * (is_8bpp ? 2 : 1);
            } else {
                cur_tile = (tile_id + tile_row * 32 + tile_col * (is_8bpp ? 2 : 1)) & 0x3FF;
            }

            uint8_t color_idx = 0;
            if (is_8bpp) {
                uint32_t addr = 0x10000 + (cur_tile * 32) + fine_y * 8 + fine_x;
                if (addr < 0x18000) color_idx = vram[addr];
            } else {
                uint32_t addr = 0x10000 + (cur_tile * 32) + fine_y * 4 + (fine_x >> 1);
                if (addr < 0x18000) {
                    uint8_t b   = vram[addr];
                    uint8_t nib = (fine_x & 1) ? (b >> 4) : (b & 0x0F);
                    if (nib != 0) color_idx = pal_bank * 16 + nib;
                }
            }

            if (color_idx != 0) {
                uint16_t bgr       = CoreShared::read_le16(pal_ram, 0x200 + color_idx * 2);
                line_dst[screen_x] = CoreShared::bgr555_to_argb8888(bgr);
            }
        }
    }
}

void GBAPPU::apply_brightness_effect(int ly) {
    uint16_t bldcnt = m_mmu.io_read16(0x050);
    uint8_t effect  = (bldcnt >> 6) & 3;
    if (effect != 2 && effect != 3) return; // Only handle Brightness Increase (2) / Decrease (3)

    // Apply full-screen fade when Backdrop (Bit 5) or all layers are targeted
    if ((bldcnt & 0x3F) == 0) return;

    uint16_t bldy = m_mmu.io_read16(0x054) & 0x1F;
    if (bldy == 0) return;
    if (bldy > 16) bldy = 16;

    uint32_t* line_dst = &m_framebuffer[ly * 240];
    for (int x = 0; x < 240; ++x) {
        uint32_t c = line_dst[x];
        int r = (c >> 16) & 0xFF;
        int g = (c >> 8)  & 0xFF;
        int b =  c        & 0xFF;

        if (effect == 2) { // Fade to White
            r += ((255 - r) * bldy) >> 4;
            g += ((255 - g) * bldy) >> 4;
            b += ((255 - b) * bldy) >> 4;
        } else {           // Fade to Black
            r -= (r * bldy) >> 4;
            g -= (g * bldy) >> 4;
            b -= (b * bldy) >> 4;
        }
        line_dst[x] = 0xFF000000U | (static_cast<uint32_t>(r) << 16) |
                                    (static_cast<uint32_t>(g) << 8)  |
                                     static_cast<uint32_t>(b);
    }
}

void GBAPPU::render_scanline(int ly) {
    uint16_t dispcnt = m_mmu.io_read16(0x000);

    if (dispcnt & 0x0080) { // Forced Blank
        std::fill_n(&m_framebuffer[ly * 240], 240, 0xFFFFFFFFU);
        return;
    }

    const uint8_t* vram    = m_mmu.vram().data();
    const uint8_t* pal_ram = m_mmu.pal_ram().data();
    uint8_t mode           = dispcnt & 0x07;

    uint16_t backdrop_bgr = CoreShared::read_le16(pal_ram, 0);
    uint32_t backdrop     = CoreShared::bgr555_to_argb8888(backdrop_bgr);
    std::fill_n(&m_framebuffer[ly * 240], 240, backdrop);

    if (mode == 3) {
        if (dispcnt & (1 << 10)) {
            for (int x = 0; x < 240; ++x) {
                uint16_t bgr = CoreShared::read_le16(vram, (ly * 240 + x) * 2);
                m_framebuffer[ly * 240 + x] = CoreShared::bgr555_to_argb8888(bgr);
            }
        }
        if (dispcnt & (1 << 12)) {
            for (int prio = 3; prio >= 0; --prio) render_sprites(ly, dispcnt, prio);
        }
    } else if (mode == 4) {
        if (dispcnt & (1 << 10)) {
            uint32_t base = (dispcnt & (1 << 4)) ? 0xA000 : 0x0000;
            for (int x = 0; x < 240; ++x) {
                uint8_t pal_idx = vram[base + ly * 240 + x];
                if (pal_idx != 0) {
                    uint16_t bgr = CoreShared::read_le16(pal_ram, pal_idx * 2);
                    m_framebuffer[ly * 240 + x] = CoreShared::bgr555_to_argb8888(bgr);
                }
            }
        }
        if (dispcnt & (1 << 12)) {
            for (int prio = 3; prio >= 0; --prio) render_sprites(ly, dispcnt, prio);
        }
    } else if (mode == 5) {
        if (dispcnt & (1 << 10)) {
            uint32_t base = (dispcnt & (1 << 4)) ? 0xA000 : 0x0000;
            if (ly < 128) {
                for (int x = 0; x < 160; ++x) {
                    uint16_t bgr = CoreShared::read_le16(vram, base + (ly * 160 + x) * 2);
                    m_framebuffer[ly * 240 + x] = CoreShared::bgr555_to_argb8888(bgr);
                }
            }
        }
        if (dispcnt & (1 << 12)) {
            for (int prio = 3; prio >= 0; --prio) render_sprites(ly, dispcnt, prio);
        }
    } else if (mode <= 2) {
        for (int prio = 3; prio >= 0; --prio) {
            for (int bg = 3; bg >= 0; --bg) {
                if (!(dispcnt & (1 << (8 + bg)))) continue;
                if (mode == 1 && bg == 3) continue; // BG3 doesn't exist in Mode 1
                if (mode == 2 && bg < 2)  continue; // BG0 & BG1 don't exist in Mode 2

                uint16_t bgcnt = m_mmu.io_read16(0x008 + bg * 2);
                if ((bgcnt & 3) != prio) continue;

                bool is_affine_bg = (mode == 1 && bg == 2) || (mode == 2 && bg >= 2);
                if (is_affine_bg) {
                    render_bg_affine(ly, bg, bgcnt);
                } else {
                    render_bg_regular(ly, bg, bgcnt);
                }
            }
            if (dispcnt & (1 << 12)) {
                render_sprites(ly, dispcnt, prio);
            }
        }
    }

    apply_brightness_effect(ly);
}

void GBAPPU::debug_update_tile_buffer() {
    auto& vram    = m_mmu.vram();
    auto& pal_ram = m_mmu.pal_ram();
    for (int tile = 0; tile < 384; ++tile) {
        int tile_x    = (tile % 16) * 8;
        int tile_y    = (tile / 16) * 8;
        uint32_t base = tile * 32;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint8_t byte   = vram[base + row * 4 + (col / 2)];
                uint8_t nibble = (col & 1) ? (byte >> 4) : (byte & 0x0F);
                uint16_t bgr   = CoreShared::read_le16(pal_ram.data(), nibble * 2);
                m_debug_tile_buffer[(tile_y + row) * 128 + (tile_x + col)] = CoreShared::bgr555_to_argb8888(bgr);
            }
        }
    }
}