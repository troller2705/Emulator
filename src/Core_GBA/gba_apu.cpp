#include "../../include/Core_GBA/gba_apu.h"
#include "../../include/Core_GBA/gba_mmu.h"

GBAAPU::GBAAPU(GBAMMU& mmu) : m_mmu(mmu) {
    m_audio_buffer.reserve(1024);
}

void GBAAPU::reset() {
    m_fifo_a.clear(); m_fifo_b.clear();
    m_latch_a = m_latch_b = 0;
    m_soundcnt_h = m_soundcnt_x = 0;
    m_sample_counter = 0;
    m_audio_buffer.clear();
}

void GBAAPU::write_register8(uint32_t offset, uint8_t val) {
    switch (offset) {
        // PSG Channel 1
        case 0x60: m_nr10 = val; break;
        case 0x62: m_nr11 = val; m_ch1_len = 64 - (val & 0x3F); break;
        case 0x63: m_nr12 = val; break;
        case 0x64: m_nr13 = val; break;
        case 0x65: m_nr14 = val;
                   if (val & 0x80) { m_ch1_enabled = true; if(m_ch1_len == 0) m_ch1_len = 64; m_ch1_vol = (m_nr12 >> 4); m_ch1_env = m_nr12 & 7; } break;
        // PSG Channel 2
        case 0x68: m_nr21 = val; m_ch2_len = 64 - (val & 0x3F); break;
        case 0x69: m_nr22 = val; break;
        case 0x6C: m_nr23 = val; break;
        case 0x6D: m_nr24 = val;
                   if (val & 0x80) { m_ch2_enabled = true; if(m_ch2_len == 0) m_ch2_len = 64; m_ch2_vol = (m_nr22 >> 4); m_ch2_env = m_nr22 & 7; } break;
        // PSG Channel 3
        case 0x70: m_nr30 = val; break;
        case 0x72: m_nr31 = val; m_ch3_len = 256 - val; break;
        case 0x73: m_nr32 = val; break;
        case 0x74: m_nr33 = val; break;
        case 0x75: m_nr34 = val;
                   if (val & 0x80) { m_ch3_enabled = true; m_ch3_pos = 0; if(m_ch3_len == 0) m_ch3_len = 256; } break;
        // PSG Channel 4
        case 0x78: m_nr41 = val; m_ch4_len = 64 - (val & 0x3F); break;
        case 0x79: m_nr42 = val; break;
        case 0x7C: m_nr43 = val; break;
        case 0x7D: m_nr44 = val;
                   if (val & 0x80) { m_ch4_enabled = true; if(m_ch4_len == 0) m_ch4_len = 64; m_ch4_vol = (m_nr42 >> 4); m_ch4_env = m_nr42 & 7; m_lfsr = 0x7FFF; } break;
        // PSG Master
        case 0x80: m_nr50 = val; break;
        case 0x81: m_nr51 = val; break;
        // DirectSound
        case 0x82: m_soundcnt_h = (m_soundcnt_h & 0xFF00) | val; break;
        case 0x83: m_soundcnt_h = (m_soundcnt_h & 0x00FF) | (val << 8);
                   if (val & (1 << 3)) m_fifo_a.clear();
                   if (val & (1 << 7)) m_fifo_b.clear(); break;
        case 0x84: m_soundcnt_x = val; m_nr52 = val; break;
        // Wave RAM
        default:
            if (offset >= 0x90 && offset <= 0x9F) m_wave_ram[offset - 0x90] = val;
            break;
    }
}

void GBAAPU::write_fifo32(uint32_t offset, uint32_t val) {
    auto& fifo = (offset == 0x0A0) ? m_fifo_a : m_fifo_b;
    for (int i = 0; i < 4; ++i) {
        if (fifo.size() < 32) fifo.push_back(static_cast<int8_t>((val >> (i * 8)) & 0xFF));
    }
}

void GBAAPU::on_timer_overflow(int timer_idx) {
    if (!(m_soundcnt_x & 0x80)) return;

    if (((m_soundcnt_h >> 10) & 1) == timer_idx) {
        if (!m_fifo_a.empty()) { m_latch_a = m_fifo_a.front(); m_fifo_a.pop_front(); }
        if (m_fifo_a.size() <= 16) m_mmu.trigger_fifo_dma(0);
    }
    if (((m_soundcnt_h >> 14) & 1) == timer_idx) {
        if (!m_fifo_b.empty()) { m_latch_b = m_fifo_b.front(); m_fifo_b.pop_front(); }
        if (m_fifo_b.size() <= 16) m_mmu.trigger_fifo_dma(1);
    }
}

static const uint8_t DUTY[4][8] = {{0,0,0,0,0,0,0,1}, {1,0,0,0,0,0,0,1}, {1,0,0,0,0,1,1,1}, {0,1,1,1,1,1,1,0}};

