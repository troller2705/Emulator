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

void GBCCore::step_instruction() {
    int cycles = m_cpu.clock_instruction();
    m_timer.tick(cycles);
    m_mmu.get_ppu()->step(cycles, m_mmu);
    m_mmu.get_apu()->tick(cycles);
}

CPUState GBCCore::get_cpu_state() const {
    // Populate this based on how your CPU registers are actually stored in cpu.h
    // Assuming you have getters or direct access to them:
    CPUState state;
    state.A = m_cpu.AF.high; // or m_cpu.get_A(), etc.
    state.F = m_cpu.AF.low;
    state.B = m_cpu.BC.high;
    state.C = m_cpu.BC.low;
    state.D = m_cpu.DE.high;
    state.E = m_cpu.DE.low;
    state.H = m_cpu.HL.high;
    state.L = m_cpu.HL.low;
    state.PC = m_cpu.PC;
    state.SP = m_cpu.SP;
    return state;
}

uint8_t GBCCore::debug_read_memory(uint16_t address) {
    return m_mmu.read(address);
}

void GBCCore::debug_write_memory(uint16_t address, uint8_t value) {
    m_mmu.write(address, value);
}

void GBCCore::debug_update_tile_buffer() {
    // Standard grayscale palette (White, Light Gray, Dark Gray, Black)
    const uint32_t palette[4] = { 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 };

    // The Game Boy has 384 tiles total across blocks 0, 1, and 2
    for (int tile = 0; tile < 384; tile++) {
        // Calculate where this tile sits on our 16x24 ImGui grid
        int tile_x = (tile % 16) * 8;
        int tile_y = (tile / 16) * 8;

        // Each tile is 16 bytes (2 bytes per row * 8 rows)
        for (int row = 0; row < 8; row++) {
            uint16_t address = 0x8000 + (tile * 16) + (row * 2);

            // Read the two bytes that make up this 8-pixel row
            uint8_t byte1 = debug_read_memory(address);     // Lower bit
            uint8_t byte2 = debug_read_memory(address + 1); // Upper bit

            for (int col = 0; col < 8; col++) {
                // The leftmost pixel is bit 7, the rightmost is bit 0
                int bit_index = 7 - col;

                uint8_t bit1 = (byte1 >> bit_index) & 0x01;
                uint8_t bit2 = (byte2 >> bit_index) & 0x01;

                // Combine the bits to get the 2-bit color index (0-3)
                uint8_t color_idx = (bit2 << 1) | bit1;

                // Write the ARGB color to the flattened 1D buffer
                m_debug_tile_buffer[(tile_y + row) * 128 + (tile_x + col)] = palette[color_idx];
            }
        }
    }
}

const uint32_t* GBCCore::debug_get_tile_buffer() const {
    return m_debug_tile_buffer.data();
}