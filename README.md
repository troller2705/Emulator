# Mega Emulator — Multi-System Emulation Suite

**Mega Emulator** (`EmulatorLauncher`) is a custom-built, multi-architecture hardware emulator and interactive debugging suite written from scratch in modern C++[cite: 3, 11]. Powered by an **SDL2** multimedia backend and a **Dear ImGui** developer overlay, it unites distinct hardware generations—currently spanning **Game Boy / Game Boy Color (`Core_GBC`)**, **Game Boy Advance (`Core_GBA`)**, **Super Nintendo (`Core_SNES`)**, and **Sega Genesis / Mega Drive (`Core_Genesis`)**—behind a single polymorphic core interface (`IEmulatorCore`)[cite: 3, 4, 10, 11, 23, 24].

---

## 🌟 Active Systems & Hardware Specifications

| System | Status | Core Class | CPU Architecture | Native Resolution | Audio Pipeline | Cartridge / Storage Support |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy / Color** (`.gb`, `.gbc`) | ✅ **Done**[cite: 20, 23] | `GBCCore` | Sharp LR35902 (8-bit, CGB Dual-Speed)[cite: 3, 11] | `160 × 144` | 4-Channel PSG (Square, Wave, LFSR Noise)[cite: 3, 4, 10, 11] | `ROMOnly`, `MBC1`, `MBC2`, `MBC3`, `MBC5` + `.sav` SRAM[cite: 3, 4, 10, 11] |
| **Game Boy Advance** (`.gba`) | ✅ **Done**[cite: 20, 23] | `GBACore`[cite: 3, 4, 10, 11] | ARM7TDMI (32-bit `ARM` + 16-bit `THUMB`)[cite: 3, 4, 10, 11] | `240 × 160`[cite: 4, 10] | Dual DirectSound DMA FIFOs + 4-Ch PSG[cite: 4, 10] | Up to 32 MB GamePak + 64 KB SRAM & 128 KB Sanyo Flash[cite: 4, 10] |
| **Super Nintendo** (`.sfc`, `.smc`) | 🚧 **In Progress**[cite: 20, 23] | `SNESCore`[cite: 3, 4, 10, 11] | Ricoh 5A22 / WDC 65C816 (16-bit, 24-bit Bus)[cite: 3, 4, 10, 11] | `256 × 224`[cite: 4, 10] | Sony SPC700 + S-DSP (8-Voice BRR, ADSR, Echo FIR)[cite: 3, 4, 10, 11] | `LoROM`, `HiROM`, **DSP-1 HLE** Coprocessor + `.sav` SRAM[cite: 3, 4, 10, 11] |
| **Sega Genesis** (`.md`, `.gen`, `.bin`) | 🚧 **In Progress**[cite: 24] | `GenesisCore` | Motorola 68000 (16/32-bit Big-Endian) + Z80 | `320 × 224` | Z80 Bus Handshake + YM2612 / SN76489 Stubs | Up to 4 MB Linear ROM (`.bin` & interleaved `.smd`) + `.sav` SRAM |

---

## 🗺️ Master Development Roadmap

The long-term goal of **Mega Emulator** is to provide a unified, multi-manufacturer preservation and debugging environment across five major hardware ecosystems[cite: 21, 22, 23, 24, 25]:

### 🎮 Nintendo (`#13`)[cite: 23]
- [x] **Game Boy / Game Boy Color / Game Boy Advance** (`#19` — *Completed*)[cite: 20, 23]
- [ ] **Super Nintendo Entertainment System (SNES)** (`#17` — *In Progress: Mode 7, HDMA & DSP-1 refinement*)[cite: 3, 11, 20, 23]
- [ ] **Nintendo 64 (N64)** (`#18` — *Planned: NEC VR4300 64-bit MIPS III + RCP/RSP/RDP*)[cite: 20, 23]
- [ ] **Nintendo GameCube** (`#20` — *Planned: IBM PowerPC Gekko + Flipper GPU*)[cite: 20, 23]
- [ ] **Nintendo DS / 2DS / 3DS** (`#21` — *Planned: Dual-screen ARM946E-S + ARM7TDMI / ARM11MPCore*)[cite: 20, 23]
- [ ] **Wii / Wii U** (`#22` — *Planned: PowerPC Broadway / Espresso + GX/Latte*)[cite: 20, 23]
- [ ] **Nintendo Switch / Switch 2** (`#23` — *Planned: ARM64 Tegra HLE*)[cite: 20, 23]

