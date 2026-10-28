#pragma once
#include <deque>
#include <vector>
#include <cstdint>

class GBAMMU;

class GBAAPU {
public:
    explicit GBAAPU(GBAMMU& mmu);

    void reset();
    void tick(int cycles);

    void write_register16(uint32_t offset, uint16_t val);
    void write_fifo32(uint32_t offset, uint32_t val);
    void on_timer_overflow(int timer_idx);

    const float* get_audio_buffer() const { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const { return m_audio_buffer.size(); }
    void clear_buffer() { m_audio_buffer.clear(); }

private:
    GBAMMU& m_mmu;
    std::vector<float> m_audio_buffer;

    std::deque<int8_t> m_fifo_a;
    std::deque<int8_t> m_fifo_b;
    int8_t m_latch_a = 0;
    int8_t m_latch_b = 0;
    uint16_t m_soundcnt_h = 0;
    uint16_t m_soundcnt_x = 0;

    int m_sample_counter = 0;
    static constexpr int CYCLES_PER_SAMPLE = 380; // 16.78 MHz / 44,100 Hz
};