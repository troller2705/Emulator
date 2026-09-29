#pragma once
#include <cstdint>
#include <vector>
#include <string>

class IEmulatorCore {
public:
    // A virtual destructor ensures derived hardware cores clean up their specific memory
    virtual ~IEmulatorCore() = default;

    // --- System Lifecycle ---
    virtual bool load_rom(const std::vector<uint8_t>& rom_data) = 0;
    virtual void reset() = 0;

    // --- Execution ---
    // Emulates exactly one frame (e.g., ~70224 cycles for GB, ~29780 for NES)
    virtual void run_frame() = 0;

    // --- Video Output ---
    // Returning const void* allows SDL to cast the buffer to whatever pixel format the system uses
    virtual const void* get_video_buffer() const = 0;
    virtual int get_video_width() const = 0;
    virtual int get_video_height() const = 0;

    // --- Audio Output (Future-proofing for the APU) ---
    // Standardizing on floating-point PCM audio for the front-end mixer
    virtual const float* get_audio_buffer() const = 0;
    virtual size_t get_audio_sample_count() const = 0;

    // --- Input Handling ---
    // Using uint32_t provides up to 32 mapped buttons (plenty for GBA/SNES later)
    virtual void set_input(uint32_t input_state) = 0;

    // --- File I/O (SRAM / EEPROM / Flash) ---
    virtual void load_battery(const std::string& path) = 0;
    virtual void save_battery(const std::string& path) = 0;
};