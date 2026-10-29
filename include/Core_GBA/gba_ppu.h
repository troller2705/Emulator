#pragma once
#include <array>
#include <cstdint>

class GBAMMU;

class GBAPPU {
public:
    explicit GBAPPU(GBAMMU& mmu);

    void reset();
    void render_scanline(int ly);
    void debug_update_tile_buffer();

    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }
    const uint32_t* get_debug_tile_buffer() const { return m_debug_tile_buffer.data(); }

private:
    GBAMMU& m_mmu;
    std::array<uint32_t, 240 * 160> m_framebuffer{};
    std::array<uint32_t, 128 * 192> m_debug_tile_buffer{};

    void render_bg_regular(int ly, int bg, uint16_t bgcnt);
    void render_bg_affine(int ly, int bg, uint16_t bgcnt);
    void render_sprites(int ly, uint16_t dispcnt, int target_prio);
    void apply_brightness_effect(int ly);
};