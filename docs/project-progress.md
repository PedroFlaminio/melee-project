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

## Current estimate — 26 September 2026

**Overall: 48% (confidence range: 38–58%).**

Moved twice on 21 September: down to 42% when the stage sweep found only 3 of 30 stages
loading, then back to 45% once the per-stage `yakumono_param` layouts took that to 15. The
first move was better measurement, not a regression; the second is real work. The 47% this
header showed from 22 September was never what the table below adds up to (45.2%); on
25 September the stage work took the table to 45.9%, reported as 46%. On 26 September every
stage and every character entered a match on three runs of three, which took it to 47.9%.

The range is wide because one area dominates the remaining work and is the least measured:
game modes. The 55% end assumes the modes beyond VS mostly reuse the engine that already runs;
the 35% end assumes each one brings its own data, scenes and blockers, the way VS did.

Percentages below are per-area completeness against the 1.0 definition, not against the MVP.
An area at 100% in the MVP table can sit well below that here.

| Area | Weight | Done | Where it stands |
| --- | ---: | ---: | --- |
| Game modes and content | 25% | 20% | 3 of the 45 modes in `gm/forward.h` are in the host table: `GM_TITLE`, `GM_MENU`, `GM_VS`. VS is complete except the challenger match (its own stage, against a CPU). The engine the other modes reuse — fighters, stages, items, HUD, results, sudden death — already runs, so the next modes should cost less than VS did, but none has been attempted. Content within VS is complete as far as the sweeps measure: all 29 stages enter a match on every run (3 on 21 September), and all 26 characters survive 30 seconds as a level 9 CPU and as the fighter Kirby swallows. Character × stage pairs are not swept yet. |
| Assets and data translation | 15% | 63% | 61 translators registered. All 28 `ftData*` are translated, each with its character's item attribute table. Stages: `coll_data` 71/71, `grGroundParam` 71/71, `map_head` 69/71, `map_plit`/`quake_model_set`/`itemdata`/`ALDYakuAll` across the disc. `yakumono_param` has a per-stage layout generated from the game's structs, pointer fields translated by pointee type. Special attributes are translated for the Bob-omb, Food and the stage items Shy Guy and Tingle, through one table by item kind; Kirby's 25 copy abilities (`ftDataKirbyCopy*`), the dynamics' animation trees and Mr. Game & Watch's items are translated too, and four fighter data symbols that never matched the disc's spelling now do. Gaps, in order of impact: the per-type special attributes and dynamics of the other common items and the Pokémon are left out and panic at `item.c:576`; every event level's `x4` parameter is NULL; loose images and palettes have no schema. |
| Rendering (GX) | 15% | 75% | A sweep of 725 joint symbols gives 100.0% of 3,379,817 triangles with the TEV fully evaluated, zero display list errors and zero rejected indices. Fog, bump (`GX_TG_BUMPn`), depth textures and indirect TEV are in the presenter; EFB copies rasterize on the CPU in I4 and in colour. Gaps: mipmaps (minification uses the magnification filter), fog range adjustment, `GXEnableTexOffsets`, `GXSetTevSwapModeTable`, `GXCopyDisp` materialization, and no visual comparison against a reference for any of it. |
| Platform layer (OS, memory, time, DVD, input) | 10% | 70% | Heap, arena, alarms, scheduler, virtual DVD, ARAM/ARQ and PAD all run. Gaps: DVD cancellation/streaming/priority, controller remapping, rumble, hotplug during a match, and the resource manager that should consume the extracted manifest. |
| Audio | 8% | 85% | The AX mixer plays the game's voices, music and effects, with aux buses (reverb and delay), ITD and surround encoded to stereo. Music and one effect match reference decoders at correlation 1.000000. Gaps: no sample-by-sample comparison against the console for reverb, ITD or surround; chorus and high reverb unported (the game does not use them). |
| Determinism and fidelity | 10% | 35% | The canonical `TRACE=` trace is identical across runs, across hours and between `host-debug` and `-O2`. That is self-consistency, not equivalence: nothing has been compared against the DOL in Dolphin. `atanf`, `sinf`, `cosf`, `tanf`, `sqrtf`, `sqrt` and `fmodf` still come from `libm`, and `__frsqrte` is the exact `1.0 / sqrt(x)` where the console uses a table estimate, so Newton-refined roots can differ in the last bits. |
| Presentation and settings | 5% | 50% | The Esc overlay works and persists: internal resolution, aspect, upscaling filter, window mode, presentation rate, MSAA, anisotropy, custom textures, FPS counter. Gaps: the high rates re-present the same frame — no visual interpolation is implemented — and input and audio latency have not been measured. |
| Saves and memory card (CARD) | 5% | 5% | Stubbed absent: `CARDProbe` answers no card in either slot (`port/src/os/absent_devices.c`). The real path keeps addresses in 32 bits in four `lb_8001*` functions, the task's `unk_18`/`unk_1C` and `hsd_3A94.c`'s command queue, which converts pointers to `s32` 14 times. Without a card only 14 characters and the starting stages are unlocked. |
| Platform coverage | 4% | 40% | Linux is the development platform. Windows MSVC and macOS Clang build and test in CI, and there are Clang-on-Visual-Studio presets. Windows now resolves every GL 2.0+ entry point it uses through `SDL_GL_GetProcAddress`, but the window still needs a runtime smoke test there. ARM64 is untouched. |
| Cutscenes (THP) | 3% | 0% | Not started. THP appears only as a named stop in `unported.c`. |

