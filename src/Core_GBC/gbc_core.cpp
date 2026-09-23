#include "gbc_core.h"
#include <iostream>

// 1. Pass the MMU into the CPU using an initializer list
GBCCore::GBCCore() : m_mmu(), m_cpu(m_mmu) {
    m_video_buffer.resize(160 * 144, 0xFF000000);
}

GBCCore::~GBCCore() {}

bool GBCCore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0x0150) {
        std::cerr << "[Core] Invalid ROM size.\n";
        return false;
    }

    // 1. CRUCIAL STEP: Hand the ROM data over to the MMU!
    m_mmu.load_rom(rom_data);

    // 2. Fetch the title byte-by-byte through the MMU
    char title[17] = {0}; // 16 chars + null terminator
    for (uint16_t i = 0; i < 16; i++) {
        title[i] = m_mmu.read(0x0134 + i);
    }

    // 3. Fetch the hardware flags
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
    const int MAX_CYCLES = 69905;
    int cycles_this_frame = 0;

    while (cycles_this_frame < MAX_CYCLES) {
        int cycles_taken = m_cpu.clock_instruction();
        cycles_this_frame += cycles_taken;

        // Pass BOTH the cycles and the MMU to the PPU
        m_mmu.get_ppu()->step(cycles_taken, m_mmu);
    }
}

void GBCCore::set_input(uint8_t button_mask) {}

const uint32_t* GBCCore::get_video_buffer() const {
    return m_video_buffer.data();
}