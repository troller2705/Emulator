#include "Core_SNES/snes_core.h"
#include "Launcher/input_mapper.h"

SNESCore::SNESCore() : m_mmu(), m_cpu(m_mmu) {}

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
    m_mmu.get_apu().clear_buffer();

    int frame_cpu_cycles = 0;
    const int target_cycles_per_frame = 262 * 227; // 262 scanlines * ~227 CPU cycles

    while (frame_cpu_cycles < target_cycles_per_frame) {
        int cpu_cycles = m_cpu.step();

        if (m_mmu.get_ppu().advance_cycles(cpu_cycles)) {
            m_mmu.notify_vblank_start();
        }
        m_mmu.get_apu().step(cpu_cycles);

        if (m_mmu.poll_nmi()) {
            m_cpu.trigger_nmi();
        }

        frame_cpu_cycles += cpu_cycles;
    }
}

void SNESCore::set_input(uint32_t input_state) {
    using namespace EmulatorInput;
    uint16_t joy = 0;
    // SNES Standard Controller 16-bit Auto-Joypad layout ($4218 / $4219)
    if (input_state & BTN_B)      joy |= (1 << 15);
    if (input_state & BTN_Y)      joy |= (1 << 14);
    if (input_state & BTN_SELECT) joy |= (1 << 13);
    if (input_state & BTN_START)  joy |= (1 << 12);
    if (input_state & BTN_UP)     joy |= (1 << 11);
    if (input_state & BTN_DOWN)   joy |= (1 << 10);
    if (input_state & BTN_LEFT)   joy |= (1 << 9);
    if (input_state & BTN_RIGHT)  joy |= (1 << 8);
    if (input_state & BTN_A)      joy |= (1 << 7);
    if (input_state & BTN_X)      joy |= (1 << 6);
    if (input_state & BTN_L)      joy |= (1 << 5);
    if (input_state & BTN_R)      joy |= (1 << 4);
    m_mmu.set_joypad_state(joy);
}

void SNESCore::step_instruction() {
    int cpu_cycles = m_cpu.step();
    if (m_mmu.get_ppu().advance_cycles(cpu_cycles)) {
        m_mmu.notify_vblank_start();
    }
    m_mmu.get_apu().step(cpu_cycles);
    if (m_mmu.poll_nmi()) {
        m_cpu.trigger_nmi();
    }
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