# Mega Emulator — Multi-System Retro Emulation Suite

**Mega Emulator** (`EmulatorLauncher`) is a custom-built, multi-architecture hardware emulator and interactive debugging suite written from scratch in modern C++. Powered by an **SDL2** multimedia backend and a **Dear ImGui** developer overlay, it unites three distinct hardware generations—**Game Boy / Game Boy Color (`Core_GBC`)**, **Game Boy Advance (`Core_GBA`)**, and **Super Nintendo (`Core_SNES`)**—behind a single polymorphic core interface (`IEmulatorCore`).

---

## 🌟 Supported Systems & Specifications

| System | Core Class | CPU Architecture | Native Resolution | Audio Pipeline | Cartridge / Storage Support |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Game Boy / Color** (`.gb`, `.gbc`) | `GBCCore` | Sharp LR35902 (8-bit, CGB Dual-Speed) | `160 × 144` | 4-Channel PSG (Square, Wave, LFSR Noise) | `ROMOnly`, `MBC1`, `MBC2`, `MBC3`, `MBC5` + `.sav` SRAM |
| **Game Boy Advance** (`.gba`) | `GBACore` | ARM7TDMI (32-bit `ARM` + 16-bit `THUMB`) | `240 × 160` | Dual DirectSound DMA FIFOs + 4-Ch PSG | Up to 32 MB GamePak + 64 KB SRAM & 128 KB Sanyo Flash |
| **Super Nintendo** (`.sfc`, `.smc`) | `SNESCore` | Ricoh 5A22 / WDC 65C816 (16-bit, 24-bit Bus) | `256 × 224` | Sony SPC700 + S-DSP & 64-Byte IPL ROM | `LoROM`, `HiROM` (with `.smc` header stripping) + `.sav` SRAM |

---

## 🏛️ Architecture & Subsystems

### 1. Unified Core Abstraction (`Core_Shared/`)
* **`IEmulatorCore`**: Polymorphic interface standardizing ROM loading, per-frame execution (`run_frame()`), dynamic resolution queries, `ARGB8888` video buffer streaming, `44.1 kHz` `float` PCM audio queuing, `.sav` battery persistence, and real-time debugger introspection (`step_instruction()`, `get_cpu_state()`, `debug_read_memory()`, `debug_write_memory()`, and `debug_update_tile_buffer()`).
* **`ICartridge`**: Abstract mapper interface for bank-switched cartridge hardware.
* **Hardware Utilities**:
  * `color_utils.h`: `constexpr` 15-bit `BGR555` to 32-bit `ARGB8888` conversion routines.
  * `bit_utils.h`: Endian-safe 16/32-bit memory accessors (`read_le16`, `read_le32`, `write_le16`, `write_le32`) and 32-bit barrel rotation (`rotr32`).
  * `battery_io.h`: Cross-core binary `.sav` file serialization.

---

### 2. Game Boy & Game Boy Color Core (`Core_GBC/`)
* **CPU (`gbc_cpu`)**: Complete Sharp LR35902 implementation including all primary opcodes, the full `0xCB` bit-manipulation matrix (`RLC`, `RRC`, `RL`, `RR`, `SLA`, `SRA`, `SWAP`, `SRL`, `BIT`, `RES`, `SET`), `DAA` BCD adjustment, `HALT` wake-up logic, CGB `STOP` speed switching (`KEY1`), and priority-ordered interrupt dispatch (`VBlank`, `LCD STAT`, `Timer`, `Serial`, `Joypad`).
* **PPU (`gbc_ppu`)**: Scanline renderer cycling accurately through STAT Modes `2` (OAM Search), `3` (Pixel Transfer), `0` (H-Blank), and `1` (V-Blank), with `LY == LYC` coincidence interrupts. Supports DMG monochrome palettes (`BGP`, `OBP0`, `OBP1`) and CGB 15-bit color palettes (`BCPS`/`BCPD`, `OCPS`/`OCPD`), tile banking, X/Y tile flipping, Window line tracking, and BG-to-OAM priority.
* **MMU & DMA (`gbc_mmu`)**: Manages 2 banks of 8 KB VRAM (`VBK`), 8 banks of 4 KB WRAM (`SVBK`), OAM DMA (`$FF46`), and CGB General Purpose DMA / H-Blank DMA (`$FF51–$FF55`).
* **APU (`gbc_apu`)**: 512 Hz frame sequencer driving 4 hardware channels (Square 1 with frequency sweep, Square 2, 32-step programmable Wave RAM channel, and 15/7-bit LFSR Noise channel) downsampled to 44.1 kHz.
* **Memory Bank Controllers**: Hardware-accurate implementations of `ROMOnly`, `MBC1` (including Mode 1 Bank 0 shifting quirks), `MBC2` (512×4-bit internal RAM and address-bit-8 register selection), `MBC3` (ROM/RAM banking + RTC latch registers), and `MBC5` (9-bit ROM banking).

