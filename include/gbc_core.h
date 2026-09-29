#pragma once
#include <cstdint>
#include <vector>
#include "mmu.h"
#include "cpu.h"
#include "timer.h"
#include "IEmulatorCore.h"
#include "ppu.h"

class GBCCore : public IEmulatorCore {
private:
    MMU m_mmu;
    CPU m_cpu;
    Timer m_timer;

    std::string m_current_save_path;

public:
    GBCCore();
    ~GBCCore() override; // Declare destructor here, implement in .cpp

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void reset() override {} // Stub for now
    void run_frame() override;

    // 3. Match IEmulatorCore exactly
    const void* get_video_buffer() const override;
    int get_video_width() const override { return 160; }
    int get_video_height() const override { return 144; }

    const float* get_audio_buffer() const override;
    size_t get_audio_sample_count() const override;

    // 4. Match IEmulatorCore exactly (uint32_t instead of uint8_t)
    void set_input(uint32_t input_state) override;

    void load_battery(const std::string& path) override { m_mmu.load_battery(path); }
    void save_battery(const std::string& path) override { m_mmu.save_battery(path); }

    void step_instruction() override;
    CPUState get_cpu_state() const override;

    uint8_t debug_read_memory(uint16_t address) override;
    void debug_write_memory(uint16_t address, uint8_t value) override;
};