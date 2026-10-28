#pragma once
#include <array>
#include <cstdint>
#include "../Core_Shared/IEmulatorCore.h"

class GBAMMU;

class GBACPU {
public:
    explicit GBACPU(GBAMMU& mmu);

    void reset();
    void step();
    void check_irqs();

    CPUState get_state() const;

private:
    GBAMMU& m_mmu;

    std::array<uint32_t, 16> m_regs{};
    uint32_t m_cpsr = 0x0000001F;

    uint32_t m_sp_usr = 0x03007F00, m_lr_usr = 0;
    uint32_t m_sp_irq = 0x03007FA0, m_lr_irq = 0, m_spsr_irq = 0;
    uint32_t m_sp_svc = 0x03007FE0, m_lr_svc = 0, m_spsr_svc = 0;
    uint32_t m_sp_fiq = 0x03007F00, m_lr_fiq = 0, m_spsr_fiq = 0;

    void switch_mode(uint8_t new_mode);
    uint32_t get_spsr() const;
    void set_spsr(uint32_t val);

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

    void handle_swi(uint8_t comment);
    void step_arm();
    void step_thumb();
};