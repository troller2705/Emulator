#pragma once
#include <cstdint>
#include <array>
#include <vector>

class SNES_APU {
public:
    SNES_APU();
    void reset();

    void step(int cpu_cycles);

    // CPU Side I/O Ports ($2140 - $2143)
    uint8_t cpuReadPort(uint16_t addr);
    void cpuWritePort(uint16_t addr, uint8_t val);

    // Audio Buffer Interface for SNESCore / Launcher
    const float* get_audio_buffer() const { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const { return m_audio_buffer.size(); }
    void clear_buffer() { m_audio_buffer.clear(); }

    // Direct ARAM / DSP inspection for debugging
    uint8_t readARAM(uint16_t addr) const { return m_aram[addr]; }

private:
    enum class IPLState {
        WaitForCC,
        TransferData
    };

    void updateIPLHandshake();

    // 64 KB Audio RAM (SPC700 memory space) & 128 DSP Registers
    std::array<uint8_t, 0x10000> m_aram{};
    std::array<uint8_t, 0x80>    m_dsp_regs{};
    std::vector<float>           m_audio_buffer;

    // Bidirectional Communication Latches ($2140 - $2143)
    std::array<uint8_t, 4> m_cpu_to_apu{};
    std::array<uint8_t, 4> m_apu_to_cpu{};

    // IPL Boot Protocol State
    IPLState m_ipl_state = IPLState::WaitForCC;
    uint16_t m_transfer_addr = 0;
    uint16_t m_last_aram_write_addr = 0;
    uint8_t  m_expected_counter = 0;
    bool     m_pending_ipl_ready = false;
    int      m_sample_counter = 0;

    // SPC700 Internal 64-byte IPL Boot ROM ($FFC0 - $FFFF)
    static const std::array<uint8_t, 64> s_ipl_rom;
};