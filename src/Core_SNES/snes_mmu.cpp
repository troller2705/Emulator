#include "Core_SNES/snes_mmu.h"
#include "Core_Shared/battery_io.h"
#include <iostream>
#include <cstring>

SNES_MMU::SNES_MMU() {
    reset();
}

void SNES_MMU::reset() {
    m_wram.fill(0);
    m_wmadd = 0;
    m_nmitimen = 0;
    m_nmi_flag = false;
    m_nmi_pending = false;
    m_joypad1 = 0;
    m_joypad_shift = 0;
    m_joypad_strobe = false;

    m_wrmpya = 0xFF;
    m_wrdiva = 0xFFFF;
    m_rddiv  = 0;
    m_rdmpy  = 0;

    std::memset(m_dma_regs, 0, sizeof(m_dma_regs));

    m_ppu.reset();
    m_apu.reset();
}

bool SNES_MMU::load_rom(const std::vector<uint8_t>& rom_data) {
    if ((rom_data.size() % 0x400) == 512) {
        m_rom.assign(rom_data.begin() + 512, rom_data.end());
    } else {
        m_rom = rom_data;
    }

    if (m_rom.size() < 0x8000) {
        std::cerr << "[SNES_MMU] ROM is too small.\n";
        return false;
    }

    uint16_t lorom_checksum = m_rom[0x7FDE] | (m_rom[0x7FDF] << 8);
    uint16_t lorom_inverse  = m_rom[0x7FDC] | (m_rom[0x7FDD] << 8);

    uint16_t hirom_checksum = (m_rom.size() >= 0x10000) ? (m_rom[0xFFDE] | (m_rom[0xFFDF] << 8)) : 0;
    uint16_t hirom_inverse  = (m_rom.size() >= 0x10000) ? (m_rom[0xFFDC] | (m_rom[0xFFDD] << 8)) : 0;

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

    uint8_t sram_code = m_rom[header_offset + 0x18];
    m_sram_size = (sram_code == 0) ? 0 : (1024ULL << sram_code);
    m_sram.assign(m_sram_size, 0);

    std::cout << "--- SNES CARTRIDGE LOADED ---\n";
    std::cout << "Title:   " << title << "\n";
    std::cout << "Mapping: " << (m_map_mode == SNESMapMode::LoROM ? "LoROM" : "HiROM") << "\n";
    std::cout << "ROM:     " << (m_rom.size() / 1024) << " KB\n";
    std::cout << "SRAM:    " << (m_sram_size / 1024) << " KB\n";
    std::cout << "-----------------------------\n";

    reset();
    return true;
}

void SNES_MMU::load_battery(const std::string& path) {
    if (!m_sram.empty()) {
        CoreShared::load_battery_file(path, m_sram.data(), m_sram.size());
    }
}

void SNES_MMU::save_battery(const std::string& path) {
    if (!m_sram.empty()) {
        CoreShared::save_battery_file(path, m_sram.data(), m_sram.size());
    }
}

void SNES_MMU::notify_vblank_start() {
    m_nmi_flag = true;
    if (m_nmitimen & 0x80) {
        m_nmi_pending = true;
    }
}

bool SNES_MMU::poll_nmi() {
    if (m_nmi_pending) {
        m_nmi_pending = false;
        return true;
    }
    return false;
}

int32_t SNES_MMU::map_rom_address(uint8_t bank, uint16_t offset) const {
    if (m_map_mode == SNESMapMode::LoROM) {
        if (offset < 0x8000) return -1;
        return ((bank & 0x7F) * 0x8000) + (offset - 0x8000);
    } else {
        return ((bank & 0x3F) * 0x10000) + offset;
    }
}

int32_t SNES_MMU::map_sram_address(uint8_t bank, uint16_t offset) const {
    if (m_sram.empty()) return -1;
    if (m_map_mode == SNESMapMode::LoROM) {
        if (((bank >= 0x70 && bank <= 0x7D) || bank >= 0xF0) && offset < 0x8000) {
            return (((bank & 0x0F) * 0x8000) + offset) % m_sram.size();
        }
    } else {
        if (((bank >= 0x20 && bank <= 0x3F) || (bank >= 0xA0 && bank <= 0xBF)) &&
            (offset >= 0x6000 && offset <= 0x7FFF)) {
            return (((bank & 0x1F) * 0x2000) + (offset - 0x6000)) % m_sram.size();
        }
    }
    return -1;
}