---

### 3. Game Boy Advance Core (`Core_GBA/`)
* **CPU (`gba_cpu`)**: Full 32-bit **ARM7TDMI** processor supporting both **32-bit ARM** and **16-bit THUMB** instruction sets, banked processor modes (`USR`, `FIQ`, `IRQ`, `SVC`) with `SPSR` preservation, 32-bit barrel shifter (`LSL`, `LSR`, `ASR`, `ROR`, `RRX`), and hardware IRQ vectoring.
* **HLE BIOS & SWI Dispatcher**: Built-in High-Level Emulation of GBA BIOS Software Interrupts (`SWI`), including `SoftReset`, `RegisterRamReset`, `IntrWait`, `VBlankIntrWait`, `Div`/`DivArm`, `Sqrt`, `ArcTan`/`ArcTan2`, `CpuSet`/`CpuFastSet`, `GetBiosChecksum` (`0xBAAE187F`), `BgAffineSet`, `ObjAffineSet`, `BitUnPack`, `LZ77UnCompWram`/`Vram`, `RLUnCompWram`/`Vram`, and `MidiKey2Freq` (for the `m4a` sound engine).
* **PPU (`gba_ppu`)**: Renders 4bpp and 8bpp regular scrolling backgrounds, affine transformation backgrounds, prioritized OAM sprites, and master brightness effects across `240 × 160` scanlines.
* **MMU, DMA & Flash (`gba_mmu`)**: Routes 32-bit memory across BIOS (with open-bus fetch latching), 256 KB EWRAM, 32 KB IWRAM, I/O registers, Palette RAM, 96 KB VRAM, OAM, and GamePak ROM. Includes a 4-channel DMA controller (Immediate, VBlank, HBlank, and Audio FIFO modes) and a full **Sanyo 128 KB Flash (`LE26FV10N1TS`)** state machine (supporting chip ID queries, 4 KB sector erase, chip erase, byte programming, and bank switching for titles like *Pokémon FireRed*, *Ruby*, *Sapphire*, and *Emerald*).
* **Timers & APU (`gba_timer`, `gba_apu`)**: 4 cascading 16-bit hardware timers with selectable prescalers (`1`, `64`, `256`, `1024`) driving both IRQ generation and the GBA's dual 32-byte DirectSound PCM FIFOs (`FIFO A` & `FIFO B`), mixed alongside the 4-channel legacy PSG synthesizer.

---

### 4. Super Nintendo Core (`Core_SNES/`)
* **CPU (`snes_cpu`)**: WDC 65C816 / Ricoh 5A22 implementation supporting 6502 Emulation Mode (`E=1`, Page 1 stack lock) and 16-bit Native Mode (`E=0`), runtime 8/16-bit register toggling via `REP`/`SEP` (`M` and `X` flags), 24-bit `PB:PC` and `DB` addressing, block moves (`MVN`), and Native/Emulation NMI interrupt vectors (`$FFEA` / `$FFFA`).
* **MMU & DMA (`snes_mmu`)**: Automatic `LoROM` / `HiROM` checksum detection, `.smc` 512-byte copier header stripping, 128 KB WRAM (`$7E–$7F` + low 8 KB mirror at `$0000–$1FFF` and `$2180–$2183` portals), hardware multiplication/division units (`$4202–$4206` / `$4214–$4217`), auto-joypad polling (`$4218–$4219`), and 8-channel General Purpose DMA (`$420B`, `$4300–$437F`).
* **PPU (`snes_ppu`)**: Manages 64 KB VRAM, 512-byte CGRAM (256 `BGR555` colors), and 544-byte OAM. Decodes 2bpp and 4bpp bitplanes with horizontal/vertical scroll latches, screen mirroring, tile flipping, master brightness scaling (`$2100`), and OAM sprites.
* **APU (`snes_apu`)**: Emulates the Sony SPC700 64 KB ARAM space and the internal 64-byte IPL Boot ROM communication protocol across `$2140–$2143` (`0xAA`/`0xBB` boot readiness, `0xCC` transfer handshake, and rolling counter echoes).

---

## 🖥️ Launcher UI & Integrated Debugging Tools (`Launcher/`)

Built with **SDL2 (`SDL_Renderer`)** and **Dear ImGui**, the frontend provides real-time inspection and control:

1. **ROM Launcher & Core Selector**:
  * Scans your configured ROM directory for `.gb`, `.gbc`, `.gba`, `.sfc`, and `.smc` files and persists your path in `emulator_config.txt`.
  * Supports **Auto-Detect (By Extension)** or manual core override (`GBCCore`, `GBACore`, `SNESCore`).
  * Automatically resizes the streaming `SDL_Texture` whenever switching between systems with different native resolutions (`160×144`, `240×160`, `256×224`).
2. **Architecture-Aware CPU Debugger**:
  * Displays live frame rate, **Pause / Resume**, and single-instruction **Step Instruction** controls.
  * Dynamically switches its register view to match the active processor:
    * **GB/GBC**: `PC`, `SP`, `AF`, `BC`, `DE`, `HL`.
    * **GBA**: Execution state (`ARM` vs `THUMB`), `CPSR`, and all 16 32-bit registers (`R0–R15`).
    * **SNES**: `65C816` mode flags (`M`, `X`, `E`), 24-bit `PB:PC`, 16-bit `A`, `X`, `Y`, `SP`, `DP`, `DB`, and `P`.
3. **Interactive Memory Hex Editor (`imgui_memory_editor.h`)**:
  * Live read/write hex and ASCII inspection of the active core's address space, complete with address jump, data type preview (`Int8`/`Uint8`/`Int16`/`Uint16`/`Int32`/`Float`), and endianness toggles.
4. **VRAM Tile Viewer**:
  * Renders a live `128 × 192` texture (scaled to `256 × 384`) decoding 384 tiles directly from the active core's VRAM.

---

## 🎮 Unified Input Mapping (`InputMapper`)

`Launcher/input_mapper.h` and `input_mapper.cpp` provide hot-pluggable **SDL2 GameController** support alongside keyboard controls, combining inputs into a unified 32-bit bitmask (`EmulatorInput`) and automatically routing analog stick directions to the D-Pad:

| Emulated Input | Keyboard Binding | Controller Binding (Nintendo Layout) |
| :--- | :--- | :--- |
| **D-Pad Up / Down / Left / Right** | `Arrow Keys` (or `W` / `S` / `A` / `D` & `I` / `K` / `J` / `L`) | `D-Pad` or `Left / Right Analog Sticks` |
| **A Button** | `Z` | `East Button` (Xbox `B` / PlayStation `Circle`) |
| **B Button** | `X` | `South Button` (Xbox `A` / PlayStation `Cross`) |
| **X Button** *(SNES / GBA A)* | `C` | `North Button` (Xbox `Y` / PlayStation `Triangle`) |
| **Y Button** *(SNES / GBA B)* | `V` | `West Button` (Xbox `X` / PlayStation `Square`) |
| **L / R Shoulder** | `Q` / `E` | `Left Shoulder (L1)` / `Right Shoulder (R1)` |
| **L2 / R2 Triggers** | `1` / `2` | `Left Trigger (L2)` / `Right Trigger (R2)` |
| **L3 / R3 Stick Click** | `3` / `4` | `Left Stick Click` / `Right Stick Click` |
| **Start** | `Enter` (`Return`) | `Start` / `Options` |
| **Select** | `Right Shift` or `Backspace` | `Back` / `Select` / `Share` |
| **Toggle Launcher Menu** | `Escape` | — |

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
│   │   ├── bit_utils.h               # Little-endian helpers & 32-bit rotate
│   │   └── color_utils.h             # BGR555 to ARGB8888 conversion
│   ├── Core_GBC/
│   │   ├── gbc_core.h, gbc_cpu.h, gbc_mmu.h, gbc_ppu.h, gbc_apu.h, gbc_timer.h
│   │   └── ROMOnly.h, MBC1.h, MBC2.h, MBC3.h, MBC5.h
│   ├── Core_GBA/
│   │   └── gba_core.h, gba_cpu.h, gba_mmu.h, gba_ppu.h, gba_apu.h, gba_timer.h
│   └── Core_SNES/
│       └── snes_core.h, snes_cpu.h, snes_mmu.h, snes_ppu.h, snes_apu.h
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
    └── Core_SNES/
        └── snes_core.cpp, snes_cpu.cpp, snes_mmu.cpp, snes_ppu.cpp, snes_apu.cpp
```
---

## 🛠️ Building from Source
### Prerequisites
- C++ Compiler: C++17 or newer (MSVC, GCC, or Clang)
- Build System: CMake 3.15+
- Dependencies: SDL2 (2.0.17+ required for SDL_RenderGeometryRaw) and Dear ImGui