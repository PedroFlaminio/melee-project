# Project plan: native Super Smash Bros. Melee PC port

Status: initial proposal  
Base analysed: `doldecomp/melee`, commit `114e34ac5024211729b673baff28562143f910a0`  
Initial target version: `GALE01` (NTSC-U 1.02)  
Date of analysis: 11 September 2026

## 1. Executive summary

The project must produce a native executable. It must not bundle Dolphin and must not run
PowerPC code under emulation. Melee's C gameplay code is compiled for the host platform,
while a new platform layer implements the services the game used to receive from the
GameCube: GX, OS, VI, DVD, PAD, CARD, AX, ARAM, DSP and THP.

The distribution model follows Ship of Harkinian:

- neither the repository nor the distributable binaries contain Nintendo assets;
- on first run the user supplies a legal, compatible copy of the disc;
- a tool verifies the dump and extracts/converts the assets into a local resource archive;
- the executable loads native code and the converted resources;
- replacement resources and mods can be mounted as additional layers.

The recommended path is not to port every subsystem at once. First build a small vertical
slice: startup, asset reading, video, input, and a Fox vs. Fox match on Final Destination.
From there, expand the subsystems until the whole game is covered.

Realistic estimate for a 1.0 release:

- core team of 8 to 12 people: 18 to 30 months;
- core team of 3 to 5 people: 30 to 48 months;
- playable proof of concept, not release quality: 4 to 8 months.

These ranges assume contributors experienced in C/C++, graphics and reverse engineering.
The biggest risk is no longer decompiling gameplay; it is reproducing the GameCube platform
correctly and removing dependencies on the 32-bit PowerPC ABI.

## 2. State of the base and its implications

### 2.1 What is already done

In the revision analysed:

- all 1,118 objects declared in `configure.py` use `Object(Matching, ...)`;
- there are roughly 541 thousand lines of C in `src` and `extern/dolphin/src`;
- the entry point and the game loop are in C;
- gameplay, characters, items, stages, menus and HAL's `baselib` are in C;
- the matching build can still rebuild `main.dol` for PowerPC.

That makes it possible to use the original DOL as a behavioural oracle and to build very
precise differential tests.

### 2.2 Why the base is not yet portable

The current build exists to reproduce the original binary, not to obey a modern ABI. The
blockers observed include:

- `s32` and `u32` are defined with `long`, whose size changes on LP64 hosts;
- structures assume 4-byte pointers and carry more than 200 size or offset asserts;
- the HSD loader relocates offsets by writing addresses into 32-bit slots;
- disc and memory-card files are big-endian;
- immediate vertices are written straight into the GX FIFO at `0xCC008000`;
- startup and the game loop call OS, VI, DVD, PAD, CARD and GX directly;
- there are 121 `asm` function definitions, plus four assembly files;
- there are hundreds of `MUST_MATCH` conditional sections and Metrowerks-specific pragmas;
- audio depends on AX/DSP/ARAM and video depends on VI/GX/THP;
- asynchronous DVD, audio and retrace callbacks assume the console's timing and concurrency;
- floating-point differences can alter physics, derived RNG and match determinism.

Consequently, "100% decompiled" means the C code recompiles to the same PowerPC; it does
not mean "100% ready for x86-64 or ARM64".

## 3. Product definition

### 3.1 MVP

The MVP must be deliberately narrow:

- Windows 10/11 x86-64 and Linux x86-64;
- `GALE01` NTSC-U 1.02 discs only;
- local asset extraction from the user-supplied ISO/GCM;
- keyboard and SDL gamepad controls;
- 16:9 or 4:3 video, configurable internal resolution and fullscreen;
- simulation fixed at the original cadence;
- presentation initially locked to the simulation's 60 Hz, without precluding a later layer
  of intermediate poses;
- working audio with music and effects;
- local Versus for two to four players;
- all 26 characters and every stage selectable;
- local save and atomic settings;
- no proprietary code or assets distributed in the releases.

