#pragma once
#include <cstdint>
#include <array>

class GenesisMMU;

class GenesisCPU {
public:
    explicit GenesisCPU(GenesisMMU& mmu);
    void reset();
    int  step();
    void trigger_interrupt(uint8_t level);

    std::array<uint32_t, 8> D{}; // Data Registers D0-D7
    std::array<uint32_t, 8> A{}; // Address Registers A0-A7 (A7 = Active SP)
    uint32_t PC  = 0;
    uint32_t USP = 0;            // User Stack Pointer
    uint32_t SSP = 0;            // Supervisor Stack Pointer
    uint16_t SR  = 0x2700;       // Status Register (Supervisor=1, IRQ Mask=7)
    bool     m_stopped = false;

    // CCR Flag Helpers (Lower 5 bits of SR: X N Z V C)
    bool flag_c() const { return (SR & 0x0001) != 0; }
    bool flag_v() const { return (SR & 0x0002) != 0; }
    bool flag_z() const { return (SR & 0x0004) != 0; }
    bool flag_n() const { return (SR & 0x0008) != 0; }
    bool flag_x() const { return (SR & 0x0010) != 0; }

    void set_c(bool v) { if (v) SR |= 0x01; else SR &= ~0x01; }
    void set_v(bool v) { if (v) SR |= 0x02; else SR &= ~0x02; }
    void set_z(bool v) { if (v) SR |= 0x04; else SR &= ~0x04; }
    void set_n(bool v) { if (v) SR |= 0x08; else SR &= ~0x08; }
    void set_x(bool v) { if (v) SR |= 0x10; else SR &= ~0x10; }

private:
    GenesisMMU& m_mmu;

    uint16_t fetch16();
    uint32_t fetch32();
    void     push16(uint16_t val);
    void     push32(uint32_t val);
    uint16_t pop16();
    uint32_t pop32();
    void     set_sr(uint16_t new_sr);
    bool     eval_cond(uint8_t cond) const;

    // Effective Address (EA) Helpers (size: 1=Byte, 2=Word, 4=Long)
    uint32_t calc_ea_addr(uint8_t mode, uint8_t reg, int size);
    uint32_t read_ea(uint8_t mode, uint8_t reg, int size, uint32_t& out_addr);
    void     write_ea(uint8_t mode, uint8_t reg, int size, uint32_t addr, uint32_t val);
};