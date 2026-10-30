#pragma once
#include <cstdint>
#include <array>

class SNES_PPU {
public:
    static constexpr int SCREEN_WIDTH = 256;
    static constexpr int SCREEN_HEIGHT = 224;

    SNES_PPU();
    void reset();

    bool advance_cycles(int cpu_cycles);
    bool stepScanline();

    // B-Bus Register Access ($2100 - $213F)
    uint8_t readRegister(uint16_t addr);
    void writeRegister(uint16_t addr, uint8_t val);

    bool isInVBlank() const { return m_in_vblank; }
    uint16_t getCurrentScanline() const { return m_scanline; }
    const uint32_t* getFramebuffer() const { return m_framebuffer.data(); }

    void debug_update_tile_buffer();
    const uint32_t* get_debug_tile_buffer() const { return m_debug_tile_buffer.data(); }

private:
    void renderScanline(int line);
    void renderBackgroundLayer(int line, int bg_index, int bpp, int target_priority);
    void renderSprites(int line, int target_priority);
    uint32_t bgr555ToArgb8888(uint16_t bgr555) const;
    uint16_t getVramAddressIncrement() const;

    // PPU Internal Memory
    std::array<uint8_t, 0x10000> m_vram{};  // 64 KB Video RAM
    std::array<uint8_t, 0x200>   m_cgram{}; // 512 Bytes Palette RAM (256 colors)
    std::array<uint8_t, 0x220>   m_oam{};   // 544 Bytes Object Attribute Memory

    // Output Framebuffers (ARGB8888)
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
    bool    m_bg3_priority = false; // $2105 bit 3
    uint8_t m_bg_tile_size = 0;     // $2105 bits 4-7 (1 = 16x16 tiles)
    uint8_t m_main_screen_mask = 0; // $212C (TM)
    uint8_t m_sub_screen_mask = 0;  // $212D (TS)

    // Color Math & Fixed Color Backdrop ($2130 - $2132)
    uint8_t m_cgwsel = 0;
    uint8_t m_cgadsub = 0;
    uint8_t m_fixed_r = 0;
    uint8_t m_fixed_g = 0;
    uint8_t m_fixed_b = 0;

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
    uint8_t  m_bg_sc[4]{};
    uint16_t m_bg_nba[2]{};
    uint16_t m_bg_hofs[4]{};
    uint16_t m_bg_vofs[4]{};
    uint8_t  m_bg_scroll_prev = 0;

    // Sprite Configuration ($2101)
    uint8_t m_obsel = 0;
};