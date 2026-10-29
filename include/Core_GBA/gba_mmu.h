#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include "../Core_Shared/bit_utils.h"

class GBATimer;
class GBAAPU;

// Interrupt Flag Masks
constexpr uint16_t IRQ_VBLANK  = 1 << 0;
constexpr uint16_t IRQ_HBLANK  = 1 << 1;
constexpr uint16_t IRQ_VCOUNT  = 1 << 2;
constexpr uint16_t IRQ_TIMER0  = 1 << 3;
constexpr uint16_t IRQ_TIMER1  = 1 << 4;
constexpr uint16_t IRQ_TIMER2  = 1 << 5;
constexpr uint16_t IRQ_TIMER3  = 1 << 6;
constexpr uint16_t IRQ_SERIAL  = 1 << 7;
constexpr uint16_t IRQ_DMA0    = 1 << 8;
constexpr uint16_t IRQ_DMA1    = 1 << 9;
constexpr uint16_t IRQ_DMA2    = 1 << 10;
constexpr uint16_t IRQ_DMA3    = 1 << 11;
constexpr uint16_t IRQ_KEYPAD  = 1 << 12;

class GBAMMU {
public:
    GBAMMU();

    void reset();
    void load_rom(const std::vector<uint8_t>& rom_data);
    void load_battery(const std::string& path);
    void save_battery(const std::string& path);

    void link_components(GBATimer* timer, GBAAPU* apu) {
        m_timer = timer;
        m_apu   = apu;
    }

    uint8_t  read8(uint32_t addr) const;
    uint16_t read16(uint32_t addr) const;
    uint32_t read32(uint32_t addr) const;

    void write8(uint32_t addr, uint8_t val);
    void write16(uint32_t addr, uint16_t val);
    void write32(uint32_t addr, uint32_t val);

    uint16_t io_read16(uint32_t offset) const {
        return CoreShared::read_le16(m_io.data(), offset);
    }
    void io_write16(uint32_t offset, uint16_t val) {
        CoreShared::write_le16(m_io.data(), offset, val);
    }

    // DMA Timing: 0 = Immediate, 1 = VBlank, 2 = HBlank, 3 = Special (FIFO)
    void run_dma(uint8_t timing_mode);
    void trigger_fifo_dma(int fifo_index);
    void request_irq(uint16_t flag);

    // Direct Memory Accessors for PPU & Core
    std::array<uint8_t, 0x04000>& bios()    { return m_bios; }
    std::array<uint8_t, 0x40000>& ewram()   { return m_ewram; }
    std::array<uint8_t, 0x08000>& iwram()   { return m_iwram; }
    std::array<uint8_t, 0x00400>& io()      { return m_io; }
    std::array<uint8_t, 0x00400>& pal_ram() { return m_pal_ram; }
    std::array<uint8_t, 0x18000>& vram()    { return m_vram; }
    std::array<uint8_t, 0x00400>& oam()     { return m_oam; }

    void set_keyinput(uint16_t keys) { m_keyinput = keys; }
    bool is_halted() const { return m_halted; }
    void set_halted(bool h) { m_halted = h; }

    bool is_in_bios() const { return m_in_bios; }
    void set_in_bios(bool in_bios) { m_in_bios = in_bios; }
    void set_bios_latch(uint32_t val) { m_bios_latch = val; }

private:
    std::array<uint8_t, 0x04000> m_bios{};    // 16 KB BIOS
    std::array<uint8_t, 0x40000> m_ewram{};   // 256 KB Work RAM
    std::array<uint8_t, 0x08000> m_iwram{};   // 32 KB Internal RAM
    std::array<uint8_t, 0x00400> m_io{};      // 1 KB I/O Registers
    std::array<uint8_t, 0x00400> m_pal_ram{}; // 1 KB Palette RAM
    std::array<uint8_t, 0x18000> m_vram{};    // 96 KB VRAM
    std::array<uint8_t, 0x00400> m_oam{};     // 1 KB OAM
    std::vector<uint8_t>         m_rom;       // Up to 32 MB GamePak ROM
    std::array<uint8_t, 0x20000> m_sram{};    // 128 KB SRAM / Flash 1M

    uint16_t m_keyinput = 0x03FF;
    bool     m_halted   = false;

    GBATimer* m_timer = nullptr;
    GBAAPU*   m_apu   = nullptr;

    // Latched DMA internal state
    struct DMAChannel {
        uint32_t src = 0;
        uint32_t dst = 0;
        uint32_t count = 0;
        bool active = false;
    };
    std::array<DMAChannel, 4> m_dma{};
    void latch_dma(int ch);
    void execute_dma_channel(int ch);

    // 128KB Flash State Machine (for Pokemon FireRed / Ruby / Sapphire / Emerald)
    enum class FlashState { Ready, Cmd1, Cmd2, EraseCmd1, EraseCmd2, WriteByte, BankSwitch };
    FlashState m_flash_state   = FlashState::Ready;
    bool       m_flash_id_mode = false;
    uint8_t    m_flash_bank    = 0;
    uint8_t read_flash(uint32_t addr) const;
    void    write_flash(uint32_t addr, uint8_t val);

    uint32_t m_bios_latch = 0xE3A02004; // Post-boot/SWI BIOS fetch latch (MOV R2, #4)
    bool     m_in_bios    = false;
};