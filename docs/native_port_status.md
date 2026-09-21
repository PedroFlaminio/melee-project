# Native port status

Updated 21 September 2026.

The completed foundations are listed in full below. The milestone log that follows condenses
what were previously ~200 long narrative entries; the full wording of each is in this file's
git history, and the same ground is covered in more detail in
[`port-mvp-progress.md`](port-mvp-progress.md) and [`fight_flow_port.md`](fight_flow_port.md).
The **In progress**, **Next gates** and **Current limitations** sections describe the live
state and are complete.

## Completed — foundations

- [x] Project plan and acceptance criteria.
- [x] CMake/Ninja build, separate from the matching build.
- [x] Initial C ABI for the host layer.
- [x] Fixed-width host types.
- [x] Monotonic clock and an initial Dolphin time facade.
- [x] Per-thread facade for Dolphin interrupt critical sections.
- [x] 32-byte aligned host heap wired to `HSD_MemAlloc`/`HSD_Free`.
- [x] Original HSD object and list allocator running with native 64-bit addresses.
- [x] Idempotent headless baselib bootstrap with the original list, vector, matrix, AObj/FObj
  pools and ID table.
- [x] Original FObj animation channels with loading, chaining, allocator and portable Hermite
  interpolation.
- [x] Original AObj controllers with FObj ownership, flags and animation requests; unsafe JObj
  references are rejected on the host.
- [x] Portable C compatibility for the paired-single matrix/vector operations HSD uses, in
  place of the PowerPC assembly.
- [x] Deterministic scheduler ordered by tick and sequence.
- [x] Deterministic input snapshot for four controllers.
- [x] `PADRead` adapter for the host snapshot (no window backend).
- [x] Original `PADClamp` calibration compiled and tested on the host.
- [x] x86-64 diagnostic executable.
- [x] Automated inventory of portability blockers.
- [x] ISO/GCM and FST parser with bounds and path validation.
- [x] `GALE01` verification by `main.dol`'s SHA-1.
- [x] Atomic extraction with a manifest and DVD entry numbers.
- [x] Runtime DVD index and resource reading through the host C ABI.
- [x] Synchronous DVD facade (`DVDOpen`/`DVDFastOpen`/`DVDReadPrio`) over extracted assets.
- [x] Asynchronous DVD read and seek delivered in order by the host scheduler.
- [x] Original `HSD_DevComRequest` queue wired to the host DVD in an integration test.
- [x] Offset-based big-endian HSD parser that does not truncate pointers.
- [x] Enumeration of HSD public roots for diagnosing real assets.
- [x] 64-bit-safe HSD runtime graph: internal references preserved as validated offsets, with
  no in-place relocation.
- [x] HSD runtime readers for big-endian fields and relocated references.
- [x] First typed HSD schema: `dbLoadCommonData` and its three tables.
- [x] Safe HSD string reading; the first real entries of `DbCo.dat`'s three tables decoded.
- [x] Full materialization of `DbCo.dat`'s name tables with validated bounds and relocations.
- [x] Real `DbCo.dat` loaded on x86-64: root `dbLoadCommonData` and 854 internal references
  validated.
- [x] Host GX recorder for vertex commands with no MMIO access.
- [x] First `GX_TRIANGLES` assembled into runtime vertices in the headless backend.
- [x] Headless conversion of `GX_TRIANGLESTRIP`, `GX_TRIANGLEFAN` and `GX_QUADS` into triangle
  lists.
- [x] Capture of direct GX vertices with position, normal, RGBA and UV.
- [x] GX VCD/VAT state for eight formats, strided arrays and 8/16-bit indices for position,
  normal, colour and the main UV.
- [x] Big-endian decoding of U8/S8/U16/S16/F32 components with fixed point, and RGB565/RGB8/
  RGBX8/RGBA4/RGBA6/RGBA8 colours.
- [x] Host `GXCallDisplayList` interprets PObj commands, VAT formats, direct/indexed attributes
  and padding, validated against truncated streams.
- [x] First safe Scene/Joint/DObj/PObj graphics schema, keeping the graph in validated 32-bit
  offsets.
- [x] Traversal of every drawable PObj in an HSD scene, including independent display lists and
  per-object vertex descriptors.
- [x] First real PObj executed: `GmPause.dat` produces 48 triangles in the headless backend
  with no display list errors.
- [x] The headless mesh preserves position, normal, colour and UV per vertex of each triangle,
  including when the attributes arrive after the position in the GX stream.
- [x] SRT transforms of the JObj tree are accumulated and applied to each decoded PObj in the
  headless backend.
- [x] GX arrays coming from HSD carry the data segment's bounds; indices and strides beyond the
  range are rejected without being read.
- [x] GX NBT/NBT3 streams preserve normal, tangent and binormal per vertex in the headless
  backend, for direct and indexed attributes.
- [x] Affine transforms apply a normalized inverse transpose to normals and a normalized
  directional transform to tangents/binormals.
- [x] JObjs with a matrix independent of the parent respect `JOBJ_MTX_INDEP_PARENT` during the
  headless traversal.
- [x] SDL3/OpenGL graphics preview for HSD PObjs: resizable window, orbit camera, depth, UVs, a
  checker fallback texture and per-vertex colours through
  `melee-pc --view-pobj FILE SYMBOL`.
- [x] Initial MObj schema: render mode, present TObj and the HSD diffuse/alpha material
  modulate the vertices the backend presents.
- [x] Safe GX image decoders for `I4`, `I8`, `IA4`, `IA8`, `RGB565`, `RGB5A3`, `RGBA8` and
  `CMPR`, including GameCube tiling, validated against the textures `GmPause.dat` references.
