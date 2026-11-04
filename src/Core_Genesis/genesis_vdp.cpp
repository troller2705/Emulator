#include "Core_Genesis/genesis_vdp.h"
#include "Core_Genesis/genesis_mmu.h"
#include <algorithm>

GenesisVDP::GenesisVDP() {
    reset();
}

void GenesisVDP::reset() {
    m_vram.fill(0);
    m_cram.fill(0);
    m_vsram.fill(0);
    m_regs.fill(0);
    m_regs[15] = 2; // Default auto-increment = 2 bytes

    m_framebuffer.fill(0xFF000000);
    m_debug_tile_buffer.fill(0xFF000000);

    m_pending_second_word = false;
    m_first_word_latch = 0;
    m_code = 0;
    m_addr = 0;

    m_scanline = 0;
    m_hint_counter = 0;
    m_vblank_flag = false;
    m_vint_pending = false;
    m_hint_pending = false;
    m_dma_fill_pending = false;
}

uint32_t GenesisVDP::cram_to_argb8888(uint16_t cram_val) const {
    // Genesis CRAM format: 0000 BBB0 GGG0 RRR0 (3 bits per channel)
    uint8_t r3 = (cram_val >> 1) & 0x07;
    uint8_t g3 = (cram_val >> 5) & 0x07;
    uint8_t b3 = (cram_val >> 9) & 0x07;

    uint32_t r8 = (r3 * 255) / 7;
    uint32_t g8 = (g3 * 255) / 7;
    uint32_t b8 = (b3 * 255) / 7;

    return 0xFF000000U | (r8 << 16) | (g8 << 8) | b8;
}

uint16_t GenesisVDP::read_data() {
    m_pending_second_word = false;
    uint16_t val = 0;

    switch (m_code & 0x0F) {
        case 0x00: // VRAM Read
            val = (static_cast<uint16_t>(m_vram[m_addr & 0xFFFE]) << 8) |
                   m_vram[(m_addr & 0xFFFE) + 1];
            break;
        case 0x08: // CRAM Read
            val = m_cram[(m_addr >> 1) & 0x3F];
            break;
        case 0x04: // VSRAM Read
            if (((m_addr >> 1) & 0x3F) < 40) {
                val = m_vsram[(m_addr >> 1) & 0x3F];
            }
            break;
    }

    m_addr += m_regs[15];
    return val;
}

void GenesisVDP::write_data(uint16_t val) {
    m_pending_second_word = false;

    // Handle Pending DMA Fill (Triggered when first data word is written after a DMA Fill command)
    if (m_dma_fill_pending) {
        m_dma_fill_pending = false;
        uint16_t len = m_regs[19] | (static_cast<uint16_t>(m_regs[20]) << 8);
        if (len == 0) len = 0xFFFF;

        // First write goes to current address normally
        m_vram[m_addr] = (val >> 8) & 0xFF;
        m_vram[m_addr ^ 1] = val & 0xFF;

        uint8_t fill_byte = (val >> 8) & 0xFF;
        do {
            m_vram[m_addr ^ 1] = fill_byte;
            m_addr += m_regs[15];
        } while (--len > 0);
        return;
    }

    switch (m_code & 0x0F) {
        case 0x01: // VRAM Write
            if (m_addr & 1) {
                // Byte-swapped odd-address write quirk on Genesis VDP!
                m_vram[m_addr & 0xFFFE]     = val & 0xFF;
                m_vram[(m_addr & 0xFFFE)+1] = (val >> 8) & 0xFF;
            } else {
                m_vram[m_addr]     = (val >> 8) & 0xFF;
                m_vram[m_addr + 1] = val & 0xFF;
            }
            break;
        case 0x03: // CRAM Write
            m_cram[(m_addr >> 1) & 0x3F] = val & 0x0EEE;
            break;
        case 0x05: // VSRAM Write
            if (((m_addr >> 1) & 0x3F) < 40) {
                m_vsram[(m_addr >> 1) & 0x3F] = val & 0x07FF;
            }
            break;
    }

    m_addr += m_regs[15];
}

