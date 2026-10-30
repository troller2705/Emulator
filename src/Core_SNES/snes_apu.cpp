#include "../../include/Core_SNES/snes_apu.h"
#include <algorithm>

const std::array<uint8_t, 64> SNES_APU::s_ipl_rom = {
    0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
    0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
    0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
    0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
};

SNES_APU::SNES_APU() {
    m_audio_buffer.reserve(1024);
    reset();
}

void SNES_APU::reset() {
    m_aram.fill(0);
    m_dsp_regs.fill(0);
    m_cpu_to_apu.fill(0);
    m_apu_to_cpu.fill(0);
    m_audio_buffer.clear();

    // Copy IPL ROM into top 64 bytes of ARAM ($FFC0 - $FFFF)
    std::copy(s_ipl_rom.begin(), s_ipl_rom.end(), m_aram.begin() + 0xFFC0);

    // Signal readiness to 65816 CPU: Port 0 = 0xAA, Port 1 = 0xBB
    m_apu_to_cpu[0] = 0xAA;
    m_apu_to_cpu[1] = 0xBB;
    m_ipl_state = IPLState::WaitForCC;
    m_transfer_addr = 0;
    m_last_aram_write_addr = 0;
    m_expected_counter = 0;
    m_pending_ipl_ready = false;
    m_sample_counter = 0;
}

void SNES_APU::step(int cpu_cycles) {
    m_sample_counter += cpu_cycles;
    while (m_sample_counter >= 80) {
        m_sample_counter -= 80;
        m_audio_buffer.push_back(0.0f);
    }
}

uint8_t SNES_APU::cpuReadPort(uint16_t addr) {
    uint8_t port = addr & 0x03;
    uint8_t val = m_apu_to_cpu[port];

    // Once the CPU verifies the echoed termination byte on Port 0 ($2140),
    // immediately restore 0xAA / 0xBB so subsequent LoadSPCData calls pass 00:8082
    if (port == 0 && m_pending_ipl_ready) {
        m_pending_ipl_ready = false;
        m_apu_to_cpu[0] = 0xAA;
        m_apu_to_cpu[1] = 0xBB;
    }

    return val;
}

void SNES_APU::cpuWritePort(uint16_t addr, uint8_t val) {
    uint8_t port = addr & 0x03;
    m_cpu_to_apu[port] = val;

    // Handle 16-bit STA $2140 writes where Port 0 ($2140) is written before Port 1 ($2141)
    if (port == 1 && m_ipl_state == IPLState::TransferData) {
        m_aram[m_last_aram_write_addr] = val;
    }

    if (port == 0) {
        updateIPLHandshake();
    }
}

void SNES_APU::updateIPLHandshake() {
    uint8_t port0 = m_cpu_to_apu[0];

    if (m_ipl_state == IPLState::WaitForCC) {
        if (port0 == 0xCC) {
            m_transfer_addr = m_cpu_to_apu[2] | (static_cast<uint16_t>(m_cpu_to_apu[3]) << 8);
            uint8_t command = m_cpu_to_apu[1];

            m_apu_to_cpu[0] = 0xCC;

            if (command != 0) {
                m_expected_counter = 0;
                m_ipl_state = IPLState::TransferData;
            } else {
                m_pending_ipl_ready = true;
            }
        }
    } else if (m_ipl_state == IPLState::TransferData) {
        if (port0 == m_expected_counter) {
            m_last_aram_write_addr = m_transfer_addr;
            m_aram[m_transfer_addr++] = m_cpu_to_apu[1];
            m_apu_to_cpu[0] = port0;
            m_expected_counter++;
        } else if (static_cast<int8_t>(port0 - m_expected_counter) > 0) {
            m_transfer_addr = m_cpu_to_apu[2] | (static_cast<uint16_t>(m_cpu_to_apu[3]) << 8);
            uint8_t command = m_cpu_to_apu[1];

            // Echo the termination / new-block byte back on Port 0
            m_apu_to_cpu[0] = port0;

            if (command == 0) {
                // Upload finished! Wait for the CPU to read the echoed termination byte once,
                // then automatically restore 0xAA / 0xBB for the next upload pack.
                m_ipl_state = IPLState::WaitForCC;
                m_pending_ipl_ready = true;
            } else {
                m_expected_counter = 0;
            }
        }
    }
}