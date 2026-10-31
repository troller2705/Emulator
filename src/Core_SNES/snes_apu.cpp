#include "../../include/Core_SNES/snes_apu.h"
#include <algorithm>
#include <iostream>

const std::array<uint8_t, 64> SNES_APU::s_ipl_rom = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

// S-DSP Hardware Envelope & Noise Rate Lookup Tables (30,720-sample master cycle)
const std::array<int, 32> SNES_APU::s_rate_period = {
    0, 2048, 1536, 1280, 1024, 768, 640, 512,
    384, 320, 256, 192, 160, 128, 96, 80,
    64, 48, 40, 32, 24, 20, 16, 12,
    10, 8, 6, 5, 4, 3, 2, 1
};

const std::array<int, 32> SNES_APU::s_rate_offset = {
    0, 0, 1040, 536, 0, 1040, 536, 0,
    1040, 536, 0, 1040, 536, 0, 1040, 536,
    0, 1040, 536, 0, 1040, 536, 0, 1040,
    536, 0, 1040, 536, 0, 1040, 0, 0
};

SNES_APU::SNES_APU() {
    m_audio_buffer.reserve(1024);
    reset();
}

void SNES_APU::reset() {
    m_aram.fill(0);
    m_dsp_regs.fill(0);
    m_dsp_regs[0x6C] = 0xE0; // Reset, Mute, and Disable Echo Write on boot
    m_cpu_to_apu.fill(0);
    m_apu_to_cpu.fill(0);
    m_audio_buffer.clear();

    std::copy(s_ipl_rom.begin(), s_ipl_rom.end(), m_aram.begin() + 0xFFC0);

    m_spc_PC = 0xFFC0;
    m_spc_A = 0; m_spc_X = 0; m_spc_Y = 0;
    m_spc_SP = 0xEF;
    m_spc_cycles_left = 0;
    set_psw(0x00);

    m_control = 0xB0;
    m_dsp_addr = 0;

    m_cpu_cycle_accum = 0;
    m_dsp_sample_accum = 0;
    m_resample_accum = 0;

    m_dsp_counter = 0;
    m_noise_lfsr = 0x4000;
    m_echo_base = 0;
    m_echo_offset = 0;
    m_echo_length = 4;
    m_fir_left.fill(0);
    m_fir_right.fill(0);
    m_fir_idx = 0;

    for (auto& t : m_timers) {
        t.target = 256; t.counter = 0; t.up_counter = 0;
        t.enabled = false; t.prescaler = 0;
    }
    for (auto& v : m_voices) {
        v = Voice{};
    }
}

uint8_t SNES_APU::cpuReadPort(uint16_t addr) {
    return m_apu_to_cpu[addr & 0x03];
}

void SNES_APU::cpuWritePort(uint16_t addr, uint8_t val) {
    m_cpu_to_apu[addr & 0x03] = val;
}

void SNES_APU::step(int cpu_cycles) {
    // 1 CPU Cycle = 6 Master Cycles
    m_cpu_cycle_accum += (cpu_cycles * 6);

    // The SPC700 runs at ~1.024 MHz. Master clock is ~21.477 MHz.
    while (m_cpu_cycle_accum >= 21) {
        m_cpu_cycle_accum -= 21;

        if (m_spc_cycles_left <= 0) {
            m_spc_cycles_left = spc_step();
        }
        m_spc_cycles_left--;

        tick_timers();

        m_dsp_sample_accum++;
        if (m_dsp_sample_accum >= 32) { // 32 kHz native DSP rate
            m_dsp_sample_accum -= 32;
            step_dsp();
        }
    }
}

bool SNES_APU::poll_dsp_rate(int rate) const {
    if (rate <= 0) return false;
    int period = s_rate_period[rate & 0x1F];
    int offset = s_rate_offset[rate & 0x1F];
    return ((m_dsp_counter + offset) % period) == 0;
}

void SNES_APU::update_envelope(int voice_idx) {
    Voice& v = m_voices[voice_idx];
    uint8_t adsr1 = m_dsp_regs[(voice_idx << 4) | 0x05];
    uint8_t adsr2 = m_dsp_regs[(voice_idx << 4) | 0x06];
    uint8_t gain  = m_dsp_regs[(voice_idx << 4) | 0x07];

    if (v.env_state == EnvState::Release) {
        v.envelope -= 8;
        if (v.envelope <= 0) {
            v.envelope = 0;
            v.active = false;
        }
        return;
    }

    if (adsr1 & 0x80) { // Hardware ADSR Mode
        int sustain_level = ((adsr2 >> 5) + 1) * 0x100;
        if (v.env_state == EnvState::Attack) {
            int ar = ((adsr1 & 0x0F) << 1) + 1;
            if (poll_dsp_rate(ar)) {
                v.envelope += (ar == 31) ? 1024 : 32;
                if (v.envelope >= 0x7FF) {
                    v.envelope = 0x7FF;
                    v.env_state = EnvState::Decay;
                }
            }
        } else if (v.env_state == EnvState::Decay) {
            if (v.envelope <= sustain_level) {
                v.env_state = EnvState::Sustain;
            } else {
                int dr = (((adsr1 >> 4) & 0x07) << 1) + 16;
                if (poll_dsp_rate(dr)) {
                    v.envelope -= ((v.envelope - 1) >> 8) + 1;
                    if (v.envelope <= sustain_level) {
                        v.env_state = EnvState::Sustain;
                    }
                }
            }
        } else if (v.env_state == EnvState::Sustain) {
            int sr = adsr2 & 0x1F;
            if (poll_dsp_rate(sr)) {
                v.envelope -= ((v.envelope - 1) >> 8) + 1;
                if (v.envelope < 0) v.envelope = 0;
            }
        }
    } else { // Hardware GAIN Mode
        if ((gain & 0x80) == 0) {
            v.envelope = (gain & 0x7F) << 4;
        } else {
            uint8_t mode = (gain >> 5) & 0x03;
            int rate = gain & 0x1F;
            if (poll_dsp_rate(rate)) {
                if (mode == 0) v.envelope -= 32;
                else if (mode == 1) v.envelope -= ((v.envelope - 1) >> 8) + 1;
                else if (mode == 2) v.envelope += 32;
                else if (mode == 3) v.envelope += (v.envelope < 0x600) ? 32 : 8;
                v.envelope = std::clamp(v.envelope, 0, 0x7FF);
            }
        }
    }
}

int SNES_APU::decode_brr_sample(int voice_idx) {
    Voice& v = m_voices[voice_idx];

    if (v.nibble_index >= 16) {
        v.block_header = m_aram[v.current_addr & 0xFFFF];
        v.current_addr = (v.current_addr + 1) & 0xFFFF;
        v.nibble_index = 0;
    }

    uint8_t b = m_aram[(v.current_addr + (v.nibble_index >> 1)) & 0xFFFF];
    int nibble = (v.nibble_index & 1) ? (b & 0x0F) : (b >> 4);
    if (nibble >= 8) nibble -= 16;

    v.nibble_index++;
    if (v.nibble_index == 16) {
        v.current_addr = (v.current_addr + 8) & 0xFFFF;
        if (v.block_header & 0x01) { // End bit in BRR block header
            m_dsp_regs[0x7C] |= (1 << voice_idx); // Set ENDX bit
            if (v.block_header & 0x02) { // Loop bit
                v.current_addr = v.loop_addr;
            } else {
                v.active = false;
                v.env_state = EnvState::Release;
                v.envelope = 0;
            }
        }
    }

    int shift  = (v.block_header >> 4) & 0x0F;
    int filter = (v.block_header >> 2) & 0x03;

    int sample = (shift <= 12) ? ((nibble << shift) >> 1) : ((nibble < 0) ? -2048 : 0);

    int p1 = v.prev1;
    int p2 = v.prev2;
    int filtered = sample;

    if (filter == 1) {
        filtered += p1 + (-p1 >> 4);
    } else if (filter == 2) {
        filtered += (p1 << 1) + ((-p1 * 3) >> 5) - p2 + (p2 >> 4);
    } else if (filter == 3) {
        filtered += (p1 << 1) + ((-p1 * 13) >> 6) - p2 + ((p2 * 3) >> 4);
    }

    filtered = std::clamp(filtered, -32768, 32767);
    filtered = static_cast<int16_t>(filtered << 1) >> 1;

    v.prev2 = v.prev1;
    v.prev1 = filtered;

    return filtered;
}

