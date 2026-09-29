#include "gbc_core.h"
#include <iostream>

// 1. Initialize components and link the timer immediately
GBCCore::GBCCore() : m_mmu(), m_cpu(m_mmu), m_timer(m_mmu) {
    m_mmu.link_timer(&m_timer);
}

// 2. Save SRAM to disk when the core shuts down
GBCCore::~GBCCore() {
    if (!m_current_save_path.empty()) {
        m_mmu.save_battery(m_current_save_path);
    }
}

bool GBCCore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0x0150) {
        std::cerr << "[Core] Invalid ROM size.\n";
        return false;
    }

    m_mmu.load_rom(rom_data);

    char title[17] = {0};
    for (uint16_t i = 0; i < 16; i++) {
        title[i] = m_mmu.read(0x0134 + i);
    }

    // Cache the save path for the destructor
    m_current_save_path = std::string(title) + ".sav";

    uint8_t cart_type = m_mmu.read(0x0147);
    uint8_t rom_size  = m_mmu.read(0x0148);
    uint8_t ram_size  = m_mmu.read(0x0149);

    std::cout << "--- CARTRIDGE LOADED VIA MMU ---\n";
    std::cout << "Title: " << title << "\n";
    std::cout << "Cart Type (Hex): 0x" << std::hex << (int)cart_type << std::dec << "\n";
    std::cout << "ROM Size ID: " << (int)rom_size << "\n";
    std::cout << "RAM Size ID: " << (int)ram_size << "\n";
    std::cout << "--------------------------------\n";

    return true;
}

void GBCCore::run_frame() {
    // Clear last frame's audio
    m_mmu.get_apu()->clear_buffer();

    const int CYCLES_PER_FRAME = 70224;
    int cycles_this_frame = 0;

    while (cycles_this_frame < CYCLES_PER_FRAME) {
        int cycles = m_cpu.clock_instruction();
        m_timer.tick(cycles);
        m_mmu.get_ppu()->step(cycles, m_mmu);

        // Drive the audio processor
        m_mmu.get_apu()->tick(cycles);

        cycles_this_frame += cycles;
    }
}

// 3. Signature matches uint32_t from header
void GBCCore::set_input(uint32_t input_state) {
    m_mmu.set_joypad_state(~static_cast<uint8_t>(input_state));
}

// 4. Return the PPU's buffer directly as a void*
const void* GBCCore::get_video_buffer() const {
    return m_mmu.get_ppu()->get_framebuffer();
}

const float* GBCCore::get_audio_buffer() const {
    return m_mmu.get_apu()->get_audio_buffer();
}

size_t GBCCore::get_audio_sample_count() const {
    return m_mmu.get_apu()->get_audio_sample_count();
}