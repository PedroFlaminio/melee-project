# Native port MVP progress

## MVP definition

A complete local flow: start the executable, navigate from the title to VS, pick two players
and a stage, and complete a local match with working video, input, audio and result.

## Current state — 21 September 2026

**Estimate: 100% (confidence range: 98–100%).** Everything the definition above describes runs
through the game's own code and has a test: the executable opens, the route goes from the
title to VS, two players and a stage are picked, the match ends with a result, and image,
sound and input are all checked — input including real key events in the window, which carry
the whole route on their own. What is still missing lies outside the definition (the list
follows the table), with one exception no automation can close: nobody has sat down and played
a match to say whether it responds the way the console does. The table describes the state of
each area today; the log at the end records the evolution since the 13 September baseline.

| Area | State | MVP weight |
| --- | --- | --- |
| Host platform (memory, clock, virtual DVD, input) | working and tested; input scripting with stick and four ports; the scripted routes freeze the clock at a fixed instant and repeat exactly; `--play` reads keyboard and gamepad as pad 1 in a 60 Hz window, with D-pad and digital L and R, up positive on the gamepad axes and the FPS in the title; a real key event sent to the window makes the fighter run and jab; a manual test found the Y axis inverted, since fixed | 15% |
| HSD/GX assets and rendering | working for the VS route's scenes, with the image checked in BMP; the colour EFB copies (results portraits, magnifier bubble) are rasterized on the CPU from the frame's capture, with the magnifier bubble not yet confirmed in the image; the game's fog goes in between the TEV and the blend on both paths that draw the capture; depth textures, bump and indirect TEV landed in the presenter, and a sweep of 725 joint symbols gives 100.0% of 3,379,817 triangles with the TEV fully evaluated; mipmaps and a visual check of those three against a reference are still missing | 20% |
| Startup, title and main menu | title and the route to the menu validated | 15% |
| VS configuration | CSS, rules menu and SSS with two pads; on leaving the results the mode still chooses between the challenger and the prize notice, both of which run; selection, stock rules and stage checked against the game's state and against the image of the rules menu and the SSS | 10% |
| Match (fighters, stage, collision, camera, HUD, KO) | Fox, Mario and Link on Hyrule Temple through the game's code: movement, running, attack, blaster, pause and L+R+A+START; with one stock P1 falls, the match ends by elimination, the "Game!" appears and the results show the winner ("FOX"), the placings and the statistics and return to the CSS. Image checked in BMP, with the panels' portraits and the stage's lights (`map_plit`) on the fighters; of the stage data, `ALDYakuAll` translates in every file and `yakumono_param` in the zeroed block Hyrule Temple keeps without reading. A timed match that ends in a draw goes through sudden death (one stock at 300%) and returns to the results with the placings it decided | 30% |
| Audio, distribution and end-to-end regression | effects and music through the game's code: the host's AX mixer plays the `.ssm` descriptors, the `.sem` commands and the `.hps` stream on a 5 ms clock tied to the retraces, with output to the sound device in `--play` and to WAV on the routes; the music and one effect match reference decoders (correlation 1.000000); the game's reverb and delay play on the aux buses, with `HandleReverb` ported from assembly to C; two title → results → menu routes are tests; the stock route gives the same canonical trace across runs and between `host-debug` and an `-O2` build; without presenting, the match runs at about 200 frames per second at `-O2` and at 19 in `host-debug`; with `--play` the whole route stays at 60 | 10% |

### Verified evidence

- Stage data: with `ALDYakuAll` and `yakumono_param`, `--sweep-archives` goes from 886
  `game_data` symbols, 884 translated, to 1038 and 989. The 105 new translations are the 76
  script tables of the random item and the 29 `yakumono_param` that are a zeroed block; the 47
  new failures are the `yakumono_param` with parameters of their own, which stop by name. On
  the route the game now writes the stage's script into the random item's state, and the stock
  route's trace stays identical across all 1739 frames.
- Fog: the `GXSetFog` state now travels with every captured draw, and the presenter's shader
  and the CPU rasterizer apply it between the TEV and the blend, over the fragment's own depth
  (the clip w), with integer blending on both sides. Comparing the same frames with
  `MELEE_HOST_FOG=0`: the title changes 3.6% of the pixels, the main menu 89.8%, the CSS 0.1%
  and the SSS 69.4% (mean 49.9), where the distant background goes from a purple plasma to
  dark blue and the icons stay intact; the Hyrule Temple match does not change a single pixel,
  because the match scene installs no fog. TEV conformance runs half its cases with a fog that
  falls mid-curve: 0 divergences across 256 cases. Trace identical across the 1739 frames.