### 3.2 Version 1.0

Version 1.0 adds:

- Windows, Linux and macOS, on x86-64 and ARM64 where applicable;
- every single-player and local multiplayer mode;
- THP cutscenes, trophies, events, compatibility debug and memory card;
- visual, audio and gameplay equivalence validated by differential tests;
- high refresh-rate presentation, up to 240 Hz where the hardware allows, by visual
  interpolation between 60 Hz simulation ticks;
- hotplug, rumble, full remapping and controller profiles;
- a settings menu opened with `Esc`, navigable by keyboard, mouse and controller, with
  changes persisted per profile;
- a versioned, verifiable, re-compatible resource package;
- a basic API for mods and replacement asset packs;
- an installer/updater with no game content.

### 3.3 Out of initial scope

- rollback netcode, matchmaking and Slippi compatibility in the main build;
- balance or mechanics changes;
- raising the simulation cadence above the original 60 Hz;
- support for every disc revision and region;
- Android, iOS, consoles and WebAssembly;
- a visual stage/character editor;
- loading ISOs without prior extraction/conversion;
- replacing the original save format before compatibility exists.

Rollback should guide a few decisions from the start — deterministic time, recordable input
and serializable state — but must not block the first release.

### 3.4 High refresh rate and a future Slippi build

Melee's simulation stays fixed at 60 Hz. It is the source of truth for physics, inputs,
frame data, timers and determinism. Presentation can be independent: the renderer keeps the
previous and current states and, between two ticks, samples intermediate poses for the
camera, skeletons, transforms and other safe visual data. The presentation option offers
only the 60, 120, 144, 165 and 240 FPS targets; there will be no unlimited mode. The product
goal is to reach the selected target without running extra logic and without changing the
outcome of a match.

This layer is developed and validated after basic visual parity at 60 Hz. Camera cuts,
teleports, spawns, scene changes, effects with no interpolable state and any discontinuity
must preserve the valid frame, never invent a position that feeds back into the simulation.

The settings menu is a native overlay, opened and closed with `Esc`, inspired by Ship of
Harkinian's settings organization without reusing its interface or code. It does not replace
Melee's original menus and must pause or merely capture input according to what is safe for
the current scene. The first page is **Video** and shows:

- **Presentation rate:** 60, 120, 144, 165 or 240 FPS;
- **Aspect:** original 4:3 or 16:9, with correct letterboxing/pillarboxing and no change to
  match logic;
- **Upscaling:** configurable internal resolution and scaling filter, always separate from
  the window resolution; the backend must expose only methods validated on every supported
  platform.

Choices must be persisted in versioned configuration, applied to the renderer without
restarting the simulation where that is safe, and reverted automatically if a video mode
change fails. The interface shows the effective presentation rate and the active scaling
mode, so that a selected preference can be told apart from a rate the GPU could not sustain.

Slippi compatibility is not a requirement for the first version. If it is adopted later, it
ships as a specific, optional build separate from the main one. That build must keep the
simulation strictly deterministic, implement the interface/protocol Slippi expects, and be
validated against Slippi Dolphin; high-rate visual features stay in the renderer only and do
not enter the synchronized state.

## 4. Proposed architecture

```text
                  +-----------------------------+
   user's ISO/GCM | verifier + extractor        |
----------------->| FST, HSD, textures, audio   |
                  +-------------+---------------+
                                |
                                v
                  +-----------------------------+
                  | melee.pak + manifest.json   |
                  | big-endian data handled     |
                  +-------------+---------------+
                                |
                                v
+-------------------+   +-------+---------+   +--------------------+
| Melee C code      |-->| libmelee_host   |-->| SDL3 / system      |
| gameplay + baselib|   | C ABI compatible|   | window/input/files |
+-------------------+   +---+---+---+-----+   +--------------------+
                          |   |   |
                    +-----+   |   +----------------+
                    v         v                    v
              +----------+ +----------+      +-----------+
              | GX host  | | AX host  |      | DVD/CARD  |
              | renderer | | audio    |      | resources |
              +----+-----+ +----+-----+      +-----------+
                   |            |
                   v            v
             wgpu-native   SDL audio graph
             D3D12/Vulkan/
             Metal
```