Weighted total: 47.9%, reported as 48%.

## What "done" currently rests on

- `ctest --preset host-debug`: 29/29, 242/242 unit tests (also 242/242 under the sanitizers, no
  ASan report).
- `port/tools/sweep_stages.py --repeat 3`: 29 of 29 stages. `port/tools/sweep_characters.py
  --repeat 3`: 26 of 26 characters in both modes. The measured lists are
  `port/data/stage_status.json` and `port/data/character_status.json`.
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
- [ ] **No crashing combination.** Every stage and every character enter a match on three
  runs of three. What is left is the product: character × stage, items on, 3–4 players. Character × stage combinations are still
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

### Every stage loads (was 3 of 29)

Reported from real play on 21 September 2026: picking most stages crashed the game at match
start. Root cause and fix below; measured with `port/tools/sweep_stages.py --repeat 3` on
25 September 2026: **all 29 stages enter a match on three runs of three.** The select screen
has 30 squares; the thirtieth is the random square, which is not a stage. It draws one of the
29 (`mnStageSel_802599EC`) before the match, so the sweep skips it; forced as a kind of its own
it is the `Dummy` stage, which has no music and asserts on the console too.
`port/data/stage_status.json` marks it `"random": true`.

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

**What is left.** The canonical measured result is
[`port/data/stage_status.json`](../port/data/stage_status.json): 27 of the 30 select-screen
entries pass every requested attempt. It generates the game's allowlist and also supplies the
sweep's order and names, so those two paths cannot silently disagree. Failure details belong
to the JSON-lines artifact written by `sweep_stages.py --results`; they are deliberately not
copied into this document, where they had become stale after their fixes landed.

**Several stages crash only on some runs, and it is the address layout.** The scripted
routes freeze the clock, so a run-to-run difference cannot come from the game. Princess
Peach's Castle settles it: with ASLR disabled it loads four runs of four, and with ASLR on it
fails two of six. Under the sanitizers it is `grcastle.c:1525`, reading
`gp->u.castle12.xC4[]` as GObj pointers where `grCastle_GroundVars11` aliases those bytes with
a bitfield, an `s16` pair and one address kept in a `u32` — so two of the three slots were
never pointers. That aliasing wants understanding before it is touched.

`sweep_stages.py --repeat N` now executes exactly N attempts and counts a stage only if all
pass, which is why the figure here is 17 rather than the 19 a single pass reports. Use
`--fail-fast` only for a shorter diagnostic run. `setarch -R` makes an address-dependent run
repeatable while chasing one.

An earlier note here blamed `MELEE_HOST_AUDIO=0` for this. That was wrong: the runs that
suggested it were this same flakiness landing either side of the comparison, and a fresh
measurement had audio on failing and audio off passing. The sweep still leaves the voices on,
for the plain reason that a real run has them on.

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

**What closed ten stages on 25 September.** Each stage below reached the match under the
sanitizers for 900 frames, then passed three runs of three in the debug build. The faults fell
into three shapes, all of them code that is right only while a pointer is four bytes:

- *Views that reach a field by a console offset.* Big Blue read its car lanes through unions of
  raw offsets from the start of the `Ground` (`pad_0[0xC8]`, `bytes + 0xD4`), and its Falcon
  Flyer and platform manager wrote at `gp + 0xC4..0xE0`, which on the host lands in the
  `Ground` header. Venom read `grVe_803E5348` and the `.data` after it as one word array
  (`base[i + 14]`, `base[k + 0xD6]`); each index now names its global, checked against
  `main.dol`. Fountain of Dreams wrote `Ground.x18` through a struct of `pad[0x18]`, which on
  the host is `xC_callback`, so the next frame called a heap address. Kongo Jungle and Venom
  stepped a `Ground*` by 0x10 and by one field to walk pairs of fields.
- *Two views of one gobj that diverge on the host.* Rainbow Cruise wrote `rcruise2.xEC` and
  read `scroll.anim_gobj`, and allocated its entries as `map.chikuwa`; Venom zeroed
  `venom.xE4` over the upper half of a `venom2` JObj pointer; Big Blue's platform manager is
  read as `u.bigblue` and `u.bigblue.manager`, which now agree.
- *Data typed as the wrong struct.* Mute City's track hit is an `lbColl_80008D30_arg1`, typed
  as `DynamicsDesc` everywhere `on_touch_line` reaches; the chain is retyped. The Shy Guy and
  Food attributes were indexed in 0x10-byte steps across a pointer and now have their real
  structs; the Tingle's first word is a pointer the game never reads.

Big Blue also set two lane states only in MWCC inline assembly, so on the host the cars never
changed state there; those writes now go through the named bitfields.

**The last two, on 25 September.** Princess Peach's Castle: its main gobj is read through five
views (`grCastle_GroundVars2`, 3, 4, 9 and 12) whose leading bytes are three satellite GObjs
kept in `u32` in some of them; each view now spells those out as pointers so the fields after
them agree on the host. The satellite a stage's piece rides in got a view of its own
(`castle_sat`) in place of six borrowed ones, and the camera gobj one (`castle_cam`) in place of
Corneria's Arwing view. Brinstar: a model's animation tables read at raw offsets from the
model array, the bubble state read as one object across three globals, a JObj kept in a `u32`,
and a lighting cache read through the wrong view of its gobj.

### Every character plays (new on 26 September)

`port/tools/sweep_characters.py` walks the menus to the stage select like the stage sweep, then
rewrites both ports' choices with a new diagnostic, `FRAME:CHAR=KIND,PORT[,CPU_LEVEL]`, and
forces Battlefield. It reaches all 26 characters without a save or the icon layout. Two modes,
each a column of [`port/data/character_status.json`](../port/data/character_status.json)
(`--update` writes it):

- `vs-cpu`: the character as a level 9 CPU against a level 9 CPU Mario for 1800 frames, so
  both fight — specials, grabs, throws and projectiles run, not just the load.
- `kirby-copy`: a level 9 CPU Kirby against the character, which loads Kirby's copy of that
  character's neutral special (`ftDataKirbyCopy<X>`); a CPU Kirby swallows and uses it.

Both are 26 of 26 on three runs of three. The first sweep had 0 of 26. What it took, in the
families the stages already showed:

