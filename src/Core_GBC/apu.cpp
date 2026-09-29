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
        case 0xFF16: return m_nr21 | 0x3F;
        case 0xFF17: return m_nr22;
        case 0xFF19: return m_nr24 | 0xBF;
        case 0xFF24: return m_nr50;
        case 0xFF25: return m_nr51;
        case 0xFF26: return m_nr52 | 0x70;
    }
    return 0xFF;
}

void APU::write_register(uint16_t address, uint8_t value) {
    // If sound is globally disabled, ignore all writes except NR52
    if ((m_nr52 & 0x80) == 0 && address != 0xFF26) return;

    switch (address) {
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
            
        case 0xFF24: m_nr50 = value; break;
        case 0xFF25: m_nr51 = value; break;
        case 0xFF26: 
            m_nr52 = value; 
            if ((m_nr52 & 0x80) == 0) {
                // Shut down all channels if master switch is flipped off
                m_ch2_enabled = false;
            }
            break;
    }
}

float APU::get_channel_2_sample() {
    if (!m_ch2_enabled || m_ch2_current_volume == 0) return 0.0f;

    uint8_t duty_idx = (m_nr21 >> 6) & 0x03;
    uint8_t amplitude = DUTY_CYCLES[duty_idx][m_ch2_duty_step];

    float out = amplitude ? 1.0f : -1.0f;

    // Multiply by our fading envelope volume instead of the static NR22 volume
    return out * (m_ch2_current_volume / 15.0f) * 0.1f;
}

void APU::tick(int cycles) {
    // --- FRAME SEQUENCER (512 Hz) ---
    m_frame_sequencer_timer -= cycles;
    if (m_frame_sequencer_timer <= 0) {
        m_frame_sequencer_timer += 8192;

        // 1. Clock the Length Counter (256 Hz - Steps 0, 2, 4, 6)
        if (m_frame_sequencer_step % 2 == 0) {
            // Check if the game enabled the length cutoff (Bit 6 of NR24)
            if (m_nr24 & 0x40) {
                if (m_ch2_length_timer > 0) {
                    m_ch2_length_timer--;
                    if (m_ch2_length_timer == 0) {
                        m_ch2_enabled = false; // Turn the note off!
                    }
                }
            }
        }

        // 2. Clock the Volume Envelope (64 Hz - Step 7)
        if (m_frame_sequencer_step == 7) {
            if (m_ch2_envelope_timer > 0) {
                m_ch2_envelope_timer--;
            }

            if (m_ch2_envelope_timer == 0) {
                m_ch2_envelope_timer = m_nr22 & 0x07; // Reload period

                if (m_ch2_envelope_timer > 0) { // If period is 0, envelope is disabled
                    // Bit 3 determines if volume increases or decreases
                    if (m_nr22 & 0x08) {
                        if (m_ch2_current_volume < 15) m_ch2_current_volume++;
                    } else {
                        if (m_ch2_current_volume > 0) m_ch2_current_volume--;
                    }
                }
            }
        }

        m_frame_sequencer_step++;
        if (m_frame_sequencer_step > 7) m_frame_sequencer_step = 0;
    }

    // 1. Step the internal hardware timers
    // The frequency is an 11-bit number made of NR23 (lower 8 bits) and NR24 (upper 3 bits)
    uint16_t frequency = m_nr23 | ((m_nr24 & 0x07) << 8);
    
    m_ch2_timer -= cycles;
    if (m_ch2_timer <= 0) {
        // Game Boy hardware frequency math
        m_ch2_timer += (2048 - frequency) * 4;
        
        m_ch2_duty_step++;
        if (m_ch2_duty_step > 7) m_ch2_duty_step = 0;
    }

    // 2. Step the downsampler to generate 44100Hz audio
    m_downsample_counter += cycles;
    while (m_downsample_counter >= CYCLES_PER_SAMPLE) {
        m_downsample_counter -= CYCLES_PER_SAMPLE;
        
        // Sum the channels (just Ch 2 for now)
        float sample = get_channel_2_sample();
        
        m_audio_buffer.push_back(sample);
    }
}