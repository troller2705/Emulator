#include "../../include/Core_SNES/snes_core.h"

SNESCore::SNESCore() : m_mmu(), m_cpu(m_mmu) {
    m_framebuffer.fill(0xFF000000);
}

bool SNESCore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (!m_mmu.load_rom(rom_data)) return false;
    reset();
    return true;
}

void SNESCore::reset() {
    m_mmu.reset();
    m_cpu.reset();
}

void SNESCore::run_frame() {
    // Basic loop just to test CPU execution limits
    for (int i = 0; i < 10000; i++) {
        m_cpu.step();
    }
}

void SNESCore::set_input(uint32_t input_state) {
    // Stubbed for now
}

void SNESCore::step_instruction() {
    m_cpu.step();
}

CPUState SNESCore::get_cpu_state() const {
    CPUState state{};
    state.is_snes = true;
    state.snes_A  = m_cpu.A.W;
    state.snes_X  = m_cpu.X.W;
    state.snes_Y  = m_cpu.Y.W;
    state.snes_D  = m_cpu.D;
    state.snes_PB = m_cpu.PB;
    state.snes_DB = m_cpu.DB;
    state.snes_P  = m_cpu.get_p();
    state.flag_m  = m_cpu.flag_m;
    state.flag_x  = m_cpu.flag_x;
    state.flag_e  = m_cpu.flag_e;
    state.PC      = m_cpu.PC;
    state.SP      = m_cpu.S.W;
    return state;
}