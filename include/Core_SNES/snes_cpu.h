#pragma once
#include <cstdint>

class SNES_MMU;

union SNES_Register {
    uint16_t W;   // 16-bit Word access
    struct {
        // Little-endian layout
        uint8_t L; // Low byte (e.g., A)
        uint8_t H; // High byte (e.g., B)
    };
};

class SNES_CPU {
public:
    explicit SNES_CPU(SNES_MMU& mmu);

    void reset();
    int step();
    void trigger_nmi();

    // The 65C816 Register File
    SNES_Register A;  // Accumulator (A is Low, B is High. Combined = C)
    SNES_Register X;  // Index X
    SNES_Register Y;  // Index Y
    SNES_Register S;  // Stack Pointer
    
    uint16_t PC;      // Program Counter (16-bit offset)
    uint8_t  PB;      // Program Bank (8-bit bank)
    uint8_t  DB;      // Data Bank (8-bit)
    uint16_t D;       // Direct Page Register

    // Status Register (P) Flags
    bool flag_n = false; // Negative
    bool flag_v = false; // Overflow
    bool flag_m = true;  // Memory/Accumulator size (1 = 8-bit, 0 = 16-bit)
    bool flag_x = true;  // Index size (1 = 8-bit, 0 = 16-bit)
    bool flag_d = false; // Decimal mode
    bool flag_i = true;  // IRQ Disable
    bool flag_z = false; // Zero
    bool flag_c = false; // Carry
    bool flag_e = true;  // Emulation Mode (1 = 6502 compatibility)

    uint8_t get_p() const;

private:
    SNES_MMU& m_mmu;
    

    void set_p(uint8_t val);

    uint8_t  fetch8();
    uint16_t fetch16();
    void    push8(uint8_t val);
    uint8_t pop8();

    void push16(uint16_t val);
};