### 4.1 Principles

1. Preserve the original matching build as a permanent reference.
2. Keep port changes under `MELEE_HOST` and behind small interfaces, avoiding per-platform
   `#ifdef` noise in gameplay.
3. Use a C ABI at the boundary between the game and the host layer; C++ may be used beneath
   it for resources, renderer, UI and tools.
4. Decode serialized data into native runtime structures. Do not treat a big-endian blob
   with 32-bit pointers as a 64-bit C struct.
5. Keep simulation and presentation separate. Resolution, widescreen and presentation FPS
   must not change match logic.
6. Prefer differential tests against the DOL over "looks right".
7. Do not couple the project to one graphics backend or operating system.

### 4.2 Suggested repository organization

```text
src/                         # original decomp, kept synchronizable
extern/dolphin/              # original headers/API
port/
  include/melee_host/        # stable C interfaces
  src/os/                    # time, threads, alarms, queues, memory
  src/io/                    # virtual DVD, files, CARD
  src/input/                 # PAD, keyboard, gamepads, rumble
  src/gx/                    # GX state, TEV, shaders and draw submission
  src/audio/                 # AX, DSP ADPCM, mixer, streaming
  src/video/                 # window, VI, frame pacing, THP
  src/assets/                # runtime resource manager
  src/ui/                    # settings and diagnostics
tools/
  extractor/                 # ISO/GCM -> melee.pak
  trace/                     # capture and comparison against Dolphin
tests/
  unit/
  differential/
  replay/
cmake/
CMakeLists.txt               # host build; the matching build stays separate
```

## 5. Technical decisions

### 5.1 Build and language

- CMake + Ninja for the native build.
- C17 for the C code and C++20 for the host layer.
- Clang and MSVC as supported compilers; GCC in CI as an extra check.
- SDL3 for window, events, gamepads, rumble and basic audio abstractions.
- `wgpu-native` as the first choice of renderer, exposing D3D12, Vulkan and Metal.
- ImGui only for settings and tools; never for the game's original UI.
- Sanitizers, high warning levels and static analyzers enabled on port code.

The renderer must sit behind its own interface. If the proof of concept shows that dynamic
TEV shader generation is a poor fit for WebGPU, it must be possible to swap internally for
Vulkan/D3D/Metal or bgfx without touching the game.

### 5.2 32-to-64-bit strategy

Shipping a 32-bit executable as the final architecture is not recommended. The transition
runs on two tracks:

- a temporary 32-bit x86 bootstrap build may speed up the first boot and help locate
  problems that are not ABI-related;
- the product build must be 64-bit clean from the first quarter onwards.

Mandatory steps:

1. replace primitive aliases with `stdint.h` in host mode;
2. classify every pointer-integer cast as address, offset, ID or flags;
3. create explicit types such as `HsdOffset32`, `AssetId` and `RuntimeHandle`;
4. separate `*Disk`/`*BE` structures from runtime structures;
5. replace in-place HSD relocation with deserialization and pointer swizzling;
6. keep tables for cyclic and external references;
7. run ASan/UBSan and tests on x86-64 and ARM64 early.

The HSD loader and the particle system are the first test case, since both write addresses
directly over 32-bit offsets.

### 5.3 Resources and distribution

The `melee-extract` tool must:

1. accept an ISO/GCM or a previously extracted directory;
2. identify revision/region by hash and disc metadata;
3. refuse nothing silently: errors must explain which file was expected;
4. read the FST and extract only the files that are needed;
5. validate size/hash of the essential files;
6. convert big-endian metadata into a versioned format;
7. preserve compressed data where expanding it brings no benefit;
8. produce `melee.pak` and a manifest that does not depend on the ISO's path;
9. allow incremental rebuild and integrity checking;
10. never upload the ISO or the extracted files to a server.

The resource manager mounts, in order: `melee.pak`, official port patches, user mods, and
loose development overrides. Each resource carries a type, schema version, hash,
dependencies and logical name.

### 5.4 OS layer and memory

Implement only the surface the game actually uses:

- arena and heaps on top of host allocators;
- alarms, ticks and calendar;
- mutexes, message queues and threads;
- interrupts as critical section/reentrancy, not as CPU emulation;
- cache flush/invalidate as a validated no-op or a barrier where needed;
- logs, asserts, panic and crash reports;
- asynchronous callbacks delivered at deterministic points on the game thread.

MetroTRK, EXI debug hardware support, PPC registers and boot ROM do not enter the host
executable. Stubs must state explicitly whether the operation is a safe no-op, unsupported,
or a fatal error.

### 5.5 GX and renderer

The renderer is the project's largest isolated front. The `gx_host` layer must model the
state observed through the GX API, not emulate the Flipper chip cycle by cycle.

Components:

- replacing `GXWGFifo` writes with a command encoder in host mode;
- vertex descriptors, formats, indexed arrays and display lists;
- position, normal and texture matrices;
- GameCube textures, TLUT, mipmaps, wrap and filters;
- blending, depth, alpha compare, culling, fog, scissor and viewport;
- EFB/XFB copies, screenshots, shadows and effects that read the framebuffer;
- TEV compiler: GX state -> canonical IR -> shader;
- persistent pipeline/shader cache keyed by state;
- a fallback ubershader to avoid stalls during compilation;
- markers and frame capture for RenderDoc;
- a software reference path for small TEV tests.

Implementation order:

1. clear, viewport, untextured triangles;
2. vertex arrays and matrices;
3. simple textures and blending;
4. single-stage TEV;
5. multiple stages, indirect texturing and fog;
6. EFB copies, shadows and special cases;
7. cache, performance, widescreen and internal resolution.

Using Dolphin code directly must only happen after an explicit licensing decision. The
emulator's architecture is broader than needed, and its license may determine the license of
the whole derived layer. A clean implementation of the APIs in use reduces coupling but
demands strong tests.

### 5.6 Audio, ARAM and DSP

The audio layer must preserve the AX voice model without emulating the DSP:

- DSP ADPCM parser and decoder;
- voices, priority, pitch, volume, pan, envelopes and looping;
- aux sends and the effects the HSD Synth uses;
- music streaming with an asynchronous buffer;
- virtual ARAM as storage/handles, not a physical address;
- floating-point mixer with a final conversion for the SDL device;
- resampling independent of the device rate;
- an audio event log for deterministic comparison.

First goal: audible music and SFX in the vertical slice. Mix fidelity, reverb and edge cases
come later; low latency and the absence of underruns are release criteria.

### 5.7 VI, time and frame pacing

- simulation on a fixed tick, derived from the original NTSC timing;
- `VIGetRetraceCount` and callbacks modelled by the host scheduler;
- decoupled rendering, without accidentally running two ticks on a 120 Hz monitor;
- an interpolation option only for validated visual transforms;
- configurable pause when the window loses focus;
- input-to-photon measurement in diagnostic mode;
- no `sleep` may be the simulation's source of truth.

### 5.8 Input and rumble

- PAD 0-3 over SDL Gamepad;
- configurable deadzones, calibration, analog triggers and octagonal gate;
- keyboard as an equivalent device;
- hotplug without changing indices during a match;
- rumble with a fallback when the device does not support it;
- per-tick input capture/replay as a stable test format;
- GameCube adapters treated first as HID/SDL, with an optional specialized backend if
  latency justifies it.

