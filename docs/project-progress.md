# Project progress

**This is the running progress document for the project.** Update it with every change that
moves the port forward, the way `port-mvp-progress.md` was updated while the MVP was open.
The MVP closed at 100% on 16 September 2026; that document is now a finished record of that
milestone and is not updated any more.

Scope here is the whole project through version 1.0, as
[`native_pc_port_plan.md`](native_pc_port_plan.md) §3.2 defines it: every single-player and
local multiplayer mode, all 26 characters and every stage, THP cutscenes, trophies, events,
memory card, audio and visual equivalence validated by differential tests, Windows/Linux/macOS,
a settings overlay, a versioned resource package, a mod API, and an installer with no game
content.

## Current estimate — 21 September 2026

**Overall: 45% (confidence range: 35–55%).**

The range is wide because one area dominates the remaining work and is the least measured:
game modes. The 55% end assumes the modes beyond VS mostly reuse the engine that already runs;
the 35% end assumes each one brings its own data, scenes and blockers, the way VS did.

Percentages below are per-area completeness against the 1.0 definition, not against the MVP.
An area at 100% in the MVP table can sit well below that here.

| Area | Weight | Done | Where it stands |
| --- | ---: | ---: | --- |
| Game modes and content | 25% | 15% | 3 of the 45 modes in `gm/forward.h` are in the host table: `GM_TITLE`, `GM_MENU`, `GM_VS`. VS is complete except the challenger match (its own stage, against a CPU). The engine the other modes reuse — fighters, stages, items, HUD, results, sudden death — already runs, so the next modes should cost less than VS did, but none has been attempted. |
| Assets and data translation | 15% | 55% | 61 translators registered. All 28 `ftData*` are translated, each with its character's item attribute table. Stages: `coll_data` 71/71, `grGroundParam` 71/71, `map_head` 69/71, `map_plit`/`quake_model_set`/`itemdata`/`ALDYakuAll` across the disc. Gaps: `yakumono_param` refuses the 47 files with parameters of their own; the per-type special attributes and dynamics of `itPublicData`'s common items and Pokémon are left out and panic at `item.c:576`; every event level's `x4` parameter is NULL; loose images and palettes have no schema. |
| Rendering (GX) | 15% | 75% | A sweep of 725 joint symbols gives 100.0% of 3,379,817 triangles with the TEV fully evaluated, zero display list errors and zero rejected indices. Fog, bump (`GX_TG_BUMPn`), depth textures and indirect TEV are in the presenter; EFB copies rasterize on the CPU in I4 and in colour. Gaps: mipmaps (minification uses the magnification filter), fog range adjustment, `GXEnableTexOffsets`, `GXSetTevSwapModeTable`, `GXCopyDisp` materialization, and no visual comparison against a reference for any of it. |
| Platform layer (OS, memory, time, DVD, input) | 10% | 70% | Heap, arena, alarms, scheduler, virtual DVD, ARAM/ARQ and PAD all run. Gaps: DVD cancellation/streaming/priority, controller remapping, rumble, hotplug during a match, and the resource manager that should consume the extracted manifest. |
| Audio | 8% | 85% | The AX mixer plays the game's voices, music and effects, with aux buses (reverb and delay), ITD and surround encoded to stereo. Music and one effect match reference decoders at correlation 1.000000. Gaps: no sample-by-sample comparison against the console for reverb, ITD or surround; chorus and high reverb unported (the game does not use them). |
| Determinism and fidelity | 10% | 35% | The canonical `TRACE=` trace is identical across runs, across hours and between `host-debug` and `-O2`. That is self-consistency, not equivalence: nothing has been compared against the DOL in Dolphin. `atanf`, `sinf`, `cosf`, `tanf`, `sqrtf`, `sqrt` and `fmodf` still come from `libm`, and `__frsqrte` is the exact `1.0 / sqrt(x)` where the console uses a table estimate, so Newton-refined roots can differ in the last bits. |
| Presentation and settings | 5% | 50% | The Esc overlay works and persists: internal resolution, aspect, upscaling filter, window mode, presentation rate, MSAA, anisotropy, custom textures, FPS counter. Gaps: the high rates re-present the same frame — no visual interpolation is implemented — and input and audio latency have not been measured. |
| Saves and memory card (CARD) | 5% | 5% | Stubbed absent: `CARDProbe` answers no card in either slot (`port/src/os/absent_devices.c`). The real path keeps addresses in 32 bits in four `lb_8001*` functions, the task's `unk_18`/`unk_1C` and `hsd_3A94.c`'s command queue, which converts pointers to `s32` 14 times. Without a card only 14 characters and the starting stages are unlocked. |
| Platform coverage | 4% | 40% | Linux is the development platform. Windows MSVC and macOS Clang build and test in CI, and there are Clang-on-Visual-Studio presets, but the renderer resolves GL 2.0+ through `GL_GLEXT_PROTOTYPES`, which only works on Linux, so the window has not run on either. ARM64 is untouched. |
| Cutscenes (THP) | 3% | 0% | Not started. THP appears only as a named stop in `unported.c`. |

