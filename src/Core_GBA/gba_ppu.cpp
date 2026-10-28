#include "../../include/Core_GBA/gba_ppu.h"
#include "../../include/Core_GBA/gba_mmu.h"
#include "../../include/Core_Shared/color_utils.h"
#include "../../include/Core_Shared/bit_utils.h"

GBAPPU::GBAPPU(GBAMMU& mmu) : m_mmu(mmu) {
    reset();
}

void GBAPPU::reset() {
    m_framebuffer.fill(0xFF000000);
    m_debug_tile_buffer.fill(0xFF000000);
}

void GBAPPU::render_bg_regular(int ly, int bg, uint16_t bgcnt) {
    auto& vram    = m_mmu.vram();
    auto& pal_ram = m_mmu.pal_ram();

    uint32_t char_base   = ((bgcnt >> 2) & 0x03) * 0x4000;
    bool is_8bpp         = (bgcnt >> 7) & 1;
    uint32_t screen_base = ((bgcnt >> 8) & 0x1F) * 0x0800;
    uint8_t screen_size  = (bgcnt >> 14) & 0x03;

    uint16_t hofs = m_mmu.io_read16(0x010 + bg * 4) & 0x1FF;
    uint16_t vofs = m_mmu.io_read16(0x012 + bg * 4) & 0x1FF;

    int y      = ly + vofs;
    int tile_y = y / 8;
    int fine_y = y & 7;

    for (int x = 0; x < 240; ++x) {
        int sx     = x + hofs;
        int tile_x = sx / 8;
        int fine_x = sx & 7;

        int block = 0;
        if (screen_size == 1)      block = (tile_x >> 5) & 1;
        else if (screen_size == 2) block = (tile_y >> 5) & 1;
        else if (screen_size == 3) block = ((tile_x >> 5) & 1) + (((tile_y >> 5) & 1) * 2);

        uint32_t map_addr = (screen_base + block * 0x800 + ((tile_y & 31) * 32 + (tile_x & 31)) * 2) & 0xFFFF;
        uint16_t entry    = CoreShared::read_le16(vram.data(), map_addr);

        uint16_t tile_id = entry & 0x3FF;
        bool hflip       = (entry >> 10) & 1;
        bool vflip       = (entry >> 11) & 1;
        uint8_t pal_bank = (entry >> 12) & 0x0F;

        int px = hflip ? (7 - fine_x) : fine_x;
        int py = vflip ? (7 - fine_y) : fine_y;

        uint8_t color_idx = 0;
        if (is_8bpp) {
            uint32_t t_addr = (char_base + tile_id * 64 + py * 8 + px) & 0xFFFF;
            color_idx = vram[t_addr];
        } else {
            uint32_t t_addr = (char_base + tile_id * 32 + py * 4 + (px >> 1)) & 0xFFFF;
            uint8_t byte   = vram[t_addr];
            uint8_t nibble = (px & 1) ? (byte >> 4) : (byte & 0x0F);
            if (nibble != 0) color_idx = (pal_bank * 16) + nibble;
        }

        if (color_idx != 0) {
            uint16_t bgr = CoreShared::read_le16(pal_ram.data(), color_idx * 2);
            m_framebuffer[ly * 240 + x] = CoreShared::bgr555_to_argb8888(bgr);
        }
    }
}

void GBAPPU::render_sprites(int ly, uint16_t dispcnt, int target_prio) {
    static const int OBJ_WIDTHS[4][4]  = {{8,16,32,64}, {16,32,32,64}, {8,8,16,32}, {8,8,8,8}};
    static const int OBJ_HEIGHTS[4][4] = {{8,16,32,64}, {8,8,16,32},   {16,32,32,64},{8,8,8,8}};

    auto& oam     = m_mmu.oam();
    auto& vram    = m_mmu.vram();
    auto& pal_ram = m_mmu.pal_ram();
    bool mapping_1d = (dispcnt >> 6) & 1;

    for (int i = 127; i >= 0; --i) {
        uint32_t base  = i * 8;
        uint16_t attr0 = CoreShared::read_le16(oam.data(), base);
        uint16_t attr1 = CoreShared::read_le16(oam.data(), base + 2);
        uint16_t attr2 = CoreShared::read_le16(oam.data(), base + 4);

        if (((attr0 >> 8) & 3) == 2) continue; // Disabled sprite
        if (((attr2 >> 10) & 3) != target_prio) continue;

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
        bool is_affine   = (attr0 >> 8) & 1;
        bool hflip       = !is_affine && ((attr1 >> 12) & 1);
        bool vflip       = !is_affine && ((attr1 >> 13) & 1);
        uint16_t tile_id = attr2 & 0x3FF;
        uint8_t pal_bank = (attr2 >> 12) & 0x0F;

        int py       = vflip ? (h - 1 - (ly - sy)) : (ly - sy);
        int tile_row = py / 8;
        int fine_y   = py & 7;

        for (int x = 0; x < w; ++x) {
            int screen_x = sx + x;
            if (screen_x < 0 || screen_x >= 240) continue;

            int px       = hflip ? (w - 1 - x) : x;
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
                if (addr < vram.size()) color_idx = vram[addr];
            } else {
                uint32_t addr = 0x10000 + (cur_tile * 32) + fine_y * 4 + (fine_x >> 1);
                if (addr < vram.size()) {
                    uint8_t b   = vram[addr];
                    uint8_t nib = (fine_x & 1) ? (b >> 4) : (b & 0x0F);
                    if (nib != 0) color_idx = pal_bank * 16 + nib;
                }
            }

            if (color_idx != 0) {
                uint16_t bgr = CoreShared::read_le16(pal_ram.data(), 0x200 + color_idx * 2);
                m_framebuffer[ly * 240 + screen_x] = CoreShared::bgr555_to_argb8888(bgr);
            }
        }
    }
}

void GBAPPU::render_scanline(int ly) {
    uint16_t dispcnt = m_mmu.io_read16(0x000);

    // Forced Blank (Bit 7 of DISPCNT) draws white scanline
    if (dispcnt & 0x0080) {
        for (int x = 0; x < 240; ++x) m_framebuffer[ly * 240 + x] = 0xFFFFFFFF;
        return;
    }

    auto& vram    = m_mmu.vram();
    auto& pal_ram = m_mmu.pal_ram();
    uint8_t mode  = dispcnt & 0x07;

    uint16_t backdrop_bgr = CoreShared::read_le16(pal_ram.data(), 0);
    uint32_t backdrop     = CoreShared::bgr555_to_argb8888(backdrop_bgr);
    for (int x = 0; x < 240; ++x) {
        m_framebuffer[ly * 240 + x] = backdrop;
    }

    if (mode == 3) {
        if (dispcnt & (1 << 10)) {
            for (int x = 0; x < 240; ++x) {
                uint16_t bgr = CoreShared::read_le16(vram.data(), (ly * 240 + x) * 2);
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
                    uint16_t bgr = CoreShared::read_le16(pal_ram.data(), pal_idx * 2);
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
                uint16_t bgcnt = m_mmu.io_read16(0x008 + bg * 2);
                if ((bgcnt & 3) != prio) continue;
                render_bg_regular(ly, bg, bgcnt);
            }
            if (dispcnt & (1 << 12)) {
                render_sprites(ly, dispcnt, prio);
            }
        }
    }
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