uint8_t SNES_MMU::read8(uint32_t addr) {
    uint8_t bank    = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // 1. Full 128 KB WRAM (Banks 0x7E and 0x7F)
    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wram_addr = ((bank & 1) << 16) | offset;
        return m_wram[wram_addr];
    }

    // 2. System Area (Banks 0x00-0x3F and 0x80-0xBF, Lower 32 KB)
    if ((bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF)) {
        // Low 8 KB WRAM Mirror ($0000 - $1FFF)
        if (offset < 0x2000) {
            return m_wram[offset];
        }
        // PPU B-Bus Registers ($2100 - $213F)
        if (offset >= 0x2100 && offset <= 0x213F) {
            return m_ppu.readRegister(offset);
        }
        // APU I/O Ports ($2140 - $2143)
        if (offset >= 0x2140 && offset <= 0x2143) {
            return m_apu.cpuReadPort(offset);
        }
        // WMDATA - WRAM Data Read ($2180)
        if (offset == 0x2180) {
            uint8_t val = m_wram[m_wmadd & 0x1FFFF];
            m_wmadd = (m_wmadd + 1) & 0x1FFFF;
            return val;
        }
        // Legacy Joypad Serial Port ($4016 - $4017)
        if (offset == 0x4016) {
            uint8_t bit = (m_joypad_shift >> 15) & 1;
            if (!m_joypad_strobe) m_joypad_shift = (m_joypad_shift << 1) | 1;
            return bit;
        }
        if (offset == 0x4017) {
            return 0x00;
        }
        // RDNMI - VBlank NMI Flag & CPU Version ($4210)
        if (offset == 0x4210) {
            uint8_t val = (m_nmi_flag ? 0x80 : 0x00) | 0x02;
            m_nmi_flag = false;
            return val;
        }
        // TIMEUP - IRQ Flag ($4211)
        if (offset == 0x4211) {
            return 0x00;
        }
        // HVBJOY - VBlank / HBlank / Auto-Joypad Status ($4212)
        if (offset == 0x4212) {
            return m_ppu.isInVBlank() ? 0x80 : 0x00;
        }
        // Hardware Math Results ($4214 - $4217)
        if (offset == 0x4214) return m_rddiv & 0xFF;
        if (offset == 0x4215) return (m_rddiv >> 8) & 0xFF;
        if (offset == 0x4216) return m_rdmpy & 0xFF;
        if (offset == 0x4217) return (m_rdmpy >> 8) & 0xFF;

        // Auto-Joypad Read Registers ($4218 - $4219)
        if (offset == 0x4218) return m_joypad1 & 0xFF;
        if (offset == 0x4219) return (m_joypad1 >> 8) & 0xFF;

        // DMA Channel Registers ($4300 - $437F)
        if (offset >= 0x4300 && offset <= 0x437F) {
            return m_dma_regs[(offset >> 4) & 7][offset & 0x0F];
        }
    }

    // 3. Cartridge SRAM
    int32_t sram_addr = map_sram_address(bank, offset);
    if (sram_addr >= 0) {
        return m_sram[sram_addr];
    }

    // 4. Cartridge ROM
    int32_t rom_addr = map_rom_address(bank, offset);
    if (rom_addr >= 0 && !m_rom.empty()) {
        return m_rom[rom_addr % m_rom.size()];
    }

    return 0x00;
}

