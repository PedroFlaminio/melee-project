# Native port development

The host build is the only build in this repository. It is still a bootstrap base: it does
not start the complete game.

## Quick build

Current requirements:

- CMake 3.25 or newer;
- Ninja;
- a C17/C++20 compiler;
- Python 3.10 or newer for tools and tests.

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
./build/host-debug/port/melee-pc --diagnose
```

On Linux, the ASan and UBSan build is:

```sh
cmake --preset host-sanitize
cmake --build --preset host-sanitize
ctest --preset host-sanitize
```

## Portability inventory

The inventory is heuristic and exists to track the reduction of blockers:

```sh
python3 tools/port_inventory.py --output build/port-inventory.json
```

The JSON separates game/baselib code from the Dolphin SDK and lists matching objects,
assembly, platform APIs, layout asserts, fixed addresses and casts to 32 bits. An inventoried
occurrence is not necessarily a bug; it flags work that needs to be classified.

## Disc inspection and extraction

Inspect a dump without extracting:

```sh
python3 tools/melee_extract.py inspect /path/to/melee.iso
```

Extract a supported `GALE01` copy:

```sh
python3 tools/melee_extract.py extract /path/to/melee.iso assets-local
```

The tool validates the GameCube magic, the game ID, the DOL/FST bounds and the SHA-1 of
`main.dol`. Existing files are not overwritten without `--force`. The extracted directory
holds `manifest.json`, `dvd-index.bin`, `sys/main.dol` and the FST's files. It contains
material from the user's disc and must never be committed or published.

## Scene scheduler

The original HSD object runtime can run without assets. The command creates two scene
objects, pauses the p_link of one of them and runs the scheduler:

```sh
./build/host-debug/port/melee-pc --diagnose-scene-runtime 60
```

It reports how many frames ran, how many objects and processes are alive and how many times
each process was called, along with VI retraces and draw-done callbacks. The paused p_link's
process must finish with zero calls; the other three counters must equal the number of frames
requested.

## OS heap

`OSAlloc.c` and `OSArena.c` are SDK code and carry pointer-width addresses only under
`MELEE_HOST`. Keep the non-host branch untouched when editing them: the console layout of
those structures is what the rest of the SDK assumes.

## Video time

The host's VI layer does not sleep. A retrace happens when the game blocks in
`VIWaitForRetrace` or when the host loop calls `melee_host_video_advance_retrace`. Pick one
of the two as the source of time; using both in the same loop makes the retrace counter
advance twice.

## HSD inspection

An individual HSD file can be validated without relocating in place:

```sh
./build/host-debug/port/melee-pc --inspect-hsd assets-local/path/file.dat
```

The parser keeps serialized 32-bit offsets separate from runtime pointers, which is the basis
for the 64-bit loader.

## Files through the game's own path

The game does not open a file by offset: it calls `lbArchive_LoadSymbols` with a list of
names, which runs `HSD_ArchiveParse`, resolves the externs and asks
`HSD_ArchiveGetPublicAddress` for each symbol. The host implements those four functions in
`port/src/assets/hsd_host_archive.cpp`. The command below repeats that path with the list
`gmTitle_801A1AC0` uses and hands each descriptor to the original loader for its type:

```sh
./build/host-debug/port/melee-pc --load-archive assets-local/GmTtAll.usd \
    TtlMoji_Top_joint TtlMoji_Top_animjoint TtlMoji_Top_matanim_joint \
    TtlMoji_Top_shapeanim_joint ScTitle_cam_int1_camera ScTitle_scene_lights \
    ScTitle_fog TtlBg_Top_joint TtlBg_Top_animjoint TtlBg_Top_matanim_joint \
    TtlBg_Top_shapeanim_joint TitleMark_sobjdesc
