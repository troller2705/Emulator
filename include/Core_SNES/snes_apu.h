#pragma once
#include <cstdint>
#include <array>
#include <vector>

class SNES_APU {
public:
    SNES_APU();
    void reset();

    // Steps the SPC700 and DSP based on master CPU cycles
    void step(int cpu_cycles);

    // SNES Main CPU Side I/O Ports ($2140 - $2143)
    uint8_t cpuReadPort(uint16_t addr);
    void cpuWritePort(uint16_t addr, uint8_t val);

    // Audio Buffer Interface for SNESCore / Launcher
    const float* get_audio_buffer() const { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const { return m_audio_buffer.size(); }
    void clear_buffer() { m_audio_buffer.clear(); }

    // Direct ARAM inspection for debugging
    uint8_t readARAM(uint16_t addr) const { return m_aram[addr]; }

private:
    // --- SPC700 CPU State ---
    int      m_spc_cycles_left = 0;
    uint8_t  m_spc_A = 0;
    uint8_t  m_spc_X = 0;
    uint8_t  m_spc_Y = 0;
    uint8_t  m_spc_SP = 0;
    uint16_t m_spc_PC = 0;

    // PSW Flags
    bool flag_n = false;
    bool flag_v = false;
    bool flag_p = false; // Direct Page (0 = 0x0000, 1 = 0x0100)
    bool flag_b = false; // Break
    bool flag_h = false; // Half Carry
    bool flag_i = false; // Interrupt Enable
    bool flag_z = false;
    bool flag_c = false;

    uint8_t get_psw() const;
    void set_psw(uint8_t val);
    void set_zn(uint8_t val);
    void set_zn16(uint16_t val);

    int spc_step();
    uint8_t spc_fetch();
    uint16_t spc_fetch16();
    uint8_t spc_read(uint16_t addr);
    void spc_write(uint16_t addr, uint8_t val);
    uint16_t spc_read16(uint16_t addr);
    void spc_write16(uint16_t addr, uint16_t val);

    // Addressing & Stack Helpers
    uint16_t dp_addr(uint8_t offset) const { return (flag_p ? 0x0100 : 0x0000) | offset; }
    uint16_t addr_dp();
    uint16_t addr_dp_x();
    uint16_t addr_dp_y();
    uint16_t addr_abs_x();
    uint16_t addr_abs_y();
    uint16_t addr_idx_ind_x();
    uint16_t addr_ind_idx_y();
    void push8(uint8_t val);
    void push16(uint16_t val);
    uint8_t pop8();
    uint16_t pop16();

    // ALU Helpers
    uint8_t op_adc(uint8_t a, uint8_t b);
    uint8_t op_sbc(uint8_t a, uint8_t b);
    void op_cmp(uint8_t a, uint8_t b);
    uint8_t op_and(uint8_t a, uint8_t b);
    uint8_t op_or(uint8_t a, uint8_t b);
    uint8_t op_eor(uint8_t a, uint8_t b);
    uint8_t op_asl(uint8_t val);
    uint8_t op_lsr(uint8_t val);
    uint8_t op_rol(uint8_t val);
    uint8_t op_ror(uint8_t val);
    uint8_t op_inc(uint8_t val);
    uint8_t op_dec(uint8_t val);
    void branch_if(bool cond, int& cycles);

    // --- SPC700 Internal Hardware ---
    std::array<uint8_t, 0x10000> m_aram{};
    std::array<uint8_t, 4> m_cpu_to_apu{}; // Written by SNES, Read by SPC ($00F4-$00F7)
    std::array<uint8_t, 4> m_apu_to_cpu{}; // Written by SPC, Read by SNES ($00F4-$00F7)

    uint8_t m_control = 0;    // $00F1
    uint8_t m_dsp_addr = 0;   // $00F2

    // --- Timers ---
    struct SPCTimer {
        int target = 256;
        int counter = 0;
        uint8_t up_counter = 0;
        bool enabled = false;
        int prescaler = 0;
    };
    std::array<SPCTimer, 3> m_timers{};
    void tick_timers();

    // --- DSP & Voice Synthesizer State ---
    enum class EnvState { Attack, Decay, Sustain, Release };

    struct Voice {
        bool active = false;
        uint16_t sample_addr = 0;
        uint32_t current_addr = 0;
        uint16_t loop_addr = 0;
        uint8_t block_header = 0;
        int nibble_index = 16;
        int prev1 = 0;
        int prev2 = 0;
        int current_val = 0;
        uint16_t pitch = 0;
        uint32_t phase = 0;
        int envelope = 0;
        EnvState env_state = EnvState::Release;
        int volume_left = 0;
        int volume_right = 0;
    };
    std::array<Voice, 8> m_voices{};

    // DSP Global Rate Counter, Noise LFSR & Echo FIR State
    int m_dsp_counter = 0;
    int m_noise_lfsr = 0x4000;
    uint16_t m_echo_base = 0;
    int m_echo_offset = 0;
    int m_echo_length = 4;
    std::array<int, 8> m_fir_left{};
    std::array<int, 8> m_fir_right{};
    int m_fir_idx = 0;

    bool poll_dsp_rate(int rate) const;
    void step_dsp();
    void update_envelope(int voice_idx);
    int decode_brr_sample(int voice_idx);

    std::array<uint8_t, 0x80>  m_dsp_regs{};
    std::vector<float>         m_audio_buffer;
    int m_cpu_cycle_accum = 0;
    int m_dsp_sample_accum = 0;
    int m_resample_accum = 0;

    // SPC700 Internal 64-byte IPL Boot ROM ($FFC0 - $FFFF)
    static const std::array<uint8_t, 64> s_ipl_rom;
    static const std::array<int, 32> s_rate_period;
    static const std::array<int, 32> s_rate_offset;
};