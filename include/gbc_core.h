#pragma once
#include <cstdint>
#include <vector>
#include "mmu.h"
#include "cpu.h" // 1. Include the CPU header
#include "ppu.h"

class GBCCore {
public:
    GBCCore();
    ~GBCCore();

    bool load_rom(const std::vector<uint8_t>& rom_data);

    PPU* get_ppu() { return m_mmu.get_ppu(); }

    // This is the function the Launcher calls 60 times a second
    void run_frame();

    void set_input(uint8_t button_mask);
    const uint32_t* get_video_buffer() const;

private:
    std::vector<uint32_t> m_video_buffer;

    // IMPORTANT: In C++, member variables are initialized in the order
    // they are declared here.
    // The MMU must be declared BEFORE the CPU, because the CPU needs the MMU!
    MMU m_mmu;
    CPU m_cpu; // 2. Add the CPU
    PPU m_ppu;
};