### 🦔 Sega (`#5`)[cite: 24]
- [ ] **Sega Genesis / Mega Drive** (`#25` — *In Progress: Motorola 68000 + 315-5313 VDP*)[cite: 24]
- [ ] **Sega Master System** (`#24` — *Planned: Zilog Z80 + Texas Instruments TMS9918-derived VDP*)[cite: 24]
- [ ] **Sega CD / Mega-CD** (`#26` — *Planned: Sub-68000 @ 12.5 MHz + ASIC scaling/rotation + CD-ROM ISO/CHD*)[cite: 24]
- [ ] **Sega Saturn** (`#27` — *Planned: Dual Hitachi SH-2 + VDP1 Quad Renderer + VDP2 Background Engine*)[cite: 24]
- [ ] **Sega Dreamcast** (`#28` — *Planned: Hitachi SH-4 + PowerVR2 CLX2*)[cite: 24]

### 📐 Sony (`#14`)[cite: 25]
- [ ] **PlayStation (PS / PS1)** (`#29` — *Planned: MIPS R3000A + GTE Coprocessor + Rasterizer GPU*)[cite: 25]
- [ ] **PlayStation 2 (PS2)** (`#30` — *Planned: Emotion Engine + VU0/VU1 + Graphics Synthesizer*)[cite: 25]
- [ ] **PlayStation 3 (PS3)** (`#31` — *Planned: Cell Broadband Engine PPU/SPU + RSX*)[cite: 25]
- [ ] **PlayStation Portable (PSP)** (`#32` — *Planned: MIPS R4000 Allegrex + VFPU + GU*)[cite: 25]
- [ ] **PlayStation Vita (PS Vita)** (`#33` — *Planned: Quad-Core ARM Cortex-A9 + SGX543MP4+*)[cite: 25]
- [ ] **PlayStation 4 (PS4)** (`#34` — *Planned: x86-64 AMD Jaguar + GCN Compatibility Layer*)[cite: 25]
- [ ] **PlayStation 5 (PS5)** (`#35` — *Planned: x86-64 AMD Zen 2 + RDNA 2 Compatibility Layer*)[cite: 25]

### 🟩 Microsoft (`#15`)[cite: 22]
- [ ] **Xbox** (`#36` — *Planned: Intel Pentium III Coppermine + NVIDIA NV2A*)[cite: 22]
- [ ] **Xbox 360** (`#37` — *Planned: Triple-Core PowerPC Xenon + ATI Xenos*)[cite: 22]
- [ ] **Xbox One** (`#38` — *Planned: x86-64 AMD Jaguar HLE*)[cite: 22]
- [ ] **Xbox Series S/X** (`#39` — *Planned: x86-64 AMD Zen 2 HLE*)[cite: 22]

### 🕹️ Atari (`#16`)[cite: 21]
- [ ] **Atari 2600** (`#40` — *Planned: MOS 6507 + TIA Scanline Racing + RIOT PIA*)[cite: 21]
- [ ] **Atari 5200** (`#41` — *Planned: MOS 6502C + ANTIC + GTIA + POKEY*)[cite: 21]
- [ ] **Atari 7800** (`#42` — *Planned: Atari SALLY 6502 + MARIA Display List Processor*)[cite: 21]

---

## 🏛️ Architecture & Implemented Subsystems

### 1. Unified Core Abstraction (`Core_Shared/`)
- **`IEmulatorCore`**: Polymorphic interface standardizing ROM loading, per-frame execution (`run_frame()`), dynamic resolution queries, `ARGB8888` video buffer streaming, `44.1 kHz` `float` PCM audio queuing, `.sav` battery persistence, and real-time debugger introspection (`step_instruction()`, `get_cpu_state()`, `debug_read_memory()`, `debug_write_memory()`, and `debug_update_tile_buffer()`)[cite: 3, 4, 10, 11].
- **`ICartridge`**: Abstract mapper interface for bank-switched cartridge hardware[cite: 4, 10].
- **Hardware Utilities**:
    - `color_utils.h`: `constexpr` 15-bit `BGR555` to 32-bit `ARGB8888` conversion routines[cite: 4, 10].
    - `bit_utils.h`: Endian-safe 16/32-bit memory accessors (`read_le16`, `read_le32`, `write_le16`, `write_le32`) and 32-bit barrel rotation (`rotr32`)[cite: 4, 10].
    - `battery_io.h`: Cross-core binary `.sav` file serialization[cite: 4, 10].