### 5.9 DVD, CARD and saves

- map DVD paths and entry numbers to the resource manager;
- keep asynchronous semantics and callback ordering;
- implement a virtual memory card compatible with Melee's data;
- write through a temporary file, with appropriate `fsync` and an atomic rename;
- rotating backups and recovery after an interruption;
- import/export GCI once technically validated;
- keep port configuration separate from the game save.

### 5.10 THP and cutscenes

Implement the THP container and decode video/audio with audited libraries, or adapt the
existing C decoder after removing PPC optimizations. Audio-video synchronization, the seek
the game uses, and colour conversion each need their own tests.

### 5.11 Determinism and floating point

The project must not promise determinism before measuring it. The strategy is:

- reference builds with FMA/FP contraction under control;
- known implementations for PPC estimates such as `frsqrte`, where they affect gameplay;
- an audit of undefined behaviour, casts, shifts and aliasing;
- RNG state and input included in every trace;
- per-tick hashes over canonical logical state only, with no pointers;
- tolerant comparisons only where the difference does not feed back into gameplay;
- long-match testing and rollback/snapshot even before netplay.

## 6. Verification strategy

### 6.1 Reference oracle

Run the corresponding DOL in an instrumented Dolphin and the native port with the same input
sequence. The decompilation's symbol map makes it possible to capture state without
inferring addresses by hand.

Per tick, record:

- scene and game state machine;
- seed/RNG;
- each fighter's action state, position, velocity, damage and stocks;
- relevant entities, items and collision results;
- audio events;
- canonical signature of the GX commands;
- hash of the serialized state.

At visual milestones, compare:

- screenshot with a mask for time-dependent elements;
- depth where needed;
- the expected pipeline/TEV;
- perceptual tolerance and a difference map.

### 6.2 Test pyramid

- unit: endianness, FST, HSD relocation, ADPCM, TEV IR, CARD and math;
- contract: each host function mimics observed cases of the Dolphin API;
- replay: short deterministic sequences per character/stage;
- differential: port versus DOL in Dolphin;
- visual: golden images per backend/GPU with a defined tolerance;
- soak: automated 8 to 24 hour matches;
- fuzz: disc, HSD, THP and save parsers;
- performance: tick time, frame time, shader compilation, audio and memory.

### 6.3 CI

Minimum matrix:

- Windows x86-64: MSVC and Clang;
- Linux x86-64: Clang and GCC;
- macOS ARM64: Clang from the 1.0 milestone onwards;
- ASan/UBSan on Linux;
- PowerPC matching build to prevent regressions in the base;
- the assetless build must always complete;
- tests that need assets run only on private workers, with public hashes and results, never
  publishing the files.

## 7. Roadmap and gates

### Phase 0 — charter, license and baseline (weeks 1-4)

Deliverables:

- scope and governance charter;
- licensing decision for new code and a contribution policy;
- synchronization strategy with `doldecomp/melee`;
- generated inventory of platform APIs, asm, casts and layouts;
- initial corpus of replays/traces on the DOL;
- CI for the matching build.

Gate: any port commit preserves the matching rebuild, and there is a clear assets/licensing
policy.

### Phase 1 — host build and ABI (months 1-3)

Deliverables:

- CMake compiles the relevant code on x86-64 with stubs;
- fixed-width integer types and host libc headers;
- explicit exclusion of Runtime/MetroTRK/hardware;
- the `melee_host` C ABI layer;
- ASan/UBSan and a report of dangerous casts;
- an executable skeleton that reaches `main` and exits in a controlled way.

Gate: host build with no PPC assembly and no MMIO address access.

### Phase 2 — resources, OS and headless boot (months 2-5)

Deliverables:

- `GALE01` verifier/extractor;
- virtual DVD and a 64-bit/big-endian HSD loader;
- memory, time, queues, alarms and asynchronous jobs;
- null renderer and audio that capture commands;
- headless boot up to the first scene/menu.

