#include "../../include/Core_SNES/snes_mmu.h"
#include <iostream>

SNES_MMU::SNES_MMU() {
    reset();
}

void SNES_MMU::reset() {
    m_wram.fill(0);
    // Initial SPC700 IPL boot handshake state
    m_apu_ports = {0xAA, 0xBB, 0x00, 0x00};
}

bool SNES_MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    // Strip SMC header if present (legacy copiers added 512 bytes)
    if ((rom_data.size() % 0x400) == 512) {
        m_rom.assign(rom_data.begin() + 512, rom_data.end());
    } else {
        m_rom = rom_data;
    }

    if (m_rom.size() < 0x8000) {
        std::cerr << "[SNES_MMU] ROM is too small.\n";
        return false;
    }

    // Heuristic: Check checksum to determine LoROM vs HiROM mapping
    // A valid SNES header has a checksum and inverse checksum that equal 0xFFFF
    uint16_t lorom_checksum = m_rom[0x7FDE] | (m_rom[0x7FDF] << 8);
    uint16_t lorom_inverse  = m_rom[0x7FDC] | (m_rom[0x7FDD] << 8);
    
    uint16_t hirom_checksum = m_rom[0xFFDE] | (m_rom[0xFFDF] << 8);
    uint16_t hirom_inverse  = m_rom[0xFFDC] | (m_rom[0xFFDD] << 8);

    int header_offset = 0;
    
    if ((lorom_checksum + lorom_inverse) == 0xFFFF) {
        m_map_mode = SNESMapMode::LoROM;
        header_offset = 0x7FC0;
    } else if ((hirom_checksum + hirom_inverse) == 0xFFFF) {
        m_map_mode = SNESMapMode::HiROM;
        header_offset = 0xFFC0;
    } else {
        std::cerr << "[SNES_MMU] Warning: Checksum invalid. Defaulting to LoROM.\n";
        m_map_mode = SNESMapMode::LoROM;
        header_offset = 0x7FC0;
    }

    char title[22] = {0};
    for (int i = 0; i < 21; ++i) title[i] = m_rom[header_offset + i];

    // SRAM size mapping (Byte 0xD8 of header)
    uint8_t sram_code = m_rom[header_offset + 0x18];
    m_sram_size = (sram_code == 0) ? 0 : (1024 << sram_code);
    m_sram.resize(m_sram_size, 0);

    std::cout << "--- SNES CARTRIDGE LOADED ---\n";
    std::cout << "Title:   " << title << "\n";
    std::cout << "Mapping: " << (m_map_mode == SNESMapMode::LoROM ? "LoROM" : "HiROM") << "\n";
    std::cout << "ROM:     " << (m_rom.size() / 1024) << " KB\n";
    std::cout << "SRAM:    " << (m_sram_size / 1024) << " KB\n";
    std::cout << "-----------------------------\n";

    reset();
    return true;
}

int32_t SNES_MMU::map_rom_address(uint8_t bank, uint16_t offset) const {
    if (m_map_mode == SNESMapMode::LoROM) {
        // LoROM: Only the upper 32KB of each bank (0x8000-0xFFFF) maps to ROM.
        // Bank determines the 32KB chunk.
        if (offset < 0x8000) return -1;
        return ((bank & 0x7F) * 0x8000) + (offset - 0x8000);
    } else {
        // HiROM: The entire 64KB bank maps directly, ignoring the top bit of the bank.
        return ((bank & 0x3F) * 0x10000) + offset;
    }
}

uint8_t SNES_MMU::read8(uint32_t addr) {
    uint8_t bank   = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // 1. Full 128KB WRAM (Banks 0x7E and 0x7F)
    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wram_addr = ((bank & 1) << 16) | offset;
        return m_wram[wram_addr];
    }

    // 2. System Area (Banks 0x00-0x3F and 0x80-0xBF, Lower 32KB)
    if ((bank <= 0x3F || (bank >= 0x80 && bank <= 0xBF)) && offset < 0x8000) {
        if (offset < 0x2000) {
            // WRAM Mirror (First 8KB of Bank 0x7E is mirrored here)
            return m_wram[offset];
        }
        if (offset >= 0x2100 && offset <= 0x21FF) {
            // PPU and APU I/O Ports
            if (offset >= 0x2140 && offset <= 0x2143) {
                uint8_t val = m_apu_ports[offset & 0x03];

                // Restore the 0xBBAA handshake after a read so polling loops
                // automatically resynchronize after register clears or transfers
                if (offset == 0x2140) m_apu_ports[0] = 0xAA;
                if (offset == 0x2141) m_apu_ports[1] = 0xBB;

                return val;
            }

            return 0; // Stubbed for now
        }
        if (offset >= 0x4200 && offset <= 0x421F) {
            // CPU I/O Ports (Timers, Joypads, NMI, etc.)

            if (offset == 0x4210) {
                // NMI Status / VBlank
                // Bit 7 flags that VBlank has occurred. Reading this register clears the flag.
                static uint8_t nmi_toggle = 0;
                nmi_toggle ^= 0x80; // Toggle bit 7 every read to fake PPU timing
                return nmi_toggle | 0x02; // Bit 1 is the CPU version number (always 2)
            }

            if (offset == 0x4212) {
                // HVBJOY - H/V-Blank and Joypad Auto-Read Status
                // Bit 7: VBlank, Bit 6: HBlank, Bit 0: Joypad Auto-Read
                static uint8_t hvbjoy_toggle = 0;
                hvbjoy_toggle ^= 0xC1; // Toggle VBlank, HBlank, and Joypad bits
                return hvbjoy_toggle;
            }

            return 0; // Stub the rest for now
        }
        return 0; // Open bus
    }

    // 3. Cartridge ROM mapping
    int32_t rom_addr = map_rom_address(bank, offset);
    if (rom_addr >= 0 && rom_addr < m_rom.size()) {
        return m_rom[rom_addr];
    }

    return 0xFF; // Open bus fallback
}

void SNES_MMU::write8(uint32_t addr, uint8_t val) {
    uint8_t bank   = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wram_addr = ((bank & 1) << 16) | offset;
        m_wram[wram_addr] = val;
        return;
    }

    if ((bank <= 0x3F || (bank >= 0x80 && bank <= 0xBF)) && offset < 0x8000) {
        if (offset < 0x2000) {
            m_wram[offset] = val;
            return;
        }
        if (offset >= 0x2100 && offset <= 0x21FF) {
            // PPU / APU I/O
            if (offset >= 0x2140 && offset <= 0x2143) {
                m_apu_ports[offset & 0x03] = val;
            }
            return;
        }
        if (offset >= 0x4000 && offset <= 0x43FF) {
            // CPU I/O, Joypad, DMA
            return;
        }
    }
}