uint16_t GenesisVDP::read_control() {
    m_pending_second_word = false;
    // Bit 9: FIFO Empty (1), Bit 3: VBlank, Bit 2: HBlank
    uint16_t status = 0x3600;
    if (m_vblank_flag)  status |= (1 << 3);
    if (m_vint_pending) status |= (1 << 7);
    m_vint_pending = false;
    return status;
}

void GenesisVDP::write_control(uint16_t val, GenesisMMU& mmu) {
    // If bits 15-14 are 10 (0x8000 - 0xBFFF) and we aren't waiting on a second command word,
    // this is a VDP Register Write: 100R RRRR DDDD DDDD
    if (!m_pending_second_word && ((val & 0xC000) == 0x8000)) {
        uint8_t reg = (val >> 8) & 0x1F;
        if (reg < 24) {
            m_regs[reg] = val & 0xFF;
        }
        m_code = 0;
        return;
    }

    if (!m_pending_second_word) {
        m_first_word_latch = val;
        m_addr = (m_addr & 0xC000) | (val & 0x3FFF);
        m_code = (m_code & 0x3C) | ((val >> 14) & 0x03);
        m_pending_second_word = true;
    } else {
        m_pending_second_word = false;
        m_addr = (m_addr & 0x3FFF) | ((val & 0x0003) << 14);
        m_code = (m_code & 0x03) | ((val >> 2) & 0x3C);

        // Check if DMA is requested (Code bit 5 = 1 and Reg 1 bit 4 = 1)
        if ((m_code & 0x20) && (m_regs[1] & 0x10)) {
            uint8_t dma_mode = (m_regs[23] >> 6) & 0x03;
            if (dma_mode == 2) {
                // Mode 2 (10): VRAM Fill (waits for next Data Port write!)
                m_dma_fill_pending = true;
            } else {
                // Mode 0/1 (68k -> VDP) or Mode 3 (VRAM Copy)
                execute_vram_copy_or_68k_dma(mmu);
            }
        }
    }
}

void GenesisVDP::execute_vram_copy_or_68k_dma(GenesisMMU& mmu) {
    uint16_t len = m_regs[19] | (static_cast<uint16_t>(m_regs[20]) << 8);
    uint32_t count = (len == 0) ? 0x10000 : len;

    uint8_t dma_mode = (m_regs[23] >> 6) & 0x03;
    if (dma_mode == 3) {
        // VRAM-to-VRAM Byte Copy
        uint16_t src = m_regs[21] | (static_cast<uint16_t>(m_regs[22]) << 8);
        while (count-- > 0) {
            m_vram[m_addr ^ 1] = m_vram[src ^ 1];
            src++;
            m_addr += m_regs[15];
        }
    } else {
        // 68000 Bus to VDP DMA (Word Transfer)
        uint32_t src_words = m_regs[21] |
                            (static_cast<uint32_t>(m_regs[22]) << 8) |
                            (static_cast<uint32_t>(m_regs[23] & 0x7F) << 16);
        uint32_t src_addr = (src_words << 1) & 0xFFFFFF;

        while (count-- > 0) {
            uint16_t word = mmu.read16(src_addr);
            write_data(word);
            // DMA source address wraps within 128KB blocks on real hardware
            src_addr = (src_addr & 0xFE0000) | ((src_addr + 2) & 0x01FFFF);
        }
    }

    m_regs[19] = 0;
    m_regs[20] = 0;
}

uint16_t GenesisVDP::read_hv_counter() const {
    uint8_t v = (m_scanline > 0xEA) ? (m_scanline - 6) : m_scanline;
    uint8_t h = 0x00;
    return (static_cast<uint16_t>(v) << 8) | h;
}

