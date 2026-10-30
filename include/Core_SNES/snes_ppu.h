#pragma once
#include <cstdint>
#include <vector>

class SNES_PPU {
public:
    SNES_PPU();
    void reset();
    void advance_cycles(int cpu_cycles);

    // VRAM Access
    uint16_t read_vram(uint16_t addr);
    void write_vram(uint16_t addr, uint16_t val);

    void set_vram_address(uint16_t addr);
    uint16_t get_vram_addr() const { return m_vram_addr; }
    void set_vram_increment_mode(uint8_t mode);
    void apply_vram_increment();

    // CGRAM Access
    void set_cgram_addr(uint8_t addr);
    void write_cgram_byte(uint8_t val);
    uint8_t read_cgram(uint8_t addr);

    // OAM Access
    uint8_t read_oam(uint16_t addr);
    void write_oam(uint16_t addr, uint8_t val);

    // Timing
    void run_frame();
    bool is_vblank() const { return m_vblank; }
    bool read_nmi_flag() {
        bool flag = m_nmi_flag;
        m_nmi_flag = false; // Reading $4210 clears Bit 7 on hardware
        return flag;
    }
    bool poll_nmi_trigger() {
        if (m_nmi_trigger) {
            m_nmi_trigger = false;
            return true;
        }
        return false;
    }

private:
    std::vector<uint16_t> m_vram;   // 64 KB (32K words)
    std::vector<uint8_t>  m_oam;    // 544 bytes
    std::vector<uint8_t>  m_cgram;  // 512 bytes

    int m_scanline;
    int m_dot;
    bool m_vblank;
    bool m_nmi_flag = false;
    bool m_nmi_trigger = false;

    uint16_t m_vram_addr;
    uint8_t  m_vram_increment_mode;
    uint16_t m_vram_increment_value;

    // CGRAM internal state
    uint8_t  m_cgram_addr;
    uint8_t  m_cgram_toggle;
    uint8_t  m_cgram_latch;
};