- [x] Texture cache by HSD offset and per-PObj association: the SDL/OpenGL preview uploads the
  supported images and uses the correct texture on each triangle.
- [x] First GX state mapping: `RENDER_XLU` per PObj controls alpha blending and depth buffer
  writes; the basic TEV case uses texture × vertex colour.
- [x] The renderer runs separate opaque and translucent passes to preserve opaque objects'
  depth before alpha blending.
- [x] First subset of GX materials completed for the formats present in `GmPause.dat`
  (I4/IA4), from HSD through to the SDL/OpenGL backend.
- [x] GX palettized textures `C4`, `C8` and `C14X2`: TLUT decoding (`IA8`, `RGB565`,
  `RGB5A3`), the HSD schema for `HSD_TlutDesc` and palette association in the SDL/OpenGL
  preview, with validated bounds.
- [x] SDL input in the preview: keyboard or the first SDL Gamepad feeds pad 0 of the host
  snapshot every tick, consumable by the original `PADRead`; hot-plug switches safely between
  gamepad and keyboard.
- [x] SDL/SDL_GameController backend feeding host input.
- [x] Pure adapter from a GameCube snapshot to the navigation bits `Menu_GetAllInputs` uses,
  with buttons, analog stick and triggers covered by tests.
- [x] Synthetic disc and HSD tests.
- [x] First original modules compiled natively: RNG, time, vectors, controller, memory,
  objalloc/list and baselib's `devcom` queue.
- [x] Vertical match core compiled natively: match configuration (`gmmain.c`/`gmmain_lib.c`),
  players (`player.c`), fighters (`fighter.c`) and terrain (`ground.c`).
- [x] Menu and VS route compiled natively: `mnmain.c`, `gmscene.c`, `gmvsmode.c`,
  `gmvsmelee.c` and `gmvs.c`.
- [x] C ABI facade for the original VS rules storage: reading, writing and restoring
  `gmMainLib_DefaultGameRules`' defaults, tested without exposing PPC structs to the host C++.
- [x] Preparation of the original `StartMeleeData` for a local two-player VS match, with the
  current rules, characters and stage validated through a safe host ABI.
- [x] Full initialization of the six original player slots: base state, stale move table,
  attack statistics and bonus state; the two prepared players are materialized into the slots
  before Fighter objects are created.
- [x] Menu input path running: host snapshot → HSD Pad → the original
  `gm_EvaluateAllControllerInputs` → an event mapping compatible with `mn_80229624`, with
  A/confirm covered by an integration test.
- [x] 64-bit host compatibility for PPC layout asserts, `OSPanic` diagnostics, time and the
  shared Dolphin declarations, without changing the matching path.
- [x] Original HSD class system (`class.c`, `object.c`, `hash.c`) compiled natively, with the
  size-class block allocator running on the 64-bit host heap.
- [x] HSD scene object runtime compiled natively: `gobj.c`, `gobjinit.c`, `gobjproc.c`,
  `gobjplink.c`, `gobjgxlink.c`, `gobjobject.c` and `gobjuserdata.c`. The four built-in classes
  (camera, light, joint and fog) are registered in the original order.
- [x] The original frame scheduler running on the host: `HSD_GObj_RunProcs` walks the processes
  by priority, respects the paused p_link mask `HSD_GObjLibInitData.unk_2` points at, and
  applies the deferred removal when a process frees its own GObj.
- [x] `melee_host_scene_runtime_*` C ABI facade with opaque 32-bit handles for scene objects,
  without exposing PPC-layout structs to the host.
- [x] `melee-pc --diagnose-scene-runtime [FRAMES]` diagnostic running the original scheduler
  for N frames.
- [x] Complete portable paired-single set for the graphics layer: `PSMTXInverse`,
  `PSMTXInvXpose`, `PSMTXTranspose`, `PSMTXMultVec`, `PSMTXMultVecSR`, `PSMTXMultVecArray`,
  `PSMTXRotAxisRad` and `PSVECAdd`, all safe for an aliased destination as the original code
  requires.
- [x] The original `spline.c` compiled natively, replacing the hand-written `hsd_spline.c`
  facade. `splGetSplinePoint` and `splArcLengthPoint` now come from the decompiled code.
- [x] Portable projection and view matrices: `MTXFrustum`, `MTXPerspective`, `MTXOrtho`,
  `C_MTXLookAt` and `MTXRotRad`.
- [x] Host GX state subset in `port/src/gx/state_recorder.cpp`: the host models the state the
  GX API describes, not Flipper's registers. It covers depth, blend, alpha compare, culling,
  scissor, viewport, projection, matrix memory, complete TEV stages (inputs, operations,
  constants, swap and S10 registers), texgen, lighting channels with ambient and material
  colours, texture and TLUT objects, lights with angular and distance attenuation, fog and EFB
  copies.
- [x] The SDK's opaque objects (`GXTexObj`, `GXTlutObj`, `GXLightObj`) keep their contents
  inside the blob itself, with 64-bit pointers split across two 32-bit fields instead of
  truncated, and moved by explicit copy so as not to depend on type punning.
- [x] GX display copy and draw synchronization: `GXSetDispCopySrc/Dst`, `GXSetDispCopyYScale`
  with a line count, gamma, clamp, clear colour, copy filter with sampling pattern and weights,
  `GXCopyDisp`, `GXSetDrawDone`, `GXWaitDrawDone`, `GXDrawDone` and `GXSetDrawDoneCallback`.
  The host has no asynchronous graphics processor, so the draw-done fence stays pending until
  someone drains it, and `melee_host_gx_drain_draw_done` lets the frame loop deliver the
  callback at a deterministic point.