- Window keyboard through real events (`port/tools/play_keyboard_probe.py`): with the `d` key
  sent only to `--play`'s window, pad 1 walks 109 to 131 units and passes through `Dash`; with
  `j` it stays put and enters `Attack11`; with no key it stays in `Wait` at the same x. And
  `port/tools/play_keyboard_match.py` plays the whole route from the keyboard — title, menu,
  CSS, SSS, match, pause, the L+R+A+START exit and the results through to the return to the
  CSS — with pad 2 in the script: three consecutive runs give the same scenes (title 1, menu
  123, CSS 243, SSS 384, match 533, results 808, CSS 1214), and a capture of the window itself
  at frame 700 shows Hyrule Temple with both Foxes, the clock at 01:59:27 and the HUD. A person
  playing and saying whether it responds like the console is still missing, as is the gamepad.
- `ctest --preset host-debug`: 26/26 (the 14 short ones, the Mario and Link data, the two sound
  bank ones, the route audio one and the seven VS routes, these with voices on); 225/225 unit
  tests; with `-j4` the suite takes 62.9 s, with the routes between 25 and 32 s.
- Challenger and prize notice, the two scenes the VS mode was missing:
  `melee-host-vs-challenger-asset` leaves the save at 50 matches, and on leaving the results
  the mode enters scene 0x29 at frame 1620 — the BMP shows "A new foe has appeared!", the
  "WARNING CHALLENGER APPROACHING" notice and Jigglypuff's silhouette — and the total read
  after the match is 51. `melee-host-vs-prize-asset` awards trophy 0x55 through the game's own
  path mid-match; scene 0x27 starts at 1620 ("You got the Maxim Tomato trophy!"), a button
  closes it and the mode continues to the CSS at 1910. The match against the challenger is out
  of scope: it is on a stage of its own and against a CPU, and it now stops with the name of
  the stage symbol the host does not translate, where it used to SIGSEGV.
- Colour EFB copies: on the stock route, `1450:EFBCOPY` finds 3 copied textures with up to 893
  colours, and the BMP of frame 1450 shows the 2nd place portrait (Fox in the defeat pose, red
  background) and the 1st (the face, blue background), which used to be black. In the match,
  the magnifier's 64×64 copy (`ifmagnify.c`, which copies at 0,0 with a clear) has 327, 347 and
  341 colours at frames 1078, 1085 and 1092; the bubble does not appear in frame 1085's BMP,
  and that has not been investigated. Frame 1450's BMP comes out identical before and after the
  rasterizer's shortcuts and with the hot files at `-O2`, and the `-O2` build's trace stays
  identical across the 1739 frames.
- Stage data: `map_plit`, `quake_model_set` and `itemdata` translate across the stages
  (`--sweep-archives`: `game_data` goes from 662 of 664 to 882 of 884, and the 4 failures are
  the ones from before). On the stock route the trace stays identical; at frame 850, with
  Hyrule Temple's lights on the fighters, the sky does not change a pixel and what changes is
  the lit geometry (castle wall with a mean difference of 20.7, grass 11.4, P1 24.9 and P2
  19.8) and the shadows, which go from one view of 936 triangles to two of 468.
- Aux buses: `melee-host-route-audio-asset` compares against the references with
  `MELEE_HOST_AUDIO_AUX=0` (music and effect 118 at correlation 1.000000) and runs again with
  reverb and delay: the music, which sends nothing, stays identical, and the difference between
  the two recordings is zero before the effect and reaches 2564 in the half second after it.
  The impulse into the game's reverb stays silent until the first comb returns what the
  pre-delay delivered (sample 1852) and gives the same samples across two runs. On the stock
  route with aux the trace stays identical and the WAV has 8 clipped samples between 16 and
  20 s, where before there were none.
