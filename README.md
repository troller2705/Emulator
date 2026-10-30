# Multi-System Retro Emulator Framework in C++

A custom-built, multi-architecture emulation framework written from scratch in modern C++. Designed with a clean, modular architecture and a unified launcher interface (`EmulatorLauncher`), this project houses dedicated, high-accuracy cores for both the **Super Nintendo Entertainment System (SNES)** and the **Game Boy / Game Boy Color (GB/GBC)**.

---

## 🏗️ Core Architectures

### 1. SNES Core (Ricoh 65816)
* **Processor**: Full implementation of the 16-bit 65816 CPU supporting both Native and Emulation modes (`E` flag).
* **Dynamic Register Sizing**: Implements runtime 8-bit/16-bit register switches for the Accumulator (`M` flag) and Index registers (`X` flag).
* **Addressing & Memory**: Comprehensive opcode dispatch handling complex addressing modes (Direct Page, Absolute, Long, Indexed, Indirect) with proper emulation-mode stack wrapping (`0x0100` – `0x01FF`).
* **Cartridge Mapping**: Supports LoROM mapping with header parsing, system RAM, and SRAM persistence.

### 2. Game Boy / Game Boy Color Core (Sharp LR35902)
* **Processor**: Custom Sharp LR35902 CPU core (Z80-derivative) with full register set and flag handling.
* **PPU & Rendering**: Scanline-accurate PPU implementation featuring background, window, and sprite tile pipelines, alongside VRAM/OAM management.
* **Memory Management**: MBC (Memory Bank Controller) mapper support for dynamic ROM/RAM banking across classic Game Boy and GBC titles.

---

## 🚀 Features

* **Unified Launcher (`EmulatorLauncher`)**: Clean separation of concerns between core selection, MMU abstraction, and platform-specific rendering/audio frontends.
* **Debugging & Diagnostics**: Built-in opcode tracing, crash logging, and PC state monitors.
* **Modular Design**: Easily extensible codebase structured to spin up multiple hardware targets from a single executable.

---

## 🛠️ Tech Stack & Requirements

* **Language**: Modern C++ (C++17/20)
* **Build System**: CMake (3.15+)
* **Compiler**: MSVC, GCC, or Clang

---

## 📦 Building and Running

1. **Clone the repository**:
```bash
git clone https://github.com/troller2705/Emulator.git
cd Emulator
```
2. **Configure and build using CMake**:

```Bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

# 🗺️ Development Roadmap
### SNES Core:

-[x] CPU Initialization, Reset Vectors, & Core Opcodes
-[x] LoROM Cartridge Loader & MMU
-[ ] NMI and VBlank Interrupt Timing
-[ ] PPU Scanline & Background Rendering
-[ ] SPC700 Audio Core

### Game Boy / GBC Core:

-[x] Sharp CPU Core & Instruction Decoder
-[x] Memory Management & MBC Controllers
-[x] PPU Scanline Pipeline & Sprite Rendering
-[x] APU Sound Channel Emulation