- [x] `GXNtsc480IntDf`, the NTSC 480i render mode with deflicker that `gmMain` installs, with
  the SDK's values.
- [x] The original `OSAlloc` heap running on host memory. The SDK's `OSAlloc.c` and `OSArena.c`
  compile natively, with addresses loaded at pointer width under `MELEE_HOST`. The `ROUND`,
  `TRUNC` and `OFFSET` macros of `os.h` got the same protection.
- [x] `melee_host_os_heap_*` facade that creates the host arena and a heap on top of it,
  repeating the boot's sequence. The heap hands out 32-byte-aligned blocks at real addresses
  above 4 GB, which the original 32-bit arithmetic would have truncated.
- [x] `OSGetPhysicalMemSize` and `OSGetConsoleSimulatedMemSize` report the console's 24 MB, not
  the host's RAM: the game uses them to decide how much to consume.
- [x] HSD graphics object layer compiled and linked natively: `cobj.c`, `lobj.c`, `jobj.c`,
  `dobj.c`, `mobj.c`, `tobj.c`, `pobj.c`, `robj.c`, `wobj.c`, `fog.c`, `displayfunc.c` and
  `shadow.c`, along with `state.c`, `tev.c`, `texp.c`, `texpdag.c`, `bytecode.c`, `perf.c`,
  `util.c`, `video.c` and `initialize.c`. The aborting stand-ins were removed: a GObj that owns
  a JObj is now freed by the class's real destructor, which is the path `ground.c` and
  `fighter.c` take.
- [x] `MTXLightFrustum`, `MTXLightPerspective` and `MTXLightOrtho`, the projections that map
  direct eye space to a texture coordinate, used by the projected shadows.
- [x] `GXGetTexBufferSize` with each GX format's block footer and the sum of the mipmap levels.
- [x] Host VI layer in `port/src/video/vi.cpp`: retrace counter, field parity, pre- and
  post-retrace callbacks in the original order, and shadow register latching on the retrace
  after `VIFlush`. Nothing sleeps and nothing reads wall-clock time: time advances when the
  game blocks in `VIWaitForRetrace` or when the host loop calls
  `melee_host_video_advance_retrace`.

## Completed — milestone log

Condensed. Each milestone's full description, with the measurements taken at the time, is in
this file's git history.

### HSD descriptors and the original render path

- [x] HSD descriptor materialization into host layout: the file keeps 32-bit offsets, the host
  rebuilds each descriptor at pointer width and hands it to the original loaders. GX payloads
  (display lists, vertex arrays, image and palette data, keyframes) stay verbatim, because the
  consumers read them big-endian. The descriptors come out of a single contiguous block, live
  as long as the scene handle, and a pointer field the file did not relocate must read as zero.
- [x] A real HSD scene loaded from disc through the original graphics object layer, with the
  `melee_host_scene_graphics_*` C ABI facade and the `--load-scene` and `--load-joint`
  diagnostics; cross-checked against the separate reading schema.
- [x] A real scene drawn through the original render path (`--render-scene`, `--render-joint`),
  with captured vertices passing through the position matrix GX had in force, an array region
  declared to the host, rejected indices counted rather than ignored, and the video mode
  installed before the render.
- [x] `GX_VA_NBT` consumed at the normal's position inside the display list, which is where the
  hardware puts it.
- [x] Per-draw pixel state and TEV program captured, with deduplication that ignores the
  components the program does not read; a survey of the real states on disc found 24 distinct
  states across 647 symbols, at most eight per symbol.
- [x] TEV evaluated per fragment, with a CPU reference tested against the hardware's formula and
  GLSL checked against it on the GPU (`--tev-conformance-scene`, `--tev-conformance-joint`).
- [x] Matrix memory with 128 rows; GX reset mirrors `GXInit`. Stand-in lights in the facade's
  render, and the SDL/OpenGL preview rewritten over shaders, with
  `MELEE_HOST_SCREENSHOT=file.bmp` rendering one frame off-screen.
- [x] Animation materialization and real animation running through the original code, including
  the three animation tables a scene model carries in `DynamicModelDesc`. A disc-wide sweep
  gives **288 of 288** model/animation pairs materializing and running with no errors.
- [x] Concatenated HSD file reader; `FigaTree` and `FigaTrack` materialized; `lbanim.c` compiled
  natively; character skeletons animating. A character animation sweep gives **6,245 of 6,245**
  animations across 33 characters attaching and moving joints with no errors.
- [x] Host evaluation of the GX lighting channels, and a deterministic frame boundary for the
  scene runtime.

### File API, boot and the title screen

- [x] The sysdolphin HSD file API implemented by the host
  (`port/src/assets/hsd_host_archive.cpp`): `HSD_ArchiveParse`,
  `HSD_ArchiveGetPublicAddress`, `HSD_ArchiveGetExtern` and `HSD_ArchiveLocateExtern`, with the
  original signatures, in place of `archive.c`. The file's identity is the buffer, not the
  `HSD_Archive`. New descriptors in the materializer: public camera, light table, fog and
  sprite.
- [x] A disc-wide sweep through the file API (`--sweep-archives`): of the 894 `.dat`/`.usd`
  files, 861 are a single HSD file.
- [x] The boot's memory sequence running on the host (`port/src/game/boot_memory.c`), in
  `gmMain`'s order, with `lbmemory.c`, `lbheap.c`, `lbfile.c`, `lblanguage.c` and
  `lbarchive.c` compiled natively; host ARAM as a stack like the SDK's; and `lb_800195D0`, the
  disc wait, as a host facade that steps the scheduler.
