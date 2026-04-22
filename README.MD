# Game Boy Emulator

A Game Boy (DMG) emulator written in C++ from scratch. Passes all 11 Blargg `cpu_instrs` tests, confirming a cycle-accurate SM83 CPU implementation.


## Features

- **SM83 CPU** — all 512 opcodes implemented, passes all 11 Blargg `cpu_instrs` tests
- **PPU** — background layer, window layer, sprite rendering with priority and flip flags
- **Timer** — DIV, TIMA, TMA, TAC registers with interrupt support
- **Interrupts** — VBlank, STAT, Timer, and Joypad interrupts
- **MBC1** — memory bank controller supporting games up to 2MB ROM
- **OAM DMA** — sprite table transfers
- **Joypad** — full 8-button input via keyboard

## Build

Requires CMake 3.20+, GCC 13+, and SDL2.

**Windows (MSYS2):**
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-SDL2
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

**Run:**
```bash
./gbemu <rom.gb>
```

## Controls

| Keyboard | Game Boy |
|----------|----------|
| Arrow Keys | D-Pad |
| Z | A |
| X | B |
| Enter | Start |
| Backspace | Select |
| Escape | Quit |

## Tested ROMs

| ROM | Status |
|-----|--------|
| Blargg cpu_instrs | ✅ All 11 tests pass |
| Tetris | ✅ Playable |
| Pokemon Red/Blue | ✅ Renders, navigates menus |

## Architecture