- `melee-host-route-audio-asset`: title → START → menu → B, recording the mixer to WAV. The
  `menu01.hps` music, decoded separately, matches window by window (67 windows of 4000 samples,
  worst correlation 1.000000 on both channels, crossing the stream's block junctions), and
  effect 118 of `main.ssm`, with the music subtracted, gives 1.000000 on both voices. With the
  mixer's old end-of-voice test, the worst windows drop to -0.66 and -0.53 and the test fails.
- Audio and game: the stock route gives the same trace across all 1739 frames with voices on
  and off (`MELEE_HOST_AUDIO=0`), and the `-O2` build the same trace with sound, in 4.96 s.
- `--play` (`-O2` build) with a scripted START: `melee-pc` appears in the sound server as a
  stream that is playing (not paused) during the menu music, and the title showed 60.1, 60.1
  and 59.8 FPS.
- `ctest --preset host-sanitize -V`: 26/26 with the seven VS routes, no ASan report, 225/225
  unit tests, the whole suite in 232.1 s. UBSan only prints: 37 distinct points, five of them
  in the audio callbacks and one only in sudden death (`gm_1601.c:3232`), described in
  `native_port_status.md`.
- The title scene, animations and the transition to the main menu all have tests with local
  assets.
- `melee-host-vs-match-asset`: title (122 frames) → menu (120) → CSS (141) → SSS (149) → match
  (175) → results (406) → CSS (120) → menu, with two scripted pads. Both ports open as HMN,
  both players pick Fox, START leads to the SSS and the cursor picks Hyrule Temple (stage 14
  with Fox in slots 0 and 1). In the match, the shadow, the stick and the A button are checked;
  pad 1 pauses after the HUD comes on (frame 655) and exits with L+R+A+START. On the results, a
  button gets past the opening and START on both ports marks both ready; the mode returns to
  the CSS, and B held leads to the menu. 24.6 s in `host-debug`.
- `melee-host-vs-sudden-death-asset`: the default rule is a two-minute time limit, and the
  route only touches the clock: `680:CLOCK=3` leaves 3 s with the HUD on, the game runs the
  time out on its own (`clock frame 950: 0s+59`), the match ends with two winners and the mode
  enters sudden death (scene 0x03 at frame 1034), where pad 1 falls off the stage. The results
  keep the end of the timed match (`outcome 1 winners 2`, three stocks each) with sudden
  death's placings (`places P1=2 P2=1`). Title (122) → menu (120) → CSS (141) → SSS (149) →
  match (501) → sudden death (469) → results (407) → CSS (120), 31 s in `host-debug` (under
  ASan the suite gives 24/24 in 221.9 s, with no ASan report and one new UBSan point, the
  `team_standings[5]` of a five-element array in `gm_80166CCC`). The same route without the
  clock shortcut, with the full two minutes, gives the same sequence (a match of 7,438 frames)
  and the same outcome, in 49 s in the `-O2` build. In the BMPs: the "Time!" with the clock at
  00:00:00, sudden death's "Go!" with both at 300% and the "Time Battle" results with 1st and
  2nd.
- `melee-host-vs-stock-match-asset`: on the CSS the rules menu swaps time for stock and lowers
  the stock from 3 to 1; in the match P1 falls off the stage, the game ends the match by
  elimination (outcome 2, P2 the winner) and the results of a completed match return to the
  CSS. Title (122) → menu (120) → CSS (361) → SSS (149) → match (460) → results (407) → CSS
  (120) → menu, 32.2 s in `host-debug`.
- That route's image in BMP: rules menu ("Stock 01"), SSS, match with the HUD, "Game!" and
  results with the title "FOX", 2nd and 1st and "READY FOR THE NEXT BATTLE".
- Canonical trace (`TRACE=`): the stock route gives the same 1739 frames between two
  simultaneous runs, between runs at different hours, and between `host-debug` and the `-O2`
  build.
- `-O2` build (`build/host-release`, outside the presets): 205/205 unit tests; the stock route
  in 5.2 s without presenting, with the match at 199.7 frames per second, the results at 226.5,
  the SSS at 266 and everything else above 1400.
- `--play` (`-O2` build): with the hidden presenter and the stock script, the whole route at
  59.5–60 frames per second, with the same outcome; the visible window opened under Wayland and
  ran the title at 60.02.
- `--diagnose-local-match` materializes two players' data, the rules and the stage, but does
  not yet start the combat scene.

## What is outside the MVP

None of this is in the MVP definition, and none of it blocks the local match:

- A person playing. The window responds to real key events and the keyboard carries the whole
  route, but nobody has played a match to say whether the game responds the way the console
  does; the gamepad has not been touched, and its Y axis only has a unit test.
- From the image: mipmaps (minification uses the magnification filter), the fog range
  adjustment, `GXEnableTexOffsets` and `GXSetTevSwapModeTable`. Bump (`GX_TG_BUMPn`), depth
  textures (`GX_ZT_REPLACE` with Z8, Z16 and Z24X8) and the refraction's indirect TEV have
  landed; what is missing in them is the visual check against a reference, because none of them
  has a compared image.
- From the sound: no sample-by-sample comparison of reverb, ITD or surround against the
  console. ITD and surround are already played by the mixer.
- From the stages: the `yakumono_param` of the 47 files with parameters of their own, one
  layout per stage. Hyrule Temple remains the target because it is unlocked without a memory
  card and has the smallest module (`grshrine.c`); Final Destination and Battlefield stay
  locked on the SSS without saved data.
- From the VS mode: the match against the challenger, which is on a stage of its own and
  against a CPU — the announcement runs, the match stops with the name of the stage symbol the
  host does not translate.
- Outside VS: the other game modes, the memory card, CARD and THP.

Items found in the first manual `--play` test:

- [x] Invert the Y axis of the SDL gamepad's main analog stick before writing it into
  `PADStatus.stickY`; the keyboard already uses up as positive, but `SDL_GAMEPAD_AXIS_LEFTY`
  uses up as negative. Both vertical axes (analog and C-stick) flip sign in `gamepad_axis_y`
  (`port/src/render/play_window.hpp`), with a unit test; there was no gamepad connected to
  check by hand.
- [x] Show the presentation FPS in the window title, updated periodically, to make the real
  pace visible during a match. The visible window shows "Melee PC — 60.0 FPS — …" twice a
  second; in the `-O2` build, read with `wmctrl` on an X11 window, the title gave 60.0, 59.8
  and 60.0 at 3, 6 and 9 s.

### Current operational limit

`host-debug` runs the two VS routes in 30 and 25 s, and the `-O2` build, which gives the same
trace, in a few seconds. That build's warnings (177 possibly uninitialized variables and 43
functions with no `return`) are the list of where to look for values that on the console come
from a register, like the ones that were spoiling the results. The integration script still
fails if two `MOVE` samples do not detect a displacement of at least 0.1 units.

