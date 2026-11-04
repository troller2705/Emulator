#include "Core_Genesis/genesis_core.h"

GenesisCore::GenesisCore() : m_mmu(), m_cpu(m_mmu) {}

bool GenesisCore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (!m_mmu.load_rom(rom_data)) return false;
    reset();
    return true;
}

void GenesisCore::reset() {
    m_mmu.reset();
    m_cpu.reset();
}

void GenesisCore::run_frame() {
    // 262 NTSC scanlines, ~488 68000 cycles per scanline (7.67 MHz / 60 Hz / 262)
    for (int line = 0; line < 262; ++line) {
        int cycles = 0;
        while (cycles < 488) {
            cycles += m_cpu.step();
        }

        m_mmu.get_vdp().step_scanline();

        if (m_mmu.get_vdp().poll_vint()) {
            m_cpu.trigger_interrupt(6); // Level 6 = V-Blank Interrupt (Vector 30 / $000078)
        } else if (m_mmu.get_vdp().poll_hint()) {
            m_cpu.trigger_interrupt(4); // Level 4 = H-Blank Interrupt (Vector 28 / $000070)
        }
    }
}

void GenesisCore::set_input(uint32_t input_state) {
    m_mmu.set_joypad_state(static_cast<uint16_t>(input_state & 0xFFFF));
}

void GenesisCore::step_instruction() {
    m_cpu.step();
}

CPUState GenesisCore::get_cpu_state() const {
    CPUState state{};
    state.is_genesis = true;
    state.m68k_d  = m_cpu.D;
    state.m68k_a  = m_cpu.A;
    state.m68k_pc = m_cpu.PC;
    state.m68k_sr = m_cpu.SR;
    return state;
}