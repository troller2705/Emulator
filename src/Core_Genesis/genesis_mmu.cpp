#include "Core_Genesis/genesis_mmu.h"
#include "Core_Shared/battery_io.h"
#include "Launcher/input_mapper.h"
#include <iostream>
#include <cstring>

GenesisMMU::GenesisMMU() {
    reset();
}

void GenesisMMU::reset() {
    m_wram.fill(0);
    m_z80_ram.fill(0);
    m_z80_busreq = 0;
    m_z80_reset  = 0;
    m_pad_data_latch = 0x40;
    m_pad_ctrl = 0x40;
    m_vdp.reset();
}

bool GenesisMMU::load_rom(const std::vector<uint8_t>& rom_data) {
    if (rom_data.size() < 0x200) {
        std::cerr << "[GenesisMMU] ROM is too small.\n";
        return false;
    }

    // Check for 512-byte SMD header & interleaved 16KB blocks
    if ((rom_data.size() % 0x4000) == 512 && rom_data[8] == 0xAA && rom_data[9] == 0xBB) {
        size_t body_size = rom_data.size() - 512;
        m_rom.resize(body_size);
        size_t blocks = body_size / 0x4000;
        for (size_t b = 0; b < blocks; ++b) {
            const uint8_t* src = rom_data.data() + 512 + (b * 0x4000);
            uint8_t* dst = m_rom.data() + (b * 0x4000);
            for (int i = 0; i < 0x2000; ++i) {
                dst[i * 2]     = src[0x2000 + i]; // Even bytes in upper half
                dst[i * 2 + 1] = src[i];          // Odd bytes in lower half
            }
        }
    } else {
        m_rom = rom_data;
    }

    char console_name[17] = {0};
    char domestic_title[49] = {0};
    if (m_rom.size() >= 0x200) {
        std::memcpy(console_name, &m_rom[0x100], 16);
        std::memcpy(domestic_title, &m_rom[0x120], 48);
    }

    m_sram.assign(0x8000, 0xFF); // 32 KB Backup SRAM
    m_sram_enabled = (m_rom.size() <= 0x200000);

    std::cout << "--- SEGA GENESIS CARTRIDGE LOADED ---\n";
    std::cout << "System:  " << console_name << "\n";
    std::cout << "Title:   " << domestic_title << "\n";
    std::cout << "ROM:     " << (m_rom.size() / 1024) << " KB\n";
    std::cout << "-------------------------------------\n";

    reset();
    return true;
}

void GenesisMMU::load_battery(const std::string& path) {
    CoreShared::load_battery_file(path, m_sram.data(), m_sram.size());
}

void GenesisMMU::save_battery(const std::string& path) {
    CoreShared::save_battery_file(path, m_sram.data(), m_sram.size());
}

uint8_t GenesisMMU::read_pad_port() const {
    using namespace EmulatorInput;
    bool th_high = (m_pad_data_latch & 0x40) != 0;
    uint8_t res = (m_pad_data_latch & 0x40) | 0x3F; // Active-low buttons (1 = Released, 0 = Pressed)

    if (m_pad_state & BTN_UP)   res &= ~(1 << 0);
    if (m_pad_state & BTN_DOWN) res &= ~(1 << 1);

    if (th_high) {
        // TH = 1: [0, 1, C, B, Right, Left, Down, Up]
        if (m_pad_state & BTN_LEFT)  res &= ~(1 << 2);
        if (m_pad_state & BTN_RIGHT) res &= ~(1 << 3);
        if (m_pad_state & BTN_A)     res &= ~(1 << 4); // Genesis B (mapped to South / BTN_A)
        if (m_pad_state & BTN_B)     res &= ~(1 << 5); // Genesis C (mapped to East / BTN_B)
    } else {
        // TH = 0: [0, 0, Start, A, 0, 0, Down, Up]
        res &= ~0x0C; // Bits 2 & 3 forced low when TH=0 (identifies a connected Genesis pad!)
        if (m_pad_state & BTN_X)     res &= ~(1 << 4); // Genesis A (mapped to West / BTN_X)
        if (m_pad_state & BTN_START) res &= ~(1 << 5); // Genesis Start
    }
    return res;
}