- [x] The title screen loaded through the game's own loader (`--boot-title-archive`) and then
  entered through the game's code (`--boot-title-scene`): `gm_801A4BD4` and
  `gm_Scene_Title_OnEnter` after `gmMain`'s boot. The boot follows the original `main()` to the
  end.
- [x] OS alarms on the host (`port/src/os/alarm.c`), `VA_END_PTR` in `src/Runtime/platform.h`,
  and data that lives inside the DOL read from the user's extracted `main.dol` by the address
  the game uses (`port/src/assets/dol_image.cpp`).
- [x] The original title screen frame loop, with a freezable OS clock
  (`melee_host_os_time_freeze`), the pad and video initialization from `main()`, a GX frame
  sink, and named stops in `unported.c` for what the host does not run.
- [x] The title loop presented through SDL/OpenGL: per-draw projection, viewport and scissor
  captured, `port/src/gx/view.{hpp,cpp}` mapping GX projection to GL clip, and
  `melee::render::FramePresenter` drawing a frame's capture in the game's order.
- [x] The whole title mode through the game's code, with the memory card absent, the title
  demo's preload, and a host ARQ with real transfers and deferred delivery.
- [x] The main menu through the game's code, with SIS text tables translated, the SIS text
  interpreter ported under `MELEE_HOST`, and C translators for data only C headers describe
  (`sqEventInitDataLevelTbl`, `lbAudioLoadData`, `MemCardIconData`, `MemSnapIconData`).

### VS selection and the rest of the decomp

- [x] Debug/sanitizer presets and a cross-platform workflow.
- [x] Character selection (`GS_CSS`) and stage selection (`GS_SSS`) running through the game's
  code inside `GM_VS`, with two-pad input; the SIS text pool sized for the host; a missing
  scene stopping by name; a per-scene report; the input script with stick and four ports
  (`FRAME[-LAST]:INPUT[+INPUT][@PORT]`); and `melee_host_vs_selection_get`.
- [x] `host-sanitize` back, then with no files excluded from instrumentation, which found a
  stack overflow in the menu, out-of-array reads and undefined shifts.
- [x] The rest of the decomp in the core: all 808 `.c` of `src/melee` compile and link, plus
  the baselib particle system. The game's own trigonometry replaces glibc's `atan2f`, `acosf`
  and `asinf`.
- [x] The match scene links with no undefined symbol.

### The match: translators and 32-bit layout fixes

Each translator below unblocked the next step of match entry, in this order:

- [x] `lbRefData`, then the host particle loader (`eff*DataTable`, `map_ptcl`/`map_texg` as
  already-located banks, spline joints), then `plLoadCommonData`.
- [x] The stage table linking strongly, then `grGroundParam` (71 of 71 `Gr*.dat`), the seven
  trophy tables of `TyDatai.usd`, and command scripts on the host (native word order, reversed
  bit-fields generated from `lb/types.h`, relative subroutine and goto targets).
- [x] `itPublicData` (98 `Article` with attributes, hurtboxes, states, models and the colour
  table, plus the bytecode RObj constraints), then `map_head` (69 of 71 stages), then
  `ftLoadCommonData` (23 common fighter tables), then `ftDataFox`, with fighter animations
  copied out of ARAM.
- [x] `_scene_data` and `_scene_models` in the file API, and `lbBgFlashColAnimData`.
- [x] A run of 32-bit layout defects found by running the route, latterly under ASan:
  `ifStatus_802F6194` walking a JObj cast to a GObj; `Camera_ApplyQuake` reading sequential
  statics and writing NaN; particle generator lists keeping addresses in `u32`; the `UnkX` view
  of `IfDamageState`; structs read over sequential statics in `lbrefract.c`, `ftmaterial.c` and
  `ft_800852B0`; `mpIsland_8005A728` allocating collision segments at the console's size; the
  fighter light using five floats in place of an `HSD_WObjDesc`; the `HSD_psAppSRT` pool at the
  console's size; `lbarchive.c`'s variadic lists ending in a literal `0`; and the `Command_04`
  opcode taken from the wrong bits of `gmScriptEventDefault`.
- [x] `GS_VS` in the host table: Fox vs. Fox on Hyrule Temple runs through the game's code, and
  `melee-host-vs-match-asset` replaces `melee-host-vs-selection-asset`.

### The image and the playable match

- [x] Frame images from any route (`FRAME:BMP=file`), and the white-quad bug fixed: the GX
  recorder now stores each draw's palette, and the indexed texture decoder stopped passing the
  blocks' padding texels through the palette.
- [x] `coll_data` translated (71 of 71 stages), so the fighters land instead of falling and the
  camera stays on the stage.
- [x] The fighters drawn at all: `fighter.c` sets the draw flag by writing `byte = 1` into the
  `UnkFlagStruct` union, whose bits MWCC and GCC allocate in opposite order.
- [x] The huge planes fixed: normal matrices kept separately from position matrices, as in GX,
  so both Foxes appear at the right size.
- [x] The black band identified as the projected shadow, and `GXCopyTex` made to materialize
  its I4 copy by rasterizing the captured pass.
- [x] Holding the stick no longer exhausts memory: the run animation's flags are read through
  bit-fields MWCC counts from the highest bit, so the script looped forever; the host declares
  them reversed. The floor-edge walkers of `mplib.c` also stopped truncating `groundCollLine`
  to `int`.
- [x] The results screen, end to end through the game's code, exiting through the game itself;
  three more sequential-statics reads fixed on the way in, and `fn_80166A8C`, the SDK's fast
  float-to-`u16` cast that exists only in assembly, implemented.
- [x] Fox's neutral special, running backwards, and the KO all measured through the game's
  code.