bool GenesisVDP::poll_vint() {
    if (m_vint_pending && (m_regs[1] & 0x20)) {
        m_vint_pending = false;
        return true;
    }
    return false;
}

bool GenesisVDP::poll_hint() {
    if (m_hint_pending && (m_regs[0] & 0x10)) {
        m_hint_pending = false;
        return true;
    }
    return false;
}

bool GenesisVDP::step_scanline() {
    if (m_scanline < SCREEN_HEIGHT) {
        render_scanline(m_scanline);
        if (m_hint_counter == 0) {
            m_hint_counter = m_regs[10];
            m_hint_pending = true;
        } else {
            m_hint_counter--;
        }
    } else {
        m_hint_counter = m_regs[10];
    }

    m_scanline++;
    bool entered_vblank = false;

    if (m_scanline == SCREEN_HEIGHT) {
        m_vblank_flag = true;
        m_vint_pending = true;
        entered_vblank = true;
    } else if (m_scanline >= 262) {
        m_scanline = 0;
        m_vblank_flag = false;
    }

    return entered_vblank;
}

void GenesisVDP::render_scanline(int line) {
    uint8_t bg_color_idx = m_regs[7] & 0x3F;
    uint32_t backdrop = cram_to_argb8888(m_cram[bg_color_idx]);

    // If display is disabled (Reg 1 bit 6 == 0), fill with backdrop color
    if ((m_regs[1] & 0x40) == 0) {
        std::fill_n(&m_framebuffer[line * SCREEN_WIDTH], SCREEN_WIDTH, backdrop);
        return;
    }

    std::fill_n(&m_framebuffer[line * SCREEN_WIDTH], SCREEN_WIDTH, backdrop);

    // Back-to-Front Priority Painter:
    // 1. Low-priority Plane B -> Plane A -> Window -> Sprites
    render_plane(line, false, false);
    render_plane(line, true,  false);
    render_window(line, false);
    render_sprites(line, false);

    // 2. High-priority Plane B -> Plane A -> Window -> Sprites
    render_plane(line, false, true);
    render_plane(line, true,  true);
    render_window(line, true);
    render_sprites(line, true);
}

void GenesisVDP::render_plane(int line, bool is_plane_a, bool high_prio) {
    static const int plane_dims[4] = {32, 64, 32, 128};
    int plane_w = plane_dims[m_regs[16] & 0x03];
    int plane_h = plane_dims[(m_regs[16] >> 4) & 0x03];

    uint16_t nametable_base = is_plane_a
        ? ((m_regs[2] & 0x38) << 10)
        : ((m_regs[4] & 0x07) << 13);

    // Horizontal Scroll Lookup (Reg 11 Bits 0-1: 00=Full, 10=Cell, 11=Line)
    uint16_t hscroll_base = (m_regs[13] & 0x3F) << 10;
    uint8_t  hscroll_mode = m_regs[11] & 0x03;
    uint16_t hscroll_offset = 0;
    if (hscroll_mode == 2)      hscroll_offset = (line & ~7) * 4;
    else if (hscroll_mode == 3) hscroll_offset = line * 4;

    uint16_t hscroll_addr = (hscroll_base + hscroll_offset + (is_plane_a ? 0 : 2)) & 0xFFFE;
    int hscroll = (static_cast<uint16_t>(m_vram[hscroll_addr]) << 8) | m_vram[hscroll_addr + 1];
    hscroll &= 0x03FF;

    bool vscroll_per_2col = (m_regs[11] & 0x04) != 0;

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        int vs_idx = vscroll_per_2col ? (((x >> 4) * 2) + (is_plane_a ? 0 : 1)) : (is_plane_a ? 0 : 1);
        int vscroll = m_vsram[vs_idx % 40] & 0x03FF;

        int eff_y = (line + vscroll) & ((plane_h * 8) - 1);
        int eff_x = (x - hscroll)    & ((plane_w * 8) - 1);

        int tile_row = eff_y >> 3;
        int tile_col = eff_x >> 3;

        uint16_t entry_addr = (nametable_base + ((tile_row * plane_w + tile_col) * 2)) & 0xFFFE;
        uint16_t entry = (static_cast<uint16_t>(m_vram[entry_addr]) << 8) | m_vram[entry_addr + 1];

        bool prio = (entry & 0x8000) != 0;
        if (prio != high_prio) continue;

        uint8_t  pal    = (entry >> 13) & 0x03;
        bool     flip_v = (entry & 0x1000) != 0;
        bool     flip_h = (entry & 0x0800) != 0;
        uint16_t tile   = entry & 0x07FF;

        int fine_y = flip_v ? (7 - (eff_y & 7)) : (eff_y & 7);
        int fine_x = flip_h ? (7 - (eff_x & 7)) : (eff_x & 7);

        uint16_t pixel_addr = ((tile * 32) + (fine_y * 4) + (fine_x >> 1)) & 0xFFFF;
        uint8_t  byte = m_vram[pixel_addr];
        uint8_t  color_idx = (fine_x & 1) ? (byte & 0x0F) : (byte >> 4);

        if (color_idx != 0) {
            m_framebuffer[line * SCREEN_WIDTH + x] = cram_to_argb8888(m_cram[(pal * 16) + color_idx]);
        }
    }
}