```

With no names, the command asks for every public symbol in the file. The sweep does that for
every file on the disc that is a single HSD file:

```sh
./build/host-debug/port/melee-pc --sweep-archives assets-local
```

Three readings of the report:

- `translated` without `loaded` is expected for animations and sprites: an animation tree
  only loads alongside the tree it drives, and the sprite needs the sprite library, which
  does not compile yet.
- `unsupported` is not a failure. It is a symbol whose suffix the host does not translate
  yet, and the game would receive NULL with the report `host HSD archive: cannot translate`.
  Only `failed` counts as an error.
- The type comes from the name's suffix. When adding a type, add the suffix to `kSuffixes`,
  longest first (`_matanim_joint` before `_joint`), and the matching method in the
  materializer. When the on-disc layout is doubtful, survey it first across every file that
  has the suffix: that is how `SceneDesc`'s fog table turned out to differ from what the
  header suggests.

The layer keys what it parsed by the buffer's address, not by the `HSD_Archive`. A test that
parses a file must call `melee_host_hsd_archive_release` with the same buffer when it
finishes, or the descriptors outlive the test.

## Memory boot and the game's loader

The command below brings memory up the way `gmMain` does and loads the title screen's file
through the original `lbArchive_LoadSymbols`:

```sh
./build/host-debug/port/melee-pc --boot-title-archive assets-local
```

The path is the game's: `lbFile` posts the read to the devcom queue, which opens the file on
the host DVD, and waits in `lb_800195D0`. On the host that wait steps the scheduler, and only
then does the read happen.

Two things to watch for:

- Run it outside the test binary. The sequence moves the OS arena and recreates the HSD
  heaps, which would break tests that assume the headless bootstrap.
- If the command stalls at 100% CPU, the disc wait is not seeing the read finish. The cause
  seen so far was the DVD refusing the range: devcom marks a static error, never calls the
  callback, and `waitForDisc` spins. An interrupted `gdb` shows `lb_800195D0` on top; check
  the requested range against the file's size.

## Title scene

```sh
./build/host-debug/port/melee-pc --boot-title-scene assets-local
```

Runs `gmMain`'s boot, reads the data that lives in `main.dol`, and runs `gm_801A4BD4` and
`gm_Scene_Title_OnEnter`. The report counts GObjs through the library's own lists and exits
with an error if the scene lacks what `gmtitle.c` assembles.

How the chain was opened, and how to open the next one:

- Put the entry point you want to reach under a real call in the executable. With
  `--gc-sections`, a function with no caller is discarded together with its references, and
  the link says nothing.
- Read the undefined references with `LANG=C`, group them by source file and decide case by
  case. Preference order: compile the original module; for data that lives in the DOL, read
  it from `main.dol`; a table that names whole swathes of game content (stages, fighters)
  gets linked strongly, because the whole decomp is in the core and a `weak` reference does
  not pull a member out of a static library (the entry silently stays null); a function
  reachable only through a path the scene does not take gets a named stop in
  `port/src/game/unported.c`.
- A data file the code reads directly as a struct (`.ssm`, `.sem`) is big-endian and usually
  relocates 32-bit pointers in place. Survey the on-disc layout before writing the host path,
  as with the file API.
- An `assert` that fails right after a disc read is almost always byte order; a segfault in a
  library allocator is almost always `gmMain` initialization that the host boot does not do
  yet.
- Run the same chain under `host-sanitize` before calling a slice done. Two errors the debug
  build walked through in silence only showed up there: `long` where the console has 32 bits
  (the audio SDK uses `long` for samples; swap it for `s32`/`u32`) and code that walks across
  neighbouring `.bss` objects as if they were one struct, relying on the DOL's ordering. For
  the second, check the range in `config/GALE01/symbols.txt` and, under `MELEE_HOST`, merge
  the objects into a single definition with macros at the original offsets, as in `toy.c`.

## Scene frame loop

```sh
./build/host-debug/port/melee-pc --run-title-scene assets-local
```

Freezes the OS clock, runs the boot, enters the title and runs `gm_801A4D34` until the scene
asks to leave. Exits with an error unless it is the 621 frames of the title's count and time
limit, with no buttons pressed and with frames drawn.

- Freeze the clock before the boot (`melee_host_os_time_freeze`). Frozen, time only advances
  when the game waits for the next alarm in `lb_800195D0`, and every run repeats the same
  frames. Without freezing, the alarms follow wall-clock time and the loop spins while
  waiting.
- A wait that never ends is almost always an interrupt the host does not deliver. On the
  console they arrive in the middle of any wait; on the host they arrive at chosen points:
  alarms in `lb_800195D0`, draw done in `VIWaitForRetrace` and `GXWaitDrawDone`. Read what
  the wait tests for and look for whoever would change that state.
- A scene that draws every frame needs a frame sink (`melee_host_gx_set_frame_sink`), or the
  GX capture grows without bound.
- Debug code reachable from the loop (`gm_801A4970`, screenshot, USB) only runs under a
  `DbLevel` or event condition. Check the condition before porting; if the host never
  satisfies it, put a named stop in `unported.c`.

## Presentation through the window

```sh
./build/host-debug/port/melee-pc --view-title-scene assets-local
./build/host-debug/port/melee-pc --view-title-scene assets-local /tmp/f.bmp 120
```

The first form opens the window and runs until the scene exits or Esc is pressed. The second
draws hidden, writes the requested frame and prints its capture: the textures with format and
size, the views, and the runs in the game's order, each with blend, alpha compare, TEV
program, textures and the box it covers on screen.

- To find what is wrong in an image, match the region against the runs' boxes. The title's
  white rectangles were runs 10 and 15, both with an I4 texture, and the bug was in the
  decoder, not the shader: TEV conformance samples random texels, so it does not cover what
  the decoder produces.
- A scene inside out, or one that disappears, is culling; check the front face before
  touching the projection.
- The presenter keeps one GL texture per image address, and the title's cache decodes each
  texture once keyed by the address of its data and palette. An animation that rewrites an
  image or a palette at the same address needs a different key.

## Game modes in sequence

```sh
./build/host-debug/port/melee-pc --run-modes assets-local 0 1
./build/host-debug/port/melee-pc --run-modes assets-local 0 2 \
    120:START 160:DOWN 200:A 240:A
```

Starts the scene manager's routing in the requested mode (`0` is the title) and runs up to N
modes, one at a time, like `gm_801A4510`'s loop: the current mode becomes the previous one
and the pending one becomes current (`gm_HostBeginGameModes` and
`gm_HostRunCurrentGameMode`, under `MELEE_HOST` in `gm_1A3F.c`). Each mode comes from the
host's table (`port/src/game/game_tables.c`, in place of `gmscdata.c`), with the state's
preload, the `on_enter`, the scene, the frame loop, the `onExit` that picks the next mode and
the memory card wait.

Each script entry is `FRAME[-LAST]:INPUT[+INPUT][@PORT]`. The input is a button (A, B, X, Y,
Z, L, R, START, UP, DOWN, LEFT, RIGHT) or `SX=N`/`SY=N` for the main stick, from -128 to 127.
Without `-LAST` it is held for three drawn frames; with it, through that frame inclusive.
Frames are counted across modes. The port runs from 1 to 4 and is 1 when omitted, and a port
named anywhere in the script is connected from the first frame.

Each mode prints a line, preceded by one line per state scene that ran. At the end come
`vs selection:` (the stage and the character of each open slot in `VsModeData`), `scenes:`
and `route:`. A mode the table does not have, or a state whose scene it does not have, ends
the script with `stopped:`. The tests check those whole lines, frame numbers included.

The `melee-host-vs-match-asset` test's script walks VS selection with two pads, enters the
match and leaves it:

```sh
./build/host-debug/port/melee-pc --run-modes assets-local 0 3 \
    120:START 160:DOWN 200:A 240:A \
    300-315:SY=127 300-315:SY=127@2 320:A 320:A@2 \
    330-337:SX=127 330-333:SX=-127@2 \
    345-354:SY=127 345-354:SY=127@2 360:A 360:A@2 \
    380:START 420-421:SX=-127 425-439:SY=127 445:A \
    680:START 700-715:L+R+A 705-715:START 730-850:B
