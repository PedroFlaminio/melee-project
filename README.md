# Super Smash Bros. Melee PC Port

A native port of Super Smash Bros. Melee to PC (Windows and Linux). The project started as
a fork of the Melee decompilation (`doldecomp/melee`), but this repository is now aimed
entirely at producing a native executable of the game — no emulator (such as Dolphin) and
no PowerPC code execution.

## Overview

Where the original decompilation aims to rebuild a 1:1 GameCube executable (`main.dol`) for
documentation and modding, this port compiles the game's C source (gameplay, characters,
menus) directly for modern platforms such as PC x86-64.

To make that possible, a new platform abstraction layer replaces the direct dependencies on
GameCube hardware and SDK (OS, GX, VI, DVD, PAD, CARD, AX, ARAM, DSP and THP) with modern
equivalents (OpenGL/Vulkan, SDL, and so on). The architectural model follows notable
decompilation-based ports such as *Ship of Harkinian*.

### Distribution and legal resources

- **No protected assets:** neither the source in this repository nor any executable
  distributed from it contains original Nintendo-owned assets (models, audio, textures and
  the like).
- **Bring your own game (BYOG):** to run the game you must supply a dump (ISO/GCM) of a
  legitimate copy of Super Smash Bros. Melee (initial target version: `GALE01` NTSC-U 1.02).
  On first run the port extracts the files and assets it needs into a local resource folder
  for the game to load.
- **PC port improvements:** flexible, customizable resolutions, direct gamepad and keyboard
  support through modern libraries, and framerate improvements.

## Building

Requirements: CMake 3.25 or newer, Ninja, a C17/C++20 compiler, and an OpenGL
driver. CMake uses an installed SDL3 when available and otherwise downloads a static SDL3
for the windowed build. Python 3.10 or newer is required by the asset tools and by the
Windows first-run extractor.

```sh
cmake --preset host-debug          # configure
cmake --build --preset host-debug  # build
ctest --preset host-debug          # run the test suite
```

The release preset builds the playable executable:

```sh
cmake --preset host-release
cmake --build --preset host-release --target melee-pc
./build/host-release/port/melee-pc --play assets-local
```

`assets-local/` is the resource folder extracted from your own disc image; it is never
committed. Presets for Clang on Visual Studio (`host-debug-windows`,
`host-release-windows`) and for an ASan/UBSan build (`host-sanitize`) are defined in
`CMakePresets.json`. Windows builds copy the extractor beside `melee-pc`; on first run the
setup window accepts a GALE01 NTSC-U 1.02 ISO/GCM and verifies the extracted manifest,
DVD index, and `main.dol` hash before starting the game.

## Documentation

For details on the port's architecture, how far along it is, and the main technical porting
challenges relative to the decompilation base, see the [`docs/`](docs/) folder —
[`docs/README.md`](docs/README.md) indexes it.

The main documents are:

- [Project progress](docs/project-progress.md) — how far along the port is, by area
- [Native PC port plan](docs/native_pc_port_plan.md) — scope, architecture and roadmap
- [Native port status](docs/native_port_status.md) — what works, and what does not yet
- [Native port development](docs/native_port_development.md) — building, running and debugging

Contribution guidelines live in [`.github/CONTRIBUTING.md`](.github/CONTRIBUTING.md).

---

**Legal notice:** *This project is an unofficial, open-source, fan-made effort dedicated to
technical preservation and to the study of software engineering through reverse engineering
of the original game. Super Smash Bros. Melee and its respective assets, designs and names
are registered and intellectual properties of Nintendo.*