void GenesisVDP::render_window(int line, bool high_prio) {
    uint8_t vp = m_regs[18];
    int win_v_pos = (vp & 0x1F) * 8;
    bool win_down = (vp & 0x80) != 0;

    bool in_v_window = win_down ? (line >= win_v_pos) : (line < win_v_pos);
    uint8_t hp = m_regs[17];
    int win_h_pos = (hp & 0x1F) * 16;
    bool win_right = (hp & 0x80) != 0;

    if (!in_v_window && win_h_pos == 0 && !win_right) return;

    bool is_h40 = (m_regs[12] & 0x01) != 0;
    int win_width_tiles = is_h40 ? 64 : 32;
    uint16_t win_base = is_h40 ? ((m_regs[3] & 0x3C) << 10) : ((m_regs[3] & 0x3E) << 10);

    int tile_row = line >> 3;
    int fine_y_base = line & 7;

    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        bool in_h_window = win_right ? (x >= win_h_pos) : (x < win_h_pos);
        if (!in_v_window && !in_h_window) continue;

        int tile_col = x >> 3;
        uint16_t entry_addr = (win_base + ((tile_row * win_width_tiles + tile_col) * 2)) & 0xFFFE;
        uint16_t entry = (static_cast<uint16_t>(m_vram[entry_addr]) << 8) | m_vram[entry_addr + 1];

        bool prio = (entry & 0x8000) != 0;
        if (prio != high_prio) continue;

        uint8_t  pal    = (entry >> 13) & 0x03;
        bool     flip_v = (entry & 0x1000) != 0;
        bool     flip_h = (entry & 0x0800) != 0;
        uint16_t tile   = entry & 0x07FF;

        int fine_y = flip_v ? (7 - fine_y_base) : fine_y_base;
        int fine_x = flip_h ? (7 - (x & 7)) : (x & 7);

        uint16_t pixel_addr = ((tile * 32) + (fine_y * 4) + (fine_x >> 1)) & 0xFFFF;
        uint8_t  byte = m_vram[pixel_addr];
        uint8_t  color_idx = (fine_x & 1) ? (byte & 0x0F) : (byte >> 4);

        if (color_idx != 0) {
            m_framebuffer[line * SCREEN_WIDTH + x] = cram_to_argb8888(m_cram[(pal * 16) + color_idx]);
        }
    }
}

