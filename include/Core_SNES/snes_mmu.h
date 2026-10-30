#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include "snes_ppu.h"
#include "snes_apu.h"

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

    void load_battery(const std::string& path);
    void save_battery(const std::string& path);

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
    const SNES_PPU& get_ppu() const { return m_ppu; }

    SNES_APU& get_apu() { return m_apu; }
    const SNES_APU& get_apu() const { return m_apu; }

    void notify_vblank_start();
    bool poll_nmi();

    void set_joypad_state(uint16_t state) { m_joypad1 = state; }
    void executeDMA(uint8_t channel_mask);

private:
    SNES_PPU m_ppu;
    SNES_APU m_apu;

    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_sram;
    std::array<uint8_t, 128 * 1024> m_wram{};

    // WRAM B-Bus Portal ($2180 - $2183)
    uint32_t m_wmadd = 0;

    // CPU I/O & Interrupt Registers ($4200 - $421F)
    uint8_t  m_nmitimen = 0;       // $4200
    bool     m_nmi_flag = false;   // $4210 bit 7
    bool     m_nmi_pending = false;
    uint16_t m_joypad1 = 0;        // $4218 - $4219
    uint16_t m_joypad_shift = 0;   // $4016 serial shift
    bool     m_joypad_strobe = false;

    // Hardware Multiply / Divide Registers ($4202 - $4206, $4214 - $4217)
    uint8_t  m_wrmpya = 0xFF;
    uint16_t m_wrdiva = 0xFFFF;
    uint16_t m_rddiv  = 0;
    uint16_t m_rdmpy  = 0;

    // 8-Channel DMA Registers ($4300 - $437F)
    uint8_t m_dma_regs[8][16]{};

    SNESMapMode m_map_mode = SNESMapMode::Unknown;
    size_t m_sram_size = 0;

    int32_t map_rom_address(uint8_t bank, uint16_t offset) const;
    int32_t map_sram_address(uint8_t bank, uint16_t offset) const;
};