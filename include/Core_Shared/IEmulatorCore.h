#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <array>

struct CPUState {
    // GB / GBC 8-bit & 16-bit Registers
    uint8_t A = 0, F = 0, B = 0, C = 0, D = 0, E = 0, H = 0, L = 0;
    uint16_t PC = 0, SP = 0;

    // GBA 32-bit ARM7TDMI Registers
    bool is_gba = false;
    std::array<uint32_t, 16> gba_regs{};
    uint32_t cpsr = 0;

    // SNES 16-bit 65C816 Registers
    bool is_snes = false;
    uint16_t snes_A = 0, snes_X = 0, snes_Y = 0, snes_D = 0;
    uint8_t  snes_PB = 0, snes_DB = 0, snes_P = 0;
    bool flag_m = false, flag_x = false, flag_e = false;
};

class IEmulatorCore {
public:
    virtual ~IEmulatorCore() = default;

    virtual bool load_rom(const std::vector<uint8_t>& rom_data) = 0;
    virtual void run_frame() = 0;
    virtual void reset() {}
    virtual void set_input(uint32_t input_state) = 0;

    virtual const void* get_video_buffer() const = 0;
    virtual const float* get_audio_buffer() const = 0;
    virtual size_t get_audio_sample_count() const = 0;

    virtual int get_video_width() const { return 160; }
    virtual int get_video_height() const { return 144; }

    virtual void load_battery(const std::string& path) {}
    virtual void save_battery(const std::string& path) {}

    // Debugging Interface
    virtual void step_instruction() = 0;
    virtual CPUState get_cpu_state() const = 0;

    virtual uint8_t debug_read_memory(uint16_t address) = 0;
    virtual void debug_write_memory(uint16_t address, uint8_t value) = 0;

    virtual void debug_update_tile_buffer() = 0;
    virtual const uint32_t* debug_get_tile_buffer() const = 0;
};