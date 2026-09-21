# Native fight flow

## Vertical goal

Reach a local two-player match, starting from `StartMeleeData`, without GameCube emulation:

1. the menu produces `StartMeleeData`;
2. the scene manager selects a VS scene and the stage;
3. `Player_InitAllPlayers` materializes the players;
4. `fighter.c` and `ground.c` run the per-frame loop;
5. PAD, DVD/HSD, renderer and host audio serve the flow.

## First entry point

`src/melee/pl/player.c:Player_InitAllPlayers` is the players' first runtime entry point. The
demo scenes in `src/melee/vi/` show the minimal sequence: fighter object initialization,
`Player_InitAllPlayers` and scene entry. The VS flow must reuse that sequence but receive a
`StartMeleeData` produced by a menu.

## Build gates

- Compiled: menu controller (`mnmain.c`), scene manager (`gmscene.c`) and the VS route
  (`gmvsmode.c`, `gmvsmelee.c`, `gmvs.c`). They still need wiring to the host facades and the
  transition needs to run.
- Running: menu input goes through the HSD Pad and the original evaluator; the output uses a
  host mapping compatible with `mn_80229624`, without loading the still-pending visual graph.
  VS configuration uses `gmMainLib`'s original storage through a host ABI that exposes no PPC
  layouts.
- Compiled: match core (`gmmain.c`/`gmmain_lib.c`), `player.c`, `fighter.c` and `ground.c`
  with no PPC assembly. The next slice must resolve their facades and wire them to a host
  entry point, not just to the static file.
- Running: the original frame scheduler. `HSD_GObjInit` comes up with the priority ceilings
  `gmScene_Init` uses (`gproc_pri_max` 0x18) and `HSD_GObj_RunProcs` runs per frame on the
  host. That loop is what `gmscene.c`, `player.c`, `ground.c` and `fighter.c` expect in order
  to run per frame.
- Running: the HSD graphics object layer. `ground.c` and `fighter.c` create GObjs and attach
  JObj/CObj/LObj, and those paths now reach the real code instead of the stand-ins that
  aborted. Scenes and models from the disc come in through `HSD_JObjLoadJoint` over
  descriptors materialized in host layout; see `docs/native_port_status.md`.
- Running: the original render path. `HSD_JObjDispAll` walks the loaded tree and the asset's
  display lists reach the host's GX recorder, which is the same path `ftdrawcommon.c` and
  `itdraw.c` take per frame.
- Running: the image from the original path. The recorder captures texgen, texture sets and
  the two rasterized channels, and the preview evaluates each draw's TEV program per fragment
  in a shader generated with the same semantics as the CPU reference (`port/src/gx/tev.cpp`),
  checked pixel by pixel by `--tev-conformance-*`. Fog, bump and EFB copies are still missing.
- Running: the HSD file API through which the game asks for assets. `HSD_ArchiveParse` and
  `HSD_ArchiveGetPublicAddress` are the host's and rebuild each symbol in 64-bit layout from
  the name's suffix; the list `gmTitle_801A1AC0` passes to `lbArchive_LoadSymbols` translates
  in full and loads through the original loaders. Details in `docs/native_port_status.md`.
- Running: boot memory and the game's loader. `gmMain`'s sequence (`HSD_InitComponent`,
  `lbMemory_8001564C`, `lbHeap_80015F3C`) and the end of the scene heap setup
  (`lbHeap_80015900`) run on the host, and `lbArchive_LoadSymbols` reads the title screen's
  file through `lbFile`, the devcom queue and the host DVD.
- Running: the game's first scene entered through its own code. The boot follows the original
  `main()`, and `gm_801A4BD4` and `gm_Scene_Title_OnEnter` assemble cameras, light, fog and
  the title's two models.
- Running: the original frame loop (`gm_801A4D34`) on the title screen, with the OS clock
  frozen: 621 frames until the scene leaves on its own, all drawn by `HSD_GObj_80390FC0` and
  copied to XFB by `video.c`'s cycle.
- Running: the title screen presented by SDL/OpenGL with the game's camera
  (`melee-pc --view-title-scene`): every frame of the original loop drawn with each draw's
  projection, viewport and scissor, at 60 Hz, with keyboard or gamepad as PAD.
- Running: the whole title mode through `runGameMode` (`melee-pc --run-modes`), through the
  host's mode and scene table: the state's preload, which keeps every preload heap, the title
  demo loading fighters, stage and effects in the background (including into ARAM), the frame
  loop and the original `onExit`. With no buttons the next mode is the opening movie; START
  leads to the menu (`GM_MENU`).