```

On the CSS the ports start closed, including that of a connected pad. Each pad moves its
cursor up to its own port's HMN button and presses A, picks up the token on the way to Fox's
portrait and drops it with A; START only counts once the ready banner is up. On the SSS the
cursor starts at (0, -13) and moves up to Hyrule Temple.

In the match, pause only works after the HUD comes on (frame 655, after GO) and ten frames
after pausing. L+R+A+START exits as it does in a tournament when one of the four arrives
freshly pressed with the other four held; the script holds L+R+A and presses START on top.
Without the results screen the mode returns to the CSS, and B held there leads to the menu.
The match does not end on its own within a test's time budget (it went past 1,368 frames
without ending, and `host-debug` draws about 13 frames per second), so every script that
enters it needs this exit path.

An entry `FRAME:BMP=file` writes that drawn frame to a BMP through the same hidden presenter
as `--view-title-scene`, and prints the capture's triangles, views and textures. There can be
several; the route exits with an error if one is not written. To look at the image, convert
it with `magick f.bmp f.png`:

```sh
./build/host-debug/port/melee-pc --run-modes assets-local 0 3 \
    ... 640:BMP=/tmp/go.bmp 695:BMP=/tmp/pause.bmp
```

- The frame is the route's global frame, the same one the button entries use. The test's
  match runs from 533 to 707, and the CSS from 243 to 383; 400 is already the SSS.
- The results screen only finishes when all four players are ready (`fn_80178050`): a CPU and
  an empty port become ready on their own, and each human with START on its own port, after
  the panel animation passes frame 50. Each human START toggles between ready and not ready,
  so a second press undoes the first. A script with START only on port 1, or with two STARTs
  per port, hangs on the results forever, with no error and no output, because `--run-modes`
  only writes when a scene or a mode ends. To see where a route stopped, run it under gdb with
  breakpoints that print and continue (`commands` ... `continue`) on the entry functions and
  on the scene's proc.
- A texture the decoder refuses prints `texture N (format 0x..) not decoded` with the reason,
  and the presenter swaps in a white texture. Before hunting for wrong geometry behind white
  quads, check whether the same frame printed that warning.
- The capture is read after the frame's last draw. State that a draw consults and that
  another draw can change must be copied into the capture at the start of the draw;
  consulted at read time, it returns the last draw's value. That was the case for the palette
  under a TLUT name (`melee_host_gx_captured_texture_tlut`): the symptom was "TLUT index
  exceeds palette" with palette sizes unrelated to the texture's format, such as 16 entries
  for a C8.
- A matching function may write through a variable that a rare path leaves unassigned. On the
  console the register still holds the earlier value; on the host the debug build falls over.
  That was `fn_8001E60C`, with a fighter part that has only translation tracks. On a crash,
  read the local variables (`info locals` in gdb) before suspecting the data: a pointer that
  points into a function, such as `HSD_AObjAlloc+81`, is the sign.
- A union of a scalar with bit-fields (`UnkFlagStruct`: `u8 byte` plus `b0`..`b7`) puts each
  bit at one position on the console and another on the host. A write or read through the
  scalar with a non-zero value changes meaning; under `MELEE_HOST`, declare the bits in
  reverse order, as in the command scripts. The symptom is not a crash but a flag stuck at 0:
  that is what kept fighters from being drawn. Zeroing through the scalar does not depend on
  the order.
- The same goes for a union of `s32` with bit-fields written whole from the data:
  `fighter.c` copies each action's `x10_animCurrFlags` into `fp->x594_s32` and reads the
  repeat flag, the part masks and the FigaTree type through the fields. On the host the run
  animation stopped at the end and the script, with every timer already expired, spun forever
  creating effects. The symptom is the process growing hundreds of MB per second in a frame
  that never ends; it is not running out of memory. Run long routes sampling `VmRSS` and
  killing by PID above a threshold, and stop in gdb at the previous frame with `N:FIGHTERS`
  (breakpoint in `melee_host_match_fighter_position`) to get the backtrace of the
  allocations.
- The GX recorder stores each matrix type where GX stores it: position and texture in matrix
  memory, normal (3x3) separately. A lit PObj loads position and normal into the same
  `GX_PNMTXn`, and merging the two makes the vertex lose the camera's translation. The
  symptom is giant geometry stuck to the screen. Before suspecting the skeleton, check the
  draw sequences' boxes in the `BMP=` report and the joints' matrices in gdb: here the two
  pointed in different directions, and the bug was between them.
- A texture the game fills with `GXCopyTex` comes out of the CPU rasterizer
  (`melee_host_gx_copy_efb_to_i4` for the shadow, `..._to_texture` for the colours), which
  draws the frame's capture up to the copy and applies the clears requested before it.
  `FRAME:EFBCOPY` decodes the colour copies the frame uses and requires one with at least 16
  colours, and a breakpoint in `HSD_ImageDescCopyFromEFB` prints how many copies there are,
  of what size and where. A black or dirty area where the texture is applied points either at
  a format the rasterizer does not handle or at geometry the capture does not have. To
  confirm an area is a copy, fill the destination with a fixed colour inside `GXCopyTex`,
  without committing: that is how the match's black band turned out to be the shadow.
- A small function that returns `int` may be reading a pointer through the console's layout:
  `mn_80231634` returns a JObj's `child` as the `int` at +10. The symptom is a SIGSEGV on a
  JObj whose address looks 32-bit (`0x5702d640`) right after the call; look for casts like
  `(HSD_JObj*) mn_80231634(...)`.
- On the results of a completed match, state 2 (`fn_80177920`) accepts any human button and
  stops at the first. A script that presses START on both ports in the same frame only passes
  the winner, and the screen waits forever: press a button first and START on the ports
  afterwards.
- A long route with no trace does not say which frame it is on. `--run-modes` prints each
  scene as it starts (`scene 0xNN from frame N`), and an `N:RULES` every 50 frames is a cheap
  marker for measuring pace, sampled alongside `VmRSS`.
- The match clock only counts down one second at a time, and the shortest rule the menu
  offers is one minute, so a route that waits for time to run out takes 7,200 match frames.
  `FRAME:CLOCK` prints the clock (`119s+3`, seconds plus frames within the second) and
  `FRAME:CLOCK=N` sets it to N seconds: the scene keeps counting from there and ends the match
  on its own, with time up at `0s+59`, which is where `gm_GetMatchOutcome` reads the time-up.
  The clock only advances with the HUD on (frame 655 in the routes), and the entry answers
  `no timer` in a match without a clock. The sudden-death test's route and the long one,
  without the shortcut, give the same scene sequence and the same outcome; the long one takes
  49 s in the `-O2` build.
- The window's keyboard is only exercised by a real event.
  `port/tools/play_keyboard_probe.py` opens `--play` on X11, waits for the match to start
  (reading the `scene 0x02 from frame N` line from the output, which only arrives in time
  with `stdbuf -oL`, because in a pipe the game's stdout comes out in blocks) and sends the
  key with `xdotool keydown --window`, which goes only to that window and does not go through
  desktop focus. Two `FIGHTERS` entries and two `ACTION` entries say what the fighter did:

```sh
DISPLAY=:1 python3 port/tools/play_keyboard_probe.py --press --key d
DISPLAY=:1 python3 port/tools/play_keyboard_probe.py --press --key j --hold 0.3
DISPLAY=:1 python3 port/tools/play_keyboard_probe.py --no-press
```

  With `d` the fighter walks 109 to 131 units and passes through `Dash` (20), with `j` it
  stays put and enters `Attack11` (44), and with no key it stays in `Wait` (14) where it
  spawned. Find the window by the process's PID, not by name: a previous run leaves its title
  in the X server's list for a while, and sending to a dead id is a BadWindow the game never
  sees.
- `port/tools/play_keyboard_match.py` plays the whole route that way: the title, the menu,
  the CSS, the SSS, the match, the pause with the L+R+A+START exit and the results until it
  returns to the CSS, with pad 1's side entirely on real key events (pad 2 stays in the
  script, because the window's keyboard is pad 1 only). The clock is no reference: each key
  goes out when the previous frame's `rules frame N` line arrives, and the `keyup` on the line
  of the last frame the key should be down, which reproduces `330-337:SX=127` as exactly eight
  frames. With the `keyup` one frame late the CSS cursor moved 1.24 units too far and the
  route picked Ness. `--shot file.png` captures the window itself with `import -window`, which
  is the only image possible here: the `BMP=` entries need the hidden presenter, which has no
  window to receive a key.
- Fog goes in between the TEV and the blend, in both paths that draw the capture, and
  `MELEE_HOST_FOG=0` captures everything with `GX_FOG_NONE`. To see what it changes, write the
  same frames with and without and compare the BMPs; on the route the title, the menu and the
  SSS change and the Hyrule Temple match does not, because the match scene installs no fog. If
  the shader and the rasterizer disagree, run conformance: half the cases use a linear fog
  that falls in the middle of the curve at the quad's depth.
- A scene that waits for a button traps the route forever, and the mode loop only comes back
  between modes. `FRAME:STOP` ends the script at that frame, with "stopped at frame N" and
  exit code 0, after the frame's other entries; the summary lines (`scenes:`, `route:`) are
  not printed, and the test checks the `scene 0xNN from frame N` lines that come out live. The
  end-of-route checks do not run either: a route that stops early never fails for a `MOVE`
  with no displacement or a `FALLS` with no KO.
- What the results screen does next depends on the save, which starts zeroed without a card:
  `FRAME:MATCHES[=TOTAL]` reads and writes the VS match total (50 is the smallest that
  unlocks a character) and `FRAME:TROPHY=ID` awards a trophy through the game's own path
  (`fn_80172C78`), which leaves the prize notice pending. The challenger appears for the port
  of the match's "smallest loser", which in the stock route is pad 2's.
- A value that comes from a register on the console disappears on the host. A variable left
  unassigned on one path (`base` in `gm_80168B34`) held whatever MWCC left in the register it
  shares with a parameter, and a function that ends without `return` (`gm_80168BF8`) returns
  the `r3` or the `f1` of the last call; on the host you get whatever is on the stack or in
  some other register. The symptom is an absurd choice: the frame of a texture animation that
  swaps a name, emblem or portrait (the "NO CONTEST" title in a completed match, noise in the
  portraits, the wrong emblem in the HUD). GCC only does the analysis with optimization on: an
  `-O2` build (`build/host-release`, with `-fno-strict-aliasing -fwrapv` and
  `MELEE_HOST_WARNINGS_AS_ERRORS=OFF`) lists the candidates under `-Wmaybe-uninitialized` and
  `-Wreturn-type`, and flags the original `gm_80168B34`. Most are false positives; before
  changing anything, read the DOL's machine code: pull the section's bytes at the address,
  wrap them with `llvm-objcopy -I binary -O elf32-powerpc` and disassemble with
  `llvm-objdump -d`.
- The scripted routes (`--run-modes`, `--run-title-scene`) freeze the OS clock at
  2001-12-03 00:00:00. Frozen at the host's own time, the clock made the seed depend on when
  the route ran: the title draws one `HSD_Rand` per second of the current minute, and the
  results' victory pose comes out of that seed. Two simultaneous runs do not show this,
  because they land in the same second; compare runs at different hours.
- `FIRST-LAST:TRACE=file` records the scene, the seed and each fighter's state per frame,
  with floats as the hex of their bits, and `port/tools/compare_match_trace.py A B` points at
  the first differing field (`--ignore field` passes over a difference already understood).
  It serves to compare two runs, `host-debug` against the `-O2` build, or a route with and
  without `BMP=`.
- To find where the time goes there is `gprof` (this machine has neither `perf` nor valgrind).
  Configure `build/host-profile` with `-O2 -g -pg -fno-strict-aliasing -fwrapv` in the C and
  C++ flags and `-pg` at link time, run the route from a working directory (`gmon.out` lands
  in the current directory when the process ends) and read `gprof -b -p melee-pc gmon.out`.
  Inlined functions add their time to the caller: `finish_draw_locked` took 92% of the route
  with `transform_captured_draw_locked`'s loop inside it, redoing every triangle of the frame
  at the end of each draw.
- `port/src/gx/command_recorder.cpp` and `port/src/gx/tev.cpp` compile with `-O2` in every
  preset, with `-g`: the GX capture runs per vertex and the EFB copies per fragment, and
  unoptimized the cancelled VS route took 83.7 s (29.8 s this way). In gdb, variables in those
  two files can show as `<optimized out>`; to debug one of them, remove the property in
  `port/CMakeLists.txt` locally.
- Without perf, a sample of where the route spends time comes from running it under
  `timeout -s INT N gdb -batch -ex run -ex "bt 12"` with a few different N; three samples were
  enough to show the per-fragment TEV of the copies.
- To measure pace per scene, run the route with `stdbuf -oL`: in a pipe `stdout` comes out in
  one block at the end, and the `scene 0xNN from frame N` lines all arrive together.
- To play, `melee-pc --play assets-local` opens a window and runs the modes from the title at
  60 frames per second, with the OS clock at host time. Keyboard as pad 1: WASD moves the
  stick, the arrow keys the C-stick and the numeric keypad (8, 4, 2, 6) the D-pad; J is A, K
  is B, U is X, I is Y, Q is Z, H is L and L is R (pressed all the way, with the digital
  click) and Enter is START; Esc or closing the window ends the process. The window title
  shows the frames presented per second, twice a second, and sound comes out of the default
  device; the pace is the NTSC field's, 59.94 frames per second. The first gamepad replaces
  the keyboard: D-pad, triggers that click at the end of travel, and Back exits. A mode or a
  scene the host does not have, such as the opening movie that follows the stalled title,
  returns to the title. Script entries override the pad, and `MELEE_HOST_PLAY_HIDDEN=1` draws
  into a hidden presenter, where `BMP=` works: that is how `--play` is checked without opening
  a window. The `-O2` build is the one to use; `host-debug` runs the match below 60 Hz. SDL
  turns SIGTERM into closing the window.
- Sound plays on the routes and in `--play`: on every retrace the host's AX clock runs a 5 ms
  frame per 5 ms of field, and `--run-modes` turns the voices on. `MELEE_HOST_AUDIO=0` turns
  off the voices and `--play`'s device; the game gives the same trace with and without sound.
  `FIRST-LAST:WAV=file` records what the mixer plays while the drawn frames are in the range,
  16-bit stereo at 32 kHz; the header is written when the range ends, so a process stopped
  afterwards (the stalled main menu never leaves on its own) still leaves a valid WAV.
  `MELEE_HOST_AUDIO_AUX=0` leaves the game's reverb and delay out of the mix; that is how a
  route's sound is compared against reference decoders, because the effects send part of the
  sound to the reverb.
- `port/tools/check_route_audio.py build/host-debug/port/melee-pc assets-local` runs
  title → menu → title recording a WAV and checks the `menu01.hps` music and effect 118 of
  `main.ssm` against decoders that do not go through the mixer: windowed correlation at a
  single lag, and the effect with the music subtracted. One sample lost or repeated at a
  block junction in the stream knocks out the following windows.
- To learn what the game plays and when, break in gdb at `AXDriver_8038E8EC` (the `.hps` path)
  and `HSD_Synth_80389334` (the effect id), with `printf "%u", VIGetRetraceCount()` in the
  breakpoint's commands.
- `port/tools/ssm_to_wav.py assets-local/audio/main.ssm --out dir` decodes a sound bank's
  ADPCM voices into WAV and measures peak, RMS, clipping and mean step. It is the reference
  for the host's AX mixer: a wrong decode shows up as mass clipping. With
  `--compare-host build/host-debug/port/melee-pc` it runs `melee-pc --decode-sound-bank` on
  the same bank and requires the same samples in every voice.

- The title state's preload (`lbDvdPreload_3`) keeps every preload heap, and the scene's
  `on_enter` registers the title demo's files: fighters, stage and effects. They load in the
  background while the title runs, through devcom, and heaps 4 and 5 sit in ARAM. That is why
  the mode reaches `ftdata.c`, each character's files and the ARQ, which the scene alone did
  not reach.
- A callback the console delivers by interrupt must not run inside the call that triggers it.
  devcom posts the last ARAM transfer and only afterwards clears the request; with the ARQ
  completing inside `ARQPostRequest`, the callback returned the request to the free list
  first, the queue pointed at it, and an already-freed request ran again. The symptom was the
  `devcom.c:36` assert several frames later. Whatever the host completes on its own goes
  through `melee_host_dvd_schedule_backend_task`, on the following step.
- A host value that disappears between being written and being read is almost always a write
  from another `.bss` object. A hardware watchpoint finds the writer: `break` where the value
  is still right and, there, `watch -l variable`. That is how `tydisplay.c` turned up sizing a
  pointer array as `0xB0 / sizeof(HSD_Archive*)`: 44 entries on the console, 22 on the host,
  and the loop that clears it writes 43.
- Under `host-sanitize`, run with `ASAN_OPTIONS=detect_leaks=0`, as ctest does. The boot and
  the scenes do not free what they allocate, LeakSanitizer ends the process with code 1 and,
  with output redirected, the command's report is lost because `std::cout`'s buffer is not
  flushed.
- A symbol whose layout only a game C header describes gets translated in C: the modules'
  `types.h` do not compile as C++ (a member named `u8` changes the type's meaning). The
  translator is registered by name (`melee_host_hsd_register_translator`) in
  `port/src/game/game_data_translators.c`, reads the file through the file API's C reader —
  which checks offsets, relocations and null fields — and fills the game's own types field by
  field, bit-fields included. Survey the on-disc layout first: the event table had one
  parameter per event, each with its own shape, and anything without a single shape stays out
  with the reason written down.
- SIS text is big-endian on disc and in the buffers the game assembles, and the interpreter
  was reading words in place. A `*(u16*)` or `*(s16*)` read from the stream becomes
  `HSD_SisLib_ReadU16` or `HSD_SisLib_ReadS16` under `MELEE_HOST`.
- Command scripts (fighter, item, colour overlay) reach the game through the file API already
  converted to native order (`melee_host_hsd_reader_command_stream`), and the game reads them
  through the structs in `port/src/game/host_command_layout.h`. When changing a command struct
  or the `ColorOverlay_x8_t` union in `lb/types.h`, run
  `port/tools/gen_host_command_layout.py`, which regenerates the header and the C check; the
  `melee-host-command-layout-generated` ctest flags when that has fallen behind. A read by
  cast of `u8`, `u16` or `s16` from the script becomes `CMD_U8`, `CMD_U16` or `CMD_S16`; the
  whole `u32` word does not change. A pointer in the script only exists as a subroutine or
  goto operand, and on the host it is the distance to the target (`rel`).
- The generator only reads `lb/types.h`. A bit-field struct over the script's word declared
  elsewhere needs the reverse order written by hand under `MELEE_HOST`: `itAnimlistCmdUnk`
  (`itanimlist.c`) and `gmScriptEventDefault` (`ft/types.h`), through which `ftaction.c`
  reads the opcode. Forgotten, it does not break at the read: an opcode taken from the low
  bits that comes out 0 is Reset, which merely ends the script, and the error shows up far
  away, in a loop or subroutine command that runs with no stack.
- Every variadic pointer list terminated by `0` needs `VA_END_PTR` when the unit enters the
  build. In the debug build the `0` may pass by luck; under ASan the slot's high half comes in
  dirty and the loader writes to an address with the low 32 bits zeroed, such as
  `0x55ae00000000`.
- An address stored in an `int` does not crash where it is truncated; it gives a half value
  further along. Before widening one side, follow the value to where it is used: on the memory
  card the images' address passes from `int` to `int` all the way to the 32-bit command queue,
  and only the write, which needs a card, reads it.
- Converting an out-of-range float to `u8` is undefined behaviour, and UBSan flags it. The
  console keeps the low byte, which is what `(u8) (s32)` gives.
- The stick reaches the game after the pad's clamp: 127 becomes 80. The CSS cursor moves
  (80² - 200) × 0.0002 = 1.24 per frame, and the SSS one (80 - 30) × 0.03 = 1.5. A menu script
  with a cursor is counted in frames from that, and moving one axis at a time avoids the
  diagonal's octagonal clamp.
- Do not calibrate a script from the image. A gdb script with a `break` on the scene's
  `OnFrame` and `commands` that print the state (cursor, ports, token, or the stage under the
  cursor with `call lb_8000B1CC(jobj, 0, $v)` for each icon's world position) shows frame by
  frame what the input did. That is how the clamp and the closed port turned up. Run it with
  `gdb -batch -ex 'set $arg_from = N' -x script.gdb --args ...`.
- "Memory Empty" in `sislib.c` is the scene's SIS text pool. Before touching its size, get a
  picture of the pool at the panic: a gdb Python command walking `used_head` and `free_head`
  summing `size` shows how many blocks there are, of what sizes, and how much is left. The
  host's pool is already twice what was asked for.
- A scene the host table does not have ends the mode before the state's preload, so the
  state's `on_enter` does not run: data it would assemble (the match's `StartMeleeData`, for
  instance) does not exist yet when the script stops. Read the selection through
  `melee_host_vs_selection_get`.
- Display lists, vertex arrays, images and keyframes are still big-endian on the host. Game
  code that reads those payloads on the CPU, rather than through GX, has to assemble the
  values from the bytes: that was the case for the shape animation in `pobj.c`, which copied
  floats with `memcpy`. The symptom is not a crash but geometry with absurd coordinates or
  NaN; UBSan flagged it further along, in the host lighting's conversion to `u8`. On seeing a
  NaN in a capture, walk up to whoever produced the value before guarding the conversion.
- An ASan `global-buffer-overflow` on a game table is usually a read the console makes past
  the end that lands in the DOL's next object. Check the size in `config/GALE01/symbols.txt`,
  see which symbol comes next and read the bytes of the extracted `main.dol` by address (the
  DOL header gives offset, address and size of each section). If only a few fields are read,
  an extra entry under `MELEE_HOST` with those bytes reproduces the console, as in the SSS's
  stage table; if the code walks across whole objects, merge them into a single definition, as
  in `toy.c`.
- Every `.c` in `src/melee` is already in the core (except `gmscdata.c`), so reaching a new
  scene needs no new source in CMake. What shows up at link time is another matter: a
  "multiple definition" against `unported.c` means the real module is now being pulled in and
  the stop must go; an undefined reference is usually SDK, baselib outside the core, or data
  that only exists in the DOL. Original code that cannot run on the host as written, such as a
  32-bit in-place relocation, gets a named stop inside its own module under `MELEE_HOST`, with
  the reason, like `psInitDataBankLocate`.
- A function type GCC refuses (`incompatible-pointer-types`) is almost always the declaration
  and the definition disagreeing. Before picking a side, look at what the callers pass and
  what the body does with the value: `on_demo_init` looked like `bool` on nearly every stage,
  but Final Destination compares the parameter against 26.
- `host-sanitize` discards the same things at link time as `host-debug`, and every compiled
  module is instrumented. If a module nobody calls starts demanding symbols only in the
  sanitized build, check that `-fsanitize-address-globals-dead-stripping` and
  `-Wl,-z,start-stop-gc` still reach the compiler and the linker: without them ASan's globals
  metadata keeps alive everything the module names. Do not go back to excluding files from
  instrumentation; when the exception list went away, a stack overflow and out-of-array reads
  it had been hiding showed up.
- A PowerPC intrinsic replaced by a macro in `src/placeholder.h` must give what the
  instruction gives, not what the name suggests. `__frsqrte` is the estimate of 1/sqrt(x)
  (`frsqrte`): the roughly 50 places that use it (`sqrtf_store`, `acosf` and `asinf` in
  `lbtrigf.c`, collision, particles, items, dynamics) refine it with Newton steps for
  1/sqrt(x) and multiply by x. The macro returned `sqrt(x)`, which only converges near x = 1:
  at x = 2 the root came out negative and at 44 it gave -1.9e41. The symptom was the legs' IK
  (`lbBgFlash_80021410`, which `ft_80089B08` runs on landing and while idle) with absurd
  lengths and a NaN angle; the thigh's matrix and everything below it stayed NaN until the
  next animation dirtied the joint, and the envelope took Mario's and Link's legs with it.
  Hiding DObjs and switching off the hat's dynamics had nothing to do with it. To find who
  writes a NaN into a matrix, set a hardware watchpoint in gdb on the joint's `mtx[0][0]` with
  `gdb.Breakpoint(expr, gdb.BP_WATCHPOINT, gdb.WP_WRITE)` and a `stop()` that only stops on
  `math.isnan`; the backtrace points at the arithmetic, and `host-debug` shows the locals. A
  route with a different character leaves the Fox script with
  `break Player_80031AD0 if slot == N` and `set player_slots[N].ckind = CKind_Link` in the
  breakpoint's commands.
- A route that needs another character no longer depends on gdb: the CSS cursor moves
  `(80*80 - 200) * 0.0002 = 1.24` units per frame with the stick at 127 (the pad delivers 80),
  and the icons' boxes are in `mncharsel.c`. With the save the recording uses, the CSS shows
  only the unlocked characters in seven columns. Starting from the Fox script, pad 1 reaches
  Mario by holding up for 15 frames instead of 10 (same column, one row up) and pad 2 reaches
  Link by holding right for 30 frames on the middle row; the token drops with A two frames
  after the last stick frame. `vs selection:` confirms the choice (`0=8 1=6` for Mario and
  Link).
- With `GXSetChanCtrl` disabling a channel's lighting, GX passes on the material colour only:
  neither the ambient register nor the lights come in. The host's evaluator started from the
  ambient and multiplied, so every unlit draw came out painted by whatever ambient colour the
  last material had left in the register. Hyrule Temple draws its scenery that way: the stage
  was dark the whole time and red while Link's boomerang flew. When a whole drawing changes
  tone without the geometry changing, compare the captured vertices' raster colour
  (`captured_vertices`) between two frames before going looking for lights.
- Decompiled code that writes into a neighbouring stack slot to match MWCC
  (`*(&y + 6) = ...` in `it_802A4BFC_sqrtf_offset`, `itlinkhookshot.c`) corrupts the caller's
  frame on the host. The symptom was a SIGBUS with a destroyed backtrace in `host-debug` and
  no visible effect at `-O2`; ASan names the object ("stack-buffer-overflow ... 'y'"). Guard
  the trick with `#ifdef MELEE_HOST` and use the variable itself; the console branch does not
  change.

