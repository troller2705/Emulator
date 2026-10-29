#pragma once
#include <deque>
#include <vector>
#include <array>
#include <cstdint>

class GBAMMU;

class GBAAPU {
public:
    explicit GBAAPU(GBAMMU& mmu);

    void reset();
    void tick(int cycles);

    void write_register8(uint32_t offset, uint8_t val);
    void write_fifo32(uint32_t offset, uint32_t val);
    void on_timer_overflow(int timer_idx);

    const float* get_audio_buffer() const { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const { return m_audio_buffer.size(); }
    void clear_buffer() { m_audio_buffer.clear(); }

private:
    GBAMMU& m_mmu;
    std::vector<float> m_audio_buffer;

    // --- DirectSound FIFOs ---
    std::deque<int8_t> m_fifo_a, m_fifo_b;
    int8_t m_latch_a = 0, m_latch_b = 0;
    uint16_t m_soundcnt_h = 0, m_soundcnt_x = 0;

    // --- PSG Channels (Mapped to GBC addresses internally) ---
    uint8_t m_nr10 = 0, m_nr11 = 0, m_nr12 = 0, m_nr13 = 0, m_nr14 = 0;
    uint8_t m_nr21 = 0, m_nr22 = 0, m_nr23 = 0, m_nr24 = 0;
    uint8_t m_nr30 = 0, m_nr31 = 0, m_nr32 = 0, m_nr33 = 0, m_nr34 = 0;
    uint8_t m_nr41 = 0, m_nr42 = 0, m_nr43 = 0, m_nr44 = 0;
    uint8_t m_nr50 = 0, m_nr51 = 0, m_nr52 = 0;
    std::array<uint8_t, 16> m_wave_ram{};

    bool m_ch1_enabled = false, m_ch2_enabled = false, m_ch3_enabled = false, m_ch4_enabled = false;
    int m_ch1_timer = 0, m_ch2_timer = 0, m_ch3_timer = 0, m_ch4_timer = 0;
    int m_ch1_duty = 0,  m_ch2_duty = 0,  m_ch3_pos = 0;
    float m_ch1_vol = 0, m_ch2_vol = 0, m_ch4_vol = 0;
    int m_ch1_env = 0, m_ch2_env = 0, m_ch4_env = 0;
    int m_ch1_len = 0, m_ch2_len = 0, m_ch3_len = 0, m_ch4_len = 0;
    int m_fs_timer = 0, m_fs_step = 0;
    uint16_t m_lfsr = 0x7FFF;

    float get_psg1(); float get_psg2(); float get_psg3(); float get_psg4();

    int m_sample_counter = 0;
    static constexpr int CYCLES_PER_SAMPLE = 380; // 16.78 MHz / 44,100 Hz
};