Weighted total: 44.9%, reported as 45%.

## What "done" currently rests on

- `ctest --preset host-debug`: 26/26, 235/235 unit tests.
- `ctest --preset host-sanitize -V`: 26/26, no ASan report. UBSan only prints; 37 distinct
  points remain, catalogued in [`native_port_status.md`](native_port_status.md).
- CI builds and tests on Linux (sanitizers), Windows MSVC and macOS Clang.
- The `-O2` build runs the whole stock route in about 5 s without presenting, and `--play`
  holds 60 frames per second.

The honest gap in all of it: **nobody has played a match.** Real key events drive the whole
route through the window, and the gamepad path has only a unit test. Until a person plays and
says whether it responds like the console, the fidelity numbers above are self-consistency
measurements.

## Milestones

- [x] **MVP — local VS match.** Closed 16 September 2026 at 100%. Record in
  [`port-mvp-progress.md`](port-mvp-progress.md).
- [ ] **A person plays.** A gamepad through a full match, and a judgement on whether it feels
  like the console. The one thing no automation closes.
- [ ] **Visual reference parity.** Compare fog, bump, depth textures, indirect TEV and the EFB
  copies against a captured reference, rather than against the port's own CPU rasterizer.
- [ ] **Saves.** A virtual memory card, which unlocks the rest of the roster and the stages,
  and with it the SSS beyond Hyrule Temple.
- [ ] **The second mode.** Whichever of Training, Classic or Event costs least from here;
  that first one measures how much the other 41 really cost.
- [ ] **Differential equivalence.** A per-tick trace compared against the DOL in Dolphin, which
  is what turns "deterministic" into "correct".
- [ ] **High refresh presentation.** Visual interpolation between 60 Hz ticks, with the
  simulation untouched.
- [ ] **Distribution.** Installer, updater, signed reproducible builds, SBOM, and an extraction
  flow an end user can follow.

## Out of scope

Unchanged from the plan: rollback netcode, matchmaking and Slippi compatibility in the main
build; balance or mechanics changes; raising the simulation above 60 Hz; every disc revision
and region; mobile, consoles and WebAssembly; and loading ISOs without extraction.

## Update log

One row per change that moves the port forward. Keep the newest at the top.

| Date | Overall | Change and evidence |
| --- | ---: | --- |
| 2026-09-21 | 45% | Baseline for this document. Repository cleaned of the decomp project: the `upstream` remote, the mwcc/Nix/decomp-toolkit build, the Doxygen site, the PowerPC-only runtime sources and the committed build artifacts are gone, and all documentation is in English. `tools/port_inventory.py` stops counting `Object(Matching, ...)`. Build clean, `ctest --preset host-debug` 26/26. |
| 2026-09-21 | — | (MVP milestone closed 2026-09-16; its per-change history is in `port-mvp-progress.md`.) |
