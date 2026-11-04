#pragma once
#include "../Core_Shared/IEmulatorCore.h"
#include "genesis_mmu.h"
#include "genesis_cpu.h"

class GenesisCore : public IEmulatorCore {
public:
    GenesisCore();
    ~GenesisCore() override = default;

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void run_frame() override;
    void reset() override;
    void set_input(uint32_t input_state) override;

    const void* get_video_buffer() const override { return m_mmu.get_vdp().get_framebuffer(); }
    const float* get_audio_buffer() const override { return m_audio_stub.data(); }
    size_t get_audio_sample_count() const override { return 0; }
    int get_video_width() const override { return GenesisVDP::SCREEN_WIDTH; }
    int get_video_height() const override { return GenesisVDP::SCREEN_HEIGHT; }

    void load_battery(const std::string& path) override { m_mmu.load_battery(path); }
    void save_battery(const std::string& path) override { m_mmu.save_battery(path); }

    void step_instruction() override;
    CPUState get_cpu_state() const override;
    uint8_t debug_read_memory(uint16_t address) override { return m_mmu.read8(0xFF0000 | address); }
    void debug_write_memory(uint16_t address, uint8_t value) override { m_mmu.write8(0xFF0000 | address, value); }
    void debug_update_tile_buffer() override { m_mmu.get_vdp().debug_update_tile_buffer(); }
    const uint32_t* debug_get_tile_buffer() const override { return m_mmu.get_vdp().get_debug_tile_buffer(); }
    bool uses_nintendo_layout() const override { return false; }

private:
    GenesisMMU m_mmu;
    GenesisCPU m_cpu;
    std::vector<float> m_audio_stub;
};