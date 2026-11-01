#include "Core_SNES/snes_mmu.h"

#include <algorithm>

#include "Core_Shared/battery_io.h"
#include <iostream>
#include <cstring>
#include <cmath>

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
    m_dsp1.reset();
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
            uint8_t val = m_irq_flag ? 0x80 : 0x00;
            m_irq_flag = false; // Reading this register clears the flag!
            return val;
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

        // Delegate to DSP-1 Coprocessor
        if (offset >= 0x6000 && offset <= 0x7FFF) {
            return m_dsp1.read(offset);
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
            if ((val & 0x30) == 0) {
                m_irq_flag = false;
                m_irq_pending = false;
                m_irq_condition_prev = false;
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

        // HTIME and VTIME Registers ($4207 - $420A)
        if (offset == 0x4207) { m_htime = (m_htime & 0xFF00) | val; return; }
        if (offset == 0x4208) { m_htime = (m_htime & 0x00FF) | ((static_cast<uint16_t>(val) & 1) << 8); return; }
        if (offset == 0x4209) { m_vtime = (m_vtime & 0xFF00) | val; return; }
        if (offset == 0x420A) { m_vtime = (m_vtime & 0x00FF) | ((static_cast<uint16_t>(val) & 1) << 8); return; }

        // MDMAEN - Trigger General Purpose DMA ($420B)
        if (offset == 0x420B) {
            executeDMA(val);
            return;
        }
        // HDMAEN - H-Blank DMA Enable ($420C)
        if (offset == 0x420C) {
            m_hdmaen = val;
            return;
        }

        // DMA Channel Registers ($4300 - $437F)
        if (offset >= 0x4300 && offset <= 0x437F) {
            m_dma_regs[(offset >> 4) & 7][offset & 0x0F] = val;
            return;
        }

        // Delegate to DSP-1 Coprocessor
        if (offset >= 0x6000 && offset <= 0x7FFF) {
            m_dsp1.write(offset, val);
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

void SNES_MMU::initHDMA() {
    for (int ch = 0; ch < 8; ++ch) {
        m_hdma_terminated[ch] = false;
        m_hdma_do_transfer[ch] = false;
        if (!(m_hdmaen & (1 << ch))) continue;

        uint16_t a1t = m_dma_regs[ch][2] | (m_dma_regs[ch][3] << 8);
        uint8_t  a1b = m_dma_regs[ch][4];
        m_hdma_table_addr[ch] = a1t;

        uint8_t line = read8((static_cast<uint32_t>(a1b) << 16) | m_hdma_table_addr[ch]++);
        if (line == 0) {
            m_hdma_terminated[ch] = true;
            continue;
        }
        m_hdma_line_counter[ch] = line;
        m_hdma_do_transfer[ch] = true;

        if (m_dma_regs[ch][0] & 0x40) { // Indirect HDMA
            uint16_t ind = read16((static_cast<uint32_t>(a1b) << 16) | m_hdma_table_addr[ch]);
            m_hdma_table_addr[ch] += 2;
            m_dma_regs[ch][5] = ind & 0xFF;
            m_dma_regs[ch][6] = (ind >> 8) & 0xFF;
        }
    }
}

void SNES_MMU::stepHDMA() {
    static const int offsets[8][4] = {
        {0, 0, 0, 0}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1},
        {0, 1, 2, 3}, {0, 1, 0, 1}, {0, 0, 0, 0}, {0, 0, 1, 1}
    };
    static const int transfer_lengths[8] = {1, 2, 2, 4, 4, 4, 2, 4};

    for (int ch = 0; ch < 8; ++ch) {
        if (!(m_hdmaen & (1 << ch)) || m_hdma_terminated[ch]) continue;

        uint8_t dmap = m_dma_regs[ch][0];
        uint8_t bbad = m_dma_regs[ch][1];
        uint8_t a1b  = m_dma_regs[ch][4];
        bool indirect = (dmap & 0x40) != 0;
        uint8_t mode  = dmap & 0x07;
        int bytes_to_transfer = transfer_lengths[mode];

        if (m_hdma_do_transfer[ch]) {
            for (int i = 0; i < bytes_to_transfer; ++i) {
                uint16_t b_addr = 0x2100 | ((bbad + offsets[mode][i]) & 0xFF);
                uint32_t a_addr = 0;
                if (indirect) {
                    uint8_t dasb = m_dma_regs[ch][7];
                    uint16_t das = m_dma_regs[ch][5] | (m_dma_regs[ch][6] << 8);
                    a_addr = (static_cast<uint32_t>(dasb) << 16) | das;
                    das++;
                    m_dma_regs[ch][5] = das & 0xFF;
                    m_dma_regs[ch][6] = (das >> 8) & 0xFF;
                } else {
                    a_addr = (static_cast<uint32_t>(a1b) << 16) | m_hdma_table_addr[ch]++;
                }
                write8(b_addr, read8(a_addr));
            }
        }

        m_hdma_line_counter[ch]--;
        m_hdma_do_transfer[ch] = (m_hdma_line_counter[ch] & 0x80) != 0;

        if ((m_hdma_line_counter[ch] & 0x7F) == 0) {
            uint8_t next_line = read8((static_cast<uint32_t>(a1b) << 16) | m_hdma_table_addr[ch]++);
            m_hdma_line_counter[ch] = next_line;
            if (next_line == 0) {
                m_hdma_terminated[ch] = true;
                continue;
            }
            m_hdma_do_transfer[ch] = true;
            if (indirect) {
                uint16_t ind = read16((static_cast<uint32_t>(a1b) << 16) | m_hdma_table_addr[ch]);
                m_hdma_table_addr[ch] += 2;
                m_dma_regs[ch][5] = ind & 0xFF;
                m_dma_regs[ch][6] = (ind >> 8) & 0xFF;
            }
        }
    }
}

void SNES_MMU::check_irq(int h_dot, int v_line) {
    uint8_t mode = (m_nmitimen >> 4) & 0x03;
    if (mode == 0) {
        m_irq_condition_prev = false;
        m_irq_flag = false;
        m_irq_pending = false;
        return;
    }

    bool condition = false;
    if (mode == 1)      condition = (h_dot >= m_htime);
    else if (mode == 2) condition = (v_line == m_vtime);
    else if (mode == 3) condition = (v_line == m_vtime && h_dot >= m_htime);

    // Edge-triggered IRQ: set TIMEUP flag ($4211) and signal CPU once
    if (condition && !m_irq_condition_prev) {
        m_irq_flag = true;
        m_irq_pending = true;
    }
    m_irq_condition_prev = condition;
}

// --- DSP-1 High-Level Emulation ---

void SNES_DSP1::reset() {
    m_cmd = 0;
    m_waiting_for_command = true;
    m_high_byte_toggle = false;
    m_low_latch = 0;
    m_in_count = 0;
    m_in_expect = 0;
    m_out_count = 0;
    m_out_expect = 0;
    m_in.fill(0);
    m_out.fill(0);
    std::memset(m_matrix, 0, sizeof(m_matrix));
    m_fx = m_fy = m_fz = 0;
    m_lfe = m_les = 0;
    m_aas = m_azs = 0;
    m_vof = m_vva = 0;
    m_cx = m_cy = 0;
    m_raster_vs = 0;
}

void SNES_DSP1::generate_next_raster_line() {
    constexpr double PI = 3.14159265358979323846;
    auto to_rad = [](int16_t a) -> double { return static_cast<double>(a) * PI / 32768.0; };

    m_out_count = 0;
    m_out_expect = 0;

    // Distance below the horizon line (in screen pixels)
    int32_t py = static_cast<int32_t>(m_raster_vs) - static_cast<int32_t>(m_vof);
    if (py == 0) py = 1;

    double rad_a = to_rad(static_cast<int16_t>(m_aas));
    double rad_z = to_rad(static_cast<int16_t>(m_azs));
    double sin_a = std::sin(rad_a), cos_a = std::cos(rad_a);
    double sin_z = std::sin(rad_z), cos_z = std::cos(rad_z);

    // Authentic DSP-1 Op 0x0A Raster formula:
    // M = (Fz * cos(A) - Lfe * sin(A)) / (vs - vof)
    // In 8.8 fixed point, 1.0 = 256, which is already the scale of Fz / py!
    double scale = (static_cast<double>(m_fz) * cos_a - static_cast<double>(m_lfe) * sin_a) / static_cast<double>(py);
    if (scale < 0.0) scale = -scale; // Ensure positive forward projection below horizon

    int16_t m7a = static_cast<int16_t>(std::clamp( scale * cos_z, -32768.0, 32767.0));
    int16_t m7b = static_cast<int16_t>(std::clamp( scale * sin_z, -32768.0, 32767.0));
    int16_t m7c = static_cast<int16_t>(std::clamp(-scale * sin_z, -32768.0, 32767.0));
    int16_t m7d = static_cast<int16_t>(std::clamp( scale * cos_z, -32768.0, 32767.0));

    push_out16(m7a);
    push_out16(m7b);
    push_out16(m7c);
    push_out16(m7d);

    m_raster_vs++;
}

uint8_t SNES_DSP1::read(uint16_t addr) {
    // In HiROM ($6000-$7FFF), A12 ($1000) selects Status Register ($7000-$7FFF) vs Data Register ($6000-$6FFF)
    if (addr & 0x1000) {
        return 0x80; // Bit 7 = RQM (Request for Master / Ready)
    }

    if (m_out_count < m_out_expect) {
        uint8_t val = m_out[m_out_count++];
        if (m_out_count >= m_out_expect) {
            if (m_cmd == 0x0A || m_cmd == 0x1A) {
                // Op 0x0A/0x1A streams consecutive scanlines indefinitely until a new command is written!
                generate_next_raster_line();
            } else {
                m_out_count = 0;
                m_out_expect = 0;
                m_waiting_for_command = true;
            }
        }
        return val;
    }
    return 0x00;
}

void SNES_DSP1::write(uint16_t addr, uint8_t val) {
    // Ignore writes to the read-only Status Register ($7000-$7FFF)
    if (addr & 0x1000) return;

    // If we are currently in the middle of reading output (e.g. continuous Op 0x0A),
    // any new write to the Data Register aborts the output stream and starts a new command
    if (!m_waiting_for_command && m_in_count >= m_in_expect && m_out_expect > 0) {
        if ((m_cmd == 0x0A || m_cmd == 0x1A) && m_out_count == 0) {
            // In Op 0x0A, Super Mario Kart writes 0x8000 to terminate continuous raster mode
            if (!m_high_byte_toggle) {
                m_low_latch = val;
                m_high_byte_toggle = true;
                return;
            } else {
                m_high_byte_toggle = false;
                uint16_t word = m_low_latch | (static_cast<uint16_t>(val) << 8);
                if (word == 0x8000) {
                    m_out_count = 0;
                    m_out_expect = 0;
                    m_waiting_for_command = true;
                    return;
                }
                // Otherwise, treat the low byte as a new command!
                val = m_low_latch;
                addr &= ~1;
                m_waiting_for_command = true;
            }
        } else {
            m_out_count = 0;
            m_out_expect = 0;
            m_waiting_for_command = true;
        }
    }

    if (m_waiting_for_command) {
        if (addr & 1) return; // Ignore high byte of a 16-bit command write

        m_cmd = val & 0x3F;
        m_in_count = 0;
        m_out_count = 0;
        m_out_expect = 0;
        m_high_byte_toggle = false;

        switch (m_cmd) {
            case 0x00: m_in_expect = 2; break; // Multiply (K, I) -> 1 word
            case 0x10: m_in_expect = 2; break; // Inverse (A, B) -> 2 words
            case 0x04: m_in_expect = 2; break; // Sin/Cos (Angle, Radius) -> 2 words
            case 0x08: m_in_expect = 3; break; // Vector Size (X, Y, Z) -> 1 word
            case 0x18: m_in_expect = 4; break; // Vector Range (X, Y, Z, R) -> 1 word
            case 0x28: m_in_expect = 3; break; // Vector Abs (X, Y, Z) -> 1 word
            case 0x0C: m_in_expect = 3; break; // 2D Rotate (Angle, X, Y) -> 2 words
            case 0x1C: m_in_expect = 6; break; // 3D Rotate -> 3 words
            case 0x02: m_in_expect = 7; break; // Parameter (Fx, Fy, Fz, Lfe, Les, Aas, Azs) -> 4 words
            case 0x0A:                         // Raster (Vs) -> continuous 4-word blocks
            case 0x1A: m_in_expect = 1; break;
            case 0x06: m_in_expect = 3; break; // Project (X, Y, Z) -> 3 words
            case 0x0E: m_in_expect = 2; break; // Target (H, V) -> 2 words
            case 0x01:                         // Attitude A (m, Rz, Ry, Rx) -> 0 words
            case 0x11:                         // Attitude B
            case 0x21: m_in_expect = 4; break; // Attitude C
            case 0x0D:                         // Objective A (X, Y, Z) -> 3 words
            case 0x1D:                         // Objective B
            case 0x2D: m_in_expect = 3; break; // Objective C
            case 0x03:                         // Subjective A (X, Y, Z) -> 3 words
            case 0x13:                         // Subjective B
            case 0x23: m_in_expect = 3; break; // Subjective C
            case 0x0B:                         // Gyrate A (X, Y, Z) -> 1 word
            case 0x1B:                         // Gyrate B
            case 0x2B: m_in_expect = 3; break; // Gyrate C
            default:
                m_waiting_for_command = true;
                return;
        }
        m_waiting_for_command = false;
        return;
    }

    // Collect 16-bit parameter words (low byte first, then high byte)
    if (!m_high_byte_toggle) {
        m_low_latch = val;
        m_high_byte_toggle = true;
    } else {
        m_high_byte_toggle = false;
        if (m_in_count < 16) {
            m_in[m_in_count++] = static_cast<int16_t>(m_low_latch | (static_cast<uint16_t>(val) << 8));
        }
        if (m_in_count >= m_in_expect) {
            execute();
            if (m_out_expect == 0) {
                m_waiting_for_command = true;
            }
        }
    }
}

void SNES_DSP1::execute() {
    constexpr double PI = 3.14159265358979323846;
    auto to_rad = [](int16_t a) -> double { return static_cast<double>(a) * PI / 32768.0; };

    switch (m_cmd) {
        case 0x00: { // Multiply: (K * I) >> 15
            int32_t res = (static_cast<int32_t>(m_in[0]) * static_cast<int32_t>(m_in[1])) >> 15;
            push_out16(static_cast<int16_t>(res));
            break;
        }
        case 0x10: { // Inverse: 1 / (a * 2^b)
            int16_t a = m_in[0];
            int16_t b = m_in[1];
            if (a == 0) {
                push_out16(0x7FFF);
                push_out16(0);
            } else {
                double val = 1.0 / (static_cast<double>(a) / 32768.0 * std::ldexp(1.0, b));
                int exp = 0;
                double mant = std::frexp(val, &exp);
                int16_t out_a = static_cast<int16_t>(std::clamp(mant * 32768.0, -32768.0, 32767.0));
                int16_t out_b = static_cast<int16_t>(exp);
                push_out16(out_a);
                push_out16(out_b);
            }
            break;
        }
        case 0x04: { // Triangle: Sin and Cos scaled by Radius
            double rad = to_rad(m_in[0]);
            double r   = static_cast<double>(m_in[1]);
            push_out16(static_cast<int16_t>(std::sin(rad) * r));
            push_out16(static_cast<int16_t>(std::cos(rad) * r));
            break;
        }
        case 0x08: { // Radius: (X^2 + Y^2 + Z^2) >> 15
            double x = m_in[0], y = m_in[1], z = m_in[2];
            int32_t d = static_cast<int32_t>((x * x + y * y + z * z) / 32768.0);
            push_out16(static_cast<int16_t>(std::clamp(d, -32768, 32767)));
            break;
        }
        case 0x18: { // Range: (X^2 + Y^2 + Z^2 - R^2) >> 15
            double x = m_in[0], y = m_in[1], z = m_in[2], r = m_in[3];
            int32_t d = static_cast<int32_t>((x * x + y * y + z * z - r * r) / 32768.0);
            push_out16(static_cast<int16_t>(std::clamp(d, -32768, 32767)));
            break;
        }
        case 0x28: { // Distance: sqrt(X^2 + Y^2 + Z^2)
            double x = m_in[0], y = m_in[1], z = m_in[2];
            push_out16(static_cast<int16_t>(std::sqrt(x * x + y * y + z * z)));
            break;
        }
        case 0x0C: { // Rotate 2D: (Angle, Y1, X1) -> (Y2, X2)
            double rad = to_rad(m_in[0]);
            double y1  = m_in[1];
            double x1  = m_in[2];
            double s = std::sin(rad), c = std::cos(rad);
            push_out16(static_cast<int16_t>(y1 * c - x1 * s));
            push_out16(static_cast<int16_t>(y1 * s + x1 * c));
            break;
        }
        case 0x1C: { // Polar 3D Rotation
            double rz = to_rad(m_in[0]), rx = to_rad(m_in[1]), ry = to_rad(m_in[2]);
            double x = m_in[3], y = m_in[4], z = m_in[5];
            double sz = std::sin(rz), cz = std::cos(rz);
            double sx = std::sin(rx), cx = std::cos(rx);
            double sy = std::sin(ry), cy = std::cos(ry);

            double x1 = x * cz - y * sz, y1 = x * sz + y * cz, z1 = z;
            double x2 = x1, y2 = y1 * cx - z1 * sx, z2 = y1 * sx + z1 * cx;
            double x3 = x2 * cy + z2 * sy, y3 = y2, z3 = -x2 * sy + z2 * cy;
            push_out16(static_cast<int16_t>(x3));
            push_out16(static_cast<int16_t>(y3));
            push_out16(static_cast<int16_t>(z3));
            break;
        }
        case 0x02: { // Parameter (Camera setup for Mode 7)
            m_fx  = m_in[0];
            m_fy  = m_in[1];
            m_fz  = m_in[2];
            m_lfe = m_in[3];
            m_les = m_in[4];
            m_aas = static_cast<uint16_t>(m_in[5]);
            m_azs = static_cast<uint16_t>(m_in[6]);

            double rad_a = to_rad(static_cast<int16_t>(m_aas));
            double rad_z = to_rad(static_cast<int16_t>(m_azs));
            double sin_a = std::sin(rad_a), cos_a = std::cos(rad_a);
            double sin_z = std::sin(rad_z), cos_z = std::cos(rad_z);

            m_vof = static_cast<int16_t>(-static_cast<double>(m_les) * sin_a);
            m_vva = static_cast<int16_t>(-static_cast<double>(m_les) * cos_a);

            double denom = (std::abs(cos_a) < 0.001) ? ((cos_a < 0) ? -0.001 : 0.001) : cos_a;
            // Fz and Lfe are in 8.8 fixed point, while Fx/Fy/Cx/Cy are integer world coordinates (>> 8)
            double dist = ((static_cast<double>(m_fz) * sin_a + static_cast<double>(m_lfe)) / denom) / 256.0;
            m_cx = static_cast<int16_t>(std::clamp(static_cast<double>(m_fx) + dist * sin_z, -32768.0, 32767.0));
            m_cy = static_cast<int16_t>(std::clamp(static_cast<double>(m_fy) - dist * cos_z, -32768.0, 32767.0));

            push_out16(m_vof);
            push_out16(m_vva);
            push_out16(m_cx);
            push_out16(m_cy);
            break;
        }
        case 0x0A:
        case 0x1A: { // Raster: Start continuous Mode 7 Matrix stream from scanline Vs
            m_raster_vs = m_in[0];
            generate_next_raster_line();
            break;
        }
        case 0x06: { // Project 3D point (X, Y, Z) onto 2D screen (H, V, M)
            double dx = m_in[0] - m_fx;
            double dy = m_in[1] - m_fy;
            double dz = m_in[2] - m_fz;

            double rad_a = to_rad(static_cast<int16_t>(m_aas));
            double rad_z = to_rad(static_cast<int16_t>(m_azs));
            double sin_a = std::sin(rad_a), cos_a = std::cos(rad_a);
            double sin_z = std::sin(rad_z), cos_z = std::cos(rad_z);

            double rx =  dx * cos_z + dy * sin_z;
            double ry = -dx * sin_z + dy * cos_z;
            double py =  ry * sin_a - dz * cos_a;
            double pz =  ry * cos_a + dz * sin_a - m_lfe;

            if (std::abs(pz) < 1.0) pz = (pz < 0) ? -1.0 : 1.0;
            double inv_z = static_cast<double>(std::abs(m_les)) / std::abs(pz);

            int16_t h = static_cast<int16_t>(std::clamp(rx * inv_z, -32768.0, 32767.0));
            int16_t v = static_cast<int16_t>(std::clamp(m_vof - py * inv_z, -32768.0, 32767.0));
            int16_t m = static_cast<int16_t>(std::clamp(inv_z * 256.0, 0.0, 32767.0));
            push_out16(h);
            push_out16(v);
            push_out16(m);
            break;
        }
        case 0x0E: { // Target: Screen (H, V) -> Ground (X, Y)
            double h = m_in[0];
            double v = m_in[1];
            double py = v - m_vof;
            if (std::abs(py) < 1.0) py = 1.0;

            double rad_a = to_rad(static_cast<int16_t>(m_aas));
            double rad_z = to_rad(static_cast<int16_t>(m_azs));
            double sin_a = std::sin(rad_a), cos_a = std::cos(rad_a);
            double sin_z = std::sin(rad_z), cos_z = std::cos(rad_z);

            double scale = static_cast<double>(m_fz * cos_a - m_lfe * sin_a) / (py * (m_les != 0 ? std::abs(m_les) : 256));
            double gx = m_cx + scale * (h * cos_z - m_vva * sin_z);
            double gy = m_cy + scale * (h * sin_z + m_vva * cos_z);
            push_out16(static_cast<int16_t>(std::clamp(gx, -32768.0, 32767.0)));
            push_out16(static_cast<int16_t>(std::clamp(gy, -32768.0, 32767.0)));
            break;
        }
        case 0x01:
        case 0x11:
        case 0x21: { // Attitude A / B / C
            int idx = (m_cmd >> 4) & 3;
            double m  = static_cast<double>(m_in[0]) / 32768.0;
            double rz = to_rad(m_in[1]);
            double ry = to_rad(m_in[2]);
            double rx = to_rad(m_in[3]);

            double sz = std::sin(rz), cz = std::cos(rz);
            double sy = std::sin(ry), cy = std::cos(ry);
            double sx = std::sin(rx), cx = std::cos(rx);

            m_matrix[idx][0][0] = static_cast<int16_t>(m * (cy * cz - sx * sy * sz) * 32768.0);
            m_matrix[idx][0][1] = static_cast<int16_t>(m * (-cx * sz) * 32768.0);
            m_matrix[idx][0][2] = static_cast<int16_t>(m * (sy * cz + sx * cy * sz) * 32768.0);

            m_matrix[idx][1][0] = static_cast<int16_t>(m * (cy * sz + sx * sy * cz) * 32768.0);
            m_matrix[idx][1][1] = static_cast<int16_t>(m * (cx * cz) * 32768.0);
            m_matrix[idx][1][2] = static_cast<int16_t>(m * (sy * sz - sx * cy * cz) * 32768.0);

            m_matrix[idx][2][0] = static_cast<int16_t>(m * (-cx * sy) * 32768.0);
            m_matrix[idx][2][1] = static_cast<int16_t>(m * (sx) * 32768.0);
            m_matrix[idx][2][2] = static_cast<int16_t>(m * (cx * cy) * 32768.0);
            break;
        }
        case 0x0D:
        case 0x1D:
        case 0x2D: { // Objective (Global -> Object coordinates)
            int idx = (m_cmd >> 4) & 3;
            int32_t x = m_in[0], y = m_in[1], z = m_in[2];
            for (int c = 0; c < 3; ++c) {
                int32_t sum = (x * m_matrix[idx][0][c] + y * m_matrix[idx][1][c] + z * m_matrix[idx][2][c]) >> 15;
                push_out16(static_cast<int16_t>(std::clamp(sum, -32768, 32767)));
            }
            break;
        }
        case 0x03:
        case 0x13:
        case 0x23: { // Subjective (Object -> Global coordinates)
            int idx = (m_cmd >> 4) & 3;
            int32_t x = m_in[0], y = m_in[1], z = m_in[2];
            for (int r = 0; r < 3; ++r) {
                int32_t sum = (x * m_matrix[idx][r][0] + y * m_matrix[idx][r][1] + z * m_matrix[idx][r][2]) >> 15;
                push_out16(static_cast<int16_t>(std::clamp(sum, -32768, 32767)));
            }
            break;
        }
        case 0x0B:
        case 0x1B:
        case 0x2B: { // Inner Product / Gyrate
            int idx = (m_cmd >> 4) & 3;
            int32_t x = m_in[0], y = m_in[1], z = m_in[2];
            int32_t sum = (x * m_matrix[idx][0][2] + y * m_matrix[idx][1][2] + z * m_matrix[idx][2][2]) >> 15;
            push_out16(static_cast<int16_t>(std::clamp(sum, -32768, 32767)));
            break;
        }
    }
}