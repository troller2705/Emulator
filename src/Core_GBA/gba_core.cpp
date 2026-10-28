#include "gba_core.h"
#include <iostream>
#include <fstream>
#include <cstring>

GBACore::GBACore() {
    m_framebuffer.fill(0xFF000000);
    m_debug_tile_buffer.fill(0xFF000000);
    m_audio_buffer.reserve(1024);
}

bool GBACore::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0xC0) {
        std::cerr << "[GBACore] Invalid GBA ROM size.\n";
        return false;
    }

    m_rom = rom_data;
    m_ewram.fill(0);
    m_iwram.fill(0);
    m_io.fill(0);
    m_pal_ram.fill(0);
    m_vram.fill(0);
    m_oam.fill(0);
    m_sram.fill(0xFF);

    // Post-BIOS ARM7TDMI Initial State
    m_regs.fill(0);
    m_regs[13] = 0x03007F00; // USR/SYS Stack Pointer (SP)
    m_regs[14] = 0x08000000; // Link Register (LR)
    m_regs[15] = 0x08000000; // Program Counter (PC -> GamePak Entry)
    m_cpsr     = 0x0000001F; // ARM State, System Mode

    // Parse GBA Header (0xA0 = 12-byte Title, 0xAC = 4-byte Game Code)
    char title[13] = {0};
    char code[5]   = {0};
    std::memcpy(title, &m_rom[0xA0], 12);
    std::memcpy(code,  &m_rom[0xAC], 4);

    std::cout << "--- GBA CARTRIDGE LOADED ---\n";
    std::cout << "Title:     " << title << "\n";
    std::cout << "Game Code: " << code << "\n";
    std::cout << "Size:      " << (m_rom.size() / 1024) << " KB\n";
    std::cout << "----------------------------\n";

    return true;
}

void GBACore::load_battery(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(m_sram.data()), m_sram.size());
    }
}

void GBACore::save_battery(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(m_sram.data()), m_sram.size());
    }
}

uint8_t GBACore::read8(uint32_t addr) const {
    switch (addr >> 24) {
        case 0x00: return (addr < 0x4000) ? m_bios[addr] : 0;
        case 0x02: return m_ewram[addr & 0x3FFFF];
        case 0x03: return m_iwram[addr & 0x7FFF];
        case 0x04: {
            uint32_t offset = addr & 0x3FF;
            if (offset == 0x130) return m_keyinput & 0xFF;
            if (offset == 0x131) return (m_keyinput >> 8) & 0xFF;
            return m_io[offset];
        }
        case 0x05: return m_pal_ram[addr & 0x3FF];
        case 0x06: {
            uint32_t offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            return m_vram[offset];
        }
        case 0x07: return m_oam[addr & 0x3FF];
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: {
            uint32_t offset = addr & 0x1FFFFFF;
            return (offset < m_rom.size()) ? m_rom[offset] : 0xFF;
        }
        case 0x0E: case 0x0F:
            return m_sram[addr & 0xFFFF];
        default: return 0;
    }
}

uint16_t GBACore::read16(uint32_t addr) const {
    addr &= ~1U;
    return read8(addr) | (static_cast<uint16_t>(read8(addr + 1)) << 8);
}

uint32_t GBACore::read32(uint32_t addr) const {
    addr &= ~3U;
    return read16(addr) | (static_cast<uint32_t>(read16(addr + 2)) << 16);
}

void GBACore::write8(uint32_t addr, uint8_t val) {
    switch (addr >> 24) {
        case 0x02: m_ewram[addr & 0x3FFFF] = val; break;
        case 0x03: m_iwram[addr & 0x7FFF]  = val; break;
        case 0x04: m_io[addr & 0x3FF]      = val; break;
        case 0x05: m_pal_ram[addr & 0x3FF] = val; break;
        case 0x06: {
            uint32_t offset = addr & 0x1FFFF;
            if (offset >= 0x18000) offset -= 0x8000;
            m_vram[offset] = val;
            break;
        }
        case 0x07: m_oam[addr & 0x3FF] = val; break;
        case 0x0E: case 0x0F:
            m_sram[addr & 0xFFFF] = val;
            break;
        default: break;
    }
}

void GBACore::write16(uint32_t addr, uint16_t val) {
    addr &= ~1U;
    write8(addr, val & 0xFF);
    write8(addr + 1, (val >> 8) & 0xFF);
}

