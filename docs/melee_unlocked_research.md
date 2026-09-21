# Melee Unlocked: references for accelerating the native port

Survey from 15 September 2026, based on the two local checkouts.

| Base examined | Revision |
| --- | --- |
| `melee-unlocked/` | `6e2d56a6351e95b7a7d6d487a29f0cbd4e6b9643` — `Make settings closed state passive` |
| Our `melee/` | `41816018339db6e05f0e02767ee11562424a6c81` — plus the local changes already present |

## Summary and recommendation

The best immediate use is to treat Melee Unlocked as a behavioural reference, a source of
tests and an example implementation of the console's services. The largest potential gains are
in audio, gameplay validation and GX rendering. Adopting its static recompilation as a base
would be an architectural change, with its own integration cost and Linux support burden.

For our port's current stage, this is the recommended order:

1. Consolidate the Fox/Fox match through to a stock victory and use its states as a
   reproducible basis for comparison. The test for that route **already exists in the
   worktree**.
2. Complete AX voice playback and produce verifiable audio.
3. Fix graphical differences with captures and equivalent GX states.
4. Broaden characters/stages and persist saves.
5. Handle high refresh rate and online after establishing that base.

This document distinguishes code observations, results actually run here, and claims made by
the project's documentation. No Melee Unlocked match was played, and no Slippi compatibility
check was performed.

## 1. The architectural difference changes what we can reuse

| Aspect | Melee Unlocked | Our port | Practical consequence |
| --- | --- | --- | --- |
| Game code | Translates the PowerPC DOL to C++; includes Gecko/Slippi codes by default | Compiles the decompilation's C with `MELEE_HOST` | The generated code does not directly replace our C modules |
| Memory | 24 MiB guest RAM, 32-bit addresses and big-endian reads/writes | Native 64-bit objects and pointers, materialized descriptors | The asset translators are still needed here |
| Console SDK | HLE: host functions receive registers from a PowerPC CPU represented in software | Facades with a C ABI and host-adapted types | Reusing the logic means moving the call boundary |
| Video | GX state through registers, HLSL shaders, D3D12 | GX API recorder, GLSL shaders, SDL/OpenGL | Formulas and test cases transfer more readily than the backend |
| Executable platform | Windows x64/MSVC, AVX2 options and Win32 services | Current development on Linux | It is not a library ready to link into our executable |
| Rollback | Copies regions of guest memory with specific exclusions | State spread across native objects and allocations | Savestates need a strategy of their own |

