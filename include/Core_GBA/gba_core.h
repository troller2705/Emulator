#pragma once
#include "../Core_Shared/IEmulatorCore.h"
#include "gba_mmu.h"
#include "gba_cpu.h"
#include "gba_ppu.h"
#include "gba_timer.h"
#include "gba_apu.h"

class GBACore : public IEmulatorCore {
public:
    GBACore();
    ~GBACore() override = default;

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void run_frame() override;
    void reset() override;
    void set_input(uint32_t input_state) override;

    const void* get_video_buffer() const override { return m_ppu.get_framebuffer(); }
    const float* get_audio_buffer() const override { return m_apu.get_audio_buffer(); }
    size_t get_audio_sample_count() const override { return m_apu.get_audio_sample_count(); }

    int get_video_width() const override { return 240; }
    int get_video_height() const override { return 160; }

    void load_battery(const std::string& path) override { m_mmu.load_battery(path); }
    void save_battery(const std::string& path) override { m_mmu.save_battery(path); }

    void step_instruction() override;
    CPUState get_cpu_state() const override { return m_cpu.get_state(); }
    uint8_t debug_read_memory(uint16_t address) override { return m_mmu.read8(0x02000000 + address); }
    void debug_write_memory(uint16_t address, uint8_t value) override { m_mmu.write8(0x02000000 + address, value); }
    void debug_update_tile_buffer() override { m_ppu.debug_update_tile_buffer(); }
    const uint32_t* debug_get_tile_buffer() const override { return m_ppu.get_debug_tile_buffer(); }

private:
    GBAMMU   m_mmu;
    GBACPU   m_cpu;
    GBAPPU   m_ppu;
    GBATimer m_timer;
    GBAAPU   m_apu;
};