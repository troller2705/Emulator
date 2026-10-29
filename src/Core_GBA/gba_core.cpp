#include "../../include/Core_GBA/gba_core.h"
#include "../../include/Launcher/input_mapper.h"
#include <iostream>
#include <cstring>

GBACore::GBACore()
    : m_mmu(),
      m_cpu(m_mmu),
      m_ppu(m_mmu),
      m_timer(m_mmu),
      m_apu(m_mmu) {
    m_mmu.link_components(&m_timer, &m_apu);
    m_timer.link_apu(&m_apu);
}

void GBACore::reset() {
    m_mmu.reset();
    m_cpu.reset();
    m_ppu.reset();
    m_timer.reset();
    m_apu.reset();
}

bool GBACore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0xC0) {
        std::cerr << "[GBACore] Invalid GBA ROM size.\n";
        return false;
    }

    m_mmu.load_rom(rom_data);
    reset();

    char title[13] = {0};
    char code[5]   = {0};
    std::memcpy(title, &rom_data[0xA0], 12);
    std::memcpy(code,  &rom_data[0xAC], 4);

    std::cout << "--- GBA CARTRIDGE LOADED ---\n";
    std::cout << "Title:     " << title << "\n";
    std::cout << "Game Code: " << code << "\n";
    std::cout << "Size:      " << (rom_data.size() / 1024) << " KB\n";
    std::cout << "----------------------------\n";

    return true;
}

void GBACore::step_instruction() {
    m_cpu.step();
    m_timer.tick(4);
    m_apu.tick(4);
}

void GBACore::run_frame() {
    m_apu.clear_buffer();

    for (int ly = 0; ly < 228; ++ly) {
        m_mmu.io_write16(0x006, static_cast<uint16_t>(ly)); // VCOUNT

        // Update VCount coincidence flag (Bit 2)
        uint16_t dispstat = m_mmu.io_read16(0x004);
        uint8_t lyc       = (dispstat >> 8) & 0xFF;
        if (ly == lyc) {
            dispstat |= (1 << 2);
            m_mmu.io_write16(0x004, dispstat);
            if (dispstat & (1 << 5)) m_mmu.request_irq(IRQ_VCOUNT);
        } else {
            dispstat &= ~(1U << 2);
            m_mmu.io_write16(0x004, dispstat);
        }

        if (ly < 160) {
            // HDraw (960 cycles): Clear VBlank (Bit 0) and HBlank (Bit 1)
            dispstat = m_mmu.io_read16(0x004) & ~0x03U;
            m_mmu.io_write16(0x004, dispstat);

            for (int i = 0; i < 480; ++i) {
                m_cpu.step();
                if (m_mmu.is_halted()) break;
            }
            m_timer.tick(960);
            m_apu.tick(960);

            m_ppu.render_scanline(ly);

            // HBlank (272 cycles): Re-read DISPSTAT so CPU writes during HDraw are preserved!
            dispstat = m_mmu.io_read16(0x004) | (1 << 1);
            m_mmu.io_write16(0x004, dispstat);
            if (dispstat & (1 << 4)) m_mmu.request_irq(IRQ_HBLANK);
            m_mmu.run_dma(2); // Trigger HBlank DMAs

            for (int i = 0; i < 136; ++i) {
                m_cpu.step();
                if (m_mmu.is_halted()) break;
            }
            m_timer.tick(272);
            m_apu.tick(272);
        } else {
            // VBlank scanlines (160..227): Clear HBlank flag at scanline start
            dispstat = m_mmu.io_read16(0x004) & ~(1U << 1);
            if (ly == 160) {
                dispstat |= (1 << 0); // Set VBlank flag
                m_mmu.io_write16(0x004, dispstat);
                if (dispstat & (1 << 3)) m_mmu.request_irq(IRQ_VBLANK);
                m_mmu.run_dma(1); // Trigger VBlank DMAs
            } else if (ly == 227) {
                dispstat &= ~(1U << 0); // Clear VBlank flag on last scanline
                m_mmu.io_write16(0x004, dispstat);
            } else {
                m_mmu.io_write16(0x004, dispstat);
            }

            for (int i = 0; i < 480; ++i) {
                m_cpu.step();
                if (m_mmu.is_halted()) break;
            }
            m_timer.tick(960);
            m_apu.tick(960);

            // HBlank flag also sets during VBlank scanlines
            dispstat = m_mmu.io_read16(0x004) | (1 << 1);
            m_mmu.io_write16(0x004, dispstat);
            if (dispstat & (1 << 4)) m_mmu.request_irq(IRQ_HBLANK);

            for (int i = 0; i < 136; ++i) {
                m_cpu.step();
                if (m_mmu.is_halted()) break;
            }
            m_timer.tick(272);
            m_apu.tick(272);
        }
    }
}

void GBACore::set_input(uint32_t input_state) {
    using namespace EmulatorInput;
    uint16_t keys = 0x03FF;
    if (input_state & (BTN_A | BTN_X))  keys &= ~(1 << 0); // A
    if (input_state & (BTN_B | BTN_Y))  keys &= ~(1 << 1); // B
    if (input_state & BTN_SELECT)       keys &= ~(1 << 2); // Select
    if (input_state & BTN_START)        keys &= ~(1 << 3); // Start
    if (input_state & BTN_RIGHT)        keys &= ~(1 << 4); // Right
    if (input_state & BTN_LEFT)         keys &= ~(1 << 5); // Left
    if (input_state & BTN_UP)           keys &= ~(1 << 6); // Up
    if (input_state & BTN_DOWN)         keys &= ~(1 << 7); // Down
    if (input_state & (BTN_R | BTN_R2)) keys &= ~(1 << 8); // R Shoulder
    if (input_state & (BTN_L | BTN_L2)) keys &= ~(1 << 9); // L Shoulder
    m_mmu.set_keyinput(keys);
}