---

### 2. Game Boy & Game Boy Color Core (`Core_GBC/`)
- **CPU (`gbc_cpu`)**: Complete Sharp LR35902 implementation including all primary opcodes, the full `0xCB` bit-manipulation matrix (`RLC`, `RRC`, `RL`, `RR`, `SLA`, `SRA`, `SWAP`, `SRL`, `BIT`, `RES`, `SET`), `DAA` BCD adjustment, `HALT` wake-up logic, CGB `STOP` speed switching (`KEY1`), and priority-ordered interrupt dispatch (`VBlank`, `LCD STAT`, `Timer`, `Serial`, `Joypad`)[cite: 3, 11].
- **PPU (`gbc_ppu`)**: Scanline renderer cycling accurately through STAT Modes `2` (OAM Search), `3` (Pixel Transfer), `0` (H-Blank), and `1` (V-Blank), with `LY == LYC` coincidence interrupts. Supports DMG monochrome palettes (`BGP`, `OBP0`, `OBP1`) and CGB 15-bit color palettes (`BCPS`/`BCPD`, `OCPS`/`OCPD`), tile banking, X/Y tile flipping, Window line tracking, and BG-to-OAM priority[cite: 4, 10].
- **MMU & DMA (`gbc_mmu`)**: Manages 2 banks of 8 KB VRAM (`VBK`), 8 banks of 4 KB WRAM (`SVBK`), OAM DMA (`$FF46`), and CGB General Purpose DMA / H-Blank DMA (`$FF51–$FF55`)[cite: 4, 10].
- **APU (`gbc_apu`)**: 512 Hz frame sequencer driving 4 hardware channels (Square 1 with frequency sweep, Square 2, 32-step programmable Wave RAM channel, and 15/7-bit LFSR Noise channel) downsampled to 44.1 kHz[cite: 3, 4, 10, 11].
- **Memory Bank Controllers**: Hardware-accurate implementations of `ROMOnly`, `MBC1` (including Mode 1 Bank 0 shifting quirks), `MBC2` (512×4-bit internal RAM and address-bit-8 register selection), `MBC3` (ROM/RAM banking + RTC latch registers), and `MBC5` (9-bit ROM banking)[cite: 3, 4, 10, 11].

---

### 3. Game Boy Advance Core (`Core_GBA/`)
- **CPU (`gba_cpu`)**: Full 32-bit **ARM7TDMI** processor supporting both **32-bit ARM** and **16-bit THUMB** instruction sets, banked processor modes (`USR`, `FIQ`, `IRQ`, `SVC`) with `SPSR` preservation, 32-bit barrel shifter (`LSL`, `LSR`, `ASR`, `ROR`, `RRX`), and hardware IRQ vectoring[cite: 3, 4, 10, 11].
- **HLE BIOS & SWI Dispatcher**: Built-in High-Level Emulation of GBA BIOS Software Interrupts (`SWI`), including `SoftReset`, `RegisterRamReset`, `IntrWait`, `VBlankIntrWait`, `Div`/`DivArm`, `Sqrt`, `ArcTan`/`ArcTan2`, `CpuSet`/`CpuFastSet`, `GetBiosChecksum` (`0xBAAE187F`), `BgAffineSet`, `ObjAffineSet`, `BitUnPack`, `LZ77UnCompWram`/`Vram`, `RLUnCompWram`/`Vram`, and `MidiKey2Freq`.
- **PPU (`gba_ppu`)**: Renders 4bpp and 8bpp regular scrolling backgrounds, affine transformation backgrounds, prioritized OAM sprites, and master brightness effects across `240 × 160` scanlines[cite: 4, 10].
- **MMU, DMA & Flash (`gba_mmu`)**: Routes 32-bit memory across BIOS (with open-bus fetch latching), 256 KB EWRAM, 32 KB IWRAM, I/O registers, Palette RAM, 96 KB VRAM, OAM, and GamePak ROM. Includes a 4-channel DMA controller (Immediate, VBlank, HBlank, and Audio FIFO modes) and a full **Sanyo 128 KB Flash (`LE26FV10N1TS`)** state machine[cite: 4, 10].
- **Timers & APU (`gba_timer`, `gba_apu`)**: 4 cascading 16-bit hardware timers with selectable prescalers (`1`, `64`, `256`, `1024`) driving both IRQ generation and the GBA's dual 32-byte DirectSound PCM FIFOs (`FIFO A` & `FIFO B`), mixed alongside the 4-channel legacy PSG synthesizer[cite: 4, 10].

