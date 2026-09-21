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

**Overall: 42% (confidence range: 32–52%).**

Revised down from 45% on 21 September, after the stage sweep found that only 3 of the 30
stages actually load (see *Known issues*). Nothing regressed; the estimate had leaned on
translator counts, and measuring what a player can actually reach showed content coverage was
worse than those counts implied. This is the value of the measurement, not a setback.

The range is wide because one area dominates the remaining work and is the least measured:
game modes. The 52% end assumes the modes beyond VS mostly reuse the engine that already runs;
the 32% end assumes each one brings its own data, scenes and blockers, the way VS did.

Percentages below are per-area completeness against the 1.0 definition, not against the MVP.
An area at 100% in the MVP table can sit well below that here.

| Area | Weight | Done | Where it stands |
| --- | ---: | ---: | --- |
| Game modes and content | 25% | 12% | 3 of the 45 modes in `gm/forward.h` are in the host table: `GM_TITLE`, `GM_MENU`, `GM_VS`. VS is complete except the challenger match (its own stage, against a CPU). The engine the other modes reuse — fighters, stages, items, HUD, results, sudden death — already runs, so the next modes should cost less than VS did, but none has been attempted. Content within VS is narrower than the roster suggests: all 26 characters load, but only 3 of 30 stages do. |
| Assets and data translation | 15% | 45% | 61 translators registered. All 28 `ftData*` are translated, each with its character's item attribute table. Stages: `coll_data` 71/71, `grGroundParam` 71/71, `map_head` 69/71, `map_plit`/`quake_model_set`/`itemdata`/`ALDYakuAll` across the disc. Gaps, in order of impact: `yakumono_param` refuses the 47 files with parameters of their own, which is what keeps 27 of 30 stages from loading at all; the per-type special attributes and dynamics of `itPublicData`'s common items and Pokémon are left out and panic at `item.c:576`; every event level's `x4` parameter is NULL; loose images and palettes have no schema. |
| Rendering (GX) | 15% | 75% | A sweep of 725 joint symbols gives 100.0% of 3,379,817 triangles with the TEV fully evaluated, zero display list errors and zero rejected indices. Fog, bump (`GX_TG_BUMPn`), depth textures and indirect TEV are in the presenter; EFB copies rasterize on the CPU in I4 and in colour. Gaps: mipmaps (minification uses the magnification filter), fog range adjustment, `GXEnableTexOffsets`, `GXSetTevSwapModeTable`, `GXCopyDisp` materialization, and no visual comparison against a reference for any of it. |
| Platform layer (OS, memory, time, DVD, input) | 10% | 70% | Heap, arena, alarms, scheduler, virtual DVD, ARAM/ARQ and PAD all run. Gaps: DVD cancellation/streaming/priority, controller remapping, rumble, hotplug during a match, and the resource manager that should consume the extracted manifest. |
| Audio | 8% | 85% | The AX mixer plays the game's voices, music and effects, with aux buses (reverb and delay), ITD and surround encoded to stereo. Music and one effect match reference decoders at correlation 1.000000. Gaps: no sample-by-sample comparison against the console for reverb, ITD or surround; chorus and high reverb unported (the game does not use them). |
| Determinism and fidelity | 10% | 35% | The canonical `TRACE=` trace is identical across runs, across hours and between `host-debug` and `-O2`. That is self-consistency, not equivalence: nothing has been compared against the DOL in Dolphin. `atanf`, `sinf`, `cosf`, `tanf`, `sqrtf`, `sqrt` and `fmodf` still come from `libm`, and `__frsqrte` is the exact `1.0 / sqrt(x)` where the console uses a table estimate, so Newton-refined roots can differ in the last bits. |
| Presentation and settings | 5% | 50% | The Esc overlay works and persists: internal resolution, aspect, upscaling filter, window mode, presentation rate, MSAA, anisotropy, custom textures, FPS counter. Gaps: the high rates re-present the same frame — no visual interpolation is implemented — and input and audio latency have not been measured. |
| Saves and memory card (CARD) | 5% | 5% | Stubbed absent: `CARDProbe` answers no card in either slot (`port/src/os/absent_devices.c`). The real path keeps addresses in 32 bits in four `lb_8001*` functions, the task's `unk_18`/`unk_1C` and `hsd_3A94.c`'s command queue, which converts pointers to `s32` 14 times. Without a card only 14 characters and the starting stages are unlocked. |
| Platform coverage | 4% | 40% | Linux is the development platform. Windows MSVC and macOS Clang build and test in CI, and there are Clang-on-Visual-Studio presets, but the renderer resolves GL 2.0+ through `GL_GLEXT_PROTOTYPES`, which only works on Linux, so the window has not run on either. ARM64 is untouched. |
| Cutscenes (THP) | 3% | 0% | Not started. THP appears only as a named stop in `unported.c`. |