## Update log

Entries from 13–15 September 2026 are condensed; the full wording of each is in this file's git
history. Entries from 15 September onwards are kept in full.

### 13 September 2026 — 25% to 45%

Baseline created: infrastructure, title and a partial menu validated. The memory card UI was
isolated on the host and the route validated as far as `GM_VS (0x02)`. CSS, SSS and the rules
menu came into the host core, then were registered in the host table, stopping explicitly at
the missing `MnSelectChrDataTable` translator rather than crashing silently.
`MnSelectChrDataTable` and `MnSelectStageDataTable` then materialized camera, lights, fog and
the CSS/SSS models in host layout. The CSS stopped at `sislib.c:95` ("Memory Empty") because
the scene's 0x2400-byte SIS pool is sized for PowerPC; the pool was doubled and its blocks
pointer-aligned under `MELEE_HOST`. CSS and SSS then ran with two-pad input and Fox/Fox on
Hyrule Temple was validated (`melee-host-vs-selection-asset`). `host-sanitize` linked again,
and the VS route under ASan found two host bugs: a read past the SSS stage table and shape
animation reading big-endian vertices as native (`pobj.c`). Instrumentation exclusions were
removed from `host-sanitize`, which surfaced a stack overflow in the menu, out-of-array reads
and undefined shifts. The whole of `src/melee` (808 files) then compiled into the core, and the
match scene (`GS_VS`) linked with no undefined symbol; the route entered the match and stopped
at `lbRefData`, which was translated next. The blocker moved to the effects:
`effCommonDataTable` and the particle banks.

### 14 September 2026 — 46% to 87%