---

### 4. Super Nintendo Core (`Core_SNES/`)
- **CPU (`snes_cpu`)**: WDC 65C816 / Ricoh 5A22 implementation supporting 6502 Emulation Mode (`E=1`, Page 1 stack lock) and 16-bit Native Mode (`E=0`), runtime 8/16-bit register toggling via `REP`/`SEP` (`M` and `X` flags), 24-bit `PB:PC` and `DB` addressing, `WAI` halt synchronization, block moves (`MVN`), and Native/Emulation NMI & H/V Timer IRQ vectors (`$FFEA`/`$FFEE` and `$FFFA`/`$FFFE`)[cite: 3, 4, 10, 11].
- **MMU, DMA & HDMA (`snes_mmu`)**: Automatic `LoROM` / `HiROM` checksum detection, `.smc` 512-byte copier header stripping, 128 KB WRAM (`$7E–$7F` + low 8 KB mirror at `$0000–$1FFF` and `$2180–$2183` portals), hardware multiplication/division units (`$4202–$4206` / `$4214–$4217`), edge-triggered H/V Timer IRQs (`$4207–$420A`, `$4211`), 8-channel General Purpose DMA (`$420B`), and scanline-synchronized **Direct/Indirect H-Blank DMA (HDMA)** (`$420C`)[cite: 3, 4, 10, 11].
- **DSP-1 Math Coprocessor (`SNES_DSP1`)**: High-Level Emulation (HLE) of the NEC uPD77C25 math coprocessor mapped to `$6000–$7FFF`[cite: 3, 4, 10, 11], supporting 16-bit fixed-point multiplication, inverse, sin/cos trigonometry, 2D/3D polar rotations, 3×3 attitude matrices, 3D-to-2D perspective projection, and continuous Mode 7 scanline raster matrix streaming (`Op 0x02` & `Op 0x0A`)[cite: 3, 11].
- **PPU (`snes_ppu`)**: Manages 64 KB VRAM (with `VMAIN` address translation), 512-byte CGRAM (256 `BGR555` colors), and 544-byte OAM. Supports Background Modes `0`, `1`, and **Mode 7 Affine Matrix Transformation** (`$211A–$2120`), dual-window clipping masks (`$2123–$212F`), Main/Sub screen layer compositing, and Color Math addition/subtraction (`CGWSEL` / `CGADSUB`)[cite: 3, 4, 10, 11].
- **APU & S-DSP (`snes_apu`)**: Full Sony SPC700 CPU core executing alongside an 8-voice **S-DSP** hardware synthesizer featuring BRR (Bit Rate Reduction) sample decoding, hardware ADSR/GAIN volume envelopes, pitch modulation, 15-bit LFSR noise generation, and an 8-tap FIR Echo ring buffer resampled from 32 kHz to 44.1 kHz[cite: 3, 4, 10, 11].

---

### 5. Sega Genesis / Mega Drive Core (`Core_Genesis/`)
- **CPU (`genesis_cpu`)**: Motorola 68000 16/32-bit CISC interpreter with eight 32-bit Data Registers (`D0–D7`), eight 32-bit Address Registers (`A0–A7` with banked `USP`/`SSP` stack pointers), all 12 Effective Addressing (EA) modes, condition evaluation (`Bcc`, `DBcc`, `Scc`), `MOVEM` register block transfers, and priority autovector interrupts (Level 4 H-Int & Level 6 V-Int).
- **MMU (`genesis_mmu`)**: Big-Endian 24-bit (`16 MB`) linear address bus supporting flat `.bin`/`.md` and interleaved `.smd` ROMs, 64 KB Main Work RAM (`$FF0000–$FFFFFF`), 8 KB Z80 RAM (`$A00000–$A01FFF`), Z80 bus request/reset handshaking (`$A11100`/`$A11200`), and `TH`-pin multiplexed 3-button/6-button controller polling (`$A10003`).
- **VDP (`genesis_vdp`)**: Sega 315-5313 Video Display Processor managing 64 KB VRAM, 64-entry 9-bit `RGB333` CRAM, and 40-entry VSRAM. Implements 68000-to-VDP DMA, VRAM Fill DMA, and VRAM Copy DMA, rendering prioritized Plane A, Plane B, Window, and linked-list hardware sprites at `320 × 224`.

