// --- PASTE THIS INTO YOUR switch(opcode) STATEMENT ---

    case 0X00: { // NOP 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X01: { // LD BC, n16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X02: { // LD BC, A
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X03: { // INC BC
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X04: { // INC B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X05: { // DEC B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X06: { // LD B, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X07: { // RLCA 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X08: { // LD a16, SP
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 20;
        break;
    }

    case 0X09: { // ADD HL, BC
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X0A: { // LD A, BC
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X0B: { // DEC BC
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X0C: { // INC C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X0D: { // DEC C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X0E: { // LD C, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X0F: { // RRCA 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X10: { // STOP n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X11: { // LD DE, n16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X12: { // LD DE, A
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X13: { // INC DE
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X14: { // INC D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X15: { // DEC D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X16: { // LD D, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X17: { // RLA 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X18: { // JR e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X19: { // ADD HL, DE
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X1A: { // LD A, DE
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X1B: { // DEC DE
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X1C: { // INC E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X1D: { // DEC E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X1E: { // LD E, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X1F: { // RRA 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X20: { // JR NZ, e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X21: { // LD HL, n16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X22: { // LD HL, A
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X23: { // INC HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X24: { // INC H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X25: { // DEC H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X26: { // LD H, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X27: { // DAA 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X28: { // JR Z, e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X29: { // ADD HL, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X2A: { // LD A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X2B: { // DEC HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X2C: { // INC L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X2D: { // DEC L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X2E: { // LD L, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X2F: { // CPL 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X30: { // JR NC, e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X31: { // LD SP, n16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X32: { // LD HL, A
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X33: { // INC SP
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X34: { // INC HL
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X35: { // DEC HL
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X36: { // LD HL, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X37: { // SCF 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X38: { // JR C, e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0X39: { // ADD HL, SP
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X3A: { // LD A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X3B: { // DEC SP
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X3C: { // INC A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X3D: { // DEC A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X3E: { // LD A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X3F: { // CCF 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X40: { // LD B, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X41: { // LD B, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X42: { // LD B, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X43: { // LD B, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X44: { // LD B, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X45: { // LD B, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X46: { // LD B, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X47: { // LD B, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X48: { // LD C, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X49: { // LD C, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X4A: { // LD C, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X4B: { // LD C, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X4C: { // LD C, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X4D: { // LD C, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X4E: { // LD C, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X4F: { // LD C, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X50: { // LD D, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X51: { // LD D, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X52: { // LD D, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X53: { // LD D, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X54: { // LD D, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X55: { // LD D, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X56: { // LD D, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X57: { // LD D, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X58: { // LD E, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X59: { // LD E, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X5A: { // LD E, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X5B: { // LD E, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X5C: { // LD E, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X5D: { // LD E, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X5E: { // LD E, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X5F: { // LD E, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X60: { // LD H, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X61: { // LD H, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X62: { // LD H, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X63: { // LD H, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X64: { // LD H, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X65: { // LD H, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X66: { // LD H, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X67: { // LD H, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X68: { // LD L, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X69: { // LD L, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X6A: { // LD L, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X6B: { // LD L, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X6C: { // LD L, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X6D: { // LD L, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X6E: { // LD L, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X6F: { // LD L, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X70: { // LD HL, B
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X71: { // LD HL, C
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X72: { // LD HL, D
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X73: { // LD HL, E
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X74: { // LD HL, H
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X75: { // LD HL, L
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X76: { // HALT 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X77: { // LD HL, A
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X78: { // LD A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X79: { // LD A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X7A: { // LD A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X7B: { // LD A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X7C: { // LD A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X7D: { // LD A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X7E: { // LD A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X7F: { // LD A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X80: { // ADD A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X81: { // ADD A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X82: { // ADD A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X83: { // ADD A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X84: { // ADD A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X85: { // ADD A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X86: { // ADD A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X87: { // ADD A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X88: { // ADC A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X89: { // ADC A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X8A: { // ADC A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X8B: { // ADC A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X8C: { // ADC A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X8D: { // ADC A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X8E: { // ADC A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X8F: { // ADC A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X90: { // SUB A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X91: { // SUB A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X92: { // SUB A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X93: { // SUB A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X94: { // SUB A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X95: { // SUB A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X96: { // SUB A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X97: { // SUB A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X98: { // SBC A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X99: { // SBC A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X9A: { // SBC A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X9B: { // SBC A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X9C: { // SBC A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X9D: { // SBC A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0X9E: { // SBC A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0X9F: { // SBC A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA0: { // AND A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA1: { // AND A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA2: { // AND A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA3: { // AND A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA4: { // AND A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA5: { // AND A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA6: { // AND A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XA7: { // AND A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA8: { // XOR A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XA9: { // XOR A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XAA: { // XOR A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XAB: { // XOR A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XAC: { // XOR A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XAD: { // XOR A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XAE: { // XOR A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XAF: { // XOR A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB0: { // OR A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB1: { // OR A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB2: { // OR A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB3: { // OR A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB4: { // OR A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB5: { // OR A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB6: { // OR A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XB7: { // OR A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB8: { // CP A, B
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XB9: { // CP A, C
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XBA: { // CP A, D
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XBB: { // CP A, E
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XBC: { // CP A, H
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XBD: { // CP A, L
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XBE: { // CP A, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XBF: { // CP A, A
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XC0: { // RET NZ
        
        // TODO: Implement logic
        cycles = 20;
        break;
    }

    case 0XC1: { // POP BC
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XC2: { // JP NZ, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XC3: { // JP a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XC4: { // CALL NZ, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 24;
        break;
    }

    case 0XC5: { // PUSH BC
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XC6: { // ADD A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XC7: { // RST $00
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XC8: { // RET Z
        
        // TODO: Implement logic
        cycles = 20;
        break;
    }

    case 0XC9: { // RET 
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XCA: { // JP Z, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XCB: { // PREFIX 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XCC: { // CALL Z, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 24;
        break;
    }

    case 0XCD: { // CALL a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 24;
        break;
    }

    case 0XCE: { // ADC A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XCF: { // RST $08
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XD0: { // RET NC
        
        // TODO: Implement logic
        cycles = 20;
        break;
    }

    case 0XD1: { // POP DE
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XD2: { // JP NC, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XD3: { // ILLEGAL_D3 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XD4: { // CALL NC, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 24;
        break;
    }

    case 0XD5: { // PUSH DE
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XD6: { // SUB A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XD7: { // RST $10
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XD8: { // RET C
        
        // TODO: Implement logic
        cycles = 20;
        break;
    }

    case 0XD9: { // RETI 
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XDA: { // JP C, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XDB: { // ILLEGAL_DB 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XDC: { // CALL C, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 24;
        break;
    }

    case 0XDD: { // ILLEGAL_DD 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XDE: { // SBC A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XDF: { // RST $18
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XE0: { // LDH a8, A
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XE1: { // POP HL
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XE2: { // LDH C, A
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XE3: { // ILLEGAL_E3 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XE4: { // ILLEGAL_E4 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XE5: { // PUSH HL
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XE6: { // AND A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XE7: { // RST $20
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XE8: { // ADD SP, e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XE9: { // JP HL
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XEA: { // LD a16, A
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XEB: { // ILLEGAL_EB 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XEC: { // ILLEGAL_EC 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XED: { // ILLEGAL_ED 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XEE: { // XOR A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XEF: { // RST $28
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XF0: { // LDH A, a8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XF1: { // POP AF
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XF2: { // LDH A, C
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XF3: { // DI 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XF4: { // ILLEGAL_F4 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XF5: { // PUSH AF
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XF6: { // OR A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XF7: { // RST $30
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XF8: { // LD HL, SP, e8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 12;
        break;
    }

    case 0XF9: { // LD SP, HL
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XFA: { // LD A, a16
        uint8_t low = m_mmu.read(PC++);
        uint8_t high = m_mmu.read(PC++);
        uint16_t d16 = (high << 8) | low;
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

    case 0XFB: { // EI 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XFC: { // ILLEGAL_FC 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XFD: { // ILLEGAL_FD 
        
        // TODO: Implement logic
        cycles = 4;
        break;
    }

    case 0XFE: { // CP A, n8
        uint8_t d8 = m_mmu.read(PC++);
        
        // TODO: Implement logic
        cycles = 8;
        break;
    }

    case 0XFF: { // RST $38
        
        // TODO: Implement logic
        cycles = 16;
        break;
    }

