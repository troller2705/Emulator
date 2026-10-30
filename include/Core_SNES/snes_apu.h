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

    int spc_step();
    uint8_t spc_fetch();
    uint16_t spc_fetch16();
    uint8_t spc_read(uint16_t addr);
    void spc_write(uint16_t addr, uint8_t val);

    // --- SPC700 Internal Hardware ---
    std::array<uint8_t, 0x10000> m_aram{};
    std::array<uint8_t, 4> m_cpu_to_apu{}; // Written by SNES, Read by SPC ($00F4-$00F7)
    std::array<uint8_t, 4> m_apu_to_cpu{}; // Written by SPC, Read by SNES ($00F4-$00F7)

    uint8_t m_control = 0;    // $00F1
    uint8_t m_dsp_addr = 0;   // $00F2

    // --- Timers ---
    struct SPCTimer {
        uint8_t target = 0;
        uint8_t counter = 0;
        uint8_t up_counter = 0;
        bool enabled = false;
        int prescaler = 0;
    };
    std::array<SPCTimer, 3> m_timers{};
    void tick_timers();

    // --- Audio Rendering ---
    std::array<uint8_t, 0x80>  m_dsp_regs{};
    std::vector<float>         m_audio_buffer;
    int m_cpu_cycle_accum = 0;
    int m_dsp_sample_accum = 0;

    // SPC700 Internal 64-byte IPL Boot ROM ($FFC0 -$FFFF)
    static const std::array<uint8_t, 64> s_ipl_rom;
};