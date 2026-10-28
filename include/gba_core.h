#pragma once
#include "IEmulatorCore.h"
#include <array>
#include <vector>
#include <string>
#include <cstdint>

class GBACore : public IEmulatorCore {
public:
    GBACore();
    ~GBACore() override = default;

    bool load_rom(const std::vector<uint8_t>& rom_data) override;
    void run_frame() override;
    void reset() override;
    void set_input(uint32_t input_state) override;

    const void* get_video_buffer() const override { return m_framebuffer.data(); }
    const float* get_audio_buffer() const override { return m_audio_buffer.data(); }
    size_t get_audio_sample_count() const override { return m_audio_buffer.size(); }

    int get_video_width() const override { return 240; }
    int get_video_height() const override { return 160; }

    void load_battery(const std::string& path) override;
    void save_battery(const std::string& path) override;

    // Debugger Interface
    void step_instruction() override;
    CPUState get_cpu_state() const override;
    uint8_t debug_read_memory(uint16_t address) override;
    void debug_write_memory(uint16_t address, uint8_t value) override;
    void debug_update_tile_buffer() override;
    const uint32_t* debug_get_tile_buffer() const override { return m_debug_tile_buffer.data(); }

private:
    // --- ARM7TDMI CPU STATE ---
    std::array<uint32_t, 16> m_regs{};
    uint32_t m_cpsr = 0x0000001F; // System Mode
    bool m_halted = false;

    // Banked R13 (SP), R14 (LR), and SPSR for IRQ, SVC, FIQ, ABT, UND, USR/SYS
    uint32_t m_sp_usr = 0x03007F00, m_lr_usr = 0;
    uint32_t m_sp_irq = 0x03007FA0, m_lr_irq = 0, m_spsr_irq = 0;
    uint32_t m_sp_svc = 0x03007FE0, m_lr_svc = 0, m_spsr_svc = 0;
    uint32_t m_sp_fiq = 0x03007F00, m_lr_fiq = 0, m_spsr_fiq = 0;

    void switch_mode(uint8_t new_mode);
    uint32_t get_spsr() const;
    void set_spsr(uint32_t val);

    // CPSR Flags (N = Bit 31, Z = Bit 30, C = Bit 29, V = Bit 28, T = Bit 5)
    bool flag_n() const { return (m_cpsr >> 31) & 1; }
    bool flag_z() const { return (m_cpsr >> 30) & 1; }
    bool flag_c() const { return (m_cpsr >> 29) & 1; }
    bool flag_v() const { return (m_cpsr >> 28) & 1; }
    bool is_thumb() const { return (m_cpsr >> 5) & 1; }

    void set_nz(uint32_t res) {
        m_cpsr &= ~0xC0000000U;
        if (res & 0x80000000U) m_cpsr |= (1U << 31);
        if (res == 0)          m_cpsr |= (1U << 30);
    }
    void set_c(bool c) {
        if (c) m_cpsr |= (1U << 29);
        else   m_cpsr &= ~(1U << 29);
    }
    void set_v(bool v) {
        if (v) m_cpsr |= (1U << 28);
        else   m_cpsr &= ~(1U << 28);
    }

    bool check_cond(uint8_t cond) const;
    uint32_t barrel_shift(uint32_t val, uint8_t type, uint32_t amount, bool& carry_out, bool immediate_shift);
    uint32_t alu_add(uint32_t a, uint32_t b, bool set_flags);
    uint32_t alu_adc(uint32_t a, uint32_t b, bool carry_in, bool set_flags);
    uint32_t alu_sub(uint32_t a, uint32_t b, bool set_flags);
    uint32_t alu_sbc(uint32_t a, uint32_t b, bool carry_in, bool set_flags);

    // --- MEMORY DOMAINS ---
    std::array<uint8_t, 0x04000> m_bios{};    // 16 KB BIOS (0x00000000)
    std::array<uint8_t, 0x40000> m_ewram{};   // 256 KB Work RAM (0x02000000)
    std::array<uint8_t, 0x08000> m_iwram{};   // 32 KB Internal RAM (0x03000000)
    std::array<uint8_t, 0x00400> m_io{};      // 1 KB I/O Registers (0x04000000)
    std::array<uint8_t, 0x00400> m_pal_ram{}; // 1 KB Palette RAM (0x05000000)
    std::array<uint8_t, 0x18000> m_vram{};    // 96 KB VRAM (0x06000000)
    std::array<uint8_t, 0x00400> m_oam{};     // 1 KB OAM (0x07000000)
    std::vector<uint8_t>         m_rom;       // Up to 32 MB GamePak (0x08000000)
    std::array<uint8_t, 0x10000> m_sram{};    // 64 KB SRAM (0x0E000000)

    // --- DISPLAY & AUDIO BUFFERS ---
    std::array<uint32_t, 240 * 160> m_framebuffer{};
    std::array<uint32_t, 128 * 192> m_debug_tile_buffer{};
    std::vector<float>              m_audio_buffer;

    uint16_t m_keyinput = 0x03FF; // Active-low 10-button state (0x04000130)

    // --- BUS & HARDWARE HELPERS ---
    uint8_t  read8(uint32_t addr) const;
    uint16_t read16(uint32_t addr) const;
    uint32_t read32(uint32_t addr) const;
    void     write8(uint32_t addr, uint8_t val);
    void     write16(uint32_t addr, uint16_t val);
    void     write32(uint32_t addr, uint32_t val);

    uint16_t io_read16(uint32_t offset) const {
        return m_io[offset] | (static_cast<uint16_t>(m_io[offset + 1]) << 8);
    }
    void io_write16(uint32_t offset, uint16_t val) {
        m_io[offset]     = val & 0xFF;
        m_io[offset + 1] = (val >> 8) & 0xFF;
    }

    void trigger_dma(int ch);
    void request_irq(uint16_t flag);
    void check_irqs();
    void handle_swi(uint8_t comment);

    void step_arm();
    void step_thumb();
    void render_scanline(int ly);
    void render_sprites(int ly, uint16_t dispcnt);

    static uint32_t bgr555_to_argb8888(uint16_t bgr);
};