The host's particle loader landed (`eff*DataTable`, `map_ptcl`/`map_texg` as already-located
banks), followed by a chain of asset translators, each unblocking the next step of match entry:
`plLoadCommonData`, then `stage_datas` linked strongly, then `grGroundParam` (all 71
`Gr*.dat`), the seven trophy tables of `TyDatai.usd`, the command-script conversion (native word
order, reversed bit-fields generated from `lb/types.h`, relative subroutine and goto targets),
`itPublicData` (98 `Article`), `map_head` (69 of 71 stages), `ftLoadCommonData` (23 common
fighter tables) and `ftDataFox`. With those, both Foxes were created. `_scene_data` and
`_scene_models` joined the file API and the HUD's tables were translated, so `fn_8016E730`
completed and the scene entered the frame loop.

Then came the 32-bit layout bugs, found by running the route under ASan: the camera's
`Camera_ApplyQuake` reading statics in sequence and writing NaN; particle generator addresses
in `u32`; the `UnkX` view of `IfDamageState`; structs read over sequential statics
(`lbrefract.c`, `ftmaterial.c`, `ft_800852B0`); and the opcode in `Command_04` taken from the
wrong bits. With those fixed, `GS_VS` entered the host table and the VS route crossed the match
through the game's code, exiting with L+R+A+START — the `melee-host-vs-match-asset` test.