- [x] The rules menu running inside the CSS, a VS match ending on its own, and
  `melee-host-vs-stock-match-asset` covering the whole stock route.
- [x] The results' image fixed: `gm_80168B34` and `gm_80168BF8` were returning stack garbage on
  the host, where the console keeps the value in a register.
- [x] Repeatable scripted routes with a frozen clock, the canonical `TRACE=` trace, and an
  `-O2` build outside the presets. `gprof` removed the 92% of route time the GX recorder was
  spending redoing every captured triangle at the end of each draw.
- [x] Playable mode: `melee-pc --play ROOT`, the modes from the title with the visible
  presenter, keyboard and gamepad as pad 1.

### Audio

- [x] The `.ssm` bank format surveyed and confirmed across the 110 files in `audio/`, with
  `port/tools/ssm_to_wav.py` as a reference decoder.
- [x] The host AX mixer (`port/src/os/ax_mixer.c`) in place of `baselib_support.c`'s voice
  facades: 64 voices with the SDK's priority stealing, 5 ms frames, DSP ADPCM, PCM16 and PCM8.
  It matches `ssm_to_wav.py` bit for bit on the real voices.
- [x] Sound effects and music through the game's code: `synth.c` builds the `.ssm` descriptors
  at pointer width, the `.sem` command streams are converted at load, and the `.hps` stream
  converts header and blocks.
- [x] The AX aux buses: reverb and delay, with `HandleReverb` ported from PowerPC assembly to
  C, operation by operation.
- [x] ITD through each voice's 32-sample circular line, and the surround channel preserved to
  the output, where stereo presentation encodes it into the out-of-phase Lt/Rt pair.

### Stage data, characters and the last VS scenes

- [x] Colour EFB copies: `GXCopyTex` in RGB5A3, RGB565 and RGBA8 rasterizes the frame's capture
  on the CPU, so the results' portraits stop being black.
- [x] `map_plit`, `quake_model_set` and `itemdata` translated, so the fighters get the stage's
  lights.
- [x] Mario's and Link's legs fixed: `src/placeholder.h` defined `__frsqrte(x)` as `sqrt(x)`
  when PowerPC's `frsqrte` estimates 1/sqrt(x), which put NaN in the thigh matrices.
- [x] Mario and Link playable, with per-slot translators for their seven items, and a rendering
  fix that applied to everything: with lighting off, GX delivers only the material colour.
- [x] Sudden death through the game's own path, with `FRAME:CLOCK[=SECONDS]` in the script.
- [x] `ALDYakuAll` and `yakumono_param`: the random item's script table translates in all 76
  files that have it, and of `yakumono_param` the host translates the zeroed block 29 files
  keep and refuses by name the 47 with parameters of their own.
- [x] The challenger and the prize notice, the two scenes the VS mode was missing, with
  `FRAME:MATCHES`, `FRAME:TROPHY` and `FRAME:STOP` in the script.
- [x] Fog applied between the TEV and the blend on both paths that draw the capture, with
  `MELEE_HOST_FOG=0` to compare.
- [x] The window's keyboard exercised through real X11 events, and then the whole route played
  from the keyboard (`port/tools/play_keyboard_probe.py`, `play_keyboard_match.py`).