## Stage parameters (`yakumono_param`)

Every `Gr*.dat` carries a public symbol called `yakumono_param`, and each stage declares its
own struct for it inside its `grXXX.c`. The host cannot tell the layouts apart by name, and
telling them apart by size is what findings R03–R05 of [`review-fa257ed.md`](review-fa257ed.md)
removed. The stage's identity comes from `melee_host_stage_current_grkind()`, which reads the
`stage_info.grkind` that `Ground_801C0754` sets before the archive read that runs the
translators.

```sh
python3 port/tools/gen_yakumono_layout.py            # rewrite the generated files
python3 port/tools/gen_yakumono_layout.py --report   # what parses, and what does not
python3 port/tools/gen_yakumono_layout.py --check    # what the ctest runs
```

It parses the structs out of `src/melee/gr` — the `.c`, its headers and `gr/types.h` — and
writes `port/src/game/yakumono_param.h` and `.c.inc`. Rerun it when a stage's struct changes;
`melee-host-yakumono-layout-generated` fails when that is due.

- Offsets are computed with the console's layout (four-byte pointers) and cross-checked against
  the decomp's own `/* 0x.. */` comments, so a struct whose comments disagree is reported
  rather than generated.
- Each translator checks the block's extent before reading. A mismatch means the stage's struct
  moved under the generator, not that the data is unusual — that is what caught Castle and
  Icicle Mountain.
