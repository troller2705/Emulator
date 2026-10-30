#pragma once
#include <cstdint>
#include <array>

class SNES_PPU {
public:
    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 224;

    SNES_PPU();
    void reset();

    // Advance PPU clock by CPU cycles; returns true on the exact transition into VBlank (scanline 225)
    bool advance_cycles(int cpu_cycles);

    // Advance PPU by one scanline; returns true when entering VBlank (scanline 225)
    bool stepScanline();

    // B-Bus Register Access ($2100 - $213F)
    uint8_t readRegister(uint16_t addr);
    void writeRegister(uint16_t addr, uint8_t val);

    // State getters for CPU / MMU ($4210 / $4212)
    bool isInVBlank() const { return m_in_vblank; }
    uint16_t getCurrentScanline() const { return m_scanline; }
    const uint32_t* getFramebuffer() const { return m_framebuffer.data(); }

    // ImGui VRAM Tile Viewer Support (128x192 ARGB8888)
    void debug_update_tile_buffer();
    const uint32_t* get_debug_tile_buffer() const { return m_debug_tile_buffer.data(); }

private:
    void renderScanline(int line);
    void renderBackgroundLayer(int line, int bg_index, int bpp);
    void renderSprites(int line);
    uint32_t bgr555ToArgb8888(uint16_t bgr555) const;
    uint16_t getVramAddressIncrement() const;

    // PPU Internal Memory
    std::array<uint8_t, 0x10000> m_vram{};  // 64 KB Video RAM
    std::array<uint8_t, 0x200>   m_cgram{}; // 512 Bytes Palette RAM (256 colors)
    std::array<uint8_t, 0x220>   m_oam{};   // 544 Bytes Object Attribute Memory

    // Output Framebuffers (ARGB8888 to match SDL_PIXELFORMAT_ARGB8888)
    std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> m_framebuffer{};
    std::array<uint32_t, 128 * 192> m_debug_tile_buffer{};

    // Timing State
    int m_cycle_counter = 0;
    uint16_t m_scanline = 0;
    bool m_in_vblank = false;

    // Display & Control Registers
    bool m_forced_blank = true;
    uint8_t m_brightness = 0x0F;
    uint8_t m_bg_mode = 0;
    uint8_t m_main_screen_mask = 0; // $212C (TM)
    uint8_t m_sub_screen_mask = 0;  // $212D (TS)

    // VRAM Addressing ($2115 - $2119)
    uint8_t  m_vmain = 0;
    uint16_t m_vram_addr = 0;
    uint16_t m_vram_prefetch = 0;

    // CGRAM Addressing ($2121 - $2122)
    uint8_t m_cgram_addr = 0;
    uint8_t m_cgram_latch = 0;
    bool    m_cgram_flipflop = false;

    // OAM Addressing ($2102 - $2104)
    uint16_t m_oam_addr = 0;
    uint16_t m_oam_reload = 0;
    uint8_t  m_oam_latch = 0;
    bool     m_oam_flipflop = false;

    // Background Configuration ($2107 - $2114)
    uint8_t  m_bg_sc[4]{};     // Tilemap base and size for BG1-BG4
    uint16_t m_bg_nba[2]{};    // Tile character data base for BG1/2 and BG3/4
    uint16_t m_bg_hofs[4]{};   // Horizontal scroll offsets
    uint16_t m_bg_vofs[4]{};   // Vertical scroll offsets
    uint8_t  m_bg_scroll_prev = 0;

    // Sprite Configuration ($2101)
    uint8_t m_obsel = 0;
};