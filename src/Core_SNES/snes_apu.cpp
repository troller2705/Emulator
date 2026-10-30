#include "../../include/Core_SNES/snes_apu.h"
#include <algorithm>
#include <iostream>

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

    std::copy(s_ipl_rom.begin(), s_ipl_rom.end(), m_aram.begin() + 0xFFC0);

    // Initial SPC700 Boot State
    m_spc_PC = 0xFFC0; // Boot directly from IPL ROM
    m_spc_A = 0; m_spc_X = 0; m_spc_Y = 0;
    m_spc_SP = 0xEF;
    set_psw(0x00);

    m_control = 0xB0; // IPL ROM enabled by default (Bit 7)
    m_dsp_addr = 0;

    m_cpu_cycle_accum = 0;
    m_dsp_sample_accum = 0;

    for (auto& t : m_timers) {
        t.target = 0; t.counter = 0; t.up_counter = 0;
        t.enabled = false; t.prescaler = 0;
    }
}

// ==============================================================================
// MAIN CPU PORT INTERFACE
// ==============================================================================

uint8_t SNES_APU::cpuReadPort(uint16_t addr) {
    return m_apu_to_cpu[addr & 0x03];
}

void SNES_APU::cpuWritePort(uint16_t addr, uint8_t val) {
    m_cpu_to_apu[addr & 0x03] = val;
}

// ==============================================================================
// SPC700 TIMING & MEMORY MAP
// ==============================================================================

void SNES_APU::step(int cpu_cycles) {
    m_cpu_cycle_accum += cpu_cycles;

    // The SPC700 runs at ~1.024 MHz, while the SNES CPU runs at ~21.477 MHz.
    // Ratio is exactly 21 CPU cycles per 1 SPC700 cycle.
    while (m_cpu_cycle_accum >= 21) {
        m_cpu_cycle_accum -= 21;

        int spc_cycles = spc_step();

        for (int i = 0; i < spc_cycles; ++i) {
            tick_timers();

            // Advance DSP Audio Engine (32000 Hz target)
            m_dsp_sample_accum++;
            if (m_dsp_sample_accum >= 32) { // 1.024MHz / 32 = 32KHz
                m_dsp_sample_accum -= 32;
                m_audio_buffer.push_back(0.0f); // Output silence until DSP is built
            }
        }
    }
}

void SNES_APU::tick_timers() {
    // T0 and T1 tick every 128 cycles (8000 Hz)
    // T2 ticks every 16 cycles (64000 Hz)
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
            case 0x00FD: {
                uint8_t val = m_timers[0].up_counter;
                m_timers[0].up_counter = 0; // Reading clears the counter
                return val;
            }
            case 0x00FE: {
                uint8_t val = m_timers[1].up_counter;
                m_timers[1].up_counter = 0;
                return val;
            }
            case 0x00FF: {
                uint8_t val = m_timers[2].up_counter;
                m_timers[2].up_counter = 0;
                return val;
            }
        }
    }
    return m_aram[addr];
}

