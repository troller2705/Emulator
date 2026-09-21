int CPU::execute_cb(uint8_t cb_opcode) {
    int cb_cycles = 0;
    switch (cb_opcode) {

        case 0X00: { // RLC B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X01: { // RLC C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X02: { // RLC D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X03: { // RLC E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X04: { // RLC H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X05: { // RLC L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X06: { // RLC HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X07: { // RLC A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X08: { // RRC B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X09: { // RRC C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X0A: { // RRC D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X0B: { // RRC E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X0C: { // RRC H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X0D: { // RRC L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X0E: { // RRC HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X0F: { // RRC A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X10: { // RL B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X11: { // RL C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X12: { // RL D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X13: { // RL E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X14: { // RL H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X15: { // RL L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X16: { // RL HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X17: { // RL A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X18: { // RR B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X19: { // RR C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X1A: { // RR D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X1B: { // RR E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X1C: { // RR H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X1D: { // RR L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X1E: { // RR HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X1F: { // RR A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X20: { // SLA B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X21: { // SLA C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X22: { // SLA D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X23: { // SLA E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X24: { // SLA H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X25: { // SLA L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X26: { // SLA HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X27: { // SLA A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X28: { // SRA B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X29: { // SRA C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X2A: { // SRA D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X2B: { // SRA E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X2C: { // SRA H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X2D: { // SRA L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X2E: { // SRA HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X2F: { // SRA A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X30: { // SWAP B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X31: { // SWAP C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X32: { // SWAP D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X33: { // SWAP E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X34: { // SWAP H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X35: { // SWAP L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X36: { // SWAP HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X37: { // SWAP A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X38: { // SRL B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X39: { // SRL C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X3A: { // SRL D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X3B: { // SRL E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X3C: { // SRL H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X3D: { // SRL L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X3E: { // SRL HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X3F: { // SRL A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X40: { // BIT 0, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X41: { // BIT 0, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X42: { // BIT 0, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X43: { // BIT 0, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X44: { // BIT 0, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X45: { // BIT 0, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X46: { // BIT 0, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X47: { // BIT 0, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X48: { // BIT 1, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X49: { // BIT 1, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X4A: { // BIT 1, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X4B: { // BIT 1, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X4C: { // BIT 1, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X4D: { // BIT 1, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X4E: { // BIT 1, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X4F: { // BIT 1, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X50: { // BIT 2, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X51: { // BIT 2, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X52: { // BIT 2, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X53: { // BIT 2, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X54: { // BIT 2, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X55: { // BIT 2, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X56: { // BIT 2, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X57: { // BIT 2, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X58: { // BIT 3, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X59: { // BIT 3, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X5A: { // BIT 3, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X5B: { // BIT 3, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X5C: { // BIT 3, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X5D: { // BIT 3, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X5E: { // BIT 3, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X5F: { // BIT 3, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X60: { // BIT 4, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X61: { // BIT 4, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X62: { // BIT 4, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X63: { // BIT 4, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X64: { // BIT 4, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X65: { // BIT 4, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X66: { // BIT 4, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X67: { // BIT 4, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X68: { // BIT 5, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X69: { // BIT 5, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X6A: { // BIT 5, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X6B: { // BIT 5, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X6C: { // BIT 5, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X6D: { // BIT 5, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X6E: { // BIT 5, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X6F: { // BIT 5, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X70: { // BIT 6, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X71: { // BIT 6, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X72: { // BIT 6, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X73: { // BIT 6, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X74: { // BIT 6, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X75: { // BIT 6, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X76: { // BIT 6, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X77: { // BIT 6, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X78: { // BIT 7, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X79: { // BIT 7, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X7A: { // BIT 7, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X7B: { // BIT 7, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X7C: { // BIT 7, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X7D: { // BIT 7, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X7E: { // BIT 7, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 12;
            break;
        }

        case 0X7F: { // BIT 7, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X80: { // RES 0, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X81: { // RES 0, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X82: { // RES 0, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X83: { // RES 0, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X84: { // RES 0, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X85: { // RES 0, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X86: { // RES 0, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X87: { // RES 0, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X88: { // RES 1, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X89: { // RES 1, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X8A: { // RES 1, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X8B: { // RES 1, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X8C: { // RES 1, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X8D: { // RES 1, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X8E: { // RES 1, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X8F: { // RES 1, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X90: { // RES 2, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X91: { // RES 2, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X92: { // RES 2, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X93: { // RES 2, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X94: { // RES 2, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X95: { // RES 2, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X96: { // RES 2, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X97: { // RES 2, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X98: { // RES 3, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X99: { // RES 3, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X9A: { // RES 3, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X9B: { // RES 3, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X9C: { // RES 3, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X9D: { // RES 3, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0X9E: { // RES 3, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0X9F: { // RES 3, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA0: { // RES 4, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA1: { // RES 4, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA2: { // RES 4, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA3: { // RES 4, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA4: { // RES 4, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA5: { // RES 4, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA6: { // RES 4, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XA7: { // RES 4, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA8: { // RES 5, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XA9: { // RES 5, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XAA: { // RES 5, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XAB: { // RES 5, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XAC: { // RES 5, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XAD: { // RES 5, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XAE: { // RES 5, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XAF: { // RES 5, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB0: { // RES 6, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB1: { // RES 6, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB2: { // RES 6, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB3: { // RES 6, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB4: { // RES 6, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB5: { // RES 6, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB6: { // RES 6, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XB7: { // RES 6, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB8: { // RES 7, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XB9: { // RES 7, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XBA: { // RES 7, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XBB: { // RES 7, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XBC: { // RES 7, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XBD: { // RES 7, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XBE: { // RES 7, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XBF: { // RES 7, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC0: { // SET 0, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC1: { // SET 0, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC2: { // SET 0, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC3: { // SET 0, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC4: { // SET 0, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC5: { // SET 0, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC6: { // SET 0, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XC7: { // SET 0, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC8: { // SET 1, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XC9: { // SET 1, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XCA: { // SET 1, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XCB: { // SET 1, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XCC: { // SET 1, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XCD: { // SET 1, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XCE: { // SET 1, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XCF: { // SET 1, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD0: { // SET 2, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD1: { // SET 2, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD2: { // SET 2, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD3: { // SET 2, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD4: { // SET 2, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD5: { // SET 2, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD6: { // SET 2, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XD7: { // SET 2, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD8: { // SET 3, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XD9: { // SET 3, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XDA: { // SET 3, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XDB: { // SET 3, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XDC: { // SET 3, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XDD: { // SET 3, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XDE: { // SET 3, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XDF: { // SET 3, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE0: { // SET 4, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE1: { // SET 4, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE2: { // SET 4, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE3: { // SET 4, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE4: { // SET 4, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE5: { // SET 4, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE6: { // SET 4, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XE7: { // SET 4, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE8: { // SET 5, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XE9: { // SET 5, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XEA: { // SET 5, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XEB: { // SET 5, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XEC: { // SET 5, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XED: { // SET 5, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XEE: { // SET 5, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XEF: { // SET 5, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF0: { // SET 6, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF1: { // SET 6, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF2: { // SET 6, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF3: { // SET 6, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF4: { // SET 6, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF5: { // SET 6, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF6: { // SET 6, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XF7: { // SET 6, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF8: { // SET 7, B
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XF9: { // SET 7, C
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XFA: { // SET 7, D
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XFB: { // SET 7, E
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XFC: { // SET 7, H
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XFD: { // SET 7, L
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        case 0XFE: { // SET 7, HL
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 16;
            break;
        }

        case 0XFF: { // SET 7, A
            std::cerr << "PANIC! Unimplemented CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n"; exit(1);
            cb_cycles = 8;
            break;
        }

        default:
            std::cerr << "PANIC! Unknown CB Opcode: 0x" << std::hex << (int)cb_opcode << "\n";
            exit(1);
    }
    return cb_cycles;
}