Gate: the same scene sequence and the same initial RNG as the DOL in a boot replay.

### Phase 3 — minimal GX renderer (months 3-8)

Deliverables:

- window, VI and frame pacing;
- GX command encoder, vertices, matrices, textures and essential TEV;
- title screen, menus and Final Destination rendered;
- automated visual capture.

Gate: the visual vertical slice runs with no graphics API validation errors.

### Phase 4 — playable vertical slice (months 5-10)

Deliverables:

- PAD/rumble;
- minimal AX audio;
- a Fox vs. Fox match, four controllers and the HUD;
- pause, match end and return to the menu;
- a five-minute differential replay.

Gate: gameplay does not diverge from the DOL during the agreed replay, and the frame budget
is held on reference hardware.

### Phase 5 — content coverage (months 8-16)

Deliverables:

- every character, item and stage;
- single-player, events, trophies and the remaining menus;
- advanced GX, particles, framebuffer effects and shadows;
- THP, full audio, CARD and saves;
- a replay suite across the character/stage matrix.

Gate: the content checklist is complete, there are no known high-severity crashes, and
gameplay traces stay within the equivalence policy.

### Phase 6 — portability, fidelity and performance (months 13-22)

Deliverables:

- macOS/ARM64 and multiple backends;
- shader cache without recurring stutter;
- profiling and latency reduction;
- 4:3, safe widescreen and internal resolution;
- basic accessibility, remapping and settings;
- soak, fuzz and broad GPU/controller compatibility.

Gate: performance and compatibility targets met across the hardware matrix.

### Phase 7 — 1.0 release (months 20-30)

Deliverables:

- installer, updater and opt-in crash diagnostics;
- an extraction flow an end user can follow;
- build, usage and troubleshooting documentation;
- license audit and confirmation that no assets are present;
- public beta, triage and reproducible release candidates.

Gate: the criteria in section 8 met by two consecutive release candidates.

Phases overlap across the team. The dates are planning ranges, not a calendar promise.

## 8. Acceptance criteria for version 1.0

Functionality:

- the user can generate the resources from a supported dump;
- every mode reachable in the target DOL can be completed;
- saves survive an unexpected shutdown during non-critical operations;
- four controllers, hotplug and rumble work;
- cutscenes, music and SFX stay in sync.

Fidelity:

- a competitive replay corpus shows no unexplained logical divergence;
- image differences stay within the per-scene approved limits;
- no enhancement changes physics/timing while compatibility mode is on;
- RNG and callback ordering are reproducible given the same input/configuration.

Performance:

- simulation tick under 4 ms on the defined minimum hardware;
- frame within budget at 1080p on the minimum hardware;
- no persistent audio underruns;
- no memory growth in an 8-hour soak test;
- shader stutter is limited to first use or absorbed by the fallback.

Quality and distribution:

- zero open critical defects and zero known save corruption;
- reproducible, signed builds for the supported platforms;
- the public package contains no game assets;
- an SBOM and third-party notices ship with the release;
- crash reporter and telemetry are opt-in.

## 9. Team organization

Fronts that can run in parallel:

- architecture/build/ABI: 2 people;
- GX/renderer: 3 to 4 people;
- OS, input, DVD and CARD: 2 people;
- assets and tools: 1 to 2 people;
- audio/THP: 2 people;
- determinism, testing and CI: 2 people;
- release, UX and documentation: 1 person, growing near the beta.

Some people can cover more than one front, but renderer, audio and differential testing need
clear owners. Each subsystem gets an owner, a backup, a documented interface and a coverage
dashboard.

Suggested cadence:

- a quarterly roadmap by gates, not by number of functions ported;
- playable demos every two weeks;
- a mandatory RFC for asset format, GX IR, ABI and save;
- divergence bugs get a minimal replay before the fix;
- every optional enhancement must be switchable off by compatibility mode.

## 10. Priority risks