- Pointers in these structs are colour-animation scripts handed to `grMaterial_801C9604`, so
  they go through `melee_host_hsd_reader_command_stream`, the same reader Battlefield's
  overlays use.
- A stage that declares `yakumono_param` as `void*` and never reads it, or never mentions it,
  needs no layout; its bytes are passed through untranslated.

## Sweeping the stages

Every stage the select screen offers, entered and reported:

```sh
python3 port/tools/sweep_stages.py                     # all 30
python3 port/tools/sweep_stages.py --stages 14,23,31   # a few
python3 port/tools/sweep_stages.py --binary build/host-sanitize/port/melee-pc
```

It walks the real menus to the select screen, then forces the stage with the `FRAME:STAGE=KIND`
route entry, which sets the same `force_stage_id` the game's own Training and Tournament modes
set (`melee_host_sss_force_stage`). Steering the cursor instead would reach only the squares a
save unlocks and would need the icon layout, which lives in the model rather than a table.

- A stage passes when the match scene `0x02` comes up and the route reaches its stop frame.
- A refusal names its own cause and beats the signal the `OSPanic` after it turns into, so read
  the `refused` line rather than the SIGABRT.
- `random_cpu_matches.py` is the other half: it varies characters but plays every match on
  Hyrule Temple (`STAGE_KIND = 14`), so the two together cover the roster and the stage list,
  but not yet their combinations.

