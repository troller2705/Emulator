#include "apu.h"

// The 4 square wave duty cycles (8 steps each)
static const uint8_t DUTY_CYCLES[4][8] = {
    {0,0,0,0,0,0,0,1}, // 12.5%
    {1,0,0,0,0,0,0,1}, // 25%
    {1,0,0,0,0,1,1,1}, // 50%
    {0,1,1,1,1,1,1,0}  // 75%
};

APU::APU() {
    // Pre-allocate buffer to avoid reallocation overhead during gameplay
    m_audio_buffer.reserve(1024); 
}

uint8_t APU::read_register(uint16_t address) const {
    switch (address) {
        case 0xFF10: return m_nr10 | 0x80;
        case 0xFF11: return m_nr11 | 0x3F;
        case 0xFF12: return m_nr12;
        case 0xFF14: return m_nr14 | 0xBF;
        case 0xFF16: return m_nr21 | 0x3F;
        case 0xFF17: return m_nr22;
        case 0xFF19: return m_nr24 | 0xBF;
        case 0xFF20: return m_nr41 | 0xFF;
        case 0xFF21: return m_nr42;
        case 0xFF22: return m_nr43;
        case 0xFF23: return m_nr44 | 0xBF;
        case 0xFF24: return m_nr50;
        case 0xFF25: return m_nr51;
        case 0xFF26: return m_nr52 | 0x70;
        case 0xFF1A: return m_nr30 | 0x7F;
        case 0xFF1B: return m_nr31 | 0xFF;
        case 0xFF1C: return m_nr32 | 0x9F;
        case 0xFF1E: return m_nr34 | 0xBF;
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                return m_wave_ram[address - 0xFF30];
            }
    }
    return 0xFF;
}

void APU::write_register(uint16_t address, uint8_t value) {
    // If sound is globally disabled, ignore all writes except NR52
    if ((m_nr52 & 0x80) == 0 && address != 0xFF26) return;

    switch (address) {
        case 0xFF10: m_nr10 = value; break;
        case 0xFF11:
            m_nr11 = value;
            m_ch1_length_timer = 64 - (value & 0x3F);
            break;
        case 0xFF12: m_nr12 = value; break;
        case 0xFF13: m_nr13 = value; break;
        case 0xFF14:
            m_nr14 = value;
            if (value & 0x80) { // TRIGGER CH1
                m_ch1_enabled = true;
                if (m_ch1_length_timer == 0) m_ch1_length_timer = 64;

                m_ch1_current_volume = (m_nr12 >> 4) & 0x0F;
                m_ch1_envelope_timer = m_nr12 & 0x07;

                // SWEEP INITIALIZATION
                m_ch1_shadow_frequency = m_nr13 | ((m_nr14 & 0x07) << 8);
                uint8_t sweep_period = (m_nr10 >> 4) & 0x07;
                m_ch1_sweep_timer = (sweep_period == 0) ? 8 : sweep_period;
                m_ch1_sweep_enabled = (sweep_period > 0 || (m_nr10 & 0x07) > 0);
            }
            break;
        case 0xFF16: // NR21
            m_nr21 = value;
            // Length timer is 64 minus the bottom 6 bits
            m_ch2_length_timer = 64 - (value & 0x3F);
            break;
        case 0xFF17: m_nr22 = value; break;
        case 0xFF18: m_nr23 = value; break;
        case 0xFF19: // NR24
            m_nr24 = value;
            // Bit 7 is the Trigger bit (restarts the note)
            if (value & 0x80) {
                m_ch2_enabled = true;

                // If length is 0, it resets to 64
                if (m_ch2_length_timer == 0) m_ch2_length_timer = 64;

                // Reload the volume from NR22
                m_ch2_current_volume = (m_nr22 >> 4) & 0x0F;
                m_ch2_envelope_timer = m_nr22 & 0x07;
            }
            break;
        case 0xFF20:
            m_nr41 = value;
            m_ch4_length_timer = 64 - (value & 0x3F);
            break;
        case 0xFF21: m_nr42 = value; break;
        case 0xFF22: m_nr43 = value; break;
        case 0xFF23:
            m_nr44 = value;
            if (value & 0x80) { // TRIGGER CH4
                m_ch4_enabled = true;
                if (m_ch4_length_timer == 0) m_ch4_length_timer = 64;

                m_ch4_current_volume = (m_nr42 >> 4) & 0x0F;
                m_ch4_envelope_timer = m_nr42 & 0x07;

                m_lfsr = 0x7FFF; // Reset the noise generator
            }
            break;
        case 0xFF24: m_nr50 = value; break;
        case 0xFF25: m_nr51 = value; break;
        case 0xFF26: 
            m_nr52 = value; 
            if ((m_nr52 & 0x80) == 0) {
                // Shut down all channels if master switch is flipped off
                m_ch2_enabled = false;
            }
            break;
        case 0xFF1A: m_nr30 = value; break;
        case 0xFF1B:
            m_nr31 = value;
            m_ch3_length_timer = 256 - value; // Ch3 length is 256, not 64!
            break;
        case 0xFF1C: m_nr32 = value; break;
        case 0xFF1D: m_nr33 = value; break;
        case 0xFF1E:
            m_nr34 = value;
            if (value & 0x80) { // TRIGGER CH3
                m_ch3_enabled = true;
                m_ch3_position = 0; // Reset wave position
                if (m_ch3_length_timer == 0) m_ch3_length_timer = 256;
            }
            break;
        default:
            if (address >= 0xFF30 && address <= 0xFF3F) {
                m_wave_ram[address - 0xFF30] = value;
            }
            break;
    }
}

