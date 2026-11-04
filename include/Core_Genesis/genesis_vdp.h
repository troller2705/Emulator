#pragma once
#include <cstdint>
#include <array>

class GenesisMMU;

class GenesisVDP {
public:
    static constexpr int SCREEN_WIDTH  = 320;
    static constexpr int SCREEN_HEIGHT = 224;

    GenesisVDP();
    void reset();

    bool step_scanline();

    // 16-bit VDP Port Interface ($C00000 - $C00009)
    uint16_t read_data();
    void     write_data(uint16_t val);
    uint16_t read_control();
    void     write_control(uint16_t val, GenesisMMU& mmu);
    uint16_t read_hv_counter() const;

    bool poll_vint();
    bool poll_hint();

    const uint32_t* get_framebuffer() const { return m_framebuffer.data(); }
    void debug_update_tile_buffer();
    const uint32_t* get_debug_tile_buffer() const { return m_debug_tile_buffer.data(); }

private:
    std::array<uint8_t,  0x10000> m_vram{};  // 64 KB VRAM
    std::array<uint16_t, 64>      m_cram{};  // 64-entry 9-bit Color RAM
    std::array<uint16_t, 40>      m_vsram{}; // 40-entry Vertical Scroll RAM
    std::array<uint8_t,  24>      m_regs{};  // VDP Registers #00 - #23

    std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> m_framebuffer{};
    std::array<uint32_t, 128 * 192>                    m_debug_tile_buffer{};

    // Command Word Latch State
    bool     m_pending_second_word = false;
    uint16_t m_first_word_latch = 0;
    uint8_t  m_code = 0;     // 6-bit transfer target/DMA code
    uint16_t m_addr = 0;     // 16-bit target address

    // Timing & Interrupts
    uint16_t m_scanline = 0;
    uint8_t  m_hint_counter = 0;
    bool     m_vblank_flag = false;
    bool     m_vint_pending = false;
    bool     m_hint_pending = false;
    bool     m_dma_fill_pending = false;

    void execute_vram_copy_or_68k_dma(GenesisMMU& mmu);
    void render_scanline(int line);
    void render_plane(int line, bool is_plane_a, bool high_prio);
    void render_window(int line, bool high_prio);
    void render_sprites(int line, bool high_prio);
    uint32_t cram_to_argb8888(uint16_t cram_val) const;
};