void GBACore::write32(uint32_t addr, uint32_t val) {
    addr &= ~3U;
    write16(addr, val & 0xFFFF);
    write16(addr + 2, (val >> 16) & 0xFFFF);
}

void GBACore::step_arm() {
    uint32_t pc = m_regs[15];
    uint32_t instr = read32(pc);
    m_regs[15] += 4;

    // Branch / Branch with Link (B / BL): 101L <24-bit signed offset>
    if (((instr >> 25) & 0x07) == 0x05) {
        bool link = (instr >> 24) & 1;
        int32_t offset = static_cast<int32_t>(instr << 8) >> 6; // Sign-extend 24-bit and * 4
        if (link) {
            m_regs[14] = m_regs[15];
        }
        m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 8) + offset);
        return;
    }

    // Branch and Exchange (BX Rn) -> Switches between ARM and Thumb
    if ((instr & 0x0FFFFFF0) == 0x012FFF10) {
        uint8_t rn = instr & 0x0F;
        uint32_t target = m_regs[rn];
        if (target & 1) {
            m_cpsr |= (1 << 5); // Enter Thumb state
            m_regs[15] = target & ~1U;
        } else {
            m_cpsr &= ~(1U << 5); // Stay in ARM state
            m_regs[15] = target & ~3U;
        }
        return;
    }

    // Single Data Transfer (LDR / STR Immediate)
    if (((instr >> 26) & 0x03) == 0x01) {
        bool is_load = (instr >> 20) & 1;
        bool is_byte = (instr >> 22) & 1;
        bool add_off = (instr >> 23) & 1;
        uint8_t rn   = (instr >> 16) & 0x0F;
        uint8_t rd   = (instr >> 12) & 0x0F;
        uint32_t off = instr & 0xFFF;

        uint32_t base = (rn == 15) ? (pc + 8) : m_regs[rn];
        uint32_t addr = add_off ? (base + off) : (base - off);

        if (is_load) {
            m_regs[rd] = is_byte ? read8(addr) : read32(addr);
        } else {
            uint32_t val = (rd == 15) ? (pc + 12) : m_regs[rd];
            if (is_byte) write8(addr, static_cast<uint8_t>(val));
            else         write32(addr, val);
        }
    }
}

void GBACore::step_thumb() {
    uint32_t pc = m_regs[15];
    uint16_t instr = read16(pc);
    m_regs[15] += 2;

    // Unconditional Thumb Branch: 11100 <11-bit offset>
    if ((instr >> 11) == 0x1C) {
        int32_t offset = static_cast<int32_t>(static_cast<int16_t>(instr << 5)) >> 4;
        m_regs[15] = static_cast<uint32_t>(static_cast<int32_t>(pc + 4) + offset);
    }
    // Move Immediate: 001 00 <Rd> <8-bit imm>
    else if ((instr >> 11) == 0x04) {
        uint8_t rd = (instr >> 8) & 0x07;
        m_regs[rd] = instr & 0xFF;
    }
}

void GBACore::step_instruction() {
    if (m_cpsr & (1 << 5)) {
        step_thumb();
    } else {
        step_arm();
    }
}

void GBACore::run_frame() {
    m_audio_buffer.clear();

    // Execute scanlines (160 visible + 68 VBlank = 228 lines)
    for (int ly = 0; ly < 228; ++ly) {
        // Update VCOUNT (0x04000006)
        m_io[0x006] = static_cast<uint8_t>(ly);
        m_io[0x007] = 0;

        // Toggle VBlank flag in DISPSTAT (0x04000004)
        if (ly == 160) m_io[0x004] |= 0x01;
        if (ly == 227) m_io[0x004] &= ~0x01;

        for (int i = 0; i < 120; ++i) {
            step_instruction();
        }
    }

    render_frame();
}

uint32_t GBACore::bgr555_to_argb8888(uint16_t bgr) {
    uint8_t r5 = bgr & 0x1F;
    uint8_t g5 = (bgr >> 5) & 0x1F;
    uint8_t b5 = (bgr >> 10) & 0x1F;

    uint8_t r8 = (r5 << 3) | (r5 >> 2);
    uint8_t g8 = (g5 << 3) | (g5 >> 2);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    return 0xFF000000 | (r8 << 16) | (g8 << 8) | b8;
}