The image followed: `FRAME:BMP=` captures, the white-quad palette bug (the GX recorder now
stores each draw's palette), `coll_data` translated so the fighters land instead of falling,
the draw flag's reversed bit-field so the fighters are drawn at all, and normal matrices kept
separately from position matrices so both Foxes appear at the right size. The black band was
identified as the projected shadow and `GXCopyTex` was made to materialize its I4 copy. Stick
response and the A button were validated (the latter after ASan found `mpFloorGetLeft`
truncating `groundCollLine` to `int`), and the run animation's infinite loop — reversed
bit-fields over `fp->x594_s32` — was fixed. The results screen then ran end to end through the
game's code, and Fox's neutral special worked once the blaster's own attributes were
translated.

### 15 September 2026 — 89% to 91%

A match played to the end: the rules menu ran (after `mn_80231634` was made to return an
`intptr_t`), P1 fell, and the results of a completed match returned to the CSS. The results and
HUD got the right image once `gm_80168B34` and `gm_80168BF8` stopped returning stack garbage.
Routes became repeatable with a frozen clock and the canonical `TRACE=`, and `gprof` on an
`-O2 -pg` build removed the 92% of route time `finish_draw_locked` was spending redoing every
captured triangle at the end of each draw. `melee-pc --play` arrived: the modes from the title
in a 60 Hz window, with keyboard and the first gamepad as pad 1, and the pad was completed with
the numeric keypad D-pad and digital L and R. Audio began with the `.ssm` bank format confirmed
byte by byte and `port/tools/ssm_to_wav.py` as a reference decoder, then the host's AX mixer
(64 voices, SDK priority stealing, 5 ms frames), which decoded the real voices identically to
the reference across 456 voices in nine banks.

### 15 September 2026 — 94%: the game plays effects and music

`synth.c` assembles the `.ssm` descriptors on the host in pointer width (file record and sounds
in one block, with their own readdress and deflag), the `.sem` has its command streams
converted at load time, the resampling ratio is no longer stored as one word over two `u16`,
and the `.hps` stream converts header and blocks. The AX clock runs a 5 ms frame per 5 ms of
field on every retrace, the voices come on in the routes and in `--play`, which plays to the
sound device and now waits one NTSC field per frame, and `WAV=` records the mixer. Comparing
the music against a separate decoder found the mixer returning to the start of the block on
every sample after looping to the next block (33 and 96 repeated samples): the end now triggers
only at the address past the end, like the DSP's accelerator. `OSGetSoundMode` answers stereo.
Music per window and effect 118 at correlation 1.000000; trace identical with and without
sound; `host-debug` 19/19, 217/217 unit tests; under ASan 19/19, with no ASan report and 31
UBSan points, four of them in the audio callbacks.

### 15 September 2026 — 95%: colour EFB copies

`GXCopyTex` in RGB5A3, RGB565 or RGBA8 rasterizes the frame's capture on the CPU up to the
copy: each draw with its projection, viewport and scissor, culling, depth test and write (HSD's
Z8 clear writes the background), per-fragment TEV over filtered texels, alpha test and blend,
starting from the clear colour and depth and from the clears earlier copies in the frame asked
for. The results copy two portraits per player every frame (975 copies on the stock route, 1944
on the cancelled one): the panels show Fox, previously black, and `FRAME:EFBCOPY` checks that
the copy has an image. The texture caches re-decode an address a copy rewrote, and the
presenter applies the mid-frame clears, which removes a red rectangle from behind the Fox on
the results. An old test copied 320×240 RGBA8 into a 64-byte array and started overflowing the
stack; it got a buffer the size of the texture. Unoptimized, the portraits cost the cancelled
route 57 s; starting at the last clear that covers the copy, testing depth before the TEV when
alpha always passes, and compiling `command_recorder.cpp` and `tev.cpp` with `-O2` bring it to
29.8 s. `host-sanitize`'s clang refused five sign conversions in the rasterizer that GCC
accepted, all fixed. `host-debug` 19/19, 218/218 unit tests; under ASan 19/19, with no ASan
report and the same 31 UBSan points.

### 15 September 2026 — 96%: stage data

`map_plit` (the `LightList` table `ftCo_8009F4A4` gives the fighters through
`Ground_801C49B4`), `quake_model_set` (a `DynamicModelDesc` with a joint and three animation
tables) and `itemdata` (the stage's items, `{type, Article*}`, empty on Hyrule Temple) all
translate through the host's readers, with the lights shared by address with `map_head`'s
overrides, the way `Ground_801C20E0` compares them. Before that the game used the two default
lights of `Ground_803E06C8`. At frame 850 of the stock route the sky stays identical and the lit
geometry and the fighters change tone; both Foxes' shadows get projections of their own. Trace
identical; `--sweep-archives` with 220 more symbols translated. `ALDYakuAll` and
`yakumono_param` are left out. `host-debug` 19/19, 219/219 unit tests; under ASan 19/19, with
no ASan report and the same 31 UBSan points.

### 15 September 2026 — 97%: reverb and delay

The AX mixer keeps the aux A and B callbacks, accumulates each voice's send (left, right and
surround, with ramping), hands the frame to the callback and mixes the return into the next
frame's output, the way the DSP does with the buffer the CPU processed. The game's default
reverb (aux A) calls `HandleReverb`, which on the console is PowerPC assembly and on the host
stopped by name; it is now C, operation by operation: pre-delay, two combs, all-pass, low-pass,
second all-pass and the dry mix, with `fmaf` on the fused sums and `fctiwz`'s saturating
truncation, the three channels contiguous. The delay (aux B) was already C.
`MELEE_HOST_AUDIO_AUX=0` turns the buses off; the audio checker compares with them off and
confirms with them on that only the effect gains a return (up to 2564, zero before). Tests
cover the reverb's impulse and the return one frame later; a first test assumed ARAM was zeroed
and hit bytes another test had written. Trace identical with aux. `host-debug` 19/19, 221/221
unit tests; under ASan 19/19, with no ASan report and one new UBSan point, the call to
`AXFXReverbStdCallback` through the `void (*)(void*, void*)` pointer the game registers.

### 15 September 2026 — 97%: Mario's and Link's legs

They were disappearing because the thigh matrices and everything below them went NaN: the legs'
IK (`lbBgFlash_80021410`, which `ft_80089B08` runs on landing and while idle) measured the
bones with `sqrtf_store`, and `src/placeholder.h` defined `__frsqrte(x)` as `sqrt(x)`, when
PowerPC's `frsqrte` estimates 1/sqrt(x). The Newton steps in about 50 places in the game
diverged away from x = 1 (the root of 44 gave -1.9e41). A hardware watchpoint that only stops on
NaN found the write. The macro is now `1.0 / sqrt(x)`, which also fixes `acosf` and `asinf`
(`acosf(0.99)` gave 1.1308 instead of 0.1415) and Link's hat dynamics. Codex's three workaround
commits are reverted: hiding DObj variants did not change a pixel of the route, and the hat
dynamics were switched off. On the Mario versus Link route there were NaNs in the legs from
frames 610 and 618; now there are none in 65 samples, and the legs appear in the BMPs. Mario's
fireball (item 48) stops by name, because the special items have no translator. The stock
route's trace changes at frame 910 (P1's x by 0.01), with the same outcome. The ARAM test passes
again with the host's 24 MiB, and a test covers `lbVector_Angle`. `host-debug` 21/21, 222/222
unit tests; under ASan, no ASan report and the same 32 UBSan points.