void SNES_APU::step_dsp() {
    // 1. Advance Master 30,720-cycle Rate Counter
    if (--m_dsp_counter < 0) {
        m_dsp_counter = 30719;
    }

    // 2. Clock 15-bit Hardware Noise LFSR
    int noise_rate = m_dsp_regs[0x6C] & 0x1F;
    if (poll_dsp_rate(noise_rate)) {
        int feedback = (m_noise_lfsr & 1) ^ ((m_noise_lfsr >> 1) & 1);
        m_noise_lfsr = (m_noise_lfsr >> 1) | (feedback << 14);
    }
    int noise_sample = static_cast<int16_t>(m_noise_lfsr << 1);

    int main_left = 0;
    int main_right = 0;
    int echo_in_left = 0;
    int echo_in_right = 0;

    bool muted = (m_dsp_regs[0x6C] & 0x40) != 0;
    if (m_dsp_regs[0x6C] & 0x80) { // Soft reset
        for (auto& v : m_voices) {
            v.active = false;
            v.envelope = 0;
            v.env_state = EnvState::Release;
        }
    }

    uint8_t pmon = m_dsp_regs[0x2D];
    uint8_t non  = m_dsp_regs[0x3D];
    uint8_t eon  = m_dsp_regs[0x4D];
    int prev_outx = 0;

    // 3. Process All 8 Voices
    for (int i = 0; i < 8; ++i) {
        Voice& v = m_voices[i];
        if (!v.active) {
            m_dsp_regs[(i << 4) | 0x08] = 0;
            m_dsp_regs[(i << 4) | 0x09] = 0;
            prev_outx = 0;
            continue;
        }

        update_envelope(i);

        int eff_pitch = v.pitch;
        if (i > 0 && (pmon & (1 << i))) {
            eff_pitch += ((prev_outx >> 5) * v.pitch) >> 10;
            eff_pitch = std::clamp(eff_pitch, 0, 0x3FFF);
        }

        v.phase += eff_pitch;
        while (v.phase >= 0x1000) {
            v.phase -= 0x1000;
            v.current_val = decode_brr_sample(i);
            if (!v.active) break;
        }

        int raw_sample = (non & (1 << i)) ? noise_sample : v.current_val;
        int env_sample = (raw_sample * v.envelope) >> 11;
        env_sample = std::clamp(env_sample, -32768, 32767);
        prev_outx = env_sample;

        m_dsp_regs[(i << 4) | 0x08] = static_cast<uint8_t>((v.envelope >> 4) & 0x7F);
        m_dsp_regs[(i << 4) | 0x09] = static_cast<uint8_t>((env_sample >> 8) & 0xFF);

        int voice_l = (env_sample * v.volume_left)  >> 7;
        int voice_r = (env_sample * v.volume_right) >> 7;

        main_left  = std::clamp(main_left  + voice_l, -32768, 32767);
        main_right = std::clamp(main_right + voice_r, -32768, 32767);

        if (eon & (1 << i)) {
            echo_in_left  = std::clamp(echo_in_left  + voice_l, -32768, 32767);
            echo_in_right = std::clamp(echo_in_right + voice_r, -32768, 32767);
        }
    }

    // 4. Process Echo Ring Buffer & 8-Tap FIR Filter
    uint16_t echo_addr = static_cast<uint16_t>(m_echo_base + m_echo_offset);
    int16_t aram_echo_l = static_cast<int16_t>(m_aram[echo_addr] | (m_aram[static_cast<uint16_t>(echo_addr + 1)] << 8));
    int16_t aram_echo_r = static_cast<int16_t>(m_aram[static_cast<uint16_t>(echo_addr + 2)] | (m_aram[static_cast<uint16_t>(echo_addr + 3)] << 8));

    m_fir_left[m_fir_idx]  = aram_echo_l >> 1;
    m_fir_right[m_fir_idx] = aram_echo_r >> 1;

    int fir_l = 0;
    int fir_r = 0;
    for (int tap = 0; tap < 8; ++tap) {
        int hist_idx = (m_fir_idx + 1 + tap) & 7;
        int8_t coeff = static_cast<int8_t>(m_dsp_regs[(tap << 4) | 0x0F]);
        fir_l += (m_fir_left[hist_idx]  * coeff) >> 6;
        fir_r += (m_fir_right[hist_idx] * coeff) >> 6;
    }
    fir_l = std::clamp(fir_l, -32768, 32767);
    fir_r = std::clamp(fir_r, -32768, 32767);
    m_fir_idx = (m_fir_idx + 1) & 7;

    // Write Echo Feedback back to ARAM if ~ECEN (FLG bit 5) is 0
    int8_t efb = static_cast<int8_t>(m_dsp_regs[0x0D]);
    int fb_l = std::clamp(echo_in_left  + ((fir_l * efb) >> 7), -32768, 32767) & ~1;
    int fb_r = std::clamp(echo_in_right + ((fir_r * efb) >> 7), -32768, 32767) & ~1;

    if ((m_dsp_regs[0x6C] & 0x20) == 0) {
        m_aram[echo_addr]                                = static_cast<uint8_t>(fb_l & 0xFF);
        m_aram[static_cast<uint16_t>(echo_addr + 1)]     = static_cast<uint8_t>((fb_l >> 8) & 0xFF);
        m_aram[static_cast<uint16_t>(echo_addr + 2)]     = static_cast<uint8_t>(fb_r & 0xFF);
        m_aram[static_cast<uint16_t>(echo_addr + 3)]     = static_cast<uint8_t>((fb_r >> 8) & 0xFF);
    }

    if (m_echo_offset == 0) {
        m_echo_base = static_cast<uint16_t>(m_dsp_regs[0x6D]) << 8;
        uint8_t edl = m_dsp_regs[0x7D] & 0x0F;
        m_echo_length = (edl == 0) ? 4 : (static_cast<int>(edl) << 11);
    }
    m_echo_offset += 4;
    if (m_echo_offset >= m_echo_length) {
        m_echo_offset = 0;
    }

    // 5. Final Master + Echo Output Mixing
    int8_t mv_l = static_cast<int8_t>(m_dsp_regs[0x0C]);
    int8_t mv_r = static_cast<int8_t>(m_dsp_regs[0x1C]);
    int8_t ev_l = static_cast<int8_t>(m_dsp_regs[0x2C]);
    int8_t ev_r = static_cast<int8_t>(m_dsp_regs[0x3C]);

    int out_l = std::clamp(((main_left  * mv_l) >> 7) + ((fir_l * ev_l) >> 7), -32768, 32767);
    int out_r = std::clamp(((main_right * mv_r) >> 7) + ((fir_r * ev_r) >> 7), -32768, 32767);

    float final_mono = muted ? 0.0f : (static_cast<float>(out_l + out_r) / 65536.0f);
    final_mono = std::clamp(final_mono, -1.0f, 1.0f);

    // Resample from 32,000 Hz DSP rate to SDL's 44,100 Hz output rate
    m_resample_accum += 44100;
    while (m_resample_accum >= 32000) {
        m_resample_accum -= 32000;
        m_audio_buffer.push_back(final_mono);
    }
}

