#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>

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
        // The 65C816 is strictly little-endian
        return read8(addr) | (static_cast<uint16_t>(read8(addr + 1)) << 8);
    }
    
    void write16(uint32_t addr, uint16_t val) {
        write8(addr, val & 0xFF);
        write8(addr + 1, (val >> 8) & 0xFF);
    }

private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_sram;              // Save RAM
    std::array<uint8_t, 128 * 1024> m_wram{}; // 128 KB Work RAM (0x7E0000 - 0x7FFFFF)

    std::array<uint8_t, 4> m_apu_ports{};

    SNESMapMode m_map_mode = SNESMapMode::Unknown;
    size_t m_sram_size = 0;

    // Address translation helpers
    int32_t map_rom_address(uint8_t bank, uint16_t offset) const;
    int32_t map_sram_address(uint8_t bank, uint16_t offset) const;
};