### 15 September 2026 — 98%: Mario and Link playable

Their seven items (fireball 48, cape 83, bomb 58, boomerang 60, hookshot 62, arrow 64 and bow
76) got a per-slot translator: blocks of scalars where the disc holds only scalars, and field by
field where there are models and animations, because the host's pointers are wider. Link's
attribute record is translated in full, keeping the PowerPC offsets, since `ftCo_0D8E.c` reads
the same bytes through another struct. `it_802A4BFC_sqrtf_offset` was writing into the
neighbouring stack slot to match MWCC and wrecked the caller's frame; the hookshot's chain
reached it (SIGBUS in `host-debug`, silent at `-O2`), and the trick now sits under
`MELEE_HOST`. The routes pick both characters through the CSS without gdb (cursor at 1.24 units
per frame; Mario 15 frames up, Link 30 to the right) and enter the suite: a one-stock match
through to the results, with Link the winner and the portraits copied from the EFB, and a
specials route that checks Mario's states (343, 345, 350, 347 and 212) and creates all seven
items. A rendering defect that applied to everything also came out: with lighting off GX
delivers only the material colour, and the host was multiplying by the last material's ambient,
which left the stage dark and red while the boomerang flew. `host-debug` 23/23, 223/223 unit
tests; under ASan all 23 pass with no ASan report, with 36 UBSan points, four of them new and
from families already recorded (three `1 << 31` and one call through a pointer of another
type).

### 16 September 2026 — 99%: sudden death

A timed match that ends in a draw goes to the `GS_SUDDEN_DEATH` scene through the game's own
path: `gmVsMelee_ExitVs` counts two winners, `gm_SetupSuddenDeath` swaps the rules for one stock
at 300% and `gm_80166CCC` returns to the end of the timed match only the placings sudden death
decided. Since the menu offers nothing shorter than a minute, the script gained
`FRAME:CLOCK[=SECONDS]`, which reads and writes the scene's clock, and `RESULT` now prints the
placings. The new test leaves 3 s on the clock, the game runs the time out at `0s+59` and enters
sudden death, where pad 1 falls: 501 match frames, 469 of sudden death and 407 of results, 31 s
in `host-debug`. The long route, without the shortcut, gives the same sequence with 7,438 match
frames and the same `outcome 1 winners 2` with `places P1=2 P2=1`, in 49 s at `-O2`.
`host-debug` 24/24 and 223/223 unit tests.

### 16 September 2026 — 99%: `ALDYakuAll` and `yakumono_param`

The script table the stage gives the random item translates in all 76 files that have it: index
0 is the zero `Ground_801C0800` skips, each entry becomes a command stream and the table closes
with NULL; in `GrSh.dat` the only script lives in the words right after `yakumono_param`. Of
that one, the host translates the zeroed block 29 files keep (Hyrule Temple among them) and
refuses by name the 47 with parameters of their own, because the layout is the struct in each
`grXXX.c` and without the field widths the byte order cannot be swapped. `--sweep-archives`:
`game_data` from 886/884 to 1038/989. On the route, `stage_info.ald_yaku_all` stops being NULL
and the game writes the script into the random item's state; the stock route's trace stays
identical across the 1739 frames between the `-O2` build before and after. `host-debug` 24/24
and 224/224 unit tests.

### 16 September 2026 — 99%: challenger and prize notice