void SNES_APU::tick_timers() {
    for (int i = 0; i < 3; ++i) {
        if (!m_timers[i].enabled) continue;
        m_timers[i].prescaler++;
        int limit = (i == 2) ? 16 : 128;
        if (m_timers[i].prescaler >= limit) {
            m_timers[i].prescaler = 0;
            m_timers[i].counter++;
            if (m_timers[i].counter >= m_timers[i].target) {
                m_timers[i].counter = 0;
                m_timers[i].up_counter = (m_timers[i].up_counter + 1) & 0x0F;
            }
        }
    }
}

uint8_t SNES_APU::spc_read(uint16_t addr) {
    if (addr >= 0xFFC0 && (m_control & 0x80)) {
        return s_ipl_rom[addr - 0xFFC0];
    }
    if (addr >= 0x00F0 && addr <= 0x00FF) {
        switch (addr) {
            case 0x00F2: return m_dsp_addr;
            case 0x00F3: return m_dsp_regs[m_dsp_addr & 0x7F];
            case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F7:
                return m_cpu_to_apu[addr - 0x00F4];
            case 0x00FD: { uint8_t v = m_timers[0].up_counter; m_timers[0].up_counter = 0; return v; }
            case 0x00FE: { uint8_t v = m_timers[1].up_counter; m_timers[1].up_counter = 0; return v; }
            case 0x00FF: { uint8_t v = m_timers[2].up_counter; m_timers[2].up_counter = 0; return v; }
        }
    }
    return m_aram[addr];
}

void SNES_APU::spc_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x00F0 && addr <= 0x00FF) {
        switch (addr) {
            case 0x00F1:
                m_control = val;
                for (int i = 0; i < 3; ++i) {
                    bool was_enabled = m_timers[i].enabled;
                    m_timers[i].enabled = (val & (1 << i)) != 0;
                    if (!was_enabled && m_timers[i].enabled) {
                        m_timers[i].counter = 0;
                        m_timers[i].prescaler = 0;
                        m_timers[i].up_counter = 0;
                    }
                }
                if (val & 0x10) { m_cpu_to_apu[0] = 0; m_cpu_to_apu[1] = 0; }
                if (val & 0x20) { m_cpu_to_apu[2] = 0; m_cpu_to_apu[3] = 0; }
                break;
            case 0x00F2:
                m_dsp_addr = val;
                break;
            case 0x00F3: {
                uint8_t reg = m_dsp_addr & 0x7F;
                m_dsp_regs[reg] = val;
                int voice_idx = reg >> 4;
                if (voice_idx < 8) {
                    Voice& v = m_voices[voice_idx];
                    int sub_reg = reg & 0x0F;
                    if (sub_reg == 0x00) v.volume_left  = static_cast<int8_t>(val);
                    if (sub_reg == 0x01) v.volume_right = static_cast<int8_t>(val);
                    if (sub_reg == 0x02) v.pitch = (v.pitch & 0x3F00) | val;
                    if (sub_reg == 0x03) v.pitch = (v.pitch & 0x00FF) | (static_cast<uint16_t>(val & 0x3F) << 8);
                }
                if (reg == 0x4C) { // KON (Key ON)
                    for (int i = 0; i < 8; ++i) {
                        if (val & (1 << i)) {
                            Voice& v = m_voices[i];
                            uint8_t dir_page = m_dsp_regs[0x5D];
                            uint16_t srcn = m_dsp_regs[(i << 4) | 0x04];
                            uint16_t table_addr = static_cast<uint16_t>((dir_page << 8) + (srcn * 4));
                            v.sample_addr = m_aram[table_addr] | (m_aram[static_cast<uint16_t>(table_addr + 1)] << 8);
                            v.loop_addr   = m_aram[static_cast<uint16_t>(table_addr + 2)] | (m_aram[static_cast<uint16_t>(table_addr + 3)] << 8);
                            v.current_addr = v.sample_addr;
                            v.nibble_index = 16;
                            v.prev1 = v.prev2 = 0;
                            v.current_val = 0;
                            v.phase = 0;
                            v.envelope = 0;
                            v.env_state = EnvState::Attack;
                            v.active = true;
                            m_dsp_regs[0x7C] &= ~(1 << i);
                        }
                    }
                } else if (reg == 0x5C) { // KOFF (Key OFF)
                    for (int i = 0; i < 8; ++i) {
                        if (val & (1 << i)) {
                            m_voices[i].env_state = EnvState::Release;
                        }
                    }
                } else if (reg == 0x7C) { // ENDX Clear
                    m_dsp_regs[0x7C] = 0;
                }
                break;
            }
            case 0x00F4: case 0x00F5: case 0x00F6: case 0x00F7:
                m_apu_to_cpu[addr - 0x00F4] = val;
                break;
            case 0x00FA: m_timers[0].target = (val == 0) ? 256 : val; break;
            case 0x00FB: m_timers[1].target = (val == 0) ? 256 : val; break;
            case 0x00FC: m_timers[2].target = (val == 0) ? 256 : val; break;
        }
    }
    m_aram[addr] = val;
}

uint16_t SNES_APU::spc_read16(uint16_t addr) {
    uint8_t low  = spc_read(addr);
    uint8_t high = spc_read(static_cast<uint16_t>(addr + 1));
    return low | (static_cast<uint16_t>(high) << 8);
}

void SNES_APU::spc_write16(uint16_t addr, uint16_t val) {
    spc_write(addr, static_cast<uint8_t>(val & 0xFF));
    spc_write(static_cast<uint16_t>(addr + 1), static_cast<uint8_t>((val >> 8) & 0xFF));
}

uint8_t SNES_APU::spc_fetch() { return spc_read(m_spc_PC++); }
uint16_t SNES_APU::spc_fetch16() { uint8_t l = spc_fetch(); uint8_t h = spc_fetch(); return l | (static_cast<uint16_t>(h) << 8); }

uint16_t SNES_APU::addr_dp()   { return dp_addr(spc_fetch()); }
uint16_t SNES_APU::addr_dp_x() { return dp_addr(static_cast<uint8_t>(spc_fetch() + m_spc_X)); }
uint16_t SNES_APU::addr_dp_y() { return dp_addr(static_cast<uint8_t>(spc_fetch() + m_spc_Y)); }
uint16_t SNES_APU::addr_abs_x() { return static_cast<uint16_t>(spc_fetch16() + m_spc_X); }
uint16_t SNES_APU::addr_abs_y() { return static_cast<uint16_t>(spc_fetch16() + m_spc_Y); }

uint16_t SNES_APU::addr_idx_ind_x() {
    uint8_t dp = static_cast<uint8_t>(spc_fetch() + m_spc_X);
    uint8_t low  = spc_read(dp_addr(dp));
    uint8_t high = spc_read(dp_addr(static_cast<uint8_t>(dp + 1)));
    return low | (static_cast<uint16_t>(high) << 8);
}

