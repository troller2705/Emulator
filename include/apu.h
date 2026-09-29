#pragma once
#include <array>
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

    // --- CHANNEL 1 REGISTERS ---
    uint8_t m_nr10 = 0x80; // Sweep
    uint8_t m_nr11 = 0xBF; // Duty / Length
    uint8_t m_nr12 = 0xF3; // Volume Envelope
    uint8_t m_nr13 = 0xFF; // Frequency (Low)
    uint8_t m_nr14 = 0xBF; // Trigger / Frequency (High)

    // --- CHANNEL 1 STATE ---
    bool m_ch1_enabled = false;
    int m_ch1_timer = 0;
    int m_ch1_duty_step = 0;
    int m_ch1_length_timer = 0;
    int m_ch1_envelope_timer = 0;
    int m_ch1_current_volume = 0;

    // --- FREQUENCY SWEEP STATE ---
    int m_ch1_sweep_timer = 0;
    uint16_t m_ch1_shadow_frequency = 0;
    bool m_ch1_sweep_enabled = false;

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

    // --- CHANNEL 3 REGISTERS & RAM ---
    uint8_t m_nr30 = 0x7F; // DAC Power
    uint8_t m_nr31 = 0xFF; // Length Load
    uint8_t m_nr32 = 0x9F; // Volume Code
    uint8_t m_nr33 = 0xFF; // Frequency (Low)
    uint8_t m_nr34 = 0xBF; // Trigger / Frequency (High)

    std::array<uint8_t, 16> m_wave_ram{};

    // --- CHANNEL 3 STATE ---
    bool m_ch3_enabled = false;
    int m_ch3_timer = 0;
    int m_ch3_position = 0; // 32 nibbles (0 to 31)
    int m_ch3_length_timer = 0;

    // --- CHANNEL 4 REGISTERS ---
    uint8_t m_nr41 = 0xFF; // Length
    uint8_t m_nr42 = 0x00; // Volume Envelope
    uint8_t m_nr43 = 0x00; // Polynomial Counter (Frequency & LFSR Width)
    uint8_t m_nr44 = 0xBF; // Trigger / Length Enable

    // --- CHANNEL 4 STATE ---
    bool m_ch4_enabled = false;
    int m_ch4_timer = 0;
    int m_ch4_length_timer = 0;
    int m_ch4_envelope_timer = 0;
    int m_ch4_current_volume = 0;

    // The LFSR generates the random noise. It boots up filled with 1s.
    uint16_t m_lfsr = 0x7FFF;

    float get_channel_1_sample();
    float get_channel_2_sample();
    float get_channel_3_sample();
    float get_channel_4_sample();
};