uint8_t GenesisMMU::read8(uint32_t addr) {
    addr &= 0xFFFFFF;

    // 1. Cartridge ROM & SRAM ($000000 - $3FFFFF)
    if (addr < 0x400000) {
        if (m_sram_enabled && addr >= 0x200000 && addr <= 0x20FFFF) {
            return m_sram[(addr - 0x200000) & 0x7FFF];
        }
        if (addr < m_rom.size()) return m_rom[addr];
        return 0x00;
    }

    // 2. Z80 RAM & YM2612 Stub ($A00000 - $A0FFFF)
    if (addr >= 0xA00000 && addr <= 0xA0FFFF) {
        uint16_t z_addr = addr & 0x7FFF;
        if (z_addr < 0x2000) return m_z80_ram[z_addr];
        if (z_addr >= 0x4000 && z_addr <= 0x4003) return 0x00; // YM2612 Ready (Bit 7 = 0)
        return 0x00;
    }

    // 3. System I/O & Z80 Control ($A10000 - $A1FFFF)
    if (addr >= 0xA10000 && addr <= 0xA1FFFF) {
        switch (addr) {
            case 0xA10000: return 0x00;
            case 0xA10001: return 0xA0; // Overseas (US) + NTSC (60Hz) + No Expansion
            case 0xA10002: return 0x00;
            case 0xA10003: return read_pad_port(); // Controller 1 Data
            case 0xA10005: return 0x7F;            // Controller 2 Data (Unplugged)
            case 0xA10009: return m_pad_ctrl;
            case 0xA11100: return 0x00; // Bit 0 of high byte = 0 -> Z80 Bus Granted to 68000!
            case 0xA11101: return 0x00;
            default:       return 0x00;
        }
    }

    // 4. VDP Ports ($C00000 - $C0001F)
    if (addr >= 0xC00000 && addr <= 0xC0001F) {
        uint16_t word = read16(addr & ~1U);
        return (addr & 1) ? (word & 0xFF) : ((word >> 8) & 0xFF);
    }

    // 5. 64 KB Work RAM ($E00000 - $FFFFFF)
    if (addr >= 0xE00000) {
        return m_wram[addr & 0xFFFF];
    }

    return 0x00;
}

uint16_t GenesisMMU::read16(uint32_t addr) {
    addr &= 0xFFFFFF;
    if (addr >= 0xC00000 && addr <= 0xC0001F) {
        switch (addr & 0x1E) {
            case 0x00:
            case 0x02: return m_vdp.read_data();
            case 0x04:
            case 0x06: return m_vdp.read_control();
            case 0x08:
            case 0x0A:
            case 0x0C:
            case 0x0E: return m_vdp.read_hv_counter();
            default:   return 0x0000;
        }
    }
    // Big-Endian 16-bit read: High byte at addr, Low byte at addr + 1
    return (static_cast<uint16_t>(read8(addr)) << 8) | read8(addr + 1);
}

uint32_t GenesisMMU::read32(uint32_t addr) {
    return (static_cast<uint32_t>(read16(addr)) << 16) | read16(addr + 2);
}

void GenesisMMU::write8(uint32_t addr, uint8_t val) {
    addr &= 0xFFFFFF;

    if (addr >= 0x200000 && addr <= 0x20FFFF && m_sram_enabled) {
        m_sram[(addr - 0x200000) & 0x7FFF] = val;
        return;
    }

    if (addr >= 0xA00000 && addr <= 0xA0FFFF) {
        uint16_t z_addr = addr & 0x7FFF;
        if (z_addr < 0x2000) m_z80_ram[z_addr] = val;
        return;
    }

    if (addr >= 0xA10000 && addr <= 0xA1FFFF) {
        switch (addr) {
            case 0xA10003: m_pad_data_latch = val; break;
            case 0xA10009: m_pad_ctrl = val; break;
            case 0xA11100: m_z80_busreq = (val & 1); break;
            case 0xA11200: m_z80_reset  = (val & 1); break;
            case 0xA130F1: m_sram_enabled = (val & 1) != 0; break;
        }
        return;
    }

    if (addr >= 0xC00000 && addr <= 0xC0001F) {
        // 8-bit writes to VDP duplicate the byte across both halves of the 16-bit bus
        if ((addr & 0x1E) <= 0x06) {
            write16(addr & ~1U, (static_cast<uint16_t>(val) << 8) | val);
        }
        return;
    }

    if (addr >= 0xE00000) {
        m_wram[addr & 0xFFFF] = val;
    }
}

void GenesisMMU::write16(uint32_t addr, uint16_t val) {
    addr &= 0xFFFFFF;
    if (addr >= 0xC00000 && addr <= 0xC0001F) {
        switch (addr & 0x1E) {
            case 0x00:
            case 0x02: m_vdp.write_data(val); return;
            case 0x04:
            case 0x06: m_vdp.write_control(val, *this); return;
            default:   return; // PSG ($C00011) ignored for now
        }
    }
    write8(addr, static_cast<uint8_t>((val >> 8) & 0xFF));
    write8(addr + 1, static_cast<uint8_t>(val & 0xFF));
}

void GenesisMMU::write32(uint32_t addr, uint32_t val) {
    write16(addr, static_cast<uint16_t>((val >> 16) & 0xFFFF));
    write16(addr + 2, static_cast<uint16_t>(val & 0xFFFF));
}