---

## 🖥️ Launcher UI & Integrated Debugging Tools (`Launcher/`)

Built with **SDL2 (`SDL_Renderer`)** and **Dear ImGui**, the frontend provides real-time inspection and control[cite: 3, 4, 10, 11]:

1. **ROM Launcher & Core Selector**:
    - Scans your configured ROM directory for `.gb`, `.gbc`, `.gba`, `.sfc`, `.smc`, `.md`, `.gen`, and `.bin` files and persists your path in `emulator_config.txt`[cite: 3, 11].
    - Supports **Auto-Detect (By Extension)** or manual core override (`GBCCore`, `GBACore`, `SNESCore`, `GenesisCore`)[cite: 3, 11].
    - Automatically resizes the streaming `SDL_Texture` whenever switching between systems with different native resolutions (`160×144`, `240×160`, `256×224`, `320×224`)[cite: 3, 4, 10, 11].
2. **Architecture-Aware CPU Debugger**:
    - Displays live frame rate, **Pause / Resume**, and single-instruction **Step Instruction** controls[cite: 1, 2, 3, 5, 6, 7, 11, 15, 16, 17, 18, 19].
    - Dynamically switches its register view to match the active processor[cite: 3, 11]:
        - **GB/GBC**: `PC`, `SP`, `AF`, `BC`, `DE`, `HL`[cite: 3, 11].
        - **GBA**: Execution state (`ARM` vs `THUMB`), `CPSR`, and all 16 32-bit registers (`R0–R15`)[cite: 3, 11].
        - **SNES**: `65C816` mode flags (`M`, `X`, `E`), 24-bit `PB:PC`, 16-bit `A`, `X`, `Y`, `SP`, `DP`, `DB`, and `P`[cite: 1, 2, 3, 5, 6, 7, 11, 15, 16, 17, 18, 19].
        - **Genesis**: `Motorola 68000` status register (`SR`), 24-bit `PC`, `SP (A7)`, and all sixteen 32-bit registers (`D0–D7` and `A0–A7`).
3. **Interactive Memory Hex Editor (`imgui_memory_editor.h`)**:
    - Live read/write hex and ASCII inspection of the active core's address space, complete with address jump, data type preview (`Int8`/`Uint8`/`Int16`/`Uint16`/`Int32`/`Float`), and endianness toggles[cite: 3, 4, 10, 11].
4. **VRAM Tile Viewer**:
    - Renders a live `128 × 192` texture (scaled to `256 × 384`) decoding 384 tiles directly from the active core's VRAM[cite: 3, 11].

---

## 🎮 Unified Input Mapping (`InputMapper`)

`Launcher/input_mapper.h` and `input_mapper.cpp` provide hot-pluggable **SDL2 GameController** support alongside keyboard controls, combining inputs into a unified 32-bit bitmask (`EmulatorInput`) and automatically routing analog stick directions to the D-Pad[cite: 3, 4, 10, 11]:

