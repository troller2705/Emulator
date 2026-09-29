#pragma once
#include <cstdint>
#include <vector>

class APU {
public:
    APU();
    
    void tick(int cycles);
    
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);

    // Front-end retrieval
    const float* get_audio_buffer() const { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const { return m_audio_buffer.size(); }
    void clear_buffer() { m_audio_buffer.clear(); }

private:
    std::vector<float> m_audio_buffer;
    
    // CPU to Audio downsampling (4,194,304 Hz / 44,100 Hz ≈ 95 cycles per sample)
    int m_downsample_counter = 0;
    const int CYCLES_PER_SAMPLE = 95;

    // Global Registers
    uint8_t m_nr50 = 0x77; // Master Volume
    uint8_t m_nr51 = 0xF3; // Panning
    uint8_t m_nr52 = 0xF1; // Sound On/Off

    // Channel 2 Registers (Square Wave)
    uint8_t m_nr21 = 0; // Duty Cycle / Length
    uint8_t m_nr22 = 0; // Volume Envelope
    uint8_t m_nr23 = 0; // Frequency (Lower 8 bits)
    uint8_t m_nr24 = 0; // Trigger / Frequency (Upper 3 bits)

    // Channel 2 Internal State
    bool m_ch2_enabled = false;
    int m_ch2_timer = 0;
    int m_ch2_duty_step = 0;

    // --- FRAME SEQUENCER ---
    int m_frame_sequencer_timer = 0;
    uint8_t m_frame_sequencer_step = 0;

    // --- CHANNEL 2 ACTIVE STATE ---
    int m_ch2_length_timer = 0;
    int m_ch2_envelope_timer = 0;
    int m_ch2_current_volume = 0;
    
    float get_channel_2_sample();
};