- Running: the main menu (`GM_MENU`), from the title to VS Melee through the scene manager's
  routing, with buttons pressed per frame: the menu enters, loads its models, SIS texts, the
  event table and the audio data, draws the text through the original interpreter and exits to
  `GM_VS` with DOWN, A and A. The music does not start, because the host does not deliver
  voices yet.
- Running: the `GM_VS` mode up to the match. The host's table has the mode's entry and the
  character selection (`GS_CSS`, `mncharsel.c`) and stage selection (`GS_SSS`,
  `mnstagesel.c`) scenes. With two scripted pads, both ports open as HMN, both players pick
  Fox, START leads to the SSS and the cursor picks Hyrule Temple; `VsModeData` ends up with
  stage 14 and Fox in slots 0 and 1 (test `melee-host-vs-selection-asset`). The host's VS
  mode ends at the match: results, sudden death and challenger are out (`gmvsmode.c` under
  `MELEE_HOST`).
- Next blocker: the match scene (`GS_VS`). The host's table needs `gm_Scene_Vs_OnFrame`,
  `gm_Scene_Vs_OnEnter` and `gm_Scene_Vs_OnExit` (`gmvs.c`), and the VS state first runs
  `gmVsMelee_EnterVs`, which assembles `StartMeleeData`. `gm_Scene_Vs_OnEnter` goes through
  `fn_8016E730`, through the HUD (`ifStatus`, `ifTime`) and from there to the fighters and the
  stage. The target is Fox vs. Fox on Hyrule Temple (`grshrine.c`, the smallest stage module
  unlocked without a card; Final Destination and Battlefield are locked).
- Measured on 13 September 2026: the direct calls of `fn_8016E730` and `gm_Scene_Vs_OnEnter`
  alone land in 15 modules outside the build: `cm/camera.c`, `ef/eflib.c`, `mp/mpcoll.c`,
  `it/item.c`, `it/itspawn.c`, `if/ifall.c`, `if/if_2F6E.c`, `if/iftime.c`, `if/ifstatus.c`,
  `gm/gmpause.c`, `ft/ftdevice.c`, `lb/lbrefract.c`, `lb/lb_00F9.c`, `lb/lb_0219.c` and
  `sfx/sfx_unk.c`. `efAsync_LoadSync(0)` and `(0x1F)` load particle banks, which stop in
  `psInitDataBankLocate` (32-bit in-place relocation), so particles enter early in this slice.
- First link wave, measured on 13 September 2026 with `GS_VS` placed in the host's table: 437
  undefined symbols, all in `melee-pc` (the test binary does not link the scene table). Who
  references them: `fighter.c` (218), `gmvs.c` (58), `ftkirby.c` (47), `gm_1601.c` (30),
  `dbinit.c` (21), `ground.c` (19), `plbonus.c` (16) and the rest in fighter files. Where they
  are defined, across about 75 files: the fighter core (`ftcommon.c`, `ftcoll.c`, `ftparts.c`,
  `ftdynamics.c`, `ftanim.c`, `ftlib.c`, `ft_08*.c`, `ftCo_*`), `plbonuslib.c` (42),
  `camera.c` (16), items, HUD, collision (`mplib.c`, `mpcoll.c`), `particle.c`
  (`psInitDataBank`, `psInitDataBankLoad`) and `it_804D6D38`, which has no C definition. Cases
  that need a decision before compiling: the 19 debug menu handlers (`db*.c`), which
  `dbinit.c` only reaches at debug levels; `grpstadium.c`, which `ground.c` references
  directly; and `gm_16A2.c`, `gm_17C0.c`, `gm_17EB.c` and `gmregclear.c`, which `gmvs.c`
  references for other modes.
- How to open the waves without breaking what already runs: measure with the `GS_VS` entry
  only locally and commit waves that compile without it. Since `host-sanitize` discards at
  link time whatever nothing calls (`-fsanitize-address-globals-dead-stripping` and
  `-Wl,-z,start-stop-gc`), a compiled module that nobody calls changes the link of neither
  preset, so the rest of the decomp can be compiled before being wired to the scene. The
  13 September 2026 probe shows that 741 of the 808 files outside the core already pass
  `-fsyntax-only`.
- Plan from there: (1) fix the 67 compile failures and put the rest of `src/melee` in the
  core, minus `gmscdata.c`, removing from `unported.c` the stops the new modules now define;
  (2) with `GS_VS` in the table, whatever stays undefined is SDK, MSL or asm, to be handled
  case by case; (3) run the route to the match and follow the crashes.
- Step (1) done: the 808 files compile and are in the core, along with `particle.c`,
  `generator.c`, `psappsrt.c`, `quatlib.c` and `src/MSL/float.c`. The existing routes did not
  change. Details in `docs/native_port_status.md`.
