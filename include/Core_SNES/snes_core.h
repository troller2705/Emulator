#pragma once
#include "../Core_Shared/IEmulatorCore.h"
#include "snes_mmu.h"
#include "snes_cpu.h"
#include <array>

class SNESCore : public IEmulatorCore {
public:
    SNESCore();
    ~SNESCore() override = default;

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void run_frame() override;
    void reset() override;
    void set_input(uint32_t input_state) override;

    const void* get_video_buffer() const override { return m_framebuffer.data(); }
    const float* get_audio_buffer() const override { return nullptr; }
    size_t get_audio_sample_count() const override { return 0; }

    int get_video_width() const override { return 256; }
    int get_video_height() const override { return 224; }

    void step_instruction() override;
    CPUState get_cpu_state() const override;
    uint8_t debug_read_memory(uint16_t address) override { return m_mmu.read8(address); }
    void debug_write_memory(uint16_t address, uint8_t value) override { m_mmu.write8(address, value); }
    void debug_update_tile_buffer() override {}
    const uint32_t* debug_get_tile_buffer() const override { return nullptr; }

private:
    SNES_MMU m_mmu;
    SNES_CPU m_cpu;
    std::array<uint32_t, 256 * 224> m_framebuffer{};
};