- *Translation gaps.* Four `ftData*` symbols were registered under spellings the disc does not
  use (`ftDataGamewatch`, `ftDataClink`, `ftDataDrmario`, `ftDataGkoopa`), so Mr. Game &
  Watch never loaded. Kirby's 25 copy files had no translator at all; they come in two layouts,
  with each field typed from its reader in ftkirby.c (table `kirby_copy_schemas`). The
  dynamics' `x10` animation trees are now FigaTrees. Item lists hold non-Articles in some slots
  (Kirby's star, Yoshi's egg, Sheik's needles, Mr. Game & Watch's visibility lookups,
  Jigglypuff's hat parts). `item_generic_scalar_attrs` checked only the first word for a pointer
  and copied the rest raw, which handed the Belay string archive offsets as joints; it now
  checks the whole block.
- *Views and strides of the console.* `ftColl_8007A06C` wrote the hit result through a struct
  laid over `fp->dmg`, so the damage source never reached the Fighter field that holds it;
  grab escape and item scope states read the Fighter at console offsets
  (`FighterOverlay`); `efLib_SetParamGfxId` reached `efLib_ParamTable` as `efLib_AnimQueue +
  0x10`; Kirby stored hat DObjs at four-byte steps and read a bone's flags as byte 9; the
  throws read their jump through `fighterthrow`, which the host lays out differently from the
  `throw_` view that writes it.
- *Console-only tricks.* `ft_80089B08` wrote the word before a stack variable; Yoshi's
  specials read `NULL->user_data` for stack padding, which the GameCube maps.

The HSD object pools have two opt-in sanitizer modes for this kind of hunt:
`MELEE_HOST_OBJALLOC_MALLOC=1` gives every object its own allocation, and
`MELEE_HOST_OBJALLOC_POISON=1` keeps the pools but poisons free objects, so a write through a
stale pointer is reported where it happens. A gdb watchpoint on the corrupted link, with ASLR
off, is what finally named the Kirby hat bug.

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
| 2026-09-26 | 48% | **Every stage and every character.** Stages 27 → 29 of 29 (Castle: five views of the main gobj and the satellite's six borrowed views; Brinstar: raw model-array offsets, the bubble state across three globals, a JObj in a `u32`, a lighting cache through the wrong view); the random square is marked as such in `stage_status.json` and skipped. Characters: new `FRAME:CHAR=KIND,PORT[,CPU_LEVEL]`, `port/tools/sweep_characters.py` and `port/data/character_status.json`; 0 → 26 of 26 as a level 9 CPU and 26 of 26 against Kirby's copy, three runs of three (details under *Known issues*). New: Kirby copy translator, FigaTree reader, Game & Watch item attributes, sanitizer modes for the HSD pools. `ctest --preset host-debug` 29/29, 242/242 unit tests, sanitized suite clean. |
| 2026-09-25 | 46% | **Stages: 17 → 27 of 30**, three runs of three; Castle, Brinstar and the random square remain. Stage items: Shy Guy and Tingle special attributes, and Food's among the common items, through one table by kind; the Shy Guy and Food code read their blocks in steps that only fit four-byte pointers and now use real structs. Mute City's track hit retyped from `DynamicsDesc` to `lbColl_80008D30_arg1` through `on_touch_line`, `ftCo_Bury.c` and `ftcoll.c`, and its car array no longer read through the neighbouring global. Kongo Jungle, Rainbow Cruise, Venom, Big Blue and Fountain of Dreams: raw console offsets, diverging views of one gobj and overlapping globals replaced with named fields (details under *Known issues*). Big Blue's lane states were only ever set in MWCC inline assembly. Pokemon Stadium and Icicle Mountain now pass every run. The header's 47% corrected to what the table adds up to. `ctest --preset host-debug` 29/29, 240/240 unit tests, also under the sanitizers. |
| 2026-09-25 | 47% | Review [`review-d34b639.md`](review-d34b639.md) and its fixes (committed as `19c7f217d`): `glUniform3fv` through the Windows GL loader, the presentation-rate combo sized from its arrays, Icicle Mountain's three `s16` tables translated as values, the stage allowlist generated from `port/data/stage_status.json` (with a `--check` test), `sweep_stages.py --repeat` running every attempt, indirect TEV S/T matrices, alpha bump and unmodified LOD, and the Windows first-run extractor bundled and its assets validated by manifest and DOL hash. `ctest --preset host-debug` 29/29. |
| 2026-09-22 | 47% | Measurement corrected and Icicle Mountain's layout derived from the asset. `sweep_stages.py --repeat` counts a stage only if it loads every run, which puts the honest figure at **17 of 30**, not 19 — several stages fail only on some runs, and Castle proves that is the address layout (ASLR off: 4 of 4; ASLR on: fails 2 of 6). The earlier claim that `MELEE_HOST_AUDIO=0` decided this is retracted. Icicle Mountain's `yakumono_param` was refused at 0xD0 against a 0x13C block: dumping the asset shows the layout right to 0xBC, with the tail being 32 spawn descriptors rather than one plus four unread floats. It now reaches the match, though not on every run. `ctest --preset host-debug` 27/27. |
| 2026-09-22 | 47% | `dynamicsdata_*` symbols the game NULL-checks itself no longer count as refusals, so Princess Peach's Castle loads with its flags not swaying rather than panicking. The stage sweep stopped forcing `MELEE_HOST_AUDIO=0`, which had been deciding whether a stage loads and had misreported Castle as broken and Pokemon Stadium as flaky. Brinstar's acid-state overlay moved out of the fields that were tearing its `Item_GObj` in half, and three of its pointer-in-`u32` fields widened, though it still does not load. **Stages entering a match: 17 → 19 of 30.** `ctest --preset host-debug` 27/27. |
| 2026-09-22 | 46% | `image_desc` becomes a symbol kind the archive layer can name, reusing the materializer the TObj path already had, and the `yakumono_param` extent check reports the size it found — which identified both layout mismatches in one run. Castle's block is 0x148 against a 0x144 struct, one unnamed trailing word, so the generator gained a `DISC_SIZES` table for that; Icicle Mountain is 0x13C against 0xD0, too wide for a tail, matching R04 on its offsets. Stages stayed at 17 of 30: Pokemon Stadium reaches the match only 2 runs in 3 and is not counted, Fountain of Dreams and Castle both advanced to their next symbol. `ctest --preset host-debug` 27/27. |
| 2026-09-22 | 46% | Stage animation tables materialized as arrays. `grAnime_801C7C1C` indexes them `aj = &aj[joint]`, so each entry is an array of animation joints on disc, one per joint of the model; the host built only the root tree, so any index past the first read the arena. Confirmed at the Brinstar crash (joint index 28, struct full of raw archive offsets) and by probing the table extents against the console strides. **Stages entering a match: 15 → 17 of 30**, and the `HSD_FObjLoadDesc` cluster that held four stages is closed. `ctest --preset host-debug` 27/27. |
| 2026-09-22 | 45% | Five host-size memory errors fixed, each confirmed by ASan before and after: console-sized allocations in `grrcruise.c`, `grbigblue.c` and `grpstadium.c`, and reads across neighbouring `.data` objects in `grzebes.c` and `grvenom.c`. Stages entering a match stayed at 15 of 30 — every one advanced to its next fault, which is what establishes that these come in chains per stage. The `HSD_FObjLoadDesc` cluster (Brinstar, Mute City, Venom, and Mushroom Kingdom II one step away) is the next lever; the suspect is `map_head`'s animation tables being materialized one tree per entry where the game indexes them as an array. `ctest --preset host-debug` 27/27. |
| 2026-09-21 | 45% | Stages. `yakumono_param` gets a per-stage layout, generated from each `grXXX.c`'s struct by `port/tools/gen_yakumono_layout.py` and selected by the loading stage's GrKind, with a `--check` test against generator drift. Two truncated 32-bit pointers fixed on the paths that reaching the stages exposed (`Ground_801C0FB8`'s callback node, `granime.c`'s `HSD_ForeachAnim` callback). The select screen refuses a square the host has not been measured to enter. **Stages entering a match: 3 → 15 of 30.** `ctest --preset host-debug` 27/27, 235/235 unit tests. |
| 2026-09-21 | 42% | Stage sweep. A person reports playing many matches, with crashes on some stages; `port/tools/sweep_stages.py` (new) forces each stage through the game's own `force_stage_id` and finds **3 of 30 stages load** — the other 27 abort on `yakumono_param`, one cause. Added `FRAME:STAGE=KIND` to `--run-modes` and `melee_host_sss_force_stage` to reach stages without the icon layout or a save. `random_cpu_matches.py` had played every match on Hyrule Temple, one of the three that work, which is why 14/14 CPU matches passed the same day. Estimate revised down from 45%: no regression, better measurement. |
| 2026-09-21 | 45% | Baseline for this document. Repository cleaned of the decomp project: the `upstream` remote, the mwcc/Nix/decomp-toolkit build, the Doxygen site, the PowerPC-only runtime sources and the committed build artifacts are gone, and all documentation is in English. `tools/port_inventory.py` stops counting `Object(Matching, ...)`. Build clean, `ctest --preset host-debug` 26/26. |
| 2026-09-21 | — | (MVP milestone closed 2026-09-16; its per-change history is in `port-mvp-progress.md`.) |