uint16_t SNES_APU::addr_ind_idx_y() {
    uint8_t dp = spc_fetch();
    uint8_t low  = spc_read(dp_addr(dp));
    uint8_t high = spc_read(dp_addr(static_cast<uint8_t>(dp + 1)));
    uint16_t base = low | (static_cast<uint16_t>(high) << 8);
    return static_cast<uint16_t>(base + m_spc_Y);
}

void SNES_APU::push8(uint8_t val) {
    spc_write(0x0100 | m_spc_SP--, val);
}

void SNES_APU::push16(uint16_t val) {
    push8(static_cast<uint8_t>((val >> 8) & 0xFF));
    push8(static_cast<uint8_t>(val & 0xFF));
}

uint8_t SNES_APU::pop8() {
    return spc_read(0x0100 | ++m_spc_SP);
}

uint16_t SNES_APU::pop16() {
    uint8_t low  = pop8();
    uint8_t high = pop8();
    return low | (static_cast<uint16_t>(high) << 8);
}

uint8_t SNES_APU::get_psw() const {
    uint8_t p = 0;
    if (flag_n) p |= 0x80;
    if (flag_v) p |= 0x40;
    if (flag_p) p |= 0x20;
    if (flag_b) p |= 0x10;
    if (flag_h) p |= 0x08;
    if (flag_i) p |= 0x04;
    if (flag_z) p |= 0x02;
    if (flag_c) p |= 0x01;
    return p;
}

void SNES_APU::set_psw(uint8_t val) {
    flag_n = (val & 0x80) != 0;
    flag_v = (val & 0x40) != 0;
    flag_p = (val & 0x20) != 0;
    flag_b = (val & 0x10) != 0;
    flag_h = (val & 0x08) != 0;
    flag_i = (val & 0x04) != 0;
    flag_z = (val & 0x02) != 0;
    flag_c = (val & 0x01) != 0;
}

void SNES_APU::set_zn(uint8_t val) {
    flag_z = (val == 0);
    flag_n = (val & 0x80) != 0;
}

void SNES_APU::set_zn16(uint16_t val) {
    flag_z = (val == 0);
    flag_n = (val & 0x8000) != 0;
}

uint8_t SNES_APU::op_adc(uint8_t a, uint8_t b) {
    int c = flag_c ? 1 : 0;
    int sum = a + b + c;
    flag_v = (~(a ^ b) & (a ^ sum) & 0x80) != 0;
    flag_h = ((a & 0x0F) + (b & 0x0F) + c) > 0x0F;
    flag_c = sum > 0xFF;
    uint8_t res = static_cast<uint8_t>(sum & 0xFF);
    set_zn(res);
    return res;
}

uint8_t SNES_APU::op_sbc(uint8_t a, uint8_t b) {
    return op_adc(a, ~b);
}

void SNES_APU::op_cmp(uint8_t a, uint8_t b) {
    int res = a - b;
    flag_c = (a >= b);
    set_zn(static_cast<uint8_t>(res & 0xFF));
}

uint8_t SNES_APU::op_and(uint8_t a, uint8_t b) { uint8_t r = a & b; set_zn(r); return r; }
uint8_t SNES_APU::op_or(uint8_t a, uint8_t b)  { uint8_t r = a | b; set_zn(r); return r; }
uint8_t SNES_APU::op_eor(uint8_t a, uint8_t b) { uint8_t r = a ^ b; set_zn(r); return r; }

uint8_t SNES_APU::op_asl(uint8_t val) {
    flag_c = (val & 0x80) != 0;
    val <<= 1;
    set_zn(val);
    return val;
}

uint8_t SNES_APU::op_lsr(uint8_t val) {
    flag_c = (val & 0x01) != 0;
    val >>= 1;
    set_zn(val);
    return val;
}

uint8_t SNES_APU::op_rol(uint8_t val) {
    bool old_c = flag_c;
    flag_c = (val & 0x80) != 0;
    val = static_cast<uint8_t>((val << 1) | (old_c ? 1 : 0));
    set_zn(val);
    return val;
}

uint8_t SNES_APU::op_ror(uint8_t val) {
    bool old_c = flag_c;
    flag_c = (val & 0x01) != 0;
    val = static_cast<uint8_t>((val >> 1) | (old_c ? 0x80 : 0));
    set_zn(val);
    return val;
}

uint8_t SNES_APU::op_inc(uint8_t val) { val++; set_zn(val); return val; }
uint8_t SNES_APU::op_dec(uint8_t val) { val--; set_zn(val); return val; }

void SNES_APU::branch_if(bool cond, int& cycles) {
    int8_t rel = static_cast<int8_t>(spc_fetch());
    if (cond) {
        m_spc_PC = static_cast<uint16_t>(m_spc_PC + rel);
        cycles += 2;
    }
}