- [x] Depth textures (`Z8`, `Z16`, `Z24X8` decoded with GX's tile geometry), the indirect
  texture path for refraction, and bump coordinates (`GX_TG_BUMPn`) in the presenter. A sweep
  of 725 joint symbols gives 3,379,817 triangles, 100.0% with the TEV fully evaluated.

## In progress

- [ ] Compile all the relevant code with no PPC assembly.
- [ ] Runtime resource manager consuming the extracted manifest.
- [ ] Cancellation, streaming and full DVD API priority.
- [ ] HSD loader with Disk/Runtime schemas and cyclic references. The file API already serves
  joints, animations, cameras, lights, fog, sprites and `_scene_data` and `_scene_models`;
  loose images and palettes, stage data and the `ftData*` of characters beyond Fox, Mario and
  Link are still missing.
- [ ] Vertical local match flow (roadmap in `docs/fight_flow_port.md`). The title, the menu, the
  CSS with the rules menu, the SSS, the match and the results all run through the game's code,
  with the image checked in BMP and with sound, and a drawn match goes through sudden death;
  playing in the window with real input is still missing.
- [ ] The lights the scene itself describes, which the materializer does not translate yet.

  The layer formed a single block: the eleven files reference one another, so adding any of
  them required adding all of them.

  History of the two blocker waves: the first started at 73 symbols and the second at 56. Both
  are closed. `synth.c` has also entered the build; `hsd_audio_stubs.c` keeps only
  `HSD_LogInit`, which on the host does not need to redirect MSL stdio because `OSReport`
  already writes to the native error stream.

## Next gates

1. Pace with presentation: without presenting, the match runs at about 200 frames per second in
   the `-O2` build; the presenter and the window still need measuring.
2. Visually check refraction (`lbrefract.c`) and its EFB copy against a reference; the colour
   copies already come out of the CPU rasterizer.
3. The window with real input and a 60 Hz wall-clock pace.
4. Check, in the machine code, the list of possibly uninitialized variables and functions with
   no `return` from the `-O2` build, starting with the ones the route reaches.
5. Visually check the bump materials against a reference. Fog has landed, with no comparison
   against the console; the curve is GX's and the depth is the fragment's clip w.
6. After parity and a stable 60 Hz pace, separate presentation from simulation and implement
   optional visual interpolation with 60, 120, 144, 165 and 240 FPS targets, and no unlimited
   mode. The game tick, the inputs and the physics stay at 60 Hz; cuts and discontinuous states
   must hold the valid pose, without extrapolating gameplay.
7. Create the settings overlay opened with `Esc`, without replacing the game's menus, with a
   Video page for rate, aspect and upscaling. The settings must persist, be navigable by
   keyboard/mouse/controller, and report both the preference and the effective presentation
   rate.

## Current limitations

- The extracted `GALE01` assets are available only in `assets-local`, which stays ignored by
  Git and is not part of any build or public artifact.
- The executable does not call `gmMain` yet.
- CARD and THP are not implemented. The host's AX plays voices in a software mixer with the aux
  buses, ITD and surround encoded for the stereo output; PAD and asynchronous DVD have basic
  bridges.
- GX state is recorded, not rasterized by the host: the image comes from the SDL/OpenGL
  preview, which draws the captured geometry with each draw's TEV program evaluated in a
  generated shader.
- The GObj runtime runs processes and can already own real graphics objects, but scenes are
  only presented by the `--view-*` diagnostics, not by the game's frame loop.
- The preview follows culling, depth, blend, colour mask, both alpha compares, fog, bump and
  indirect TEV; the colour comes from the per-fragment TEV. It does not read mipmaps
  (minification uses the magnification filter), does not yet model `GXEnableTexOffsets` or
  `GXSetTevSwapModeTable` (it uses `GXInit`'s tables, and the recorder stops by name if the
  game installs another), and calls the GL 2.0+ functions through `GL_GLEXT_PROTOTYPES`, which
  only resolves on Linux.
- Lighting is still per vertex, as in GX; what is per fragment is the TEV. A loose model is lit
  by the stand-in lights, not the stage's, so a character's colour in the preview is not a
  match's.
- In the world-space capture, Mario's joint appears upside down in the preview, while trophies
  and scenes appear in the right orientation. The camera reproduces the same
  `glFrustum`/`glRotate` sequence as before; the cause has not been investigated.
- `GX_BM_LOGIC` and `GX_CULL_ALL` are not modelled by the preview: the first falls back to no
  blending, the second discards the group. No symbol on the disc uses either, so this has never
  been exercised by real data.
- The preview's winding convention is GX's, clockwise as the front face, and it has not been
  verified visually. The F key flips it, because a model appearing inside out is the clearest
  evidence that the assumption is wrong for an asset.
- Character animation runs through the JObj tree, not through a `Fighter`. The bone mapping is
  positional: the nth node in the list lands on the nth joint in construction order. `ftanim.c`
  walks the same way but skips parts by fighter flags, so when the match runtime comes in that
  mapping has to go through it instead of the raw order.
- An `HSD_AObjDesc.obj_id` takes a reference to the JObj it names. If the asset points at the
  very JObj that owns the AObj, it creates an ownership cycle; the synthetic test releases it
  explicitly before tearing the tree down. Real assets should keep that reference in an
  external object, the way the original runtime expects.
- A loose joint symbol has no scene and therefore no camera; in that case the render uses the
  host's stand-in camera. The report line says which of the two was used.
- `melee_host_scene_graphics_render` draws one model of the scene per call, which is the unit
  the game uses (one GObj per model). A scene of several models needs one call per index.
- The descriptor materializer refuses, with a message of its own, what it does not yet know how
  to translate: particle joints, light animation that follows a joint through the ID table's
  key (the two light tables of `TyLight.dat`), material render descriptors
  (`HSD_MObjDesc.renderdesc`, whose shape only the custom setup knows) and expression RObj
  constraints, which hold a console function address. Spline joints and bytecode constraints
  already translate.
- The materialized descriptors are validated field by field, but the GX payloads handed to the
  original loaders are raw pointers. A vertex array has no size known to the descriptor, so
  only the base is checked: from there on the host trusts the asset, as the console did.
  Display lists are the exception, because the block count gives the exact size.
- The descriptors live as long as the scene handle. The loaded objects point into them, as they
  would point into a file still resident in the console's heap, which is why
  `melee_host_scene_graphics_release` frees the tree before the descriptors.
- `GXInitFogAdjTable` writes the neutral table (256, or 1.0 in 8.8 fixed point). The real
  derivation from the projection is not modelled, and the fog state reports that in
  `range_adjust_modelled`; the fog the host applies also ignores the range adjustment, which on
  the console opens the fog at the edges of the screen. None of this has been compared against
  the console: the curve is GX's formula and the depth is the fragment's clip w, with no
  reference recording.
- `GXCopyTex` produces the I4 (shadow) and colour (RGB5A3, RGB565 and RGBA8) copies by
  rasterizing the frame's capture on the CPU; copies in other formats are only recorded.
  `GXCopyDisp` only records.
- In the SDK the draw-done callback can also arrive by interrupt, with no wait. The host does
  not reproduce that: with no asynchronous graphics processor, the fence is only delivered by
  `GXWaitDrawDone`, `GXDrawDone` or the frame loop. Code that depended on interrupt delivery
  would not see the callback.
- `VIGetDTVStatus` reports no digital output rather than guessing a progressive mode the host
  would not honour.
- `hsd_3A76.c` calls `MTXOrtho` with a three-row `Mtx` buffer where a projection needs four. On
  the PowerPC path that is covered by the `MUST_MATCH` block; on the host it would be a stack
  overflow. The file does not enter the host build yet, but it needs attention when it does.
- The host's file API does not see the game's heap. A buffer freed without being parsed again
  leaves its descriptors alive until `melee_host_hsd_archive_release`, so memory grows per file
  loaded until the address is reused. When the scene flow comes in, destroying the scene heaps
  has to free what was parsed inside them.
- `HSD_ArchiveLocateExtern` with a non-null address is refused: binding the extern would mean
  writing a host pointer into a descriptor that only exists once the symbol is requested. The
  compiled code only calls it with NULL.
- `ftdata.c` tells an animation stored in ARAM from one stored in memory by the address being
  below `0x80000000`. On the host the memory address comes from the host allocator, which on
  x86-64 sits above that limit; a host that allocated below 2 GB would confuse the two.
- The materializer ignores an infinite light's parameter pointer and an ambient light's
  position, because `LObjLoad` does not read them; the disc's files carry the first one
  relocated.
- `HSD_MemAlloc` on the host uses the host allocator, not the OS heap `HSD_InitComponent`
  creates. Destroying a scene's heaps, which on the console frees everything it allocated, does
  not free HSD objects on the host; every scene change will leak until `HSD_MemAlloc` starts
  using the current OSAlloc heap.
- The host's disc wait only steps the scheduler. Drive error screens, reset and the memory card
  do not exist. A read that fails marks devcom's static error, which never calls the callback,
  and the game waits forever; the flag is `static` and the host cannot see it yet.
- The boot still skips `GXInit` (the FIFO is reserved in the arena but not handed over) and
  `lbMthp_8001F87C`. The debug level of a development disc is not selected.
- Reverb and delay have not been compared against the console sample by sample: the
  `HandleReverb` port follows the assembly, but there is no reference recording. The host's ITD
  was checked through its delay line and target transition; a hardware capture is still needed
  for a sample-by-sample comparison against the DSP. Chorus and high reverb remain unported,
  because the game does not use them.
- The IPL's sound mode does not come from SRAM: the host starts in stereo, and the game's sound
  menu can change it.
- Alarms follow wall-clock time unless the host freezes the OS clock. Frozen, time only advances
  in `lb_800195D0`'s wait, and only when the raw pad queue is empty: a wait for another alarm
  with pad samples in the queue does not advance. Nothing presents frames at 60 Hz of
  wall-clock time yet.
- The host's mode and scene table has the title, the main menu and the whole VS mode, on the
  console's state table: both selection scenes, the match, sudden death, the results, the
  challenger and the prize notice. Of the match against the challenger only the announcement
  runs: it is on a stage of the challenger's own and against a CPU, and the host stops with the
  name of the stage symbol it does not translate. Requesting a mode outside the table is
  refused before the game follows the NULL it would find, and a scene outside the table ends
  the mode; in both cases `--run-modes` ends the script with `stopped:`.
- The CSS and the SSS were checked against the game's state (ports, tokens, character and stage
  chosen) and, on the stock route, against the BMP image of the rules menu and the SSS; no
  frame of those scenes has been presented in a window.
