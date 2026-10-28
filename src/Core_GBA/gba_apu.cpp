#include "../../include/Core_GBA/gba_apu.h"
#include "../../include/Core_GBA/gba_mmu.h"

GBAAPU::GBAAPU(GBAMMU& mmu) : m_mmu(mmu) {
    m_audio_buffer.reserve(1024);
}

void GBAAPU::reset() {
    m_fifo_a.clear();
    m_fifo_b.clear();
    m_latch_a = m_latch_b = 0;
    m_soundcnt_h = m_soundcnt_x = 0;
    m_sample_counter = 0;
    m_audio_buffer.clear();
}

void GBAAPU::write_register16(uint32_t offset, uint16_t val) {
    if (offset == 0x082) { // SOUNDCNT_H
        m_soundcnt_h = val;
        if (val & (1 << 11)) m_fifo_a.clear();
        if (val & (1 << 15)) m_fifo_b.clear();
    } else if (offset == 0x084) { // SOUNDCNT_X
        m_soundcnt_x = val;
    }
}

void GBAAPU::write_fifo32(uint32_t offset, uint32_t val) {
    auto& fifo = (offset == 0x0A0) ? m_fifo_a : m_fifo_b;
    for (int i = 0; i < 4; ++i) {
        if (fifo.size() < 32) {
            fifo.push_back(static_cast<int8_t>((val >> (i * 8)) & 0xFF));
        }
    }
}

void GBAAPU::on_timer_overflow(int timer_idx) {
    if (!(m_soundcnt_x & 0x80)) return;

    int timer_a = (m_soundcnt_h >> 10) & 1;
    int timer_b = (m_soundcnt_h >> 14) & 1;

    if (timer_a == timer_idx) {
        if (!m_fifo_a.empty()) {
            m_latch_a = m_fifo_a.front();
            m_fifo_a.pop_front();
        }
        if (m_fifo_a.size() <= 16) m_mmu.trigger_fifo_dma(0);
    }

    if (timer_b == timer_idx) {
        if (!m_fifo_b.empty()) {
            m_latch_b = m_fifo_b.front();
            m_fifo_b.pop_front();
        }
        if (m_fifo_b.size() <= 16) m_mmu.trigger_fifo_dma(1);
    }
}

void GBAAPU::tick(int cycles) {
    m_sample_counter += cycles;
    while (m_sample_counter >= CYCLES_PER_SAMPLE) {
        m_sample_counter -= CYCLES_PER_SAMPLE;
        float a = (m_latch_a / 128.0f) * ((m_soundcnt_h & (1 << 2)) ? 1.0f : 0.5f);
        float b = (m_latch_b / 128.0f) * ((m_soundcnt_h & (1 << 3)) ? 1.0f : 0.5f);
        m_audio_buffer.push_back((a + b) * 0.25f);
    }
}