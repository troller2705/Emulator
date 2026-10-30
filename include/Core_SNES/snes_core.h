#pragma once
#include "../Core_Shared/IEmulatorCore.h"
#include "snes_mmu.h"
#include "snes_cpu.h"

class SNESCore : public IEmulatorCore {
public:
    SNESCore();
    ~SNESCore() override = default;

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void run_frame() override;
    void reset() override;
    void set_input(uint32_t input_state) override;

    const void* get_video_buffer() const override { return m_mmu.get_ppu().getFramebuffer(); }
    const float* get_audio_buffer() const override { return m_mmu.get_apu().get_audio_buffer(); }
    size_t get_audio_sample_count() const override { return m_mmu.get_apu().get_audio_sample_count(); }

    int get_video_width() const override { return SNES_PPU::SCREEN_WIDTH; }
    int get_video_height() const override { return SNES_PPU::SCREEN_HEIGHT; }

    void load_battery(const std::string& path) override { m_mmu.load_battery(path); }
    void save_battery(const std::string& path) override { m_mmu.save_battery(path); }

    void step_instruction() override;
    CPUState get_cpu_state() const override;
    uint8_t debug_read_memory(uint16_t address) override { return m_mmu.read8(address); }
    void debug_write_memory(uint16_t address, uint8_t value) override { m_mmu.write8(address, value); }
    void debug_update_tile_buffer() override { m_mmu.get_ppu().debug_update_tile_buffer(); }
    const uint32_t* debug_get_tile_buffer() const override { return m_mmu.get_ppu().get_debug_tile_buffer(); }

private:
    SNES_MMU m_mmu;
    SNES_CPU m_cpu;
};