float APU::get_channel_1_sample() {
    if (!m_ch1_enabled || m_ch1_current_volume == 0) return 0.0f;

    uint8_t duty_idx = (m_nr11 >> 6) & 0x03;
    uint8_t amplitude = DUTY_CYCLES[duty_idx][m_ch1_duty_step];

    float out = amplitude ? 1.0f : -1.0f;
    return out * (m_ch1_current_volume / 15.0f) * 0.1f;
}

float APU::get_channel_2_sample() {
    if (!m_ch2_enabled || m_ch2_current_volume == 0) return 0.0f;

    uint8_t duty_idx = (m_nr21 >> 6) & 0x03;
    uint8_t amplitude = DUTY_CYCLES[duty_idx][m_ch2_duty_step];

    float out = amplitude ? 1.0f : -1.0f;

    // Multiply by our fading envelope volume instead of the static NR22 volume
    return out * (m_ch2_current_volume / 15.0f) * 0.1f;
}

float APU::get_channel_3_sample() {
    // If the channel is disabled or the DAC is off (Bit 7 of NR30), output 0
    if (!m_ch3_enabled || (m_nr30 & 0x80) == 0) return 0.0f;

    // Volume shift code (0=Mute, 1=100%, 2=50%, 3=25%)
    uint8_t volume_code = (m_nr32 >> 5) & 0x03;
    if (volume_code == 0) return 0.0f;

    // Read the current byte from Wave RAM (m_ch3_position / 2)
    uint8_t byte = m_wave_ram[m_ch3_position / 2];

    // Extract the upper or lower 4-bit nibble
    uint8_t nibble = (m_ch3_position % 2 == 0) ? (byte >> 4) : (byte & 0x0F);

    // Apply the hardware volume shift
    if (volume_code == 2) nibble >>= 1; // 50% volume
    if (volume_code == 3) nibble >>= 2; // 25% volume

    // Convert the 0-15 nibble to a floating point PCM value between -1.0 and 1.0
    float out = (nibble / 7.5f) - 1.0f;
    return out * 0.1f;
}