| Risk | Impact | Mitigation |
|---|---:|---|
| 64-bit HSD pointer swizzling | Critical | Disk/Runtime structures, IDs and cyclic graph tests |
| Incomplete TEV/EFB semantics | Critical | Canonical IR, reference renderer and comparison against Dolphin |
| Floating-point divergence | Critical | Per-tick traces, controlled PPC functions and a competitive corpus |
| AX/DSP mixing incorrectly | High | Voice log, tested decoder and offline comparison |
| Asynchronous callbacks change order | High | Deterministic scheduler and delivery on the game thread |
| Shader stutter | High | Persistent cache, prewarm and ubershader fallback |
| Save corruption | High | Atomic writes, backups and interruption fuzzing |
| Fork diverges from the decomp | High | Dual build, automated upstream merges and few ifdefs |
| License of reused code | High | RFC/license review before copying Dolphin or another port |
| Scope creeps into netplay/mods | High | Frozen MVP and separate later epics |
| Assets enter CI/releases | Critical | Private workers, artifact scanner and hash manifests |

## 11. First 90 days

### Days 1-30

- approve the charter, license and fork structure;
- automate the inventory of GameCube dependencies;
- define the `host_os`, `host_gx`, `host_audio`, `host_io` and `host_pad` interfaces;
- prepare five reference replays in Dolphin;
- create an empty CMake build and cross-platform CI;
- start converting to fixed-width types without breaking matching.

### Days 31-60

- compile `lb`, `gm` and parts of `sysdolphin` on the host;
- replace MSL/Runtime and PPC assembly with host implementations or exclusion;
- implement FST/ISO, the manifest and synchronous DVD reads;
- prototype a big-endian HSD parser with 64-bit swizzling;
- create OSReport/panic, a clock and an allocator;
- have a test program load and inspect a model from the disc.

### Days 61-90

- wire the entry point to auditable stubs;
- reach headless boot with command logs;
- open an SDL window and clear the framebuffer;
- map PAD and record/replay input per tick;
- implement the first triangle through a GX subset;
- publish an updated risk report and an estimate for the vertical slice.

Expected outcome after 90 days: not a complete game, but a demonstration that reduces the
three biggest risks — host build, 64-bit assets and the GX path — and provides enough data
to confirm or revise the schedule.

## 12. Progress indicators

Avoid using "percentage of code compiled" as the main indicator. Measure:

- number of host APIs called versus implemented and validated;
- scenes that complete boot and transition;
- character/stage combinations covered by replay;
- consecutive ticks without logical divergence;
- GX/TEV states covered and fallback pipelines used;
- asset formats decoded;
- game modes completed;
- GPUs, systems and controllers approved;
- fidelity defects, crashes and corruption by severity;
- p95/p99 of tick, frame, audio callback and shader compilation.

The dashboard must distinguish `stub`, `functional`, `equivalent` and `optimized`. An API
that merely returns zero does not count as done.

## 13. Go/no-go decision

The project gets a green light for full development once the proof of concept demonstrates,
at the same time:

1. a real HSD asset loaded correctly on x86-64;
2. a real frame emitted by baselib through the host GX subset;
3. repeatable deterministic boot up to a known scene;
4. no unavoidable dependency on PowerPC emulation;
5. an approved licensing and distribution path.

If the GX subset turns out to cost disproportionately, the project must revisit renderer,
libraries and license — not silently swap the goal for bundling an emulator. If 64-bit
swizzling proves unfeasible in time, a 32-bit x86 build can keep serving as a research tool,
but it does not replace the cross-platform product goal.

## Architecture references

- Melee decompilation: <https://github.com/doldecomp/melee>
- Shipwright / Ship of Harkinian: <https://github.com/HarbourMasters/Shipwright>
- Shipwright build instructions:
  <https://github.com/HarbourMasters/Shipwright/blob/develop/docs/BUILDING.md>