- Step (2) done: with `psdisp.c`, `psdisptev.c`, indirect texturing and `GXEnableTexOffsets`
  on the host, the match scene links with no undefined symbol.
- Step (3) in progress. With `GS_VS` in the table (locally only), the route enters the match.
  `lbRefData`, the first data it asks for, already has a translator.
- Particle loader done (details in `docs/native_port_status.md`). The host's file API
  translates `eff*DataTable` and `map_ptcl`/`map_texg` into already-located banks, in pointer
  width, and `psInitDataBankLocate` and `psInitDataBankLoad` take their tables. The 36 effect
  files and the 20 stage bank pairs on the disc translate. With `GS_VS` in the table (locally
  only), match entry passes `efAsync_LoadSync(0)` and `(0x1F)` and stops in
  `Player_80036DD8`, which asks `PdPm.dat` for `plLoadCommonData`.
- `plLoadCommonData` translator done. Entry passes through the player data and the start of
  the stage (`ftCo_800C06C0`, `mpColl_80041C78`, `Ground_801C0378`) and falls into
  `Ground_801C0754`, because Hyrule Temple's entry in `stage_datas` is null: the host linked
  the stages by weak reference (`port/src/game/host_weak_stages.h`), which does not pull
  `grshrine.c` out of the static library.
- Done: `stage_datas` links strongly. Without `host_weak_stages.h`, `melee-pc` links every
  stage in the table with no undefined symbol, `Ground_801C0754` finds Hyrule Temple and
  `grDatFiles_801C6038` reads `GrSh.dat`. Entry stops at the stage data: none of the eight
  symbols it asks for has a translation (`map_head`, `coll_data`, `grGroundParam`, `itemdata`,
  `ALDYakuAll`, `yakumono_param`, `map_plit`, `quake_model_set`), and `Ground_801C28CC`
  follows the null `stage_info.param`.
- Next blocker: those eight translators, present in the 71 `Gr*.dat` (`map_plit` and
  `quake_model_set` in 67). `map_head` (`UnkStageDat`) is the biggest: models with camera,
  lights and fog (`UnkStageDat_x8_t`), splines, shadows and animation markers. `coll_data`
  (`MapCollData`) carries collision vertices, lines and joints; `grGroundParam`
  (`GroundParam`) mixes scalars, colours and an array of `StageParam`.
- `grGroundParam` translator done (all 71 `Gr*.dat` translate). Entry passes
  `Ground_801C28CC` and, still in `Ground_801C0754`, reaches `Ground_801C5878`, which starts
  the trophy display (`tyDisplay_8031C2CC`): `Toy_803124BC` asks `TyDatai.usd` for seven
  tables (`tyInitModelTbl`, `tyInitModelDTbl`, `tyModelSortTbl`, `tyExpDifferentTbl`,
  `tyNoGetUsTbl`, `tyDisplayModelTbl`, `tyDisplayModelUsTbl`), all arrays of scalar-only
  structs terminated by -1, and stops at the first. The absence of `map_head` is not fatal up
  to that point.
- Translators for the seven trophy tables done. With them, stage entry finishes
  (`Stage_802251E8` returns) and `fn_8016E730` continues to `Item_80266F70`, where
  `it_8027870C` asks `ItCo.usd` for `itPublicData`; the record keeps in `x0`–`x14` the six
  common item tables it copies into `it_804D6D28`, `it_804D6D24`, `it_804D6D38`,
  `it_804D6D30`, `it_804D6D40` and `it_804D6D04`. That is the next translator. The missing
  stage data (`map_head`, `coll_data` and five more) has not stopped entry yet.
