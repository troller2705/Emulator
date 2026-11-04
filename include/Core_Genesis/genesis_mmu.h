#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include "genesis_vdp.h"

class GenesisMMU {
public:
    GenesisMMU();
    void reset();
    bool load_rom(const std::vector<uint8_t>& rom_data);

    void load_battery(const std::string& path);
    void save_battery(const std::string& path);

    // Big-Endian Memory Bus Accessors
    uint8_t  read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    uint32_t read32(uint32_t addr);

    void write8(uint32_t addr, uint8_t val);
    void write16(uint32_t addr, uint16_t val);
    void write32(uint32_t addr, uint32_t val);

    void set_joypad_state(uint16_t state) { m_pad_state = state; }

    GenesisVDP& get_vdp() { return m_vdp; }
    const GenesisVDP& get_vdp() const { return m_vdp; }

private:
    GenesisVDP m_vdp;

    std::vector<uint8_t>          m_rom;
    std::vector<uint8_t>          m_sram;
    std::array<uint8_t, 0x10000>  m_wram{};    // 64 KB 68000 Work RAM ($FF0000-$FFFFFF)
    std::array<uint8_t, 0x2000>   m_z80_ram{}; // 8 KB Z80 RAM ($A00000-$A01FFF)

    bool     m_sram_enabled = false;
    uint16_t m_z80_busreq = 0; // $A11100
    uint16_t m_z80_reset  = 0; // $A11200

    // 3-Button / 6-Button Controller I/O ($A10003 / $A10009)
    uint16_t m_pad_state = 0; // Active-high bitmask from InputMapper
    uint8_t  m_pad_data_latch = 0x40; // TH pin state (Bit 6)
    uint8_t  m_pad_ctrl = 0x40;
    uint8_t  read_pad_port() const;
};