- The colour EFB copies have not been checked against the console pixel by pixel. The texture
  filter is the presenter's (bilinear, no mipmap), the clear's Z8 is read as the high bits of
  the depth, `GXSetZCompLoc` is not modelled (depth is tested after alpha, or before when alpha
  always passes) and, if two TEV stages sample the same map with different coordinates, the
  first one's coordinate wins.
- The host's SIS pool is twice the size the scene asks for. That guarantees each block fits in
  twice what it occupied on the console, but the fragmentation may be different; a "Memory
  Empty" in another scene should be measured with a picture of the pool (used and free blocks)
  before touching the factor.
- Without a memory card only the 14 characters and the starting stages are unlocked. Final
  Destination and Battlefield stay locked on the SSS, and the first match targets Hyrule Temple.
- The host's VS mode has neither CPU nor handicap checked: the script only opens HMN ports. The
  rules menu runs (mode and stocks checked); the items and extra rules submenus, the name change
  and the CSS's team buttons reach named stops in `unported.c`.
- The title demo's files load, but nothing uses them yet. The particle banks they carry arrive
  already located through the host's file API. `particle.c` stops by name when faced with a form
  bank or a reference table; the effects and the stages pass both as null.
  `grDatFiles_801C5FC0`, which parses the stage file, is now the original code and has not been
  run yet.
- The host's ARQ models neither the SDK's two priority queues nor its chunking: everything
  completes on the next step, in the order posted. Posting with no active backend stops by name.
- Each event level's `x4` stays NULL. It is each event's own parameter (two ints, a coin count,
  floats, a character list, an int list and, on one level, an int and a pointer), read in place
  by the event's code, and it needs per-event translation before the event mode can enter the
  table.
- The memory card path keeps addresses in 32 bits: the parameters of `lb_8001BB48`,
  `lb_8001BC18`, `lb_8001BE30` and `lb_8001BF04`, the task's `unk_18` and `unk_1C` fields and
  the command queue of `hsd_3A94.c`, where `hsd_3B27.c` converts pointers to `s32` 14 times.
  Without a card the task chain stops at the probe (`CARD_RESULT_NOCARD` becomes `0xF`, which
  the next task does not accept) and none of this is read; a virtual card needs that path at
  pointer width.
- `ScNtcCommon_scene_data`, the card access notice, is not translated and stays NULL;
  `lb_8001CF18` checks and does not create the scene.
- SIS text opcodes 8 and 9 stop by name, and with them the cursor push in `string_buffer`, which
  would also keep an address in 32 bits.
- With a frame sink installed, the GX capture is only valid until the next `GXCopyDisp`;
  whoever needs it afterwards has to copy inside the sink.
- `db_TakeScreenshotIfPending` passes the XFB's address as an `int` to `hsd_80393A5C`, which
  truncates a 64-bit pointer. It only runs with a pending screenshot, which
  `db_CheckScreenshot` marks at debug levels.