- Surveyed for the `itPublicData` translator (measured on `ItCo.usd` on 14 September 2026,
  with a script over the file's relocations):
  - The record has six pointers: `ItemCommonData` (`data+0x2FC`), the `Article*` tables of the
    43 common items (`0x3EAC`), the 118 character items (`0x4EC8`) and the 47 Pokémon
    (`0xBB38`), `it_804D6D40_t` and `Fighter_804D653C_t`. The counts come from
    `It_Kind_Kuriboh` (43), `It_PKind_Start` (161) and `It_Kind_Old_Kuri` (208), and the
    Pokémon table ends exactly where `it_804D6D40_t` starts. The stage items
    (`it_804A0F60`) do not come from `ItCo`.
  - All 43 common items and all 47 Pokémon are present; of the 118 character items, only 8
    pointers are non-null in the file. Each `Article` points at common attributes (`ItemAttr`,
    with bit-fields and `itECB`), its own attributes in a `void*`, hurtboxes
    (`ItHurtBoneList`: 14 common, 6 character, 4 Pokémon), states (`ItemStateDesc`: three
    animations and the script), model (`ItemModelDesc`) and dynamics (`BoneDynamicsDesc`, in
    only 3 common items).
  - The item's own attributes change layout per item. In 11 blocks there are relocated
    pointers (common items 18, 26, 27 and 29; character items 0 to 3 and 115; Pokémon 11 and
    38), so those need per-type translation; the others have no pointers, but only each item's
    struct says how wide the fields are.
  - Each state's script (`xC_script`) is read by `itanimlist.c` through the `CmdUnion` of
    `lb/types.h`, which declares each command as bit-fields over a `u32` (`opcode : 6` first).
    MWCC allocates bit-fields starting from the most significant bit and the host's GCC, on
    little-endian, from the least significant, so the stream cannot stay verbatim the way the
    animations do. It is the fighter scripts' format too: the readers are `ftaction.c`,
    `ftcolanim.c`, `grmaterial.c`, `itanimlist.c`, `lbcommand.c`, `lb_013B.c` and `lb_0219.c`,
    and the command structs run to 295 lines of bit-fields. The decision covers items and
    fighters and must come before the translator. Measured on 14 September 2026: with
    `__attribute__((scalar_storage_order("big-endian")))` on a copy of `struct unk0`, GCC 16
    reads `opcode`, `unk1` and `unk2` correctly from big-endian bytes, but `host-sanitize`
    compiles with clang 22, which ignores the attribute with a warning. The path that serves
    both presets is to convert the stream's words and declare the fields in reverse order
    under `MELEE_HOST`, or read the fields through accessors.
- Command script decision done (details in `docs/native_port_status.md`): the file API
  converts a script's words to native order in place, the host's command structs come from
  `port/tools/gen_host_command_layout.py` with the fields in reverse order, reads by cast go
  through `CMD_U8`/`CMD_U16`/`CMD_S16`, and subroutine and goto targets become relative
  distances. It covers items, fighters and colour overlay. Across the 150 state scripts of
  `ItCo.usd` the stopping rule holds: 154 stretches converted, no pointer outside a subroutine
  or goto, and all of them end in reset, return or goto before the boundary. The next step is
  the `itPublicData` translator.
- `itPublicData` translator done (details in `docs/native_port_status.md`), with the bytecode
  RObj constraints the item models use. Per-type item attributes and dynamics are left out and
  stop by name when an item that has them is created. With `GS_VS` in the table (locally
  only), entry passes the items (`Item_80266F70`, `Item_80266FCC`, `it_8026D018`) and the
  audio and falls into `Ground_801C1E94` (called by `Ground_801C0800`), which reads the
  stage's `map_head` (`UnkStageDat`, via `grDatFiles_GetArchive()->unk4`), still untranslated.
- Next blocker: the stage data, starting with `map_head`. It points at the models with camera,
  lights and fog (`UnkStageDat_x8_t`), splines, shadows and the light table that
  `LightOverrideEntry` compares by pointer against the models' `LightList`; that is why the
  translator has to deliver the same `HSD_LightDesc*` in both structures.
- `map_head` translator done (details in `docs/native_port_status.md`); 69 of the 71 stages
  translate. With `GS_VS` in the table (locally only), entry passes through all of
  `Stage_8022524C` (the game warns "use dummy CamRange" and "use dummy DeadRange", because the
  rest of the stage data does not arrive yet), through the camera and through `fn_8016E2BC`,
  and stops in `Fighter_LoadCommonData`, which asks `PlCo.dat` for `ftLoadCommonData`.
- Next blocker: `ftLoadCommonData`, a record of 23 pointers that `Fighter_LoadCommonData`
  copies into globals. Surveyed on 14 September 2026:
  - Scalars only, no relocation inside: `ftCommonData` (0x818 bytes; the byte-sized exceptions
    are `x6DC_colorsByPlayer`, `x6EC` and `x7D8`), the `ftCo_ItemThrowAttrs` rows of
    `Fighter_804D6550` (which `ftCo_ItemThrow.c` walks by arithmetic with console offsets,
    valid because the rows are floats only), `654C` (rows of 5 floats), `6548`, the scale,
    bunny, metal and gravity modifiers (`6524` to `6518`), `CrowdConfig`, `6510` (no reader)
    and the bytes of `650C` and `6508`.
  - With pointers: `ftPartsTable` (per fighter type, two `u8` arrays and the count),
    `Fighter_804D6540` (per type, pointer and count of `u8` records), the colour script tables
    `653C` and `6538` (read by `lb_800144C8`, like the items'), `6534` (the joint and the
    animation of the respawn platform), `6530` (pairs of `Vec2*` and a count, with the count
    read from a pointer slot), the two shake tables, the joints of `6514` (trophy pedestal)
    and `6504`, and `Fighter_804D64FC`, the per-character CPU AI tables, with the scripts of
    `cmdscripts`.
- `ftLoadCommonData` translator done (details in `docs/native_port_status.md`). With `GS_VS`
  in the table (locally only), entry passes the rest of `Fighter_800679B0` and the players'
  initialization and stops in `Fighter_Create` → `ftData_8008572C`, which asks `PlFx.dat` for
  `ftDataFox`: the character's own data.
- `ftDataFox` translator done (details in `docs/native_port_status.md`). With `GS_VS` in the
  table (locally only), both Foxes are created: `fn_8016E2BC` finishes, with data, costume,
  animations and starting position. That also required assembling the ARAM queue (`lbarq.c`)
  at boot and keeping, in `map_head`, the joint of each pair entry, through which
  `Ground_801C34AC` registers the players' starting points.
- Next blocker, resolved: `fn_8016E730` → `fn_801A1134` (`gmpause.c:86`) loads
  `ScGamPause_scene_data` from `GmPause.dat`, the pause menu's model, a `_scene_data`
  (`SceneDesc`). The file API now serves that type; cameras and fogs are arrays with no
  terminator, counted while the entry has a descriptor and up to the next boundary.
- Next blocker, resolved: the HUD. `ifAll_802F390C` loads `ScInfDmg_scene_data` and the models
  of `IfAll.dat`: the `_scene_models` (`ScInfCnt`, `DmgNum`, `DmgMrk`, `ScInfTim`, `ScInfPnm`,
  `ScInfStc`) and, without a suffix, `Stc_scemdls`, `Stc_rarwmdls`, `tdsce` and `lupe`, all
  NULL-terminated `DynamicModelDesc*` tables (`lupe` is a table of one, which `ifmagnify.c`
  reads through `*(DynamicModelDesc**)`). After that, `lbBgFlash_Init` asks `LbBf.dat` for
  `lbBgFlashColAnimData`, the colour animations in the items' format.
- Next blocker: `fn_8016E730` finishes. In `gm_Scene_Vs_OnEnter`, `ifStatus_802F665C` →
  `ifStatus_802F5EC0` (`ifstatus.c:712`) finds the damage digits with `ifStatus_802F6194`,
  which receives a JObj cast to a GObj and walks through `next_gx` and `next`: on the console
  those fields land where the JObj keeps `child` and `next`, and on the host pointer width
  separates them. Fixed under `MELEE_HOST`, walking through the JObj itself.
- With that fix, `gm_Scene_Vs_OnEnter` finishes and the scene enters the frame loop
  (`gm_801A4D34`). The first frame runs the procs and, at draw time,
  `ftDrawCommon_80080E18` → `ftLib_80086A8C` → `Camera_80030CFC` projects a point of the
  fighter's camera box outside ±50,000 (assert in `lbvector.c:383`). The cause was the camera:
  `Camera_ApplyQuake` reads the camera description (`cm_803BCB64`) through the layout of
  statics laid out in sequence from `cm_803BCB18`, which does not hold on the host; the shake
  translation came out NaN and contaminated eye and interest. Fixed under `MELEE_HOST`,
  reading `cm_803BCB64` directly.
- With the camera fixed the first frame draws, and the following frames' procs found two more
  points that depend on the 32-bit layout: the particle generator lists keep addresses in
  `u32` (`hsd_804D78F8` and `hsd_804D78F4`), and the HUD reads `IfDamageState` through another
  struct with fillers at the console's offsets (`UnkX`). Both fixed under `MELEE_HOST`.
- After them the game aborted in glibc's `malloc`, a sign of a heap corrupted earlier. The
  same route under ASan first found two more views of statics in sequence
  (`lbRefract_800222A4` and `ftmaterial.c`), also fixed, and then the write that corrupted the
  heap: the collision segments of `mpIsland_8005A728` were allocated with the console's size
  (0x2C), smaller than the struct on the host. Fixed under `MELEE_HOST` with `sizeof`.
- The following ASan rounds found two more points in fighter creation: `ft_800852B0` clears
  caches by the distance between globals on the console (`ftData_Table_Unk0`,
  `ftData_UnkIntPairs`, `ft_8045993C`), and each fighter's light (`ftCo_09F4.c`) uses five
  floats in place of an `HSD_WObjDesc`. Both fixed under `MELEE_HOST`.
- Then, the symbol lists of `lbarchive.c`'s loaders end in a literal `0`, which on x86-64
  reaches `va_arg` with its high half undefined when it goes on the stack. On the host,
  `lbarchive.h`'s macros gather the arguments into a pointer array, where the `0` becomes
  null.
- With that, the route under ASan passes through the whole scene entry and reaches the frames'
  procs, where it found `lbVector_WorldToScreen` passing a 3x4 matrix to `MTXPerspective`
  (which writes 4x4) and `ifstatus.c` converting 255 as a float directly to `s8`. Both fixed
  under `MELEE_HOST`.
- The next round already had the fighters in `Fighter_procUpdate` and found the `HSD_psAppSRT`
  pool created with the console's size (0xA4, 184 bytes on the host). Fixed under
  `MELEE_HOST` with `sizeof`.
- Next blocker, resolved: with that batch, the match route under ASan (`GS_VS` locally only)
  ran the fighters' procs up to `Fighter_8006A360` → `ftCo_RebirthWait_Anim` →
  `ftCo_8008A7A8` → `ftAnim_8006EBA4` → `ftAction_80073240`, and `lbcommand.c:57`'s
  `Command_04` read an invalid address (SEGV). The command should not even have run:
  `ftAction_80073240` reads the opcode through `gmScriptEventDefault` (`ft/types.h`),
  bit-fields outside `lb/types.h` that the layout generator does not cover, and the host was
  taking the opcode from the word's low six bits. Fixed under `MELEE_HOST` with the fields
  reversed.
- With the right opcode, the match runs the frame loop without error: 600 match frames in
  ~60 s in `host-debug` and 400 s under ASan, without ending on its own.
- Next blocker, resolved with no host code: the route had no end, and the game itself provides
  the exit. The HUD releases pause on frame 655; START pauses and L+R+A+START ends the match
  as it does in a tournament (`fn_8016CF4C` → `gm_801A4B60`). `gm_Scene_Vs_OnExit` runs, the
  host's VS mode returns to the CSS and B held leads to the menu. `GS_VS` entered the host's
  table and the route became the `melee-host-vs-match-asset` test (a 175-frame match).
- Done: the match's image. `--run-modes` writes a requested frame (`FRAME:BMP=file`) through
  the hidden presenter. Frame 640 of the test's route shows the stage, the "Go!", the timer,
  P1 and P2 and the damage panels; 695 shows the pause menu. Details in
  `docs/native_port_status.md`.
- Resolved: the start banner came out as white quads because the texture received another
  draw's palette. The frame is read after the last draw, and the palette was requested by name
  (`GX_TLUT0`); the GX recorder now stores each draw's palette. "Ready" and the countdown
  appear.
- Resolved: the camera was descending because the fighters were falling. Without `coll_data`,
  `mpLibLoad` used an empty map; measured under gdb, both fell from y 22 to -201 between
  frames 340 and 440 of the mode, with the camera behind. With the `coll_data` translator they
  land and the camera stays on the stage. The landing went through two host points, in
  `lbanim.c` and `lb_020A.c`. Details in `docs/native_port_status.md`.
- Resolved: the fighters were not drawn because `fighter.c` sets the draw flag by writing
  `byte = 1` into a union of `u8` with bit-fields (`UnkFlagStruct`), and MWCC and GCC allocate
  those bits in opposite order. The host declares the bits reversed.
- Resolved: the huge planes were vertices without the camera's translation. The host's GX
  recorder stored the normal matrix in the position matrix's rows, and HSD loads the inverse
  transpose, with no translation, right after the position of every lit PObj. With normal
  matrices kept separately, as in GX, both Foxes appear at the right size on the stage.
- Measured: the black band is the fighters' projected shadow. The shadow pass copies what it
  drew into a 4-bit texture with `GXCopyTex`, which the host only records, and the texture
  ends up holding the contents of an unzeroed allocation. Filling the copy with white, in a
  local experiment, made the band go away.
- Next blocker: producing the EFB copies on the host. `GXCopyTex` must rasterize what was
  drawn into the copy's target since the start of the pass and write it in the requested
  format. For the shadow, that is a white background and a grey silhouette, untextured, in
  orthographic projection, written as `GX_CTF_R4`; `lbrefract.c` and `tobj.c` also copy.
  After that: see the fighters respond to the stick and the buttons, check the SSS (it came
  out almost entirely blue before the palette fix and has not been looked at since) and the
  missing stage data (`itemdata`, `ALDYakuAll`, `yakumono_param`, `map_plit`,
  `quake_model_set`). The results screen (`onExitVs`) is still out.
- Done in commits from 14 September 2026: the shadow's I4 copy, the stick displacing P1 and
  the A button taking P1 out of `Wait`, all checked through the test's route.
- Resolved: holding the stick locked the match at frame 670 with the process growing without
  bound. The run script was spinning because the animation did not repeat: each action's flags
  are written whole into `fp->x594_s32` and read through bit-fields that MWCC counts from the
  highest bit. The host declares those bits from the least significant. Details in
  `docs/native_port_status.md`.
- Done: the results screen, through the game's code. The test's route goes through CSS, SSS,
  match, results (406 frames) and back to the CSS, and B leads to the menu. On the way out,
  the game went to the prize notice for a trophy awarded with garbage: `fn_80166A8C`, the
  float-to-`u16` conversion by the SDK's fast cast, exists only in assembly and wrote nothing
  on the host. Fixed; details in `docs/native_port_status.md`.
- Done: Fox's neutral special. B stopped by name at the blaster, whose own attributes the host
  did not translate; the `ftDataFox` translator now translates Fox's three items, which in
  `PlFx.dat` hold only floats.
- Measured: the KO runs through the game. P1 jumps over the left-hand wall (which at runtime
  sits at 0.9 times the file's coordinates, transformed by the map's joint), falls, enters
  `DeadDown` and respawns on the platform. Next step: the KO in the test's route and a match
  played to the end.
- Done: a match played to the end. On the CSS the rules menu, which stopped at a JObj whose
  address `mn_80231634` truncated, sets the rules to 1 stock; P1 falls off the stage, the
  match ends by elimination and the results of a completed match return to the CSS. Test
  `melee-host-vs-stock-match-asset`; details in `docs/native_port_status.md`. Next step: look
  at that route's frames (the "GAME!", the winner, the SSS) and measure the pace in an
  optimized build, because `host-debug` sits at 4 to 5 frames per second.
- Done: the stock route's image. In BMP, the rules menu, the SSS, the match and the "Game!"
  come out right; on the results the title said "NO CONTEST", the portraits were noise and the
  HUD showed a different emblem, because `gm_80168B34`, which gives the frame of those texture
  animations, returned stack garbage on the host (the console keeps `base` in `ckind`'s
  register). Fixed under `MELEE_HOST`, along with `gm_80168BF8`, which ended without a
  `return`. The panels' portraits are still black: they are EFB copies (`gm_1798.c`). The
  scripted routes freeze the clock at a fixed instant and gain the canonical `TRACE=` trace,
  identical between runs and between `host-debug` and an `-O2` build. Next step: audio (AX
  voices), and the results' EFB copies.
- How the results screen works on the host. With the console's state table and `GS_RESULTS` in
  the scene table, a cancelled match enters `gm_Scene_Results_OnEnter` through the game itself
  (`fn_8016CF4C` with `OUTCOME_NO_CONTEST`), and the scene runs its frames: in the
  `fn_80179350` proc, `x1` goes from 0 to 3 over about 120 frames, across two pages. In state
  3 (`fn_80178050`) each human player has to press START on their own port; a disconnected
  port counts as ready. Entry went through three reads of statics in sequence (camera,
  `ResultsDisplayLayout` and `ftMapping_list`), all fixed; details in
  `docs/native_port_status.md`. In a match that was not cancelled, the demo Fox may create the
  blaster (item 74), whose own attributes the host does not translate yet.
- Surveyed for the `ftData*` translator (measured on `PlFx.dat` and the 58 character files on
  14 September 2026):
  - `ftDataFox` has 24 fields, all filled except `x28`. Scalars only: the common attributes
    (`ftCo_DatAttrs`, 0x184 bytes, 82 members, only the last one a `u8`), `x24` (a `WaitStruct`
    of integers, which `ftCo_8008A7A8` receives), `x34`, `x38`, the camera (`x3C`), `itPickup`
    (`x40`), `x50` (a `Vec2` that `ftchangeparam.c` copies) and the edge limits of `x44` (with
    six `s16`).
  - The character's own attributes (`x4`) change layout per character. Fox's
    (`ftFox_DatAttrs`, 0xD4 bytes) are 44 scalars and a `ReflectDesc` ending in a `u8`.
  - The action tables (`xC` and `x14`) have 327 and 14 `Fighter_WaitAnimData` entries of 0x18
    bytes: name, offset and size of the animation in `PlFxAJ.dat`, command script and flags.
    `x10` and `x18` are `u8` pairs per action. Across the 58 character files, the 10,091
    scripts obey the command conversion rule, except for 52 pointers in the 14 Kirby copy
    files (`ftDataKirbyCopy*`), which have another format.
  - Parts (`x8`): `FtPartsDesc`, with `vis_table` in rows per costume of four
    `{count, pointer}` records (format still to be confirmed), and `ftData_x8_x8`, with a
    table of `u16` pairs. `x1C` is five `ftData_x1C` with a byte array of parts and an array
    of animations; `x20` is an array in which only index 2 is a joint pointer (the others are
    0 and 8); `x5C` is the metal joint.
  - With pointers and their own format: the dynamics (`x2C`, with `ArticleDynamicBones` and
    `FigaTree`), the hurtboxes (`x30`, `ftHurtboxInit`), the character's items (`x48_items`,
    four `Article` like those of `itPublicData`), the SFX (`x4C`, `FtSFX` with `FtSFXArr`) and
    the IK (`x58`, `ftData_x58_t`, `u8` indices and `f32` lengths).
  - `x54` is declared `int`, but on disc it is a relocated pointer and `ftCo_09F7.c` reads it
    as `int*`. On the host the field needs a pointer type: as an `int` the address would be
    truncated and `x58` would land at the wrong offset.
  - After `ftData_8008572C`, `Fighter_Create` loads the costume (`ftData_80085820`) and the
    animations. `ftData_80085A14` writes into each action's `x14` the address of the animation
    inside `PlFxAJ.dat`, which lives in ARAM, and `ftData_80085E50` copies the requested
    animation with a synchronous ARAM read (`lbArq_80014BD0`) before parsing it. When another
    fighter already has the same animation, the copy comes from them and goes through
    `lbArchiveRelocate`.