| Emulated Input | Keyboard Binding | Controller Binding (Nintendo / Standard Layout) |
| :--- | :--- | :--- |
| **D-Pad Up / Down / Left / Right** | `Arrow Keys` (or `W` / `S` / `A` / `D` & `I` / `K` / `J` / `L`)[cite: 3, 11] | `D-Pad` or `Left / Right Analog Sticks`[cite: 3, 11] |
| **Primary A / Genesis B** | `Z`[cite: 3, 11] | `East Button` (Nintendo `A`) / `South Button` (Standard)[cite: 3, 4, 10, 11] |
| **Primary B / Genesis C** | `X`[cite: 3, 11] | `South Button` (Nintendo `B`) / `East Button` (Standard)[cite: 3, 4, 10, 11] |
| **X Button / Genesis A** | `C`[cite: 3, 11] | `North Button` (Nintendo `X`) / `West Button` (Standard)[cite: 3, 11] |
| **Y Button** | `V`[cite: 3, 11] | `West Button` (Nintendo `Y`) / `North Button` (Standard)[cite: 3, 11] |
| **L / R Shoulder** | `Q` / `E`[cite: 3, 11] | `Left Shoulder (L1)` / `Right Shoulder (R1)`[cite: 3, 4, 10, 11] |
| **L2 / R2 Triggers** | `1` / `2`[cite: 3, 11] | `Left Trigger (L2)` / `Right Trigger (R2)`[cite: 3, 4, 10, 11] |
| **L3 / R3 Stick Click** | `3` / `4`[cite: 3, 11] | `Left Stick Click` / `Right Stick Click`[cite: 3, 4, 10, 11] |
| **Start** | `Enter` (`Return`)[cite: 3, 11] | `Start` / `Options`[cite: 3, 11] |
| **Select / Mode** | `Right Shift` or `Backspace`[cite: 3, 11] | `Back` / `Select` / `Share`[cite: 3, 11] |
| **Toggle Launcher Menu** | `Escape`[cite: 3, 4, 10, 11] | — |

---

## 📂 Repository Structure

```text
Emulator/
├── CMakeLists.txt
├── emulator_config.txt               # Persistent ROM directory configuration
├── include/
│   ├── imgui_impl_sdl2.h
│   ├── imgui_impl_sdlrenderer2.h
│   ├── imgui_memory_editor.h
│   ├── Launcher/
│   │   └── input_mapper.h            # Unified 32-bit input mask & controller hotplugging
│   ├── Core_Shared/
│   │   ├── IEmulatorCore.h           # Polymorphic core & debugger interface
│   │   ├── ICartridge.h              # Abstract cartridge mapper interface
│   │   ├── battery_io.h              # Save file (.sav) reader/writer
│   │   ├── bit_utils.h               # Endian helpers & 32-bit rotate
│   │   └── color_utils.h             # BGR555 to ARGB8888 conversion
│   ├── Core_GBC/
│   │   ├── gbc_core.h, gbc_cpu.h, gbc_mmu.h, gbc_ppu.h, gbc_apu.h, gbc_timer.h
│   │   └── ROMOnly.h, MBC1.h, MBC2.h, MBC3.h, MBC5.h
│   ├── Core_GBA/
│   │   └── gba_core.h, gba_cpu.h, gba_mmu.h, gba_ppu.h, gba_apu.h, gba_timer.h
│   ├── Core_SNES/
│   │   └── snes_core.h, snes_cpu.h, snes_mmu.h, snes_ppu.h, snes_apu.h
│   └── Core_Genesis/
│       └── genesis_core.h, genesis_cpu.h, genesis_mmu.h, genesis_vdp.h
└── src/
    ├── Launcher/
    │   ├── main.cpp                  # SDL2 + Dear ImGui application loop & debugger UI
    │   └── input_mapper.cpp
    ├── imgui/
    │   ├── imgui_impl_sdl2.cpp
    │   └── imgui_impl_sdlrenderer2.cpp
    ├── Core_GBC/
    │   └── gbc_core.cpp, gbc_cpu.cpp, gbc_mmu.cpp, gbc_ppu.cpp, gbc_apu.cpp, gbc_timer.cpp
    ├── Core_GBA/
    │   └── gba_core.cpp, gba_cpu.cpp, gba_mmu.cpp, gba_ppu.cpp, gba_apu.cpp, gba_timer.cpp
    ├── Core_SNES/
    │   └── snes_core.cpp, snes_cpu.cpp, snes_mmu.cpp, snes_ppu.cpp, snes_apu.cpp
    └── Core_Genesis/
        └── genesis_core.cpp, genesis_cpu.cpp, genesis_mmu.cpp, genesis_vdp.cpp
```
---
## 🛠️ Building from Source
### Prerequisites
- C++ Compiler: C++17 or newer (MSVC, MinGW-w64 GCC, or Clang)
- Build System: CMake 3.15+ (with Ninja or Make)
- Dependencies: SDL2 (2.0.17+ required for SDL_RenderGeometryRaw)[cite: 3, 4, 10, 11] and Dear ImGui[cite: 3, 4, 10, 11]