## Scene loading through the object layer

The command below materializes the file's descriptors in host layout and calls
`HSD_JObjLoadJoint`, which is the same entry point every scene in the game uses. It reports
what the original loaders built:

```sh
./build/host-debug/port/melee-pc --load-scene \
    assets-local/GmPause.dat ScGamPause_scene_data
```

The optional third argument picks the model inside `SceneDesc.models`. A public symbol that
names a joint directly goes through the other command:

```sh
./build/host-debug/port/melee-pc --load-joint \
    assets-local/PlMrNr.dat PlyMario5K_Share_joint
```

The PObj and display-list block counts can be compared against those `--inspect-pobj`
produces through the separate reading schema; the two routes read the same file by
independent paths, so a divergence between them is a sign of a bug in one of them.

When extending the materializer, prefer refusing a field you do not yet know how to translate
over guessing its shape. A wrong pointer handed to the original loaders shows up much later,
far from the cause.

## Animation

The command below loads a model, attaches an animation through `HSD_JObjAddAnimAll` and
advances N frames with `HSD_JObjAnimAll`, reporting how many joints moved:

```sh
./build/host-debug/port/melee-pc --animate-joint \
    assets-local/GmTtAll.dat TtlMoji_Top_joint \
    assets-local/GmTtAll.dat TtlMoji_Top_animjoint TtlMoji_Top_matanim_joint 200
```