- What the effects blocker was: `efAsync_LoadSync(0)` loads `EfCoData.dat` and asks for
  `effCommonDataTable`, whose structure points at the particles' command and texture banks
  (and the effects' models). On the console `psInitDataBankLocate` relocates the banks in
  place with 32-bit addresses and `psInitDataBankLoad` builds tables of pointers into them
  (`psCmdListArray`, `ptclref_804D0E5C`, `psTexGroupArray`, `psNumCmdList`). The host needs a
  particle loader that builds those tables in pointer width, keeping the command streams
  verbatim like the animations and the display lists, and translates the texture groups. The
  three points to port together are the effects table, the banks and `particle.c`'s command
  interpreter, which reads those streams.
- What has already been surveyed for that loader:
  - The table (`effCommonDataTable` at `data+0` of `EfCoData.dat`) starts with two pointers,
    the command bank and the texture bank, which `efAsync_OnLoad` passes to
    `psInitDataBankLocate`. From `+0x8` comes an array of `EF_EffectDesc` (an `f32` duration
    and a `StaticModelDesc`, 0x14 bytes on the console), which `efLib_Create` indexes by
    `gfx_id % 1000` with no bound; the array's size is not stored and has to be deduced, for
    instance from the first relocation target after the table.
  - `HSD_PSCmdList` has no pointers: a 0x3C-byte header (u16, u32 and floats) followed by the
    embedded command bytes. On the host the header can be converted and the bytes copied as
    they are; each list's end comes from the start of the next one or from the end of the
    bank.
  - `HSD_PSTexGroup` has scalars (`num`, `fmt`, `tlutfmt`, `width`, `height`, `palnum`,
    `palflag`) and an array of pointers to images and palettes at `+0x18`, which doubles in
    width on the host. The texels and palettes stay verbatim, because the GX decoders already
    read them big-endian.
  - `psInitDataBankLoad` stores the texture count with `((s32*) psFormGroupArray)[bank]`,
    which on the host writes into half of a pointer; the host path needs somewhere to keep
    that count.
  - The interpreter assembles 16-bit operands byte by byte, big-endian, but `psReadFloat`
    copies the four bytes into `hsd_804D78D0` and reads it as an `f32`, which reverses the
    order on the host: it needs a fix under `MELEE_HOST`.
  - The effects pass a null `formBank` and `ref`; the forms (`formTable`) and the `*(u32*)` /
    `*(f32*)` reads of them in `psdisp.c` only matter once a form bank exists.
- The paired-single math, the GX state subset and the VI layer this layer consumes are already
  done and tested. The frame loop already has both halves it needed: `HSD_GObj_RunProcs` for
  the simulation and the VI retrace for presentation, both deterministic and sleep-free.
- The OS heap already runs: the original allocator hands out blocks at real 64-bit addresses,
  and `initialize.c` came in with the graphics object layer. Details in
  `docs/native_port_status.md`.
- Implement the GX, audio and DVD facades that come up during this slice, only as they are
  needed.

## Acceptance criterion

Two host controllers select fighters, enter a stage scene, the fighters receive input on
deterministic ticks, and the scene is presented by the SDL/OpenGL backend.