Weighted total: 42.7%, reported as 42%.

## What "done" currently rests on

- `ctest --preset host-debug`: 26/26, 235/235 unit tests.
- `ctest --preset host-sanitize -V`: 26/26, no ASan report. UBSan only prints; 37 distinct
  points remain, catalogued in [`native_port_status.md`](native_port_status.md).
- CI builds and tests on Linux (sanitizers), Windows MSVC and macOS Clang.
- The `-O2` build runs the whole stock route in about 5 s without presenting, and `--play`
  holds 60 frames per second.

**A person has played many matches** and reports the game running correctly. That closes the
one gap no automation could: the port is playable, not just scriptable. What it exposed
instead is the item below — some character and stage combinations still crash, and the
automated suite does not see them.

The fidelity numbers above remain self-consistency measurements: playing well is not the same
as matching the console, and nothing has been compared against the DOL in Dolphin.

## Milestones

- [x] **MVP — local VS match.** Closed 16 September 2026 at 100%. Record in
  [`port-mvp-progress.md`](port-mvp-progress.md).
- [x] **A person plays.** Many matches played through the window; the game runs. Reported
  21 September 2026.
- [ ] **No crashing combination.** Some character and stage combinations still crash in real
  play. See *Known issues* below; this is the top priority.
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

## Known issues

### Only 3 of 30 stages load — `yakumono_param` (top priority)

Reported from real play on 21 September 2026: the game runs and matches play through, but
picking certain stages crashes it at match start. Swept with
`port/tools/sweep_stages.py` on 21 September against the `-O2` build: **3 of the 30 stages the
select screen offers enter a match. The other 27 abort while loading.**

| Result | Stages |
| --- | --- |
| Loads | 14 Hyrule Temple, 23 Poke Floats, 31 Battlefield |
| `yakumono_param has no stage-specific verified layout` | 17 stages: 2 Fountain of Dreams, 3 Pokemon Stadium, 8 Yoshi's Story, 9 Onett, 11 Rainbow Cruise, 12 Jungle Japes, 13 Great Bay, 15 Brinstar Depths, 16 Yoshi's Island, 17 Green Greens, 18 Fourside, 19 Mushroom Kingdom, 20 Mushroom Kingdom II, 24 Big Blue, 27 Flat Zone, 28 Dream Land N64, 29 Yoshi's Island N64 |
| `yakumono_param contains an unsupported relocated object` | 9 stages: 4 Princess Peach's Castle, 5 Kongo Jungle, 6 Brinstar, 7 Corneria, 10 Mute City, 22 Venom, 25 Icicle Mountain, 30 Kongo Jungle N64, 32 Final Destination |
| Random square (0) | resolves to one of the above and fails with it |

It is a single cause. The host refuses `yakumono_param` for the 47 stage files that carry
parameters of their own, and `game_data_translators.c:3655` turns that refusal into an
`OSPanic`, which aborts the process:

```
host HSD archive: cannot translate yakumono_param: yakumono_param has no stage-specific verified layout
OS panic at port/src/game/game_data_translators.c:3655: the host cannot load this stage
```