float GBAAPU::get_psg1() {
    if (!m_ch1_enabled || m_ch1_vol == 0 || !(m_nr52 & 0x80)) return 0.0f;
    return (DUTY[m_nr11 >> 6][m_ch1_duty] ? 1.0f : -1.0f) * (m_ch1_vol / 15.0f);
}
float GBAAPU::get_psg2() {
    if (!m_ch2_enabled || m_ch2_vol == 0 || !(m_nr52 & 0x80)) return 0.0f;
    return (DUTY[m_nr21 >> 6][m_ch2_duty] ? 1.0f : -1.0f) * (m_ch2_vol / 15.0f);
}
float GBAAPU::get_psg3() {
    if (!m_ch3_enabled || !(m_nr30 & 0x80) || !(m_nr52 & 0x80)) return 0.0f;
    uint8_t vol_code = (m_nr32 >> 5) & 3;
    if (vol_code == 0) return 0.0f;
    uint8_t byte = m_wave_ram[m_ch3_pos / 2];
    uint8_t nibble = (m_ch3_pos & 1) ? (byte & 0x0F) : (byte >> 4);
    if (vol_code == 2) nibble >>= 1;
    if (vol_code == 3) nibble >>= 2;
    return ((nibble / 7.5f) - 1.0f);
}
float GBAAPU::get_psg4() {
    if (!m_ch4_enabled || m_ch4_vol == 0 || !(m_nr52 & 0x80)) return 0.0f;
    return ((~m_lfsr & 1) ? 1.0f : -1.0f) * (m_ch4_vol / 15.0f);
}

void GBAAPU::tick(int cycles) {
    m_sample_counter += cycles;

    // PSG Frame Sequencer (512Hz)
    m_fs_timer -= cycles;
    if (m_fs_timer <= 0) {
        m_fs_timer += 32768; // 16.78MHz / 512
        if (m_fs_step % 2 == 0) {
            if ((m_nr14 & 0x40) && m_ch1_len > 0 && --m_ch1_len == 0) m_ch1_enabled = false;
            if ((m_nr24 & 0x40) && m_ch2_len > 0 && --m_ch2_len == 0) m_ch2_enabled = false;
            if ((m_nr34 & 0x40) && m_ch3_len > 0 && --m_ch3_len == 0) m_ch3_enabled = false;
            if ((m_nr44 & 0x40) && m_ch4_len > 0 && --m_ch4_len == 0) m_ch4_enabled = false;
        }
        if (m_fs_step == 7) {
            if (m_ch1_env > 0 && --m_ch1_env == 0) {
                m_ch1_env = m_nr12 & 7;
                if (m_ch1_env > 0) {
                    if (m_nr12 & 0x08) { if (m_ch1_vol < 15) m_ch1_vol++; } else { if (m_ch1_vol > 0) m_ch1_vol--; }
                }
            }
            if (m_ch2_env > 0 && --m_ch2_env == 0) {
                m_ch2_env = m_nr22 & 7;
                if (m_ch2_env > 0) {
                    if (m_nr22 & 0x08) { if (m_ch2_vol < 15) m_ch2_vol++; } else { if (m_ch2_vol > 0) m_ch2_vol--; }
                }
            }
            if (m_ch4_env > 0 && --m_ch4_env == 0) {
                m_ch4_env = m_nr42 & 7;
                if (m_ch4_env > 0) {
                    if (m_nr42 & 0x08) { if (m_ch4_vol < 15) m_ch4_vol++; } else { if (m_ch4_vol > 0) m_ch4_vol--; }
                }
            }
        }
        m_fs_step = (m_fs_step + 1) & 7;
    }

    // PSG Hardware Timers
    uint32_t freq1 = m_nr13 | ((m_nr14 & 7) << 8);
    m_ch1_timer -= cycles;
    if (m_ch1_timer <= 0) { m_ch1_timer += (2048 - freq1) * 64; m_ch1_duty = (m_ch1_duty + 1) & 7; }

    uint32_t freq2 = m_nr23 | ((m_nr24 & 7) << 8);
    m_ch2_timer -= cycles;
    if (m_ch2_timer <= 0) { m_ch2_timer += (2048 - freq2) * 64; m_ch2_duty = (m_ch2_duty + 1) & 7; }

    uint32_t freq3 = m_nr33 | ((m_nr34 & 7) << 8);
    m_ch3_timer -= cycles;
    if (m_ch3_timer <= 0) { m_ch3_timer += (2048 - freq3) * 32; m_ch3_pos = (m_ch3_pos + 1) & 31; }

    static const int divs[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    m_ch4_timer -= cycles;
    if (m_ch4_timer <= 0) {
        m_ch4_timer += (divs[m_nr43 & 7] << ((m_nr43 >> 4) & 0x0F)) * 16;
        uint8_t xor_res = (m_lfsr & 1) ^ ((m_lfsr >> 1) & 1);
        m_lfsr = (m_lfsr >> 1) | (xor_res << 14);
        if (m_nr43 & 8) m_lfsr = (m_lfsr & ~(1 << 6)) | (xor_res << 6);
    }

    // Output mixed audio
    while (m_sample_counter >= CYCLES_PER_SAMPLE) {
        m_sample_counter -= CYCLES_PER_SAMPLE;

        float dsa = (m_latch_a / 128.0f) * ((m_soundcnt_h & (1 << 2)) ? 1.0f : 0.5f);
        float dsb = (m_latch_b / 128.0f) * ((m_soundcnt_h & (1 << 3)) ? 1.0f : 0.5f);
        float psg = (get_psg1() + get_psg2() + get_psg3() + get_psg4()) * 0.25f;

        m_audio_buffer.push_back((dsa + dsb + psg) * 0.25f);
    }
}