int SNES_APU::spc_step() {
    uint8_t opcode = spc_fetch();
    int cycles = 2;

    // 1. Handle TCALL 0..15 (0x01, 0x11, ..., 0xF1)
    if ((opcode & 0x0F) == 0x01) {
        int n = opcode >> 4;
        push16(m_spc_PC);
        uint16_t vec = static_cast<uint16_t>(0xFFDE - (n * 2));
        m_spc_PC = spc_read16(vec);
        return 8;
    }

    // 2. Handle SET1 / CLR1 dp.bit (0x02, 0x12, ..., 0xF2)
    if ((opcode & 0x0F) == 0x02) {
        uint8_t bit = opcode >> 5;
        uint16_t a = addr_dp();
        uint8_t v = spc_read(a);
        if (opcode & 0x10) v &= ~(1 << bit);
        else               v |=  (1 << bit);
        spc_write(a, v);
        return 4;
    }

    // 3. Handle BBS / BBC dp.bit, rel (0x03, 0x13, ..., 0xF3)
    if ((opcode & 0x0F) == 0x03) {
        uint8_t bit = opcode >> 5;
        uint8_t v = spc_read(addr_dp());
        bool is_set = (v & (1 << bit)) != 0;
        bool cond = (opcode & 0x10) ? !is_set : is_set;
        cycles = 5;
        branch_if(cond, cycles);
        return cycles;
    }

    switch (opcode) {
        case 0x00: cycles = 2; break; // NOP

        // --- OR Family ---
        case 0x04: m_spc_A = op_or(m_spc_A, spc_read(addr_dp())); cycles = 3; break;
        case 0x05: m_spc_A = op_or(m_spc_A, spc_read(spc_fetch16())); cycles = 4; break;
        case 0x06: m_spc_A = op_or(m_spc_A, spc_read(dp_addr(m_spc_X))); cycles = 3; break;
        case 0x07: m_spc_A = op_or(m_spc_A, spc_read(addr_idx_ind_x())); cycles = 6; break;
        case 0x08: m_spc_A = op_or(m_spc_A, spc_fetch()); cycles = 2; break;
        case 0x09: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); spc_write(d, op_or(spc_read(d), spc_read(s))); cycles = 6; break; }
        case 0x14: m_spc_A = op_or(m_spc_A, spc_read(addr_dp_x())); cycles = 4; break;
        case 0x15: m_spc_A = op_or(m_spc_A, spc_read(addr_abs_x())); cycles = 5; break;
        case 0x16: m_spc_A = op_or(m_spc_A, spc_read(addr_abs_y())); cycles = 5; break;
        case 0x17: m_spc_A = op_or(m_spc_A, spc_read(addr_ind_idx_y())); cycles = 6; break;
        case 0x18: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); spc_write(d, op_or(spc_read(d), imm)); cycles = 5; break; }
        case 0x19: { uint16_t d = dp_addr(m_spc_X); spc_write(d, op_or(spc_read(d), spc_read(dp_addr(m_spc_Y)))); cycles = 5; break; }

        // --- AND Family ---
        case 0x24: m_spc_A = op_and(m_spc_A, spc_read(addr_dp())); cycles = 3; break;
        case 0x25: m_spc_A = op_and(m_spc_A, spc_read(spc_fetch16())); cycles = 4; break;
        case 0x26: m_spc_A = op_and(m_spc_A, spc_read(dp_addr(m_spc_X))); cycles = 3; break;
        case 0x27: m_spc_A = op_and(m_spc_A, spc_read(addr_idx_ind_x())); cycles = 6; break;
        case 0x28: m_spc_A = op_and(m_spc_A, spc_fetch()); cycles = 2; break;
        case 0x29: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); spc_write(d, op_and(spc_read(d), spc_read(s))); cycles = 6; break; }
        case 0x34: m_spc_A = op_and(m_spc_A, spc_read(addr_dp_x())); cycles = 4; break;
        case 0x35: m_spc_A = op_and(m_spc_A, spc_read(addr_abs_x())); cycles = 5; break;
        case 0x36: m_spc_A = op_and(m_spc_A, spc_read(addr_abs_y())); cycles = 5; break;
        case 0x37: m_spc_A = op_and(m_spc_A, spc_read(addr_ind_idx_y())); cycles = 6; break;
        case 0x38: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); spc_write(d, op_and(spc_read(d), imm)); cycles = 5; break; }
        case 0x39: { uint16_t d = dp_addr(m_spc_X); spc_write(d, op_and(spc_read(d), spc_read(dp_addr(m_spc_Y)))); cycles = 5; break; }

        // --- EOR Family ---
        case 0x44: m_spc_A = op_eor(m_spc_A, spc_read(addr_dp())); cycles = 3; break;
        case 0x45: m_spc_A = op_eor(m_spc_A, spc_read(spc_fetch16())); cycles = 4; break;
        case 0x46: m_spc_A = op_eor(m_spc_A, spc_read(dp_addr(m_spc_X))); cycles = 3; break;
        case 0x47: m_spc_A = op_eor(m_spc_A, spc_read(addr_idx_ind_x())); cycles = 6; break;
        case 0x48: m_spc_A = op_eor(m_spc_A, spc_fetch()); cycles = 2; break;
        case 0x49: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); spc_write(d, op_eor(spc_read(d), spc_read(s))); cycles = 6; break; }
        case 0x54: m_spc_A = op_eor(m_spc_A, spc_read(addr_dp_x())); cycles = 4; break;
        case 0x55: m_spc_A = op_eor(m_spc_A, spc_read(addr_abs_x())); cycles = 5; break;
        case 0x56: m_spc_A = op_eor(m_spc_A, spc_read(addr_abs_y())); cycles = 5; break;
        case 0x57: m_spc_A = op_eor(m_spc_A, spc_read(addr_ind_idx_y())); cycles = 6; break;
        case 0x58: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); spc_write(d, op_eor(spc_read(d), imm)); cycles = 5; break; }
        case 0x59: { uint16_t d = dp_addr(m_spc_X); spc_write(d, op_eor(spc_read(d), spc_read(dp_addr(m_spc_Y)))); cycles = 5; break; }

        // --- CMP Family ---
        case 0x64: op_cmp(m_spc_A, spc_read(addr_dp())); cycles = 3; break;
        case 0x65: op_cmp(m_spc_A, spc_read(spc_fetch16())); cycles = 4; break;
        case 0x66: op_cmp(m_spc_A, spc_read(dp_addr(m_spc_X))); cycles = 3; break;
        case 0x67: op_cmp(m_spc_A, spc_read(addr_idx_ind_x())); cycles = 6; break;
        case 0x68: op_cmp(m_spc_A, spc_fetch()); cycles = 2; break;
        case 0x69: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); op_cmp(spc_read(d), spc_read(s)); cycles = 6; break; }
        case 0x74: op_cmp(m_spc_A, spc_read(addr_dp_x())); cycles = 4; break;
        case 0x75: op_cmp(m_spc_A, spc_read(addr_abs_x())); cycles = 5; break;
        case 0x76: op_cmp(m_spc_A, spc_read(addr_abs_y())); cycles = 5; break;
        case 0x77: op_cmp(m_spc_A, spc_read(addr_ind_idx_y())); cycles = 6; break;
        case 0x78: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); op_cmp(spc_read(d), imm); cycles = 5; break; }
        case 0x79: op_cmp(spc_read(dp_addr(m_spc_X)), spc_read(dp_addr(m_spc_Y))); cycles = 5; break;

        case 0xC8: op_cmp(m_spc_X, spc_fetch()); cycles = 2; break;
        case 0x3E: op_cmp(m_spc_X, spc_read(addr_dp())); cycles = 3; break;
        case 0x1E: op_cmp(m_spc_X, spc_read(spc_fetch16())); cycles = 4; break;
        case 0xAD: op_cmp(m_spc_Y, spc_fetch()); cycles = 2; break;
        case 0x7E: op_cmp(m_spc_Y, spc_read(addr_dp())); cycles = 3; break;
        case 0x5E: op_cmp(m_spc_Y, spc_read(spc_fetch16())); cycles = 4; break;

        // --- ADC Family ---
        case 0x84: m_spc_A = op_adc(m_spc_A, spc_read(addr_dp())); cycles = 3; break;
        case 0x85: m_spc_A = op_adc(m_spc_A, spc_read(spc_fetch16())); cycles = 4; break;
        case 0x86: m_spc_A = op_adc(m_spc_A, spc_read(dp_addr(m_spc_X))); cycles = 3; break;
        case 0x87: m_spc_A = op_adc(m_spc_A, spc_read(addr_idx_ind_x())); cycles = 6; break;
        case 0x88: m_spc_A = op_adc(m_spc_A, spc_fetch()); cycles = 2; break;
        case 0x89: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); spc_write(d, op_adc(spc_read(d), spc_read(s))); cycles = 6; break; }
        case 0x94: m_spc_A = op_adc(m_spc_A, spc_read(addr_dp_x())); cycles = 4; break;
        case 0x95: m_spc_A = op_adc(m_spc_A, spc_read(addr_abs_x())); cycles = 5; break;
        case 0x96: m_spc_A = op_adc(m_spc_A, spc_read(addr_abs_y())); cycles = 5; break;
        case 0x97: m_spc_A = op_adc(m_spc_A, spc_read(addr_ind_idx_y())); cycles = 6; break;
        case 0x98: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); spc_write(d, op_adc(spc_read(d), imm)); cycles = 5; break; }
        case 0x99: { uint16_t d = dp_addr(m_spc_X); spc_write(d, op_adc(spc_read(d), spc_read(dp_addr(m_spc_Y)))); cycles = 5; break; }

        // --- SBC Family ---
        case 0xA4: m_spc_A = op_sbc(m_spc_A, spc_read(addr_dp())); cycles = 3; break;
        case 0xA5: m_spc_A = op_sbc(m_spc_A, spc_read(spc_fetch16())); cycles = 4; break;
        case 0xA6: m_spc_A = op_sbc(m_spc_A, spc_read(dp_addr(m_spc_X))); cycles = 3; break;
        case 0xA7: m_spc_A = op_sbc(m_spc_A, spc_read(addr_idx_ind_x())); cycles = 6; break;
        case 0xA8: m_spc_A = op_sbc(m_spc_A, spc_fetch()); cycles = 2; break;
        case 0xA9: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); spc_write(d, op_sbc(spc_read(d), spc_read(s))); cycles = 6; break; }
        case 0xB4: m_spc_A = op_sbc(m_spc_A, spc_read(addr_dp_x())); cycles = 4; break;
        case 0xB5: m_spc_A = op_sbc(m_spc_A, spc_read(addr_abs_x())); cycles = 5; break;
        case 0xB6: m_spc_A = op_sbc(m_spc_A, spc_read(addr_abs_y())); cycles = 5; break;
        case 0xB7: m_spc_A = op_sbc(m_spc_A, spc_read(addr_ind_idx_y())); cycles = 6; break;
        case 0xB8: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); spc_write(d, op_sbc(spc_read(d), imm)); cycles = 5; break; }
        case 0xB9: { uint16_t d = dp_addr(m_spc_X); spc_write(d, op_sbc(spc_read(d), spc_read(dp_addr(m_spc_Y)))); cycles = 5; break; }

        // --- MOV to A ---
        case 0xE8: m_spc_A = spc_fetch(); set_zn(m_spc_A); cycles = 2; break;
        case 0xE6: m_spc_A = spc_read(dp_addr(m_spc_X)); set_zn(m_spc_A); cycles = 3; break;
        case 0xBF: m_spc_A = spc_read(dp_addr(m_spc_X++)); set_zn(m_spc_A); cycles = 4; break;
        case 0xE4: m_spc_A = spc_read(addr_dp()); set_zn(m_spc_A); cycles = 3; break;
        case 0xF4: m_spc_A = spc_read(addr_dp_x()); set_zn(m_spc_A); cycles = 4; break;
        case 0xE5: m_spc_A = spc_read(spc_fetch16()); set_zn(m_spc_A); cycles = 4; break;
        case 0xF5: m_spc_A = spc_read(addr_abs_x()); set_zn(m_spc_A); cycles = 5; break;
        case 0xF6: m_spc_A = spc_read(addr_abs_y()); set_zn(m_spc_A); cycles = 5; break;
        case 0xE7: m_spc_A = spc_read(addr_idx_ind_x()); set_zn(m_spc_A); cycles = 6; break;
        case 0xF7: m_spc_A = spc_read(addr_ind_idx_y()); set_zn(m_spc_A); cycles = 6; break;

        // --- MOV to X ---
        case 0xCD: m_spc_X = spc_fetch(); set_zn(m_spc_X); cycles = 2; break;
        case 0xF8: m_spc_X = spc_read(addr_dp()); set_zn(m_spc_X); cycles = 3; break;
        case 0xF9: m_spc_X = spc_read(addr_dp_y()); set_zn(m_spc_X); cycles = 4; break;
        case 0xE9: m_spc_X = spc_read(spc_fetch16()); set_zn(m_spc_X); cycles = 4; break;

        // --- MOV to Y ---
        case 0x8D: m_spc_Y = spc_fetch(); set_zn(m_spc_Y); cycles = 2; break;
        case 0xEB: m_spc_Y = spc_read(addr_dp()); set_zn(m_spc_Y); cycles = 3; break;
        case 0xFB: m_spc_Y = spc_read(addr_dp_x()); set_zn(m_spc_Y); cycles = 4; break;
        case 0xEC: m_spc_Y = spc_read(spc_fetch16()); set_zn(m_spc_Y); cycles = 4; break;

        // --- MOV from A ---
        case 0xC6: spc_write(dp_addr(m_spc_X), m_spc_A); cycles = 4; break;
        case 0xAF: spc_write(dp_addr(m_spc_X++), m_spc_A); cycles = 4; break;
        case 0xC4: spc_write(addr_dp(), m_spc_A); cycles = 4; break;
        case 0xD4: spc_write(addr_dp_x(), m_spc_A); cycles = 5; break;
        case 0xC5: spc_write(spc_fetch16(), m_spc_A); cycles = 5; break;
        case 0xD5: spc_write(addr_abs_x(), m_spc_A); cycles = 6; break;
        case 0xD6: spc_write(addr_abs_y(), m_spc_A); cycles = 6; break;
        case 0xC7: spc_write(addr_idx_ind_x(), m_spc_A); cycles = 7; break;
        case 0xD7: spc_write(addr_ind_idx_y(), m_spc_A); cycles = 7; break; // MOV [dp]+Y, A

        // --- MOV from X ---
        case 0xD8: spc_write(addr_dp(), m_spc_X); cycles = 4; break;
        case 0xD9: spc_write(addr_dp_y(), m_spc_X); cycles = 5; break;
        case 0xC9: spc_write(spc_fetch16(), m_spc_X); cycles = 5; break;

        // --- MOV from Y ---
        case 0xCB: spc_write(addr_dp(), m_spc_Y); cycles = 4; break;
        case 0xDB: spc_write(addr_dp_x(), m_spc_Y); cycles = 5; break;
        case 0xCC: spc_write(spc_fetch16(), m_spc_Y); cycles = 5; break;

        // --- Register & Direct Page MOV ---
        case 0x7D: m_spc_A = m_spc_X; set_zn(m_spc_A); cycles = 2; break;
        case 0xDD: m_spc_A = m_spc_Y; set_zn(m_spc_A); cycles = 2; break;
        case 0x5D: m_spc_X = m_spc_A; set_zn(m_spc_X); cycles = 2; break;
        case 0xFD: m_spc_Y = m_spc_A; set_zn(m_spc_Y); cycles = 2; break;
        case 0x9D: m_spc_X = m_spc_SP; set_zn(m_spc_X); cycles = 2; break;
        case 0xBD: m_spc_SP = m_spc_X; cycles = 2; break;
        case 0xFA: { uint16_t s = addr_dp(); uint16_t d = addr_dp(); spc_write(d, spc_read(s)); cycles = 5; break; }
        case 0x8F: { uint8_t imm = spc_fetch(); uint16_t d = addr_dp(); spc_write(d, imm); cycles = 5; break; }

        // --- Shifts & Rotates ---
        case 0x0B: { uint16_t a = addr_dp(); spc_write(a, op_asl(spc_read(a))); cycles = 4; break; }
        case 0x1B: { uint16_t a = addr_dp_x(); spc_write(a, op_asl(spc_read(a))); cycles = 5; break; }
        case 0x0C: { uint16_t a = spc_fetch16(); spc_write(a, op_asl(spc_read(a))); cycles = 5; break; }
        case 0x1C: m_spc_A = op_asl(m_spc_A); cycles = 2; break;

        case 0x4B: { uint16_t a = addr_dp(); spc_write(a, op_lsr(spc_read(a))); cycles = 4; break; }
        case 0x5B: { uint16_t a = addr_dp_x(); spc_write(a, op_lsr(spc_read(a))); cycles = 5; break; }
        case 0x4C: { uint16_t a = spc_fetch16(); spc_write(a, op_lsr(spc_read(a))); cycles = 5; break; }
        case 0x5C: m_spc_A = op_lsr(m_spc_A); cycles = 2; break;

        case 0x2B: { uint16_t a = addr_dp(); spc_write(a, op_rol(spc_read(a))); cycles = 4; break; }
        case 0x3B: { uint16_t a = addr_dp_x(); spc_write(a, op_rol(spc_read(a))); cycles = 5; break; }
        case 0x2C: { uint16_t a = spc_fetch16(); spc_write(a, op_rol(spc_read(a))); cycles = 5; break; }
        case 0x3C: m_spc_A = op_rol(m_spc_A); cycles = 2; break;

        case 0x6B: { uint16_t a = addr_dp(); spc_write(a, op_ror(spc_read(a))); cycles = 4; break; }
        case 0x7B: { uint16_t a = addr_dp_x(); spc_write(a, op_ror(spc_read(a))); cycles = 5; break; }
        case 0x6C: { uint16_t a = spc_fetch16(); spc_write(a, op_ror(spc_read(a))); cycles = 5; break; }
        case 0x7C: m_spc_A = op_ror(m_spc_A); cycles = 2; break;

        // --- INC / DEC ---
        case 0xAB: { uint16_t a = addr_dp(); spc_write(a, op_inc(spc_read(a))); cycles = 4; break; }
        case 0xBB: { uint16_t a = addr_dp_x(); spc_write(a, op_inc(spc_read(a))); cycles = 5; break; }
        case 0xAC: { uint16_t a = spc_fetch16(); spc_write(a, op_inc(spc_read(a))); cycles = 5; break; }
        case 0xBC: m_spc_A = op_inc(m_spc_A); cycles = 2; break;
        case 0x3D: m_spc_X = op_inc(m_spc_X); cycles = 2; break;
        case 0xFC: m_spc_Y = op_inc(m_spc_Y); cycles = 2; break;

        case 0x8B: { uint16_t a = addr_dp(); spc_write(a, op_dec(spc_read(a))); cycles = 4; break; }
        case 0x9B: { uint16_t a = addr_dp_x(); spc_write(a, op_dec(spc_read(a))); cycles = 5; break; }
        case 0x8C: { uint16_t a = spc_fetch16(); spc_write(a, op_dec(spc_read(a))); cycles = 5; break; }
        case 0x9C: m_spc_A = op_dec(m_spc_A); cycles = 2; break;
        case 0x1D: m_spc_X = op_dec(m_spc_X); cycles = 2; break;
        case 0xDC: m_spc_Y = op_dec(m_spc_Y); cycles = 2; break;

        // --- 16-Bit Word Operations ---
        case 0xBA: {
            uint8_t dp = spc_fetch();
            m_spc_A = spc_read(dp_addr(dp));
            m_spc_Y = spc_read(dp_addr(static_cast<uint8_t>(dp + 1)));
            set_zn16(m_spc_A | (static_cast<uint16_t>(m_spc_Y) << 8));
            cycles = 5;
            break;
        }
        case 0xDA: {
            uint8_t dp = spc_fetch();
            spc_write(dp_addr(dp), m_spc_A);
            spc_write(dp_addr(static_cast<uint8_t>(dp + 1)), m_spc_Y);
            cycles = 5;
            break;
        }
        case 0x3A: { // INCW dp
            uint8_t dp = spc_fetch();
            uint16_t low_a = dp_addr(dp);
            uint16_t high_a = dp_addr(static_cast<uint8_t>(dp + 1));
            uint16_t val = static_cast<uint16_t>((spc_read(low_a) | (spc_read(high_a) << 8)) + 1);
            spc_write(low_a, static_cast<uint8_t>(val & 0xFF));
            spc_write(high_a, static_cast<uint8_t>(val >> 8));
            set_zn16(val);
            cycles = 6;
            break;
        }
        case 0x1A: { // DECW dp
            uint8_t dp = spc_fetch();
            uint16_t low_a = dp_addr(dp);
            uint16_t high_a = dp_addr(static_cast<uint8_t>(dp + 1));
            uint16_t val = static_cast<uint16_t>((spc_read(low_a) | (spc_read(high_a) << 8)) - 1);
            spc_write(low_a, static_cast<uint8_t>(val & 0xFF));
            spc_write(high_a, static_cast<uint8_t>(val >> 8));
            set_zn16(val);
            cycles = 6;
            break;
        }
        case 0x7A: { // ADDW YA, dp
            uint8_t dp = spc_fetch();
            uint16_t ya = m_spc_A | (static_cast<uint16_t>(m_spc_Y) << 8);
            uint16_t m  = spc_read(dp_addr(dp)) | (static_cast<uint16_t>(spc_read(dp_addr(static_cast<uint8_t>(dp + 1)))) << 8);
            uint32_t sum = ya + m;
            flag_v = (~(ya ^ m) & (ya ^ sum) & 0x8000) != 0;
            flag_h = ((ya & 0x0FFF) + (m & 0x0FFF)) > 0x0FFF;
            flag_c = sum > 0xFFFF;
            uint16_t res = static_cast<uint16_t>(sum & 0xFFFF);
            m_spc_A = static_cast<uint8_t>(res & 0xFF);
            m_spc_Y = static_cast<uint8_t>(res >> 8);
            set_zn16(res);
            cycles = 5;
            break;
        }
        case 0x9A: { // SUBW YA, dp
            uint8_t dp = spc_fetch();
            uint16_t ya = m_spc_A | (static_cast<uint16_t>(m_spc_Y) << 8);
            uint16_t m  = spc_read(dp_addr(dp)) | (static_cast<uint16_t>(spc_read(dp_addr(static_cast<uint8_t>(dp + 1)))) << 8);
            int32_t diff = static_cast<int32_t>(ya) - static_cast<int32_t>(m);
            flag_v = ((ya ^ m) & (ya ^ diff) & 0x8000) != 0;
            flag_h = (ya & 0x0FFF) >= (m & 0x0FFF);
            flag_c = diff >= 0;
            uint16_t res = static_cast<uint16_t>(diff & 0xFFFF);
            m_spc_A = static_cast<uint8_t>(res & 0xFF);
            m_spc_Y = static_cast<uint8_t>(res >> 8);
            set_zn16(res);
            cycles = 5;
            break;
        }
        case 0x5A: { // CMPW YA, dp
            uint8_t dp = spc_fetch();
            uint16_t ya = m_spc_A | (static_cast<uint16_t>(m_spc_Y) << 8);
            uint16_t m  = spc_read(dp_addr(dp)) | (static_cast<uint16_t>(spc_read(dp_addr(static_cast<uint8_t>(dp + 1)))) << 8);
            int32_t diff = static_cast<int32_t>(ya) - static_cast<int32_t>(m);
            flag_c = diff >= 0;
            set_zn16(static_cast<uint16_t>(diff & 0xFFFF));
            cycles = 4;
            break;
        }
        case 0xCF: { // MUL YA
            uint16_t res = static_cast<uint16_t>(m_spc_Y) * static_cast<uint16_t>(m_spc_A);
            m_spc_A = static_cast<uint8_t>(res & 0xFF);
            m_spc_Y = static_cast<uint8_t>(res >> 8);
            set_zn(m_spc_Y);
            cycles = 9;
            break;
        }
        case 0x9E: { // DIV YA, X
            uint16_t ya = m_spc_A | (static_cast<uint16_t>(m_spc_Y) << 8);
            flag_h = (m_spc_Y & 0x0F) >= (m_spc_X & 0x0F);
            flag_v = (m_spc_Y >= m_spc_X);
            if (m_spc_Y < (m_spc_X << 1)) {
                m_spc_A = static_cast<uint8_t>((ya / m_spc_X) & 0xFF);
                m_spc_Y = static_cast<uint8_t>((ya % m_spc_X) & 0xFF);
            } else {
                m_spc_A = static_cast<uint8_t>(255 - (ya - (m_spc_X << 9)) / (256 - m_spc_X));
                m_spc_Y = static_cast<uint8_t>(m_spc_X + (ya - (m_spc_X << 9)) % (256 - m_spc_X));
            }
            set_zn(m_spc_A);
            cycles = 12;
            break;
        }

        // --- Branches & Loop Helpers ---
        case 0x2F: cycles = 2; branch_if(true, cycles); break;     // BRA
        case 0x10: cycles = 2; branch_if(!flag_n, cycles); break;  // BPL
        case 0x30: cycles = 2; branch_if(flag_n, cycles); break;   // BMI
        case 0x50: cycles = 2; branch_if(!flag_v, cycles); break;  // BVC
        case 0x70: cycles = 2; branch_if(flag_v, cycles); break;   // BVS
        case 0x90: cycles = 2; branch_if(!flag_c, cycles); break;  // BCC
        case 0xB0: cycles = 2; branch_if(flag_c, cycles); break;   // BCS
        case 0xD0: cycles = 2; branch_if(!flag_z, cycles); break;  // BNE
        case 0xF0: cycles = 2; branch_if(flag_z, cycles); break;   // BEQ

        case 0x2E: { uint8_t v = spc_read(addr_dp()); cycles = 5; branch_if(m_spc_A != v, cycles); break; }
        case 0xDE: { uint8_t v = spc_read(addr_dp_x()); cycles = 6; branch_if(m_spc_A != v, cycles); break; }
        case 0x6E: { uint16_t a = addr_dp(); uint8_t v = spc_read(a) - 1; spc_write(a, v); cycles = 5; branch_if(v != 0, cycles); break; }
        case 0xFE: { m_spc_Y--; cycles = 4; branch_if(m_spc_Y != 0, cycles); break; }

        // --- Jumps, Calls & Returns ---
        case 0x5F: m_spc_PC = spc_fetch16(); cycles = 3; break;
        case 0x1F: m_spc_PC = spc_read16(static_cast<uint16_t>(spc_fetch16() + m_spc_X)); cycles = 6; break;
        case 0x3F: { uint16_t dst = spc_fetch16(); push16(m_spc_PC); m_spc_PC = dst; cycles = 8; break; }
        case 0x4F: { uint8_t up = spc_fetch(); push16(m_spc_PC); m_spc_PC = 0xFF00 | up; cycles = 6; break; }
        case 0x0F: { push16(m_spc_PC); push8(get_psw()); flag_b = true; flag_i = false; m_spc_PC = spc_read16(0xFFDE); cycles = 8; break; }
        case 0x6F: m_spc_PC = pop16(); cycles = 5; break;
        case 0x7F: set_psw(pop8()); m_spc_PC = pop16(); cycles = 6; break;

        // --- Stack & Flags ---
        case 0x2D: push8(m_spc_A); cycles = 4; break;
        case 0x4D: push8(m_spc_X); cycles = 4; break;
        case 0x6D: push8(m_spc_Y); cycles = 4; break;
        case 0x0D: push8(get_psw()); cycles = 4; break;
        case 0xAE: m_spc_A = pop8(); cycles = 4; break;
        case 0xCE: m_spc_X = pop8(); cycles = 4; break;
        case 0xEE: m_spc_Y = pop8(); cycles = 4; break;
        case 0x8E: set_psw(pop8()); cycles = 4; break;

        case 0x20: flag_p = false; cycles = 2; break;
        case 0x40: flag_p = true;  cycles = 2; break;
        case 0x60: flag_c = false; cycles = 2; break;
        case 0x80: flag_c = true;  cycles = 2; break;
        case 0xE0: flag_v = false; flag_h = false; cycles = 2; break;
        case 0xED: flag_c = !flag_c; cycles = 3; break;
        case 0xA0: flag_i = true;  cycles = 3; break;
        case 0xC0: flag_i = false; cycles = 3; break;

        // --- Bitwise / Carry Operations ---
        case 0x0E: { uint16_t a = spc_fetch16(); uint8_t v = spc_read(a); set_zn(m_spc_A - v); spc_write(a, v | m_spc_A); cycles = 6; break; }
        case 0x4E: { uint16_t a = spc_fetch16(); uint8_t v = spc_read(a); set_zn(m_spc_A - v); spc_write(a, v & ~m_spc_A); cycles = 6; break; }
        case 0x0A: { uint16_t m = spc_fetch16(); flag_c = flag_c || ((spc_read(m & 0x1FFF) >> (m >> 13)) & 1); cycles = 5; break; }
        case 0x2A: { uint16_t m = spc_fetch16(); flag_c = flag_c || !((spc_read(m & 0x1FFF) >> (m >> 13)) & 1); cycles = 5; break; }
        case 0x4A: { uint16_t m = spc_fetch16(); flag_c = flag_c && ((spc_read(m & 0x1FFF) >> (m >> 13)) & 1); cycles = 4; break; }
        case 0x6A: { uint16_t m = spc_fetch16(); flag_c = flag_c && !((spc_read(m & 0x1FFF) >> (m >> 13)) & 1); cycles = 4; break; }
        case 0x8A: { uint16_t m = spc_fetch16(); flag_c = flag_c != (((spc_read(m & 0x1FFF) >> (m >> 13)) & 1) != 0); cycles = 5; break; }
        case 0xAA: { uint16_t m = spc_fetch16(); flag_c = ((spc_read(m & 0x1FFF) >> (m >> 13)) & 1) != 0; cycles = 4; break; }
        case 0xCA: {
            uint16_t m = spc_fetch16();
            uint16_t a = m & 0x1FFF;
            uint8_t b = m >> 13;
            uint8_t v = spc_read(a);
            if (flag_c) v |= (1 << b); else v &= ~(1 << b);
            spc_write(a, v);
            cycles = 6;
            break;
        }
        case 0xEA: {
            uint16_t m = spc_fetch16();
            uint16_t a = m & 0x1FFF;
            uint8_t b = m >> 13;
            spc_write(a, spc_read(a) ^ (1 << b));
            cycles = 5;
            break;
        }

        // --- Misc ---
        case 0x9F: m_spc_A = static_cast<uint8_t>((m_spc_A >> 4) | (m_spc_A << 4)); set_zn(m_spc_A); cycles = 5; break; // XCN A
        case 0xDF: { // DAA A
            if (flag_c || m_spc_A > 0x99) { m_spc_A += 0x60; flag_c = true; }
            if (flag_h || (m_spc_A & 0x0F) > 0x09) { m_spc_A += 0x06; }
            set_zn(m_spc_A);
            cycles = 3;
            break;
        }
        case 0xBE: { // DAS A
            if (!flag_c || m_spc_A > 0x99) { m_spc_A -= 0x60; flag_c = false; }
            if (!flag_h || (m_spc_A & 0x0F) > 0x09) { m_spc_A -= 0x06; }
            set_zn(m_spc_A);
            cycles = 3;
            break;
        }
        case 0xEF: cycles = 3; break; // SLEEP
        case 0xFF: cycles = 3; break; // STOP
    }

    return cycles;
}