- CTest does not fail on a UBSan report, which only prints; check with
  `ctest --preset host-sanitize -V`. Outside the match four remain, all calls through a function
  pointer of another type: `FogRelease` through `class.h`'s release pointer and
  `HSD_JObjRemoveAll` through `gobjobject.c`, in HSD's class system, and, since the CSS and SSS
  run, `HSD_AObjStopAnim` passed to `HSD_ForeachAnim` (`aobj.c:301`) and the render callback
  `fn_8026407C` of `mncharsel.c` (`gobj.c:154`). Since the test crosses the match there are 17
  distinct points; the list is in `melee-host-vs-match-asset`'s entry. With the stock route the
  suite showed 28 distinct points; with audio on there were 31, the same across two runs, and
  with the aux buses there are 32 (the new one is `ax_mixer.c:588`, the reverb's callback); with
  the Mario and Link routes there are 36 and with sudden death's 37. Four came earlier, calls
  through a function pointer of another type in the audio callbacks: `devcom.c:84`
  (`HSD_SynthSFXGroupDataReaddressCallback`), `devcom.c:229` (`HSD_Synth_8038B120`),
  `devcom.c:255` (`HSD_SynthPStreamFirstHakoHeaderCallback`) and `synth.c:1610`
  (`fn_8038CC1C`). The other 27 are from the earlier ones; the `FogRelease` call through
  `class.h`'s release pointer did not appear in those runs. Among the new ones,
  `plbonus.c:44` reads `by_attack_hi[107]` in a `u32[65]` and `gm_1601.c:3026` writes `kills[4]`
  and `kills[5]` in a `u16[4]`; both land in following members of the same struct, with the same
  layout on the host. The others are `1 << 31` in an `int` (`ftCo_Guard.c`, `ftCo_Escape.c`,
  `ftCo_Catch.c`, `ftCo_Attack100.c`, `ftCo_Damage.c`, `fighter.c`) and calls through a function
  pointer of another type.
- Many matches have been played through the `--play` window, and the game runs. What that
  exposed is that **some character and stage combinations still crash**; the combinations are
  not yet recorded, and the automated suite does not reach them, because
  `port/tools/random_cpu_matches.py` plays every match on Hyrule Temple
  (`STAGE_KIND = 14`, from when it was the only stage that loaded) and draws from the 14
  characters the select screen offers without a save. See the known issues in
  [`project-progress.md`](project-progress.md). `--view-title-scene` still has the window path
  unused.
- `--view-title-scene` ends when `gm_801A4D34` returns: START ends the scene, and the next one
  does not exist on the host yet.
- The title's texture cache and the presenter recognize an image by the address of its data and
  its palette. An EFB copy that rewrites the address is re-decoded by the copy's generation, but
  an animation that rewrites an image by some other means keeps showing the first one.
- **Only 3 of the 30 stages the select screen offers enter a match**: 14 Hyrule Temple,
  23 Poke Floats and 31 Battlefield. Measured 21 September 2026 with
  `port/tools/sweep_stages.py`. The other 27 abort on `yakumono_param`, 17 with "no
  stage-specific verified layout" and 9 with "contains an unsupported relocated object"; the
  random square resolves to one of them. `game_data_translators.c:3655` turns the refusal into
  an `OSPanic`, so picking such a stage ends the process. `ground.c`'s `stage_datas` table
  links every stage, and the host's file API translates `grGroundParam`, `coll_data`,
  `map_head` (69 of 71), `map_plit`, `quake_model_set`, `itemdata` and `ALDYakuAll`; of
  `yakumono_param` only the zeroed block 29 files keep, Hyrule Temple among them. The other 47 have parameters of their own, with the
  layout of the struct each `grXXX.c` declares (floats, ints, pairs of u16 in one word and
  pointers), and stop by name: without the field widths, a block of bytes from the disc does not
  become host values. Stage items created from `itemdata` stop by name where the item needs its
  per-type attributes.
- `lbFile_800164A4` chooses a direct read into RAM because the destination is above
  `0x80000000`, which the host's 64-bit addresses satisfy; `lbmemory.c`'s split between ARAM and
  RAM uses 16 MB on the host.
- The game has 59 variadic pointer lists terminated by `0`; 10 of them use `VA_END_PTR`
  (`gmTitle_801A1AC0`, `lb_80014534` and the eight on the menu path). Every unit now compiles in
  the core, so what is missing does not show up in the build: each new path that reaches one of
  the other 49 needs the same swap, and under ASan the symptom is a write to an address with the
  low 32 bits zeroed.
- Part of the floating-point maths is still glibc's, not the game's: `melee-pc` resolves
  `atanf`, `sinf`, `cosf`, `tanf`, `sqrtf`, `sqrt` and `fmodf` through `libm`. `lbtrigf.c`'s
  `atanf` only compiles under `__MWERKS__`, because it uses PowerPC's `__fnmsubs` intrinsic, and
  the others come from MSL, which the host does not compile. For determinism against the console
  those functions have to give the same result bit for bit. `lbtrigf.c` also reads floats through
  `*(u32*) &f`, which works in the debug build and needs `-fno-strict-aliasing` or `memcpy` in an
  optimized build.
- `__frsqrte` is `1.0 / sqrt(x)` (`src/placeholder.h`). The console starts from `frsqrte`'s table
  estimate and the host from the exact value, so roots refined by Newton may differ from the
  console in the last bits.
- All 27 `ftData*` translators carry their character's own item attribute table. What is
  still left out are the per-type special attributes and dynamics of the items that come from
  `itPublicData` — the common items, the Pokémon — and of the stage items created from
  `itemdata`. Such an item stops with "OS panic" at `item.c:576` as soon as it comes out, and
  the process ends with it, in `--play` too.