The two scenes the VS mode was missing enter the host's table with `gmscdata.c`'s callbacks;
what chooses between them is `gmVsMelee_ExitResults`, from the save. Since the smallest total
that unlocks a character is 50 matches and the notice depends on a new trophy, the script
gained `FRAME:MATCHES[=TOTAL]`, `FRAME:TROPHY=ID` (which goes through the game's `fn_80172C78`)
and `FRAME:STOP`, which ends the route at a frame, since a scene waiting for a button would trap
the script. Two tests: the challenger with Jigglypuff's silhouette and "A new foe has
appeared!", and the prize with "You got the Maxim Tomato trophy!" followed by the return to the
CSS. The match against the challenger stays outside the MVP (its own stage and a CPU), and what
used to be a SIGSEGV in `grStadium_801D13E0` became a named stop: `grDatFiles_801C6038` checks
on the host whether any stage symbol lookup was refused. `host-debug` 26/26 and 224/224 unit
tests.

### 16 September 2026 — 99%: fog

The `GXSetFog` state becomes part of the captured draw state, and both paths that draw the
capture apply it between the TEV and the blend, the way the hardware does: the presenter's
shader and the CPU rasterizer of the EFB copies. The depth is the fragment's own — in a
perspective projection, the clip w — the weight comes from the type's curve (linear, `2^-8t`,
`2^-8t²` and the two inverted) over the depth normalized between `startz` and `endz`, and the
blend is integer on both sides, which makes them round identically. `MELEE_HOST_FOG=0` turns it
off for comparison. The game uses four linear configurations on the route; with fog the title
changes 3.6% of the pixels, the menu 89.8%, the CSS 0.1% and the SSS 69.4%, and the Hyrule
Temple match none, because the match scene installs no fog. TEV conformance now exercises fog
(half the cases, 0 divergences) and the stock route's trace stays identical. `host-debug` 26/26
and 225/225 unit tests.

### 16 September 2026 — 99%: window keyboard through real events

The routes reach the game through the same `PADRead` the window fills, so they never went
through the keyboard. `port/tools/play_keyboard_probe.py` opens `--play` on X11, waits for the
match to start by reading the game's output (with `stdbuf -oL`, or stdout in a pipe comes out in
blocks and the line arrives too late), sends the key with `xdotool keydown --window` — only to
that window, without going through desktop focus — and reads the fighter back. With `d`, pad 1
walks 109 to 131 units and passes through `Dash` (20); with `j` it stays put and enters
`Attack11` (44); with no key it stays in `Wait` (14) at the same x. A person playing is still
missing.

### 16 September 2026 — 100%: the whole route played from the keyboard

`port/tools/play_keyboard_match.py` takes pad 1 from the title to the results and back to the
CSS using only real key events in `--play`'s window — title, menu, CSS, SSS, match, pause and
the L+R+A+START exit — with pad 2 in the script. Each key goes out on the previous frame's
`rules frame N` line and is released on the line of the last frame it should be down, which
reproduces the scripts frame by frame; with the `keyup` one frame late the CSS cursor moved 1.24
units too far and the route picked Ness. Three consecutive runs give the same scenes (1, 123,
243, 384, 533, 808, 1214), and the capture of the window itself at frame 700 shows the match
with the HUD. With that the MVP is closed: what remains are items outside the definition (bump,
depth textures, mipmaps, ITD and surround, other stages, the challenger match and CPU) and the
judgement of a person playing.

### 21 September 2026 — 100%: bump, depth textures and indirect TEV in the presenter

The `GX_TG_BUMPn` coordinates are now evaluated in a pass of their own, after the position
transform, where lights, vertices and the tangent basis are in the same space; the `Z8`, `Z16`
and `Z24X8` formats are decoded with GX's tile geometry and the shader reconstructs a 24-bit
depth for `GX_ZT_REPLACE`; and the `GXSetTevIndirect` state travels with each draw, with the
GLSL offsetting the direct coordinate by the indirect sample (ST bias, matrix, exponent, scale
and wrap), the matrices as uniforms so as not to multiply programs. The map that only an
indirect stage samples now enters the draw's texture set, or refraction would bind the white
fallback texture. A sweep of 725 joint symbols across every `.dat` and `.usd` gives 3,379,817
triangles, **100.0% with the TEV fully evaluated** (it was 98.3%, with 58,759 triangles in 55
symbols sampling bump), zero display list errors and zero rejected indices. In the audio, ITD
now uses each voice's 32-sample circular line, moving each ear's delay towards its target one
sample at a time, and the surround channel is preserved through to the output, where stereo
presentation encodes it into the out-of-phase Lt/Rt pair. The emission of the indirect matrix in
GLSL was truncated at a parenthesis — `exp2(float(...)` unclosed, and with the scale divided
inside the `exp2` instead of outside, contrary to the C reference — which would have made every
refraction draw fail to compile and vanish from the image; no test read the GLSL as a language,
so one was added that checks balanced delimiters across 4×4×8×7 combinations of matrix, format,
bias and wrap. The MVP percentage stays at 100%: those three items were outside the definition,
and what remains in them is a visual check against a reference. `host-debug` 26/26 and 235/235
unit tests.
