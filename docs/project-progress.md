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

**Overall: 46% (confidence range: 36–56%).**

Moved twice on 21 September: down to 42% when the stage sweep found only 3 of 30 stages
loading, then back to 45% once the per-stage `yakumono_param` layouts took that to 15. The
first move was better measurement, not a regression; the second is real work.

The range is wide because one area dominates the remaining work and is the least measured:
game modes. The 55% end assumes the modes beyond VS mostly reuse the engine that already runs;
the 35% end assumes each one brings its own data, scenes and blockers, the way VS did.

Percentages below are per-area completeness against the 1.0 definition, not against the MVP.
An area at 100% in the MVP table can sit well below that here.

| Area | Weight | Done | Where it stands |
| --- | ---: | ---: | --- |
| Game modes and content | 25% | 15% | 3 of the 45 modes in `gm/forward.h` are in the host table: `GM_TITLE`, `GM_MENU`, `GM_VS`. VS is complete except the challenger match (its own stage, against a CPU). The engine the other modes reuse — fighters, stages, items, HUD, results, sudden death — already runs, so the next modes should cost less than VS did, but none has been attempted. Content within VS: all 26 characters load, and 17 of 30 stages, up from 3 on 21 September. |
| Assets and data translation | 15% | 55% | 61 translators registered. All 28 `ftData*` are translated, each with its character's item attribute table. Stages: `coll_data` 71/71, `grGroundParam` 71/71, `map_head` 69/71, `map_plit`/`quake_model_set`/`itemdata`/`ALDYakuAll` across the disc. Gaps, in order of impact: `yakumono_param` now has a per-stage layout generated from the game's structs, so 17 of 30 stages load; two have a wrong layout and the rest fail past it; the per-type special attributes and dynamics of `itPublicData`'s common items and Pokémon are left out and panic at `item.c:576`; every event level's `x4` parameter is NULL; loose images and palettes have no schema. |
| Rendering (GX) | 15% | 75% | A sweep of 725 joint symbols gives 100.0% of 3,379,817 triangles with the TEV fully evaluated, zero display list errors and zero rejected indices. Fog, bump (`GX_TG_BUMPn`), depth textures and indirect TEV are in the presenter; EFB copies rasterize on the CPU in I4 and in colour. Gaps: mipmaps (minification uses the magnification filter), fog range adjustment, `GXEnableTexOffsets`, `GXSetTevSwapModeTable`, `GXCopyDisp` materialization, and no visual comparison against a reference for any of it. |
| Platform layer (OS, memory, time, DVD, input) | 10% | 70% | Heap, arena, alarms, scheduler, virtual DVD, ARAM/ARQ and PAD all run. Gaps: DVD cancellation/streaming/priority, controller remapping, rumble, hotplug during a match, and the resource manager that should consume the extracted manifest. |
| Audio | 8% | 85% | The AX mixer plays the game's voices, music and effects, with aux buses (reverb and delay), ITD and surround encoded to stereo. Music and one effect match reference decoders at correlation 1.000000. Gaps: no sample-by-sample comparison against the console for reverb, ITD or surround; chorus and high reverb unported (the game does not use them). |
| Determinism and fidelity | 10% | 35% | The canonical `TRACE=` trace is identical across runs, across hours and between `host-debug` and `-O2`. That is self-consistency, not equivalence: nothing has been compared against the DOL in Dolphin. `atanf`, `sinf`, `cosf`, `tanf`, `sqrtf`, `sqrt` and `fmodf` still come from `libm`, and `__frsqrte` is the exact `1.0 / sqrt(x)` where the console uses a table estimate, so Newton-refined roots can differ in the last bits. |
| Presentation and settings | 5% | 50% | The Esc overlay works and persists: internal resolution, aspect, upscaling filter, window mode, presentation rate, MSAA, anisotropy, custom textures, FPS counter. Gaps: the high rates re-present the same frame — no visual interpolation is implemented — and input and audio latency have not been measured. |
| Saves and memory card (CARD) | 5% | 5% | Stubbed absent: `CARDProbe` answers no card in either slot (`port/src/os/absent_devices.c`). The real path keeps addresses in 32 bits in four `lb_8001*` functions, the task's `unk_18`/`unk_1C` and `hsd_3A94.c`'s command queue, which converts pointers to `s32` 14 times. Without a card only 14 characters and the starting stages are unlocked. |
| Platform coverage | 4% | 40% | Linux is the development platform. Windows MSVC and macOS Clang build and test in CI, and there are Clang-on-Visual-Studio presets, but the renderer resolves GL 2.0+ through `GL_GLEXT_PROTOTYPES`, which only works on Linux, so the window has not run on either. ARM64 is untouched. |
| Cutscenes (THP) | 3% | 0% | Not started. THP appears only as a named stop in `unported.c`. |

Weighted total: 45.2%, reported as 45%.

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
- [ ] **No crashing combination.** 15 of 30 stages load, up from 3; the rest are in
  *Known issues* and remain the top priority. Character × stage combinations are still
  untested: each harness varies one dimension only.
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

### 17 of 30 stages load (was 3)

