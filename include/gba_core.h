#pragma once
#include "IEmulatorCore.h"
#include <array>
#include <vector>
#include <string>
#include <cstdint>

class GBACore : public IEmulatorCore {
public:
    GBACore();
    ~GBACore() override = default;

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void run_frame() override;
    void set_input(uint32_t input_state) override;

    const void* get_video_buffer() const override { return m_framebuffer.data(); }
    const float* get_audio_buffer() const override { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const override { return m_audio_buffer.size(); }

    int get_video_width() const override { return 240; }
    int get_video_height() const override { return 160; }

    void load_battery(const std::string& path) override;
    void save_battery(const std::string& path) override;

    // Debugger Hooks
    void step_instruction() override;
    CPUState get_cpu_state() const override;
    uint8_t debug_read_memory(uint16_t address) override;
    void debug_write_memory(uint16_t address, uint8_t value) override;
    void debug_update_tile_buffer() override;
    const uint32_t* debug_get_tile_buffer() const override { return m_debug_tile_buffer.data(); }

private:
    // ARM7TDMI Registers (R0-R12, R13=SP, R14=LR, R15=PC) & CPSR
    std::array<uint32_t, 16> m_regs{};
    uint32_t m_cpsr = 0x0000001F; // System Mode

    // GBA Memory Domains
    std::array<uint8_t, 0x04000>  m_bios{};    // 16 KB BIOS (0x00000000)
    std::array<uint8_t, 0x40000>  m_ewram{};   // 256 KB Work RAM (0x02000000)
    std::array<uint8_t, 0x08000>  m_iwram{};   // 32 KB Internal RAM (0x03000000)
    std::array<uint8_t, 0x00400>  m_io{};      // 1 KB I/O Registers (0x04000000)
    std::array<uint8_t, 0x00400>  m_pal_ram{}; // 1 KB Palette RAM (0x05000000)
    std::array<uint8_t, 0x18000>  m_vram{};    // 96 KB VRAM (0x06000000)
    std::array<uint8_t, 0x00400>  m_oam{};     // 1 KB OAM (0x07000000)
    std::vector<uint8_t>          m_rom;       // Up to 32 MB GamePak (0x08000000)
    std::array<uint8_t, 0x10000>  m_sram{};    // 64 KB SRAM (0x0E000000)

    // Video & Debug Buffers
    std::array<uint32_t, 240 * 160> m_framebuffer{};
    std::array<uint32_t, 128 * 192> m_debug_tile_buffer{};
    std::vector<float>              m_audio_buffer;

    uint16_t m_keyinput = 0x03FF; // Active-low 10-button register (0x04000130)

    // Bus Access Helpers
    uint8_t  read8(uint32_t addr) const;
    uint16_t read16(uint32_t addr) const;
    uint32_t read32(uint32_t addr) const;
    void     write8(uint32_t addr, uint8_t val);
    void     write16(uint32_t addr, uint16_t val);
    void     write32(uint32_t addr, uint32_t val);

    void step_arm();
    void step_thumb();
    void render_frame();
    static uint32_t bgr555_to_argb8888(uint16_t bgr);
};