The refusal is deliberate and correct — it came from findings R03–R05 in
[`review-fa257ed.md`](review-fa257ed.md), which replaced layout guessing by size with an
explicit refusal, removing real memory corruption. What was not decided then is what should
happen to a player who picks such a stage. Two separable pieces of work:

1. **Translate the layouts** (the real fix): one `yakumono_param` schema per stage, from the
   struct each `grXXX.c` declares. 47 files. Doing the 9 with relocated objects needs the
   pointer targets materialized as well, as R03 describes.
2. **Stop aborting the process** (small, and immediately visible): an untranslatable stage
   should be refused at the select screen, or fall back the way
   `Ground_801C06B8` already handles a stage with no data — not take the game down. This is a
   behaviour decision, not a bug fix, and it would mask case 1 if done alone.

### Why the suite never saw it

`port/tools/random_cpu_matches.py` played every one of its matches on Hyrule Temple —
`STAGE_KIND = 14` hard-coded, with a comment explaining that it was once the only stage that
loaded. Hyrule Temple is one of the three that still work, so 14 of 14 CPU matches passed on
21 September while 27 of 30 stages were broken. The suite also draws only from the 14
characters the select screen offers without a save, while play with *Unlock Everything*
reaches all 26.

`port/tools/sweep_stages.py` closes the stage half of that gap: it walks the real menus to the
select screen and then forces the stage through `FRAME:STAGE=KIND`, the same `force_stage_id`
the game's own Training and Tournament modes set, so every square is reachable without the
icon layout and without a save.

### Historical crash data (16–17 September 2026)

An earlier sweep of 11 CPU matches, all on Hyrule Temple, recorded 4 SIGSEGVs. **Yoshi was in
three of them**, all dying at scene `0x09` (the stage select, or the match it was loading):

| Fighters | Died in | At frame |
| --- | --- | ---: |
| ness vs pikachu | `0x02` the match | 963 |
| yoshi vs mario | `0x09` stage select / match load | 684 |
| yoshi vs link vs mario | `0x09` stage select / match load | 684 |
| iceclimbers vs yoshi | `0x09` stage select / match load | 604 |

This predates the Yoshi attribute and Ness item fixes (`cd8f479f5`, `e45ca8e4f`) and the
21 September renderer work, so it says where to look, not what is currently broken.

## Out of scope

Unchanged from the plan: rollback netcode, matchmaking and Slippi compatibility in the main
build; balance or mechanics changes; raising the simulation above 60 Hz; every disc revision
and region; mobile, consoles and WebAssembly; and loading ISOs without extraction.

## Update log

One row per change that moves the port forward. Keep the newest at the top.

| Date | Overall | Change and evidence |
| --- | ---: | --- |
| 2026-09-21 | 42% | Stage sweep. A person reports playing many matches, with crashes on some stages; `port/tools/sweep_stages.py` (new) forces each stage through the game's own `force_stage_id` and finds **3 of 30 stages load** — the other 27 abort on `yakumono_param`, one cause. Added `FRAME:STAGE=KIND` to `--run-modes` and `melee_host_sss_force_stage` to reach stages without the icon layout or a save. `random_cpu_matches.py` had played every match on Hyrule Temple, one of the three that work, which is why 14/14 CPU matches passed the same day. Estimate revised down from 45%: no regression, better measurement. |
| 2026-09-21 | 45% | Baseline for this document. Repository cleaned of the decomp project: the `upstream` remote, the mwcc/Nix/decomp-toolkit build, the Doxygen site, the PowerPC-only runtime sources and the committed build artifacts are gone, and all documentation is in English. `tools/port_inventory.py` stops counting `Object(Matching, ...)`. Build clean, `ctest --preset host-debug` 26/26. |
| 2026-09-21 | — | (MVP milestone closed 2026-09-16; its per-change history is in `port-mvp-progress.md`.) |