Use `-` in place of a symbol the file does not have. The animation file can be a different
one: that is how a character keeps its model and its moves apart.

Two readings of the report avoid a wrong diagnosis:

- `AObj frame` is the only proof that the animation advanced. It counts N-1 for N calls,
  because the first interpretation uses a rate of zero due to `AOBJ_FIRST_PLAY`.
- `joints moved` can be zero with the animation running perfectly: a material animation
  changes colour and texture without touching the skeleton. Look at the frame before
  concluding that nothing worked.

Character animations live in one file per character holding several HSD files in sequence,
one per action. List them and play one by name:

```sh
./build/host-debug/port/melee-pc --list-animations assets-local/PlMrAJ.dat
./build/host-debug/port/melee-pc --animate-named \
    assets-local/PlMrNr.dat PlyMario5K_Share_joint \
    assets-local/PlMrAJ.dat PlyMario5K_Share_ACTION_WalkMiddle_figatree 45
```

That path does not use the HSD trees: a character uses `FigaTree`, Melee's own format, and
`lbAnim_8001E6D8` applies it directly to an `HSD_JObj`. The bone mapping is positional, so a
model and an animation from different characters will attach without error and produce
garbage; check that the symbols' prefixes match.

A freshly loaded AObj already plays at one frame per call, because `HSD_AObjAlloc` leaves
`framerate` at 1.0. Setting the rate is for choosing another speed or the reverse direction,
which is what the game does per fighter action.