void SNES_APU::spc_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x00F0 && addr <= 0x00FF) {
        switch (addr) {
            case 0x00F1:
                m_control = val;
                if (val & 0x01) { m_timers[0].enabled = true; m_timers[0].counter = 0; m_timers[0].prescaler = 0; } else { m_timers[0].enabled = false; }
                if (val & 0x02) { m_timers[1].enabled = true; m_timers[1].counter = 0; m_timers[1].prescaler = 0; } else { m_timers[1].enabled = false; }
                if (val & 0x04) { m_timers[2].enabled = true; m_timers[2].counter = 0; m_timers[2].prescaler = 0; } else { m_timers[2].enabled = false; }
                if (val & 0x10) m_cpu_to_apu[0] = m_cpu_to_apu[1] = 0;
                if (val & 0x20) m_cpu_to_apu[2] = m_cpu_to_apu[3] = 0;
                break;
            case 0x00F2:
                m_dsp_addr = val;
                break;
            case 0x00F3:
                if (m_dsp_addr < 0x80) m_dsp_regs[m_dsp_addr] = val;
                break;
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

// ==============================================================================
// SPC700 CPU CORE
// ==============================================================================

uint8_t SNES_APU::spc_fetch() {
    return spc_read(m_spc_PC++);
}

uint16_t SNES_APU::spc_fetch16() {
    uint8_t low = spc_fetch();
    uint8_t high = spc_fetch();
    return low | (high << 8);
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

int SNES_APU::spc_step() {
    uint8_t opcode = spc_fetch();
    int cycles = 2;

    switch (opcode) {
        case 0xCD: { // MOV X, #imm
            m_spc_X = spc_fetch();
            set_zn(m_spc_X);
            cycles = 2;
            break;
        }
        case 0xBD: { // MOV SP, X
            m_spc_SP = m_spc_X;
            cycles = 2;
            break;
        }
        case 0xE8: { // MOV A, #imm
            m_spc_A = spc_fetch();
            set_zn(m_spc_A);
            cycles = 2;
            break;
        }
        case 0xC6: { // MOV (X), A
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + m_spc_X;
            spc_write(dp_addr, m_spc_A);
            cycles = 4;
            break;
        }
        case 0x1D: { // DEC X
            m_spc_X--;
            set_zn(m_spc_X);
            cycles = 2;
            break;
        }
        case 0xD0: { // BNE rel
            int8_t offset = static_cast<int8_t>(spc_fetch());
            if (!flag_z) {
                m_spc_PC += offset;
                cycles = 4;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0x8F: { // MOV dp, #imm
            uint8_t imm = spc_fetch();
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            spc_write(dp_addr, imm);
            cycles = 5;
            break;
        }
        case 0x78: { // CMP dp, #imm
            uint8_t imm = spc_fetch();
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            uint8_t val = spc_read(dp_addr);
            int result = val - imm;
            set_zn(result & 0xFF);
            flag_c = (val >= imm);
            cycles = 5;
            break;
        }
        case 0x2F: { // BRA rel - Branch Relative Always
            int8_t offset = static_cast<int8_t>(spc_fetch());
            m_spc_PC += offset;
            cycles = 4;
            break;
        }
        case 0xBA: { // MOVW YA, dp - Move Word to YA from Direct Page
            uint8_t dp_offset = spc_fetch();
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + dp_offset;
            m_spc_A = spc_read(dp_addr);
            m_spc_Y = spc_read((dp_addr + 1) & 0xFFFF);

            uint16_t ya = m_spc_A | (m_spc_Y << 8);
            flag_z = (ya == 0);
            flag_n = (ya & 0x8000) != 0;
            cycles = 5;
            break;
        }
        case 0xDA: { // MOVW dp, YA - Move Word to Direct Page from YA
            uint8_t dp_offset = spc_fetch();
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + dp_offset;
            spc_write(dp_addr, m_spc_A);
            spc_write((dp_addr + 1) & 0xFFFF, m_spc_Y);
            cycles = 5;
            break;
        }
        case 0xC4: { // MOV dp, A - Move Accumulator to Direct Page
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            spc_write(dp_addr, m_spc_A);
            cycles = 4;
            break;
        }
        case 0xDD: { // MOV A, Y - Move Y Register to Accumulator
            m_spc_A = m_spc_Y;
            set_zn(m_spc_A);
            cycles = 2;
            break;
        }
        case 0x5D: { // MOV X, A - Move Accumulator to X Register
            m_spc_X = m_spc_A;
            set_zn(m_spc_X);
            cycles = 2;
            break;
        }
        case 0xEB: { // MOV Y, dp - Move Direct Page to Y Register
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            m_spc_Y = spc_read(dp_addr);
            set_zn(m_spc_Y);
            cycles = 3;
            break;
        }
        case 0x7E: { // CMP Y, dp - Compare Y Register with Direct Page
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            uint8_t val = spc_read(dp_addr);
            int result = m_spc_Y - val;
            set_zn(result & 0xFF);
            flag_c = (m_spc_Y >= val);
            cycles = 3;
            break;
        }
        case 0xE4: { // MOV A, dp - Move Direct Page to Accumulator
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            m_spc_A = spc_read(dp_addr);
            set_zn(m_spc_A);
            cycles = 3;
            break;
        }
        case 0xCB: { // MOV dp, Y - Move Y Register to Direct Page
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            spc_write(dp_addr, m_spc_Y);
            cycles = 4;
            break;
        }
        case 0xD7: { // MOV [X], A - Move Accumulator to Indirect X Address
            uint16_t ptr_addr = (flag_p ? 0x0100 : 0x0000) + m_spc_X;
            uint16_t target_addr = spc_read(ptr_addr) | (spc_read((ptr_addr + 1) & 0xFFFF) << 8);
            spc_write(target_addr, m_spc_A);
            cycles = 6;
            break;
        }
        case 0x00: { // NOP - No Operation
            cycles = 2;
            break;
        }
        case 0xFC: { // INC Y - Increment Y Register
            m_spc_Y++;
            set_zn(m_spc_Y);
            cycles = 2;
            break;
        }
        case 0x10: { // BPL rel - Branch if Plus (N flag clear)
            int8_t offset = static_cast<int8_t>(spc_fetch());
            if (!flag_n) {
                m_spc_PC += offset;
                cycles = 4;
            } else {
                cycles = 2;
            }
            break;
        }
        case 0xEF: { // SLEEP - Sleep Processor
            cycles = 2;
            break;
        }
        case 0xF6: { // MOV A, abs+Y - Move Absolute Indexed with Y to Accumulator
            uint16_t abs_addr = spc_fetch16();
            uint16_t target = abs_addr + m_spc_Y;
            m_spc_A = spc_read(target);
            set_zn(m_spc_A);
            cycles = 5;
            break;
        }
        case 0xAB: { // INC dp - Increment Direct Page
            uint16_t dp_addr = (flag_p ? 0x0100 : 0x0000) + spc_fetch();
            uint8_t val = spc_read(dp_addr) + 1;
            spc_write(dp_addr, val);
            set_zn(val);
            cycles = 4;
            break;
        }
        case 0x01: { // TCALL 0 - Table Call Vector 0
            // Push PC to stack
            spc_write(0x0100 + m_spc_SP--, (m_spc_PC >> 8) & 0xFF);
            spc_write(0x0100 + m_spc_SP--, m_spc_PC & 0xFF);
            // Jump to vector at 0xFFDE (Table Call 15)
            uint16_t vector = 0xFFDE; // TCALL 0 is usually mapped near top of ARAM
            m_spc_PC = spc_read(vector) | (spc_read(vector + 1) << 8);
            cycles = 8;
            break;
        }
        case 0x1F: { // JMP (abs+X) - Jump Absolute Indexed Indirect with X
            uint16_t abs_addr = spc_fetch16();
            uint16_t ptr = abs_addr + m_spc_X;
            m_spc_PC = spc_read(ptr) | (spc_read((ptr + 1) & 0xFFFF) << 8);
            cycles = 6;
            break;
        }
        case 0xC0: { // DI - Disable Interrupts
            flag_i = false;
            cycles = 2;
            break;
        }
        case 0xFF: { // STOP - Stop Processor
            cycles = 2;
            break;
        }
        case 0x05: { // OR A, abs - Logical OR Accumulator Absolute
            uint16_t addr = spc_fetch16();
            m_spc_A |= spc_read(addr);
            set_zn(m_spc_A);
            cycles = 4;
            break;
        }
        case 0x6F: { // RET - Return from Subroutine
            uint8_t low = spc_read(0x0100 + ++m_spc_SP);
            uint8_t high = spc_read(0x0100 + ++m_spc_SP);
            m_spc_PC = low | (high << 8);
            cycles = 5;
            break;
        }
        case 0xA1: { // MOV (X), Y - Move Y to memory pointed by X (or similar variant)
            uint16_t addr = (flag_p ? 0x0100 : 0x0000) + m_spc_X;
            spc_write(addr, m_spc_Y);
            cycles = 4;
            break;
        }
        case 0xDE: { // MOV abs+X, A - Move Accumulator to Absolute Indexed with X
            uint16_t abs_addr = spc_fetch16();
            spc_write(abs_addr + m_spc_X, m_spc_A);
            cycles = 5;
            break;
        }
        default: {
            std::printf("\n--- CRASH: Unimplemented SPC700 Opcode 0x%02X ---\n", opcode);
            std::printf("At PC: 0x%04X\n", m_spc_PC - 1);
        }
    }
    return cycles;
}