void GenesisVDP::render_sprites(int line, bool high_prio) {
    bool is_h40 = (m_regs[12] & 0x01) != 0;
    uint16_t sat_base = is_h40 ? ((m_regs[5] & 0x7E) << 9) : ((m_regs[5] & 0x7F) << 9);

    // Traverse hardware sprite linked list first so we can draw back-to-front
    uint8_t order[80];
    int count = 0;
    uint8_t curr = 0;

    for (int step = 0; step < 80; ++step) {
        order[count++] = curr;
        uint16_t addr = (sat_base + curr * 8) & 0xFFFE;
        uint8_t next_link = m_vram[addr + 3] & 0x7F;
        if (next_link == 0 || next_link >= 80) break;
        curr = next_link;
    }

    // Render in reverse linked-list order so Sprite 0 has top priority
    for (int idx = count - 1; idx >= 0; --idx) {
        uint16_t addr = (sat_base + order[idx] * 8) & 0xFFFE;

        uint16_t word0 = (static_cast<uint16_t>(m_vram[addr]) << 8)     | m_vram[addr + 1];
        uint16_t word1 = (static_cast<uint16_t>(m_vram[addr + 2]) << 8) | m_vram[addr + 3];
        uint16_t word2 = (static_cast<uint16_t>(m_vram[addr + 4]) << 8) | m_vram[addr + 5];
        uint16_t word3 = (static_cast<uint16_t>(m_vram[addr + 6]) << 8) | m_vram[addr + 7];

        bool prio = (word2 & 0x8000) != 0;
        if (prio != high_prio) continue;

        int y_pos = (word0 & 0x03FF) - 128;
        int h_cells = ((word1 >> 8) & 0x03) + 1;
        int w_cells = ((word1 >> 10) & 0x03) + 1;
        int height = h_cells * 8;
        int width  = w_cells * 8;

        if (line < y_pos || line >= (y_pos + height)) continue;

        int x_pos = (word3 & 0x03FF) - 128;
        uint8_t  pal    = (word2 >> 13) & 0x03;
        bool     flip_v = (word2 & 0x1000) != 0;
        bool     flip_h = (word2 & 0x0800) != 0;
        uint16_t base_tile = word2 & 0x07FF;

        int rel_y = flip_v ? (height - 1 - (line - y_pos)) : (line - y_pos);
        int cell_y = rel_y >> 3;
        int fine_y = rel_y & 7;

        for (int px = 0; px < width; ++px) {
            int screen_x = x_pos + px;
            if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;

            int rel_x  = flip_h ? (width - 1 - px) : px;
            int cell_x = rel_x >> 3;
            int fine_x = rel_x & 7;

            // Genesis sprites are column-major! (Column 0 top-to-bottom, then Column 1...)
            uint16_t tile = (base_tile + (cell_x * h_cells) + cell_y) & 0x07FF;
            uint16_t pixel_addr = ((tile * 32) + (fine_y * 4) + (fine_x >> 1)) & 0xFFFF;
            uint8_t  byte = m_vram[pixel_addr];
            uint8_t  color_idx = (fine_x & 1) ? (byte & 0x0F) : (byte >> 4);

            if (color_idx != 0) {
                m_framebuffer[line * SCREEN_WIDTH + screen_x] = cram_to_argb8888(m_cram[(pal * 16) + color_idx]);
            }
        }
    }
}

void GenesisVDP::debug_update_tile_buffer() {
    for (int tile = 0; tile < 384; ++tile) {
        int tile_x = (tile % 16) * 8;
        int tile_y = (tile / 16) * 8;
        uint16_t base = (tile * 32) & 0xFFFF;

        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint8_t b = m_vram[(base + row * 4 + (col >> 1)) & 0xFFFF];
                uint8_t c_idx = (col & 1) ? (b & 0x0F) : (b >> 4);
                m_debug_tile_buffer[(tile_y + row) * 128 + (tile_x + col)] = cram_to_argb8888(m_cram[c_idx]);
            }
        }
    }
}