void SNES_MMU::write8(uint32_t addr, uint8_t val) {
    uint8_t bank    = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // 1. Full 128 KB WRAM (Banks 0x7E and 0x7F)
    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wram_addr = ((bank & 1) << 16) | offset;
        m_wram[wram_addr] = val;
        return;
    }

    // 2. System Area (Banks 0x00-0x3F and 0x80-0xBF, Lower 32 KB)
    if ((bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF)) {
        // Low 8 KB WRAM Mirror ($0000 - $1FFF)
        if (offset < 0x2000) {
            m_wram[offset] = val;
            return;
        }
        // PPU B-Bus Registers ($2100 - $213F)
        if (offset >= 0x2100 && offset <= 0x213F) {
            m_ppu.writeRegister(offset, val);
            return;
        }
        // APU I/O Ports ($2140 - $2143)
        if (offset >= 0x2140 && offset <= 0x2143) {
            m_apu.cpuWritePort(offset, val);
            return;
        }
        // WRAM Data & Address Portals ($2180 - $2183)
        if (offset == 0x2180) {
            m_wram[m_wmadd & 0x1FFFF] = val;
            m_wmadd = (m_wmadd + 1) & 0x1FFFF;
            return;
        }
        if (offset == 0x2181) { m_wmadd = (m_wmadd & 0x1FF00) | val; return; }
        if (offset == 0x2182) { m_wmadd = (m_wmadd & 0x100FF) | (static_cast<uint32_t>(val) << 8); return; }
        if (offset == 0x2183) { m_wmadd = (m_wmadd & 0x0FFFF) | (static_cast<uint32_t>(val & 1) << 16); return; }

        // Joypad Strobe ($4016)
        if (offset == 0x4016) {
            m_joypad_strobe = (val & 1) != 0;
            if (m_joypad_strobe) m_joypad_shift = m_joypad1;
            return;
        }

        // NMITIMEN - Interrupt Enable ($4200)
        if (offset == 0x4200) {
            bool was_enabled = (m_nmitimen & 0x80) != 0;
            m_nmitimen = val;
            if (!was_enabled && (val & 0x80) && m_nmi_flag) {
                m_nmi_pending = true;
            }
            return;
        }

        // Hardware Multiply & Divide Registers ($4202 - $4206)
        if (offset == 0x4202) { m_wrmpya = val; return; }
        if (offset == 0x4203) {
            m_rdmpy = static_cast<uint16_t>(m_wrmpya) * static_cast<uint16_t>(val);
            return;
        }
        if (offset == 0x4204) { m_wrdiva = (m_wrdiva & 0xFF00) | val; return; }
        if (offset == 0x4205) { m_wrdiva = (m_wrdiva & 0x00FF) | (static_cast<uint16_t>(val) << 8); return; }
        if (offset == 0x4206) {
            if (val == 0) {
                m_rddiv = 0xFFFF;
                m_rdmpy = m_wrdiva;
            } else {
                m_rddiv = m_wrdiva / val;
                m_rdmpy = m_wrdiva % val;
            }
            return;
        }

        // MDMAEN - Trigger General Purpose DMA ($420B)
        if (offset == 0x420B) {
            executeDMA(val);
            return;
        }

        // DMA Channel Registers ($4300 - $437F)
        if (offset >= 0x4300 && offset <= 0x437F) {
            m_dma_regs[(offset >> 4) & 7][offset & 0x0F] = val;
            return;
        }
    }

    // 3. Cartridge SRAM
    int32_t sram_addr = map_sram_address(bank, offset);
    if (sram_addr >= 0) {
        m_sram[sram_addr] = val;
    }
}

void SNES_MMU::executeDMA(uint8_t channel_mask) {
    static const int offsets[8][4] = {
        {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1}
    };

    for (int ch = 0; ch < 8; ++ch) {
        if (!(channel_mask & (1 << ch))) continue;

        uint8_t dmap  = m_dma_regs[ch][0];
        uint8_t bbad  = m_dma_regs[ch][1];
        uint16_t a1t  = m_dma_regs[ch][2] | (m_dma_regs[ch][3] << 8);
        uint8_t a1b   = m_dma_regs[ch][4];
        uint16_t das  = m_dma_regs[ch][5] | (m_dma_regs[ch][6] << 8);

        uint8_t mode    = dmap & 0x07;
        bool dir_to_cpu = (dmap & 0x80) != 0;
        bool fixed_addr = (dmap & 0x08) != 0;
        bool dec_addr   = (dmap & 0x10) != 0;

        uint32_t count = (das == 0) ? 0x10000 : das;
        int pos = 0;

        while (count > 0) {
            uint16_t b_addr = 0x2100 | ((bbad + offsets[mode][pos & 3]) & 0xFF);
            uint32_t a_addr = (static_cast<uint32_t>(a1b) << 16) | a1t;

            if (!dir_to_cpu) {
                write8(b_addr, read8(a_addr));
            } else {
                write8(a_addr, read8(b_addr));
            }

            if (!fixed_addr) {
                a1t += dec_addr ? -1 : 1;
            }
            pos++;
            count--;
        }

        m_dma_regs[ch][2] = a1t & 0xFF;
        m_dma_regs[ch][3] = (a1t >> 8) & 0xFF;
        m_dma_regs[ch][5] = 0;
        m_dma_regs[ch][6] = 0;
    }
}