## Rendering through the original layer

The same two commands with `--render-` instead of `--load-` draw the tree through
`HSD_JObjDispAll`, in the original render callback's three passes, and report what reached the
GX recorder:

```sh
./build/host-debug/port/melee-pc --render-scene \
    assets-local/GmPause.dat ScGamPause_scene_data
./build/host-debug/port/melee-pc --render-joint \
    assets-local/PlMrNr.dat PlyMario5K_Share_joint
```

Two lines of the report are worth more than the triangle count:

- `display list errors` other than zero means the stream the original code handed to GX is
  not one the host can follow.
- `rejected vertex indices` other than zero means an index fell outside the array and the
  attribute was discarded, so the capture is incomplete. This does not show up in the
  geometry: the vertex merely lacks that attribute. Treat it as an error, not a warning.

The space the vertices come out in depends on the view the command asks for. The `--render-`
commands use the scene's camera, like the game, and deliver view space. The preview commands
ask for an identity view, which leaves the matrices loaded into GX as world transforms,
because what moves the camera there is the viewer itself:

```sh
./build/host-debug/port/melee-pc --view-scene \
    assets-local/GmPause.dat ScGamPause_scene_data
./build/host-debug/port/melee-pc --view-joint \
    assets-local/PlMrNr.dat PlyMario5K_Share_joint
```

The window shows the geometry the original display path drew. Each draw is coloured by its TEV
program evaluated per fragment, with the textures of every map the stages sample and the
coordinates the original texgen generated. `--view-pobj` still shows what the reading schema
decodes on its own, with a MODULATE stage in place of the material, and serves as a second
opinion when the two images disagree.

The `--render-` commands list the captured states, one per line. It is worth reading those
lines before blaming the geometry: `blend=1` with `zwrite=0` is HSD's normal translucent mode,
`cull=0` marks a two-sided object, and an `alpha` other than `7@0` means the material clips by
alpha.

The `TEV evaluated per fragment` line says how many triangles have their TEV program
reproduced in full and how many use something the port does not model yet; each program's
`tev N` line says what is missing (`unmodelled=bump texgen`). The preview generates one GLSL
shader per distinct program from `port/src/gx/tev.cpp`, with the hardware's integer
arithmetic, and registers and konst come in as uniforms, so the same material in another
colour does not generate another shader.

When touching the evaluator or the generator, run conformance. It draws each pair of TEV
program and pixel state from the capture into a one-pixel target, with rasterized colours and
random texels, and requires the pixel read back to be what `melee::gx::evaluate_tev` and the
alpha test compute, discard included:

```sh
./build/host-debug/port/melee-pc --tev-conformance-joint \
    assets-local/PlMrNr.dat PlyMario5K_Share_joint
./build/host-debug/port/melee-pc --tev-conformance-scene \
    assets-local/GmPause.dat ScGamPause_scene_data
```

A divergence means the GLSL and the reference disagree. The reference is the one with unit
tests against the hardware's formula, so start by suspecting the generator.

To get an image without opening a window, set `MELEE_HOST_SCREENSHOT` to a `.bmp` path; the
`--view-` command renders one frame off-screen and exits:

```sh
MELEE_HOST_SCREENSHOT=/tmp/mario.bmp ./build/host-debug/port/melee-pc \
    --view-joint assets-local/PlMrNr.dat PlyMario5K_Share_joint
```

A model on its own has none of the stage's lights. The façade's render registers an ambient
light and an infinite one through the original `HSD_LObj`, as it already did with the stand-in
camera. Without them every lit material comes out black: the channel's ambient is the
material's ambient multiplied by the current ambient light, and with no ambient light it is
zero. A black preview with clean conformance is almost always this, not the shader.

One detail that trips up anyone touching this: `HSD_TExpSetReg` assembles the register values
in an uninitialized local array and writes only the components the expression names. What is
left over is stack garbage that reaches GX. It does not affect the image, because no stage
reads those components, but any comparison of TEV state has to ignore them, or the same
material counts as several and the count changes between builds and between runs.

In the window, F flips the front face. The convention adopted is GX's, clockwise as front, and
it has not been verified visually: if a model shows up inside out, that key is the first test.

When comparing against `--inspect-pobj`, remember the two routes do not cover the same set.
The schema walks every model in the scene and draws everything; the original path draws one
model per call and skips hidden objects and PObjs that cull both faces. Compare only on a
single-model scene, and check the `not drawn` line before concluding the divergence is a bug.

To test the DVD bridge against a genuinely extracted file without starting the game, read a
sample of the resource by its FST name:

```sh
./build/host-debug/port/melee-pc --read-resource assets-local DbCo.dat
```

## Rules during the bootstrap

- Host code uses `MELEE_HOST`. The non-host branch of a decompiled file is the console's
  behaviour and stays readable as such.
- New port code treats warnings as errors.
- Decompiled code keeps the legacy warning set, without reformatting or cosmetic casts.
- No game asset enters the repository or the public CI artifacts.
