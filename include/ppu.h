#ifndef PPU_H
#ifndef PPU_H
#define PPU_H

#include <cstdint>
#include <vector>

class PPU {
public:
    PPU();
    ~PPU() = default;

    // Core step function driven by your CPU / clock cycle loops
    void step(int cycles);

    // Memory read/write interface for PPU registers (like 0xFF40 LCDC, 0xFF44 LY)
    uint8_t read_register(uint16_t address) const;
    void write_register(uint16_t address, uint8_t value);

    // LCDC Bit Check Helpers
    bool is_lcd_enabled() const { return (m_lcdc & 0x80) != 0; }
    uint16_t get_bg_tile_map_address() const { return (m_lcdc & 0x08) ? 0x9C00 : 0x9800; }
    uint16_t get_tile_data_address() const { return (m_lcdc & 0x10) ? 0x8000 : 0x8800; }
    bool is_window_enabled() const { return (m_lcdc & 0x20) != 0; }
    uint16_t get_window_tile_map_address() const { return (m_lcdc & 0x40) ? 0x9C00 : 0x9800; }
    bool are_sprites_enabled() const { return (m_lcdc & 0x02) != 0; }
    uint8_t get_sprite_height() const { return (m_lcdc & 0x04) ? 16 : 8; }
    bool is_bg_window_enabled() const { return (m_lcdc & 0x01) != 0; }

private:
    // Registers
    uint8_t m_lcdc; // LCD Control (0xFF40)
    uint8_t m_stat; // LCD Status (0xFF41)
    uint8_t m_scy;  // Scroll Y (0xFF42)
    uint8_t m_scx;  // Scroll X (0xFF43)
    uint8_t m_ly;   // LCD Y-Coordinate (0xFF44)
    uint8_t m_lyc;  // LY Compare (0xFF45)

    // Internal PPU Timing State
    int m_scanline_counter;
    
    void change_mode(uint8_t mode);
};

#endif
#endif