float APU::get_channel_4_sample() {
    if (!m_ch4_enabled || m_ch4_current_volume == 0) return 0.0f;

    // The output is high if bit 0 of the LFSR is 0
    uint8_t amplitude = (~m_lfsr) & 0x01;

    float out = amplitude ? 1.0f : -1.0f;
    return out * (m_ch4_current_volume / 15.0f) * 0.1f;
}

void APU::tick(int cycles) {
    // --- FRAME SEQUENCER (512 Hz) ---
    m_frame_sequencer_timer -= cycles;
    if (m_frame_sequencer_timer <= 0) {
        m_frame_sequencer_timer += 8192;

        // 1. Clock the Length Counter (256 Hz - Steps 0, 2, 4, 6)
        if (m_frame_sequencer_step % 2 == 0) {
            // Channel 1 Length
            if (m_nr14 & 0x40) {
                if (m_ch1_length_timer > 0) {
                    m_ch1_length_timer--;
                    if (m_ch1_length_timer == 0) m_ch1_enabled = false;
                }
            }
            // Channel 2 Length
            if (m_nr24 & 0x40) {
                if (m_ch2_length_timer > 0) {
                    m_ch2_length_timer--;
                    if (m_ch2_length_timer == 0) m_ch2_enabled = false;
                }
            }
            // Channel 3 Length
            if (m_nr34 & 0x40) {
                if (m_ch3_length_timer > 0) {
                    m_ch3_length_timer--;
                    if (m_ch3_length_timer == 0) m_ch3_enabled = false;
                }
            }
            // Channel 4 Length
            if (m_nr44 & 0x40) {
                if (m_ch4_length_timer > 0) {
                    m_ch4_length_timer--;
                    if (m_ch4_length_timer == 0) m_ch4_enabled = false;
                }
            }
        }

        // 2. Clock the Sweep Unit (128 Hz - Steps 2 and 6)
        if (m_frame_sequencer_step == 2 || m_frame_sequencer_step == 6) {
            if (m_ch1_sweep_enabled && m_ch1_sweep_timer > 0) {
                m_ch1_sweep_timer--;
                if (m_ch1_sweep_timer == 0) {
                    uint8_t sweep_period = (m_nr10 >> 4) & 0x07;
                    m_ch1_sweep_timer = (sweep_period == 0) ? 8 : sweep_period;

                    uint8_t sweep_shift = m_nr10 & 0x07;
                    if (sweep_period > 0 && sweep_shift > 0) {
                        uint16_t new_freq = m_ch1_shadow_frequency >> sweep_shift;

                        if (m_nr10 & 0x08) { // Subtraction (Pitch goes down)
                            new_freq = m_ch1_shadow_frequency - new_freq;
                        } else {             // Addition (Pitch goes up)
                            new_freq = m_ch1_shadow_frequency + new_freq;
                        }

                        if (new_freq > 2047) {
                            m_ch1_enabled = false; // Overflow shuts the channel down
                        } else {
                            m_ch1_shadow_frequency = new_freq;
                            // Write the new frequency back into the registers
                            m_nr13 = new_freq & 0xFF;
                            m_nr14 = (m_nr14 & 0xF8) | ((new_freq >> 8) & 0x07);
                        }
                    }
                }
            }
        }

        // 3. Clock the Volume Envelope (64 Hz - Step 7)
        if (m_frame_sequencer_step == 7) {
            // Channel 1 Envelope
            if (m_ch1_envelope_timer > 0) m_ch1_envelope_timer--;
            if (m_ch1_envelope_timer == 0) {
                m_ch1_envelope_timer = m_nr12 & 0x07;
                if (m_ch1_envelope_timer > 0) {
                    if (m_nr12 & 0x08) {
                        if (m_ch1_current_volume < 15) m_ch1_current_volume++;
                    } else {
                        if (m_ch1_current_volume > 0) m_ch1_current_volume--;
                    }
                }
            }

            // Channel 2 Envelope
            if (m_ch2_envelope_timer > 0) m_ch2_envelope_timer--;
            if (m_ch2_envelope_timer == 0) {
                m_ch2_envelope_timer = m_nr22 & 0x07;
                if (m_ch2_envelope_timer > 0) {
                    if (m_nr22 & 0x08) {
                        if (m_ch2_current_volume < 15) m_ch2_current_volume++;
                    } else {
                        if (m_ch2_current_volume > 0) m_ch2_current_volume--;
                    }
                }
            }

            // Channel 4 Envelope
            if (m_ch4_envelope_timer > 0) m_ch4_envelope_timer--;
            if (m_ch4_envelope_timer == 0) {
                m_ch4_envelope_timer = m_nr42 & 0x07;
                if (m_ch4_envelope_timer > 0) {
                    if (m_nr42 & 0x08) {
                        if (m_ch4_current_volume < 15) m_ch4_current_volume++;
                    } else {
                        if (m_ch4_current_volume > 0) m_ch4_current_volume--;
                    }
                }
            }
        }

        m_frame_sequencer_step++;
        if (m_frame_sequencer_step > 7) m_frame_sequencer_step = 0;
    }

    // --- INTERNAL HARDWARE TIMERS ---

    // Channel 1
    uint16_t freq_1 = m_nr13 | ((m_nr14 & 0x07) << 8);
    m_ch1_timer -= cycles;
    if (m_ch1_timer <= 0) {
        m_ch1_timer += (2048 - freq_1) * 4;
        m_ch1_duty_step++;
        if (m_ch1_duty_step > 7) m_ch1_duty_step = 0;
    }

    // Channel 2
    uint16_t freq_2 = m_nr23 | ((m_nr24 & 0x07) << 8);
    m_ch2_timer -= cycles;
    if (m_ch2_timer <= 0) {
        m_ch2_timer += (2048 - freq_2) * 4;
        m_ch2_duty_step++;
        if (m_ch2_duty_step > 7) m_ch2_duty_step = 0;
    }

    // Channel 3
    uint16_t freq_3 = m_nr33 | ((m_nr34 & 0x07) << 8);
    m_ch3_timer -= cycles;
    if (m_ch3_timer <= 0) {
        m_ch3_timer += (2048 - freq_3) * 2;
        m_ch3_position++;
        if (m_ch3_position > 31) m_ch3_position = 0;
    }

    // Channel 4
    static const int divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    m_ch4_timer -= cycles;
    if (m_ch4_timer <= 0) {
        uint8_t divisor_code = m_nr43 & 0x07;
        uint8_t clock_shift = (m_nr43 >> 4) & 0x0F;
        m_ch4_timer += divisors[divisor_code] << clock_shift;

        // XOR bit 0 and 1
        uint8_t xor_result = (m_lfsr & 0x01) ^ ((m_lfsr >> 1) & 0x01);
        m_lfsr >>= 1; // Shift right

        // Put the XOR result into bit 14
        m_lfsr |= (xor_result << 14);

        // If "width mode" is 1 (Bit 3 of NR43), ALSO put the XOR result into bit 6 (creates harsher, metallic noise)
        if (m_nr43 & 0x08) {
            m_lfsr &= ~(1 << 6); // Clear bit 6
            m_lfsr |= (xor_result << 6);
        }
    }

    // --- AUDIO DOWNSAMPLING ---
    m_downsample_counter += cycles;
    while (m_downsample_counter >= CYCLES_PER_SAMPLE) {
        m_downsample_counter -= CYCLES_PER_SAMPLE;

        // Sum both square wave channels
        float sample1 = get_channel_1_sample();
        float sample2 = get_channel_2_sample();
        float sample3 = get_channel_3_sample();
        float sample4 = get_channel_4_sample();

        m_audio_buffer.push_back(sample1 + sample2 + sample3 + sample4);
    }
}