void GBACore::render_frame() {
    uint16_t dispcnt = m_io[0] | (static_cast<uint16_t>(m_io[1]) << 8);
    uint8_t mode = dispcnt & 0x07;

    if (mode == 3) {
        // Mode 3: 240x160 15-bit BGR555 Bitmap
        for (int i = 0; i < 240 * 160; ++i) {
            uint16_t pixel = m_vram[i * 2] | (static_cast<uint16_t>(m_vram[i * 2 + 1]) << 8);
            m_framebuffer[i] = bgr555_to_argb8888(pixel);
        }
    } else if (mode == 4) {
        // Mode 4: 240x160 8-bit Paletted Bitmap
        uint32_t base = (dispcnt & (1 << 4)) ? 0xA000 : 0x0000;
        for (int i = 0; i < 240 * 160; ++i) {
            uint8_t pal_idx = m_vram[base + i];
            uint16_t color = m_pal_ram[pal_idx * 2] | (static_cast<uint16_t>(m_pal_ram[pal_idx * 2 + 1]) << 8);
            m_framebuffer[i] = bgr555_to_argb8888(color);
        }
    } else {
        // Mode 0/1/2 Backdrop + 4BPP Tile Preview
        uint16_t backdrop = m_pal_ram[0] | (static_cast<uint16_t>(m_pal_ram[1]) << 8);
        uint32_t bg_color = bgr555_to_argb8888(backdrop);
        m_framebuffer.fill(bg_color);
    }
}

void GBACore::set_input(uint32_t input_state) {
    // Map launcher buttons into GBA active-low KEYINPUT (0x04000130)
    uint16_t keys = 0x03FF;
    if (input_state & (1 << 0)) keys &= ~(1 << 0); // A
    if (input_state & (1 << 1)) keys &= ~(1 << 1); // B
    if (input_state & (1 << 2)) keys &= ~(1 << 2); // Select
    if (input_state & (1 << 3)) keys &= ~(1 << 3); // Start
    if (input_state & (1 << 4)) keys &= ~(1 << 4); // Right
    if (input_state & (1 << 5)) keys &= ~(1 << 5); // Left
    if (input_state & (1 << 6)) keys &= ~(1 << 6); // Up
    if (input_state & (1 << 7)) keys &= ~(1 << 7); // Down
    m_keyinput = keys;
}

CPUState GBACore::get_cpu_state() const {
    CPUState state{};
    // Map lower bits of R0-R3 and R13(SP)/R15(PC) into the shared debugger view
    state.A  = static_cast<uint8_t>(m_regs[0] & 0xFF);
    state.F  = static_cast<uint8_t>((m_cpsr >> 24) & 0xFF);
    state.B  = static_cast<uint8_t>(m_regs[1] & 0xFF);
    state.C  = static_cast<uint8_t>(m_regs[2] & 0xFF);
    state.D  = static_cast<uint8_t>(m_regs[3] & 0xFF);
    state.E  = static_cast<uint8_t>(m_regs[4] & 0xFF);
    state.H  = static_cast<uint8_t>((m_regs[15] >> 24) & 0xFF); // Upper byte of 32-bit PC
    state.L  = static_cast<uint8_t>((m_regs[15] >> 16) & 0xFF);
    state.PC = static_cast<uint16_t>(m_regs[15] & 0xFFFF);
    state.SP = static_cast<uint16_t>(m_regs[13] & 0xFFFF);
    return state;
}

uint8_t GBACore::debug_read_memory(uint16_t address) {
    // Inspect GamePak ROM header/code or IWRAM in the 64KB Hex Editor
    return read8(0x08000000 + address);
}

void GBACore::debug_write_memory(uint16_t address, uint8_t value) {
    write8(0x02000000 + address, value);
}

void GBACore::debug_update_tile_buffer() {
    const uint32_t palette[4] = { 0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000 };
    // Decode 4BPP GBA tiles from VRAM 0x06000000 into the 128x192 viewer
    for (int tile = 0; tile < 384; ++tile) {
        int tile_x = (tile % 16) * 8;
        int tile_y = (tile / 16) * 8;
        uint32_t base = tile * 32; // 32 bytes per 4BPP 8x8 tile
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                uint8_t byte = m_vram[base + row * 4 + (col / 2)];
                uint8_t nibble = (col & 1) ? (byte >> 4) : (byte & 0x0F);
                m_debug_tile_buffer[(tile_y + row) * 128 + (tile_x + col)] = palette[nibble & 0x03];
            }
        }
    }
}