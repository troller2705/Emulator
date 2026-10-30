#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include "snes_ppu.h" // Include your PPU header

enum class SNESMapMode {
    LoROM,
    HiROM,
    Unknown
};

class SNES_MMU {
public:
    SNES_MMU();

    void reset();
    bool load_rom(const std::vector<uint8_t>& rom_data);

    uint8_t  read8(uint32_t addr);
    void     write8(uint32_t addr, uint8_t val);

    uint16_t read16(uint32_t addr) {
        return read8(addr) | (static_cast<uint16_t>(read8(addr + 1)) << 8);
    }

    void write16(uint32_t addr, uint16_t val) {
        write8(addr, val & 0xFF);
        write8(addr + 1, (val >> 8) & 0xFF);
    }

    SNES_PPU& get_ppu() { return m_ppu; }

    bool poll_nmi() {
        return m_ppu.poll_nmi_trigger() && ((m_nmitimen & 0x80) != 0);
    }

private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_sram;
    std::array<uint8_t, 128 * 1024> m_wram{};
    std::array<uint8_t, 4> m_apu_ports{};

    SNES_PPU m_ppu; // Embedded PPU instance

    // VRAM port latches
    uint8_t m_vram_addr_latch = 0;
    uint8_t m_vram_addr_h_latch = 0;

    SNESMapMode m_map_mode = SNESMapMode::Unknown;
    size_t m_sram_size = 0;

    int32_t map_rom_address(uint8_t bank, uint16_t offset) const;
    int32_t map_sram_address(uint8_t bank, uint16_t offset) const;

    uint8_t m_nmitimen = 0; // $4200: Bit 7 enables VBlank NMI
};