Reported from real play on 21 September 2026: picking most stages crashed the game at match
start. Root cause and fix below; measured with `port/tools/sweep_stages.py`.

| | Stages |
| --- | --- |
| Loads (17) | 7 Corneria, 9 Onett, 12 Jungle Japes, 14 Hyrule Temple, 15 Brinstar Depths, 16 Yoshi's Island, 17 Green Greens, 18 Fourside, 19 Mushroom Kingdom, 20 Mushroom Kingdom II, 23 Poke Floats, 27 Flat Zone, 28 Dream Land N64, 29 Yoshi's Island N64, 30 Kongo Jungle N64, 31 Battlefield, 32 Final Destination |
| Wrong layout (2) | 4 Princess Peach's Castle, 25 Icicle Mountain — the generated extent check catches both |
| Stops at another symbol (2) | 2 Fountain of Dreams, 3 Pokemon Stadium — `image_desc`, which has no translator |
| Crashes deeper in stage setup (10) | 5 Kongo Jungle, 6 Brinstar, 8 Yoshi's Story, 10 Mute City, 11 Rainbow Cruise, 13 Great Bay, 16 Yoshi's Island, 20 Mushroom Kingdom II, 22 Venom, 24 Big Blue |
| Random square (1) | resolves to one of the above |

**What it was.** Every `Gr*.dat` carries a symbol called `yakumono_param`, and each stage
declares its own struct for it inside its `grXXX.c`. The host could not tell the layouts apart
by name, and telling them apart by size is what findings R03–R05 of
[`review-fa257ed.md`](review-fa257ed.md) removed, because several stages share an extent with
incompatible fields. So it refused all 47 non-zero blocks, and
`melee_host_stage_symbols_check` turned each refusal into an `OSPanic`.

**What fixed it.** The stage's identity comes from the game: `Ground_801C0754` sets
`stage_info.grkind` before `grDatFiles_801C6038` reads the archive, and that read is what runs
the translators. `port/tools/gen_yakumono_layout.py` parses the structs out of `src/melee/gr`
and emits one translator per GrKind, selected by that identity. It follows
`gen_host_command_layout.py`: a generated copy plus a `--check` that the
`melee-host-yakumono-layout-generated` test runs, so a struct that moves under the generator
fails the suite instead of corrupting data. Computed offsets are cross-checked against the
decomp's own `/* 0x.. */` comments, and each translator verifies the block's extent before
reading it. Four stages need no layout: Hyrule Temple and Poke Floats declare `yakumono_param`
as `void*` and never read it; Great Bay and Yoshi's Island never mention it.

Reaching the stages then exposed two pointers truncated to 32 bits, both the documented
"address kept in an `int`" shape:

- `Ground_801C10B8` writes its deferred-callback node as `{void*, HSD_GObj*, HSD_GObjEvent}`
  and `Ground_801C0FB8` read it back as `{void*, s32, void(*)(s32)}` — the same struct on the
  console, a truncated GObj and a wrong call signature on the host.
- `fn_801C82E8`, the `AOBJ_ARG_AV` callback `granime.c` hands to `HSD_ForeachAnim`, took the
  `HSD_AObj` as an `int`.

**What a player sees now.** The select screen refuses a square the host has not been measured
to enter, the way it refuses a locked one. `melee_host_stage_is_playable` holds the measured
list; the `OSPanic` stays as the backstop for data that fails unexpectedly, because there is no
path in the engine to unwind a half-built stage. A stage asked for another way — a route's
`FRAME:STAGE`, or a mode that sets `force_stage_id` — still loads and still stops loudly, so
this hides nothing from the sweep.

**What is left, with a named cause each.** A sanitizer sweep on 21 September 2026
(`sweep_stages.py --binary build/host-sanitize/port/melee-pc`) gave every remaining stage a
file and line:

| Stage | Cause, after the 21–22 September fixes |
| --- | --- |
| 2 Fountain of Dreams | SIGSEGV — *was* `image_desc`, now translated |
| 3 Pokemon Stadium | **flaky** — reaches the match 2 runs in 3, so something reads uninitialised memory; `image_desc` and the `memzero` overflow both fixed |
| 4 Princess Peach's Castle | `dynamicsdata_flag3` has no translator — *was* a wrong block size, fixed |
| 5 Kongo Jungle | SEGV in `HSD_JObjSetRotationZ`, from `grKongo_801D77E0` |
| 6 Brinstar | truncated `Item_GObj` in `grZebes_801DA528` — *was* `HSD_FObjLoadDesc`, fixed |
| 8 Yoshi's Story | `item.c:576`, stage item kind 210 has no attribute translator |
| 10 Mute City | SIGBUS — *was* `HSD_FObjLoadDesc`, fixed |
| 11 Rainbow Cruise | SEGV at `grrcruise.c:767` — *was* a console-sized allocation, fixed |
| 13 Great Bay | `item.c:576`, stage item kind 221 has no attribute translator |


| 22 Venom | SEGV — *was* `HSD_FObjLoadDesc`, fixed |
| 24 Big Blue | SEGV in `grBigBlue_801ED694` — *was* a console-sized allocation, fixed |
| 25 Icicle Mountain | block is 0x13C, layout describes 0xD0 — the struct's offsets are wrong, as R04 noted |

**The `HSD_FObjLoadDesc` cluster is closed.** It was the animation tables: `grAnime_801C7C1C`
indexes them as `aj = &aj[joint]`, so each entry is an array of animation joints on disc, one
per joint of the model, and the host built only the tree at the root. Confirmed at the crash —
the joint index was 28 and the struct read back held raw archive offsets — and fixed by
`stage_anim_table`, which builds the count the model's joint tree gives. That took Yoshi's
Island and Mushroom Kingdom II into a match and moved Brinstar, Mute City and Venom to faults
of their own.

The shape of the work is settled, and it is not one lever per stage. These are the documented
host-size families — an allocation sized from the console's table, a read that runs past a
global the console could read past, a descriptor offset used as a pointer — each small and
well-precedented, but **there is a chain of them per stage**. Five such fixes went in on
21–22 September (Rainbow Cruise, Pokemon Stadium, Brinstar, Venom, Big Blue) and the number of
stages entering a match did not move: every one advanced to its next fault instead. That is
the measurement, not a setback — it is what tells you to budget several fixes per stage.

The sanitizer is the tool that walks those chains; a stage is done when a sanitized run of it
reaches the match scene, not when one ASan report clears.

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
| 2026-09-22 | 46% | `image_desc` becomes a symbol kind the archive layer can name, reusing the materializer the TObj path already had, and the `yakumono_param` extent check reports the size it found — which identified both layout mismatches in one run. Castle's block is 0x148 against a 0x144 struct, one unnamed trailing word, so the generator gained a `DISC_SIZES` table for that; Icicle Mountain is 0x13C against 0xD0, too wide for a tail, matching R04 on its offsets. Stages stayed at 17 of 30: Pokemon Stadium reaches the match only 2 runs in 3 and is not counted, Fountain of Dreams and Castle both advanced to their next symbol. `ctest --preset host-debug` 27/27. |
| 2026-09-22 | 46% | Stage animation tables materialized as arrays. `grAnime_801C7C1C` indexes them `aj = &aj[joint]`, so each entry is an array of animation joints on disc, one per joint of the model; the host built only the root tree, so any index past the first read the arena. Confirmed at the Brinstar crash (joint index 28, struct full of raw archive offsets) and by probing the table extents against the console strides. **Stages entering a match: 15 → 17 of 30**, and the `HSD_FObjLoadDesc` cluster that held four stages is closed. `ctest --preset host-debug` 27/27. |
| 2026-09-22 | 45% | Five host-size memory errors fixed, each confirmed by ASan before and after: console-sized allocations in `grrcruise.c`, `grbigblue.c` and `grpstadium.c`, and reads across neighbouring `.data` objects in `grzebes.c` and `grvenom.c`. Stages entering a match stayed at 15 of 30 — every one advanced to its next fault, which is what establishes that these come in chains per stage. The `HSD_FObjLoadDesc` cluster (Brinstar, Mute City, Venom, and Mushroom Kingdom II one step away) is the next lever; the suspect is `map_head`'s animation tables being materialized one tree per entry where the game indexes them as an array. `ctest --preset host-debug` 27/27. |
| 2026-09-21 | 45% | Stages. `yakumono_param` gets a per-stage layout, generated from each `grXXX.c`'s struct by `port/tools/gen_yakumono_layout.py` and selected by the loading stage's GrKind, with a `--check` test against generator drift. Two truncated 32-bit pointers fixed on the paths that reaching the stages exposed (`Ground_801C0FB8`'s callback node, `granime.c`'s `HSD_ForeachAnim` callback). The select screen refuses a square the host has not been measured to enter. **Stages entering a match: 3 → 15 of 30.** `ctest --preset host-debug` 27/27, 235/235 unit tests. |
| 2026-09-21 | 42% | Stage sweep. A person reports playing many matches, with crashes on some stages; `port/tools/sweep_stages.py` (new) forces each stage through the game's own `force_stage_id` and finds **3 of 30 stages load** — the other 27 abort on `yakumono_param`, one cause. Added `FRAME:STAGE=KIND` to `--run-modes` and `melee_host_sss_force_stage` to reach stages without the icon layout or a save. `random_cpu_matches.py` had played every match on Hyrule Temple, one of the three that work, which is why 14/14 CPU matches passed the same day. Estimate revised down from 45%: no regression, better measurement. |
| 2026-09-21 | 45% | Baseline for this document. Repository cleaned of the decomp project: the `upstream` remote, the mwcc/Nix/decomp-toolkit build, the Doxygen site, the PowerPC-only runtime sources and the committed build artifacts are gone, and all documentation is in English. `tools/port_inventory.py` stops counting `Object(Matching, ...)`. Build clean, `ctest --preset host-debug` 26/26. |
| 2026-09-21 | — | (MVP milestone closed 2026-09-16; its per-change history is in `port-mvp-progress.md`.) |