Sources: [Unlocked's CMake](../../melee-unlocked/port/CMakeLists.txt),
[PPC context and memory](../../melee-unlocked/port/runtime/ppc/ppc.h),
[HLE ABI](../../melee-unlocked/port/runtime/hle/hle.h),
[our CMake](../port/CMakeLists.txt) and
[HSD materialization](../port/src/assets/hsd_materialize.cpp).

Unlocked preserves the console's memory layout. A character running there therefore does not
demonstrate that a reusable 64-bit `ftData*` schema exists. In our code the character-specific
record is still `ftDataFox`, in
[game_data_translators.c](../port/src/game/game_data_translators.c).

There is one option useful for comparing against the unmodified game:
`port/recomp/recomp.py --no-slippi`. It disables the inclusion of the Slippi tables in the
translation. Its existence was confirmed; a vanilla build was not run. Do not compare our
vanilla behaviour directly against a modified build without aligning codes, rules, saves, RNG
and inputs.

## 2. Our port's actual starting point

The history in [port-mvp-progress.md](port-mvp-progress.md) records Fox/Fox on Hyrule Temple,
movement, attack, blaster, KO/respawn under probing, and the results route after a
cancellation. That document's introduction still shows an old estimate of 40%, while the
history reaches 87%; those numbers are not a fresh measurement from this survey.

The worktree is ahead of parts of the documentation:

- [port/CMakeLists.txt](../port/CMakeLists.txt) already defines
  `melee-host-vs-stock-match-asset`: it changes the rules through the menu, picks one stock,
  makes P1 fall and requires a result with P2 as the winner, stocks `0/1`, and a return to
  character selection.
- [match_trace.c](../port/src/game/match_trace.c) already exposes position, action, falls,
  rules and result; [main.cpp](../port/src/main.cpp) consumes `FALLS`, `RULES` and `RESULT`
  events in the script.
- [baselib_support.c](../port/src/os/baselib_support.c) still returns `NULL` from
  `AXAcquireVoice`: the synth initializes, but no voice is made available.
- The renderer already has TEV shader generation and caching; fog and indirect state recorded
  in the GX layer do not mean those effects are applied by the presented shader.

So the next step towards the KO is to verify and consolidate the existing test. Our port's
suite was not re-run in this task; the test's presence is not treated as evidence that it
passes. The six pre-existing local changes were preserved.

## 3. Map of references by expected return

The costs below are relative adaptation estimates, not schedules.

| Priority | Reference in Unlocked | Application here | Relative cost |
| --- | --- | --- | --- |
| P0 | [`validate_native.py`](../../melee-unlocked/tools/validate_native.py), [`replay_compare.py`](../../melee-unlocked/tools/replay_compare.py) | Compare match states and locate the first divergent frame | Low/medium for the method; high for full equivalence |
| P0 | [`ax_ucode.cpp`](../../melee-unlocked/port/runtime/hle/ax_ucode.cpp), [`ax_ucode_test.cpp`](../../melee-unlocked/port/tests/ax_ucode_test.cpp) | AX decoder/mixer, voice state writes and tests with known samples | Medium/high |
| P1 | [`gx_shader.cpp`](../../melee-unlocked/port/runtime/gx/gx_shader.cpp), [`gx_regs.h`](../../melee-unlocked/port/runtime/gx/gx_regs.h) | Indirect TEV, fog, swaps and depth | Medium, per effect |
| P1 | [`texture_snapshot.h`](../../melee-unlocked/port/runtime/gx/texture_snapshot.h), [`texture_snapshot_test.cpp`](../../melee-unlocked/port/tests/texture_snapshot_test.cpp) | Ensure each draw retains its texture and palette | Low, to adapt the tests |
| P1 | [`hle_card.cpp`](../../melee-unlocked/port/runtime/hle/hle_card.cpp) | Saves in a GCI folder and the CARD API contract | Medium |
| P1 | [`hle_dvd.cpp`](../../melee-unlocked/port/runtime/hle/hle_dvd.cpp) | Worker I/O with deterministic delivery | Medium; our queue already exists |
| P2 | [`ppc.h`](../../melee-unlocked/port/runtime/ppc/ppc.h), [`ppc_runtime.cpp`](../../melee-unlocked/port/runtime/ppc/ppc_runtime.cpp) | Reference for floating-point differences | High for full equivalence |
| P2 | [`authored_pose.cpp`](../../melee-unlocked/port/runtime/gx/authored_pose.cpp), [`subframe.cpp`](../../melee-unlocked/port/runtime/gx/subframe.cpp) | Presentation above 60 Hz without speeding up gameplay | High |
| P3 | [`slippi_online.cpp`](../../melee-unlocked/port/runtime/hle/slippi_online.cpp), [`slippi_net.cpp`](../../melee-unlocked/port/runtime/hle/slippi_net.cpp) | Study the protocol, snapshots and synchronization | Very high in our native layout |

## 4. Validation: probably the quickest win

### 4.1 Separate three questions

| Question | Technique found | Limit |
| --- | --- | --- |
| Does the renderer alter the simulation? | `validate_native.py` compares checkpoints in headless, hidden window, threaded renderer and authored mode | Compares Unlocked against itself |
| Does the game reproduce the reference? | `replay_compare.py` compares the states of a `.slp` recording against a new run | Observes only part of the state and has coverage gaps |
| Do the extra frames have a different image? | [`diff_captures.py`](../../melee-unlocked/tools/diff_captures.py) counts differing pixels between PPM captures | A visual difference does not demonstrate a correct pose or lower latency |

`validate_native.py` requires the requested number of checkpoints and rejects invalid
MMIO/FATAL diagnostics. The runtime produces hashes of CPU, RAM and ARAM; the `events` field
summarizes part of the event state, it does not serialize the whole queue. The script itself
states that it does not verify equivalence with Dolphin nor the full rollback state.

### 4.2 Concrete gaps in the replay comparator

In the revision examined, `replay_compare.py`:

- Compares `state`, `x`, `y`, `facing`, `percent`, `stocks` and `char`, per player/follower.
  Although it reads `shield`, it does not include it in the final comparison.
- Uses the **intersection** of the frames: an incomplete replay can pass if the frames they
  have in common are equal. It does not require the same temporal coverage.
- Reads settings and inputs, but does not compare them in the final loop.
- Compares floats as Python numbers, not as binary representations.
- Prints the executable's exit code, but does not use it directly as a failure condition as
  long as it still finds a recording to compare.

When adapting it, require frame/player coverage, a compatible configuration and a successful
run. Use binary comparison when the goal is bit-for-bit determinism; numerical tolerance only
serves an explicitly approximate criterion. The current parser should also not be assumed to
be a generic reader for every `.slp` version.

### 4.3 Proposed application in our code

Extend the existing `match_trace` facade to emit, per tick, a canonical record: scene, RNG,
inputs, action, position, velocity, damage, stocks, relevant flags and result. Use stable
slots/IDs for objects. Do not hash whole native structs: padding and 64-bit addresses
introduce differences that are not gameplay differences.

First compare two of our own runs with the same script and assets. Then compare presentation
off against on. Finally confront an external reference under the same conditions and with the
same sampling point in the frame.

**Suggested acceptance:** the current route through to the results produces the same records
across repeated runs; any divergence reports the first tick, entity, field and values. The
stock test keeps passing under the conditions already defined in CMake.

## 5. Audio: a concrete implementation to study

Unlocked separates three layers:

1. The recompiled AX library builds command lists and voice parameter blocks; the AI/DSP
   bridges are in [`hle_stubs.cpp`](../../melee-unlocked/port/runtime/hle/hle_stubs.cpp).
2. `ax_ucode.cpp` interprets those commands and mixes audio, accessing RAM/ARAM through a
   callback interface defined in
   [`ax_ucode.h`](../../melee-unlocked/port/runtime/hle/ax_ucode.h).
3. [`host/audio.cpp`](../../melee-unlocked/port/runtime/host/audio.cpp) delivers blocks of
   32 kHz stereo PCM to the device through WASAPI, with a WinMM fallback.

The mixer has a boundary small enough to study in isolation: its test compiles and passes on
Linux. That does not remove the integration work. Our port has to implement voice management,
synchronize the parameters, advance the DSP in virtual time and return the state the synth
expects. Wiring an SDL output to the current `AXAcquireVoice` is not enough.

Proposed sequence:

1. Exercise a synthetic ADPCM voice and validate PCM, current address and termination.
2. Implement allocation/release and the parameter updates `synth.c` requires, choosing an
   explicit representation for the AX blocks.
3. Connect ARAM and callbacks to the scheduler; check real menu/attack sound.
4. Record WAV before depending on real-time output.
5. Feed the SDL output through a ring buffer and measure underruns/overruns.

Unlocked's audio backend handles the difference between the simulation's clock and the sound
card's clock by gradually adjusting the buffer's consumption rate. That technique transfers;
WASAPI/WinMM are not the right backend for our Linux. The buffer size has to be measured here,
not adopted as an ideal value.

[`jukebox.cpp`](../../melee-unlocked/port/runtime/hle/jukebox.cpp) contains HPS/DSP-ADPCM
music reading with looping, but the triggering responds to the Slippi commands that replace
the original music. It is a format reference, not an automatic replacement for the vanilla
streaming path.

[`wav_stats.py`](../../melee-unlocked/tools/wav_stats.py) helps detect silence and amplitude.
RMS does not prove fidelity: also check duration, looping, channels, clipping and expected
samples.

## 6. GX: transfer the semantics and the tests

### 6.1 Textures and palettes must belong to the draw

`TextureSnapshotCache` preserves image and palette bytes in immutable snapshots, deduplicated
by content. The test verifies a palette change at the same address, a change in a later mip,
and validity after clearing the source/cache.

This connects directly to the TLUT bug recorded in our history: capturing the state after the
last draw can use a different palette from the one that existed when the object was drawn. The
fix is already in our history; the gain is broadening the regression with Unlocked's cases.

Do not replace our bounds-checked readers with their decoder without keeping the current
guarantees. The snapshot test is small and does not certify the safety of every format or of
malformed streams.

### 6.2 Fog, indirect TEV, swaps and depth

`gx_shader.cpp` offers concrete HLSL generation paths for fog, indirect texture lookups,
channel selection and depth computation. Use it alongside `gx_regs.h` to identify the
formula's inputs and express them in our [tev.cpp](../port/src/gx/tev.cpp), as each scene
requires.

D3D12's depth/projection conventions must not be copied literally to OpenGL. Validate each
effect with controlled state and an equivalent capture. `a_bump` in indirect TEV and
`GX_TG_BUMPn` coordinate generation are not automatically the same implementation; our code
still reports `bump texgen` in `tev_unmodelled_features`.

### 6.3 EFB copies and the frame queue

The EFB is the console's framebuffer; its copies can feed textures used later. In
[`frame_queue.h`](../../melee-unlocked/port/runtime/gx/frame_queue.h) and
[`threaded_backend.cpp`](../../melee-unlocked/port/runtime/gx/threaded_backend.cpp), frames
are processed in order; when there is a delay, the renderer may skip presenting intermediate
frames while still executing the commands.

If we parallelize our renderer, discarding intermediate work means preserving those
dependencies. Test shadow/refraction and transitions with a lagging consumer. The current code
limits the queue to 32 frames; the tracker mentions a queue of four at an earlier stage. Those
are different historical states, not a recommendation for our latency.

### 6.4 Shader cache and measurement

Unlocked normalizes shader keys by the relevant registers and includes hashes of the
shader/layout/backend sources in the cache's identity. There are precompilation recipes and
generic pipelines used while the specific shader compiles. The tracker reports that simply
skipping draws made objects flicker.

Our [`sdl_gl_renderer.cpp`](../port/src/render/sdl_gl_renderer.cpp) already keys programs by
GLSL source. Before adding workers or a persistent cache, measure how many new compilations
and how much compile time appear per match. An approximate fallback can preserve geometry, but
it must be identified as an approximation in image comparisons.

[`benchmark_native.py`](../../melee-unlocked/tools/benchmark_native.py) gives a good model:
separate startup from the match, cold/warm cache, record the executable/script hash and the
p95/p99 percentiles. Its CPU submission intervals measure neither GPU completion nor the
physical latency from controller to screen.

## 7. CARD and DVD: useful contracts, with ABI adaptation

### GCI saves

`hle_card.cpp` implements a folder of `.gci` files: a 64-byte directory entry followed by the
8 KiB data blocks. The model implemented exposes slot A and treats slot B as absent. There are
API error results, asynchronous completions and writes through a temporary file followed by a
rename.

Application: give the rules/unlocks persistence and allow save fixtures for tests. That also
helps broaden the scenarios currently limited by the SSS without a save. Their current bridge
reads structures and callbacks at PPC addresses; our backend will have to receive the correct
native types/pointers.

Acceptance: create, save, close and reopen; check the error for a missing file, capacity and
the callback; run scenarios with an empty folder and with a known fixture.

### Asynchronous I/O with predictable timing

`hle_dvd.cpp` reads in a worker and schedules completion for a fixed virtual deadline,
described as a quarter of a frame, preserving request order. If the worker has not finished by
the deadline, the simulation waits. The mechanism therefore reduces early blocking, but does
not guarantee the absence of stalls.

Our [`os/dvd.cpp`](../port/src/os/dvd.cpp) already delivers operations through the scheduler.
The future gain is decoupling the physical read while preserving the instant and order the
game observes. Test consecutive reads, failure/truncation, and a callback that schedules
another operation, before optimizing throughput.

## 8. Determinism, high refresh rate and Slippi

### Floating point

The PPC helpers include rounding to single, mantissa adjustment, FMA, conversions and
reciprocal/root estimates. The header declares Slippi Dolphin's Jit64 semantics as the target
on the path indicated; that is not proof of universal identity with the hardware.

Our status records residual use of `libm` and potential aliasing differences. If the first
gameplay divergence points at maths, compare specific operations with test vectors, including
signed zeros, conversion limits and rounding. Swapping isolated functions for the reference
does not certify the whole game's determinism.

### Presentation above 60 Hz

The reusable principle is to keep the simulation at 60 Hz and work on presentation snapshots.
`authored_pose` captures animation channels, joint hierarchy, weights and envelope matrices;
the solver samples fractional poses and preserves the current pose when reconstruction is not
supported. Part of the movement uses extrapolation, subject to limits and discontinuities.

The code also distinguishes interpolation between two states, which introduces one frame of
delay, from prediction ahead of the current state. The choice requires measuring quality and
latency. Respawn, action changes, camera, effects and address reuse all need explicit
handling.

We already have native FObj/AObj/JObj: use the separation of simulation from presentation as a
reference, without replacing the existing animation. Any experiment must pass the state
isolation test from section 4.

### Slippi takes more than networking

`slippi_online.cpp` implements savestates by copying regions of guest RAM and excluding
specific audio/VI ranges. `exi_slippi.cpp`, `slippi_net.cpp` and `slippi_report.cpp` make up
other parts of the device and protocol.

Those addresses do not represent the objects in our native heap. For rollback we will have to
restore objects, references, RNG, queues and relevant state without depending on where the
allocations happen to land. That should guide a future IDs/arenas/serialization strategy, not
block the offline MVP.

The README announces online; the tracker holds more limited historical evidence and passages
that still ask for validation against Dolphin. No replay or log of an external session was
reproduced here. Treat compatibility as a hypothesis to test, and avoid taking a pair of
identical instances as proof of equivalence with another emulator.

## 9. Conditions for reproducing the reference

There are concrete obstacles in the checkout examined:

- The [main CMake](../../melee-unlocked/CMakeLists.txt) requires Windows x64/MSVC to enable
  the experimental executable.
- `native_animation` depends on `melee/src/sysdolphin/baselib/fobj.c`, `fobj.h` and `spline.c`
  through [`generate_fobj_host.py`](../../melee-unlocked/tools/generate_fobj_host.py). The
  **internal** folder `melee-unlocked/melee/` is not present and is ignored by Git. Our
  `../melee/` is a different checkout and does not automatically satisfy that path. To
  reproduce, resolve that dependency and pin its revision.
- The C++ generation in `port/generated/` is a separate step starting from the DOL. The
  presence of the recompiler does not mean those generated sources exist.
- `HANDOFF_FABLE_3.md` and the reports cited in the tracker are not in the checkout examined;
  the performance figures and matches described there are the project's own reports, not
  results reproduced in this analysis.

The [README](../../melee-unlocked/README.md) declares GPL-2.0-or-later and that parts of the
runtime originate in Dolphin/Slippi. The files examined carry SPDX headers and the project has
a [LICENSE](../../melee-unlocked/LICENSE). Record origin, revision and license per component
if anything is incorporated. This survey does not assess license compatibility and does not
transfer code between the projects.

## 10. Checks run during this survey

Isolated compilation with `c++ -std=c++17 -O2`, outside the checkouts, in a temporary
directory. None of these cases needs an ISO or online access.

| Check | Observed result | Scope |
| --- | --- | --- |
| `ax_ucode_test.cpp` + `ax_ucode.cpp` | Passed | Synthetic ADPCM, position write into the voice block, interleaving and mix control |
| `texture_snapshot_test.cpp` + `gx_texture.cpp` | Passed | Mutable palette, mips, ownership and deduplication |
| `port/tests/dol_validation_test.py` | 2 tests passed | Truncated header and DOL section bounds |

The results show that those slices can be exercised on Linux. They do not cover gameplay, the
D3D12 backend, real audio, performance, Slippi, or the full safety of the parsers.

Reproducible commands from the `project-melee/` root:

```sh
review_dir=$(mktemp -d /tmp/melee-unlocked-checks.XXXXXX)

c++ -std=c++17 -O2 -I melee-unlocked/port/runtime/hle \
  melee-unlocked/port/tests/ax_ucode_test.cpp \
  melee-unlocked/port/runtime/hle/ax_ucode.cpp -o "$review_dir/ax-test"
"$review_dir/ax-test"

c++ -std=c++17 -O2 -I melee-unlocked/port/runtime/gx \
  melee-unlocked/port/tests/texture_snapshot_test.cpp \
  melee-unlocked/port/runtime/gx/gx_texture.cpp -o "$review_dir/texture-test"
"$review_dir/texture-test"

PYTHONDONTWRITEBYTECODE=1 python3 \
  melee-unlocked/port/tests/dol_validation_test.py
```

## 11. Suggested backlog, not implemented in this task

| Order | Deliverable | Completion criterion |
| --- | --- | --- |
| 1 | Consolidate the existing stock-victory route | Stock test passing, the milestone images checked and documentation consistent with the code |
| 2 | Canonical match trace | Two runs and both presentation modes with the same state; diagnosis of the first divergent field |
| 3 | First audible AX voice | Correct synthetic vector, a WAV of real sound and coherent callbacks/voice state |
| 4 | Capture-guided GX fixes | One effect at a time, palette/mip tests and shadow/refraction scenes without regression |
| 5 | More content and saves | Next character chosen by data dependencies; reproducible GCI fixture and a complete match |
| 6 | Performance and decoupled presentation | Cold/warm measurement, p95/p99 and simulation isolation preserved |

**Suggested decision:** keep the current architecture and take small components along with
their tests. Unlocked is especially valuable for turning an implementation question into an
observable comparison: which state, which audio sample or which draw differs, and when.
