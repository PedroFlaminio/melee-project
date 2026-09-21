# Technical review from `fa257ed`

Date: 19 September 2026. Repository: `melee`.

**Verdict:** there is useful progress, but the whole still needs fixes before it can be
considered stable. The main blockers are memory corruption in the translators, incorrect
translation of stage parameters, and undefined depth in the shader. The existing suite passes
but does not sufficiently cover the functionality that was added.

The review applies the rigour of a senior review of a junior delivery: it questions
assumptions, contracts, failure handling and evidence of working behaviour. The observations
are about the code, without inferring the experience of whoever wrote it.

## Scope and method

- **Inclusive** range: `fa257ed6da1f80060a1496a65f9de6bda3102a61` through
  `1348ec4a59dced31313bcdfb2c25c14e12c10ec9`.
- Comparison base: `e82e7c554138ea540e07f20ebf2dbd3d4ec65446`, the parent of `fa257ed`.
- 21 commits, 70 files, 22,877 lines added and 2,138 removed. The two third-party headers
  account for 15,512 of the added lines.
- History, cumulative diff, final implementation, consumers of the changed structures, tests,
  scripts and documentation were all examined. Defects fixed within the range itself are not
  presented as outstanding.
- The focus was on our own code and on the integration of the dependencies. No full audit was
  made of the internal algorithms of `stb_image.h` and `xxhash.h`, and `port/test_ui` was not
  reverse-engineered.
- Builds, existing tests, structural inspection of the local assets and isolated reproductions
  were run. No interactive GPU, physical controller, audio or Windows tests were performed.
  Findings on those paths are identified as static analysis.
- No production code was fixed during this review. This document is the deliverable;
  auxiliary diagnostic files were left in `/tmp`.

## Priorities

**P1:** fix before broadening usage testing or distributing a version. **P2:** a functional or
robustness defect that should enter the next round of fixes. Maintenance improvements are in a
separate section.

| ID | Priority | Problem | Main evidence |
| --- | --- | --- | --- |
| R01 | P1 | Samus object under-allocated on 64 bits | Code and isolated reproduction with ASan |
| R02 | P1 | Index copy writes past the allocation | Code, Kirby asset and isolated reproduction with ASan |
| R03 | P1 | Stage offsets treated as native pointers | Reader contract and Battlefield asset |
| R04 | P1 | Unknown parameters are silently zeroed | Inspection of 11 affected files |
| R05 | P1 | Different stages receive the same incorrect layout | Unreachable branches and asset sizes |
| R06 | P1 | Shader leaves depth undefined | Code and the GLSL specification |
| R07 | P2 | Menu discards quit and controller connection events | Static analysis |
| R08 | P2 | Toggling custom textures does not refresh the cache | Analysis of both caches |
| R09 | P2 | Anisotropy does not apply to new textures or after a restart | Static analysis |
| R10 | P2 | MSAA validates the wrong framebuffer and ignores failures | Static analysis |
| R11 | P2 | BMP captures do not resolve MSAA | Static analysis of the read path |
| R12 | P2 | An open failure can destroy an uninitialized ImGui | Analysis of the error paths |
| R13 | P2 | `npm run play` does not start the game | Command executed |
| R14 | P2 | `Unlock Everything` behaves in a way incompatible with a checkbox | Analysis of boot, UI and persistence |
| R15 | P2 | The random test accepts success without proving a match happened | Reproduction with simulated output |

## Proposed fixes

### R01 — Allocate the native size of the Samus object

**Location:** [game_data_translators.c](../port/src/game/game_data_translators.c), lines
3852–3898, especially 3865.

`samus_throw_beam_model` declares a structure with four `void*` but allocates `0x10` bytes. On
x86-64 it occupies 32 bytes. The assignments to `beam->x8` and `beam->xC` write at offsets 16
and 24, past the 16 reserved bytes. Since the animation array is allocated next in the same
arena, those fields may also overlap that array's data.

**Verification:** the original function, extracted into a program with a simulated reader and
individual allocations, produced `AddressSanitizer: heap-buffer-overflow`, an 8-byte write.
Normal loading of `PlSs.dat` did not trip ASan; that does not rule out the overlap inside the
arena.

**Fix:** use a named type, `sizeof(*beam)` and its native alignment. Also review the fixed
count of four array entries against the asset's actual format.

**Regression:** load Samus's data and check model, four animations and matanim individually;
run grab/grapple under sanitizers with per-object bounds detection in the arena.

### R02 — Fix the doubled offset in the index copy

**Location:** [game_data_translators.c](../port/src/game/game_data_translators.c), lines
1009–1017 and 2832–2873; the problematic call is on line 2858.

`copy_bytes` adds `begin` to both source and destination. The call
`copy_bytes(reader, target, yoshi2->xC, 12, extent)` already passes a destination offset by 12
bytes and adds another 12. For `extent == 16` it writes bytes 24–27 of a 16-byte allocation,
leaving the expected stretch uncopied.

**Verification:** the third sub-block of `x1C` in `PlKb.dat` has extent 16. Isolated
reproduction of the original function and helper produced `heap-buffer-overflow`, a 1-byte
write. The function also serves Yoshi; the inspected sub-blocks of `PlYs.dat` were 4, 8 or 12
bytes, so they did not take that branch. It should not be claimed that both characters
reproduced the same overflow with these assets.

**Fix:** pass the `yoshi2` base to the helper so the offsets stay absolute. Validate minimum
sizes before the writes and confirm each variant's format, rather than assuming they are all
three integers followed by bytes.

**Regression:** test extents 4, 8, 12 and 16, recognizable values in the tail, and guards
after the allocation; include Kirby's loading.

### R03 — Materialize the targets of the stage pointers

**Location:** [yakumono_param.c.inc](../port/src/game/yakumono_param.c.inc), lines 27–40 and
the other `(void*)(uintptr_t)target` assignments.

`melee_host_hsd_reader_pointer` returns an **offset into the data section**, per
[hsd_host_archive.cpp](../port/src/assets/hsd_host_archive.cpp), lines 605–618. Converting
that integer to a `void*` does not produce a valid process address.

A concrete example: `GrNBa.dat` has an 8-byte block whose targets are `0x34088` and `0x340a4`.
The translator hands those values over as pointers. In [grbattle.c](../src/melee/gr/grbattle.c),
lines 401–407, the fields are forwarded to `grMaterial_801C9604`, which uses them as scripts.
The translation reports success even though it never builds those scripts.

**Fix:** materialize each target according to its type: integer list, script with command
conversion, joint or animation. Use the raw payload only where the consumer genuinely expects
untranslated bytes. While a type is unsupported, report an explicit failure.

**Regression:** check the values and the ownership of the objects pointed at, and run
Battlefield's overlay swap; testing that the symbol returned a non-`NULL` pointer is not
enough.

### R04 — Do not replace unknown parameters with zeros

**Location:** [yakumono_param.c.inc](../port/src/game/yakumono_param.c.inc), lines 880–883;
[hsd_host_archive_test.cpp](../port/tests/hsd_host_archive_test.cpp), the `yakumono_param`
test near line 1400.

The `default` allocates a zeroed block and returns success for data it does not recognize. The
earlier check, which refused unsupported parameters, was removed along with its negative test,
with no equivalent validation. That makes `melee_host_stage_symbols_check` stop detecting
untranslated content.

**Verification:** a reading of the headers, relocations, public symbols and block bounds,
following `translator_extent`'s criterion, found 43 non-null `yakumono_param` blocks. Eleven
fall into the `default`:

| Asset | Extent in bytes |
| --- | ---: |
| `GrCs.dat` | 328 |
| `GrGb.dat` | 164 |
| `GrIm.dat` | 316 |
| `GrNFg.dat` | 24 |
| `GrNKr.dat` | 388 |
| `GrNPo.dat` | 532 |
| `GrNSr.dat` | 296 |
| `GrOt.dat` | 104 |
| `GrOy.dat` | 28 |
| `GrTPr.dat` | 4 |
| `GrZe.dat` | 400 |

**Impact:** timers, speeds, limits and pointers can silently become zero, changing the logic or
causing later crashes. These numbers demonstrate incorrect translator selection, not that each
stage was run until it failed.

**Fix:** explicitly refuse unknown layouts and implement translators identified per stage.
Restore the negative test and add fixtures for the real sizes. The Ice Mountain branch also
needs its offsets fixed: it reads `x9C` at `0xA0` and looks for pointers at `0xAE/0xB2/0xB6`,
while the asset records `0xAC/0xB0/0xB4`. Adjusting only `case 212` to 316 does not fix the
layout.

**Regression:** test the parameters' contents, errors for unsupported types, and each stage's
hazards in action.

### R05 — Do not infer the stage type from the size alone

**Location:** [yakumono_param.c.inc](../port/src/game/yakumono_param.c.inc), lines 192–225,
287–326 and 377–452.

The `if (1)` statements make several alternatives unreachable. The 52-byte block always uses
`grOldpupupu`, never `grKraid`; the 76-byte one always uses `grFourside`, never `grInishie2`;
the 84-byte one always uses `grIzumi`, never `grInishie1` or `grPStadium`.

**Verification:** the local assets `GrKr.dat`, `GrI2.dat`, `GrI1.dat` and `GrPs.dat` are 52,
76, 84 and 84 bytes respectively. The layouts include different combinations of `u16`, `u32`
and floats; applying another layout can swap the order of the 16-bit pairs and corrupt the
values without producing a memory error.

**Fix:** select by file/stage identity or by an explicit schema. Size and relocations should
validate the choice, not substitute for it.

**Regression:** fixtures for two stages of the same size, checking 16- and 32-bit fields and at
least one hazard behaviour.

### R06 — Define depth on every shader path

**Location:** [tev.cpp](../port/src/gx/tev.cpp), lines 579 and 764–766;
[sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 382–383 and 403–436.

The fragment shader writes `gl_FragDepth` only if `u_z_texture_op == 1`. When a shader contains
that write, the paths that do not execute it can produce undefined depth. That affects ordinary
draws too, with Z texture disabled. The requirement is in the
[GLSL 3.30 specification, section 7.2](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.3.30.pdf).

There are three further gaps in the implementation: the uniform's location is obtained but its
value is never sent in `set_program_uniforms`; `GX_ZT_REPLACE` is 2, while 1 is `GX_ZT_ADD`;
and format and bias are not taken into account. The advertised depth texture support is
therefore not correctly connected to the GX state.

**Fix:** preserve `gl_FragCoord.z` on the normal path, or generate a variant with no depth
write. For Z texture, send the state and implement ADD/REPLACE, format and bias correctly,
using the reference that already exists in the CPU rasterizer.

**Regression:** compare depth, occlusion and colour between CPU and GPU with Z texture
disabled, with ADD and with REPLACE. The current TEV colour test alone does not prove the depth
buffer's behaviour. No specific visual artifact was reproduced in this review.

### R07 — Separate input capture from lifecycle events

**Location:** [settings_ui.cpp](../port/src/render/settings_ui.cpp), lines 74–78;
[sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 2225–2243.

`process_settings_event` returns `WantCaptureKeyboard || WantCaptureMouse` regardless of the
event's type. The caller does a `continue` before handling `SDL_EVENT_QUIT` and gamepad
connection and removal. With the menu capturing input, closing the window can be ignored. The
Esc key advertised for toggling the menu can also be consumed before it reaches the toggle.

**Fix:** always process quit and device connection; apply capture only to the corresponding
keyboard/mouse events. Define Esc's priority explicitly.

**Regression:** open the menu, close it with Esc, quit through the window, and connect and
disconnect a controller while the UI has focus.

### R08 — Invalidate decoded and uploaded textures when the feature is toggled

**Location:** [sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), line 2128;
[main.cpp](../port/src/main.cpp), lines 839–912.

The checkbox only changes `g_custom_textures_enabled`. `TitleTextureCache` re-decodes only when
the key changes or an EFB copy changes the generation. The option takes part in neither
criterion. Original textures already loaded stay original when it is enabled; replacements
already loaded stay active when it is disabled. The GL cache also depends on the image's
generation.

**Fix:** introduce a revision for the texture settings and propagate it to both caches,
invalidating and re-decoding the affected images. Preserve the validity of the pointers the
presenter uses as keys.

**Regression:** load a scene with a known replacement and toggle the checkbox both ways,
without restarting and without changing scene.

### R09 — Apply anisotropy when each texture is created

**Location:** [sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 586–613,
1453–1458 and 2142–2145.

Anisotropy is applied to existing textures only when some UI control changes. `create_texture`
does not use `g_anisotropy_level`. As a result the setting loaded from disk is not applied to
new textures; textures from another scene and re-uploads by generation also lose the selected
value.

**Fix:** centralize the application of the sampler parameters and call it on creation and
update. Check for extension support and clamp the value to the capability the GPU reports,
rather than assuming 16x.

**Regression:** select 8x, change scene, recreate a texture and restart the program; check the
GL parameter in every case.

### R10 — Validate both framebuffers and handle MSAA failures

**Location:** [sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 736–785,
1384–1399 and 2147.

With MSAA, the constructor finishes with `resolve_framebuffer` bound. `complete()` checks the
currently bound framebuffer, so it tests the resolve target and may accept an incomplete
multisample framebuffer. Furthermore, selecting 8x does not consult the GPU's limits and
`configure_render_target`'s return value is ignored by the UI.

**Impact:** an unsupported configuration or an allocation failure can be saved as applied and
result in empty rendering or an inconsistency between the menu and the effective state.

**Fix:** validate each FBO explicitly, query capabilities and apply the change as a
transaction: create, validate, replace the previous one and only then persist. On error, keep
the previous configuration and report the reason.

**Regression:** simulate a limit below 8 samples and a creation failure; check the fallback,
that the previous target is kept, and that a rejected value is not persisted.

### R11 — Resolve MSAA before saving a BMP

**Location:** [sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 789–800 and
2204–2214.

`save_bmp` binds the multisample framebuffer directly and calls `glReadPixels`. The resolve
exists only inside the visible presentation path. With MSAA saved in the preferences, the
hidden presenter also creates that target, but the capture does not resolve the samples before
reading. The GL error is not queried, so the file may be written with no valid pixels.

**Fix:** share a resolve routine between presentation and capture; read from the single-sample
FBO and propagate read failures. Preferably allow explicit configuration for screenshot tests,
independent of personal preferences.

**Regression:** save the same hidden scene with AA off, 2x, 4x and 8x where supported; validate
dimensions, pixels and GL errors.

### R12 — Destroy the UI only after it has been initialized

**Location:** [sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 1874 and
1912–1922; [settings_ui.cpp](../port/src/render/settings_ui.cpp), lines 64–69.

When `configure_render_target` fails, `open` transfers the state into `state_` and returns
before calling `init_settings_ui`. The destructor calls `destroy_settings_ui`
unconditionally. ImGui's OpenGL backend requires previously initialized state and contains an
assertion for shutdown without a backend.

**Fix:** record which resources were initialized, or wrap each one in an object responsible for
its own lifetime. Also check the backends' initialization return values.

**Regression:** inject a failure in render target creation and in the backends; `open` must
return an error and release the resources without an assertion or a crash.

### R13 — Separate build from run in the npm script

**Location:** [package.json](../package.json), line 6.

There is no separator between `cmake --build ... -j2` and the executable's path. Running
`npm run play`, CMake received the executable as an extra argument and printed
`Unknown argument ./build/host-release/port/melee-pc`; the game did not start.

**Fix:** use, for example,
`cmake --build --preset host-release --target melee-pc -j2 && ./build/host-release/port/melee-pc --play assets-local`.
Document the initial configuration with `cmake --preset host-release` or include it in the
flow. If Windows is to be supported by that command, handle the binary's path and extension.

**Regression:** run the command in a configured tree and in a fresh tree; check that build
failures prevent the launch and that a successful build starts it.

### R14 — Define a coherent contract for `Unlock Everything`

**Location:** [gmmain_lib.c](../src/melee/gm/gmmain_lib.c), lines 1315–1316;
[settings_ui.cpp](../port/src/render/settings_ui.cpp), line 182;
[sdl_gl_renderer.cpp](../port/src/render/sdl_gl_renderer.cpp), lines 1420–1485 and 2124–2126.

The boot already calls `gm_80164F18` unconditionally, even with the checkbox unticked. Ticking
the control makes it call the full unlock on every presentation, but unticking it does not undo
the changes. `unlock_all` is also neither saved nor loaded with the other options.

**Fix:** decide whether the product keeps everything unlocked by default or offers an explicit
action. In the second case, a one-shot button describes the operation better. If the intent is
a persistent preference, implement its persistence and semantics without promising a reversal
that does not exist. Avoid mutating progress inside the frame composition loop.

**Regression:** check boot, click, untick and restart, including the state of characters,
stages and saved progress. The product decision must preserve any prior user preference for
unlocked characters.

### R15 — Do not classify an interrupted route as a validated match

**Location:** [random_cpu_matches.py](../port/tools/random_cpu_matches.py), lines 453–465.

In `--until-end` mode, the output merely containing `stopped at frame` is enough for the run to
receive `status = "ok"`. That condition is checked before the character check and requires
neither entry into the match scene nor a result. STOP itself is emitted on reaching the budget,
including when the match did not end.

**Verification:** calling `run_match` with a simulated process return of `0` and only
`stopped at frame 2000`, the result was `status=ok`, with no `match_from`, `selected` or
`result`.

**Fix:** separate `match completed`, `budget reached`, `wrong selection` and `match never
started`. Validate scene entry, characters and stage regardless of how the run terminated. A
smoke test that survived the budget can be useful, but it must be labelled as such.

**Regression:** test menu exits with no match, a wrong selection, a match still running at STOP,
and a match that actually completed.

## Implementation carried out

The fixes below were applied after this review.

| Finding | Status | Implementation |
|---|---|---|
| R01 | Fixed | `samus_throw_beam_model`'s allocation uses `sizeof` of the native type. |
| R02 | Fixed | The copy of Yoshi/Kirby variable data passes the allocation's base to the helper. |
| R03–R05 | Fixed with a safe refusal | Only Battlefield's known block materializes its two command streams; non-zero blocks with no specific schema now fail, instead of converting offsets into pointers, picking a branch through `if (1)`, or zeroing data. |
| R06 | Partly fixed | The shader always initializes `gl_FragDepth` and receives operation and bias; `GX_ZT_REPLACE` is now the operation that changes depth. Carrying the 24-bit depth texture through for full equivalence with the reference rasterizer is still missing. |
| R07 | Fixed | Quit, Escape and gamepad events are processed before ImGui's capture; capture depends on the event's category. |
| R08 | Fixed | The custom textures option has its own revision, which forces re-decoding and re-upload to the GPU. |
| R09 | Fixed | Anisotropy is applied on texture creation and update, clamped to the maximum the driver exposes. |
| R10–R11 | Fixed | Both FBOs are checked and the BMP path resolves the MSAA colour before `glReadPixels`; MSAA levels above the driver's maximum are refused. |
| R12 | Fixed | ImGui is destroyed only after it has been initialized. |
| R13 | Fixed | The npm script separates build from run with `&&`. |
| R14 | Fixed | The unlock became a one-shot action button, is not persisted, and is not applied every frame. |
| R15 | Fixed | `--until-end` requires match entry, the expected selection and a result before reporting success. |

Support for the other `yakumono_param` was not faked as complete: they now fail with an
explicit message until each schema is associated with the stage's identity and its targets are
materialized. That reduces coverage of stages with non-zero parameters, but it removes the
memory and state corruption that size-based inference caused.

## Maintenance and coverage improvements

1. **Pin the ImGui version.** [imgui.cmake](../port/cmake/imgui.cmake), line 8, uses
   `GIT_TAG master`. Builds of the same commit can pick up different APIs. Use a fixed commit,
   record the validated version and offer an offline flow with the dependencies already
   available. The build that was run used the existing local dependency and did not demonstrate
   a clean installation.

2. **Harden the allocator in diagnostic mode.** The arena in
   [hsd_materialize.cpp](../port/src/assets/hsd_materialize.cpp), lines 531–546, does not
   delimit each sub-allocation for ASan. Adding redzones/poisoning, or a mode with separate
   allocations, would let R01/R02 be detected during integration. Add size validation, reader
   failure handling and allocation return checks to the generated translators.

3. **Replace textual generation with verifiable schemas.** `yakumono_param.h` duplicates game
   types and the `.c.inc` contains ambiguous selection and inconsistent offsets.
   `gen_translators_script.py` uses an absolute path from the machine and templates that no
   longer match `fighter_data`'s current signature. Define disc fields, sizes, alignment and
   pointer translation rules in a single source; generate reproducible code and check the
   result in CI. The host's `sizeof` must not automatically determine how many bytes exist on
   disc.

4. **Take experimental artifacts out of the main flow.** The five `port/patch_*.py` rewrite
   files as text and are not safe or idempotent migrations. `gen_translators.py` and
   `test_present.cpp` are placeholders; `test_ui.cpp` is a manual program with no CTest
   integration; `port/test_ui` is a committed binary. Remove them, or move experiments to a
   clearly identified area, and keep examples buildable through their own targets.
   `play_cpu_match.py` merely opens the game interactively: rename it or implement the promised
   CPU match. *(Done on 21 September 2026: the patch scripts, `test_present.cpp`, `test_ui.cpp`
   and the `test_ui` binary were removed during the repository cleanup.)*

5. **Remove the old UI and apply only the change that is needed.** `draw_video_menu`,
   `draw_fps_counter`, glyphs, the display list and the previous menu's state all remain
   alongside ImGui. `cycle()` is still tested but is not the path the new controls use. Extract
   parsing/persistence and option application into testable code. Changing Show FPS or
   anisotropy should not recreate the FBO, reapply the window mode and change VSync.

6. **Harden texture discovery and decoding.** `custom_textures.cpp` builds the index once, uses
   the working directory, and resolves duplicate names by iteration order. It indexes `.dds`
   but only looks for `.png` and uses a decoder with no DDS support. Add a deterministic
   priority, diagnostics for a refused file, an explicit refresh, and dimension validation
   before converting `int` to `uint16_t`. Handle errors during the recursive iteration, not
   only in the constructor. Validate name/hash compatibility with real fixtures from the
   intended pack before announcing it.

7. **Revisit the local match contract.** [local_match.c](../port/src/game/local_match.c), lines
   74–76, changed `melee_host_prepare_local_two_player_match` to create P2 as a level 9 CPU.
   The test was changed to accept that, but the name and contract still describe the previous
   setup. Make type and level parameters, or create a specific function for human versus CPU.
   The use identified currently is diagnostics; there is no evidence that this function changes
   every interactive two-player match.

8. **Versioned, observable persistence.** The reader now accepts optional fields, which is an
   improvement, but the format is still positional and the write truncates the file directly,
   with no failure reporting. Adopt a version/schema, a temporary write followed by a
   replacement, and an error return. Test old, truncated and invalid-value files; avoid
   duplicating the enum → effective value mapping in both the loader and the renderer.

9. **Measure simulation and presentation separately.** The high rates re-present the same
   frame; no interpolation is implemented. Validate that the 60 Hz simulation holds on 60, 120,
   144 and 165 Hz monitors, in Unlimited, and after pauses and resizes. Measure input and audio
   latency too. Normals/tangents added to the stream do not yet constitute bump mapping;
   measure that stream's cost before keeping it with no consumer.

10. **Update the documentation with current evidence.** `port-characters-stages.md` claims
    "all of them run" and "free of data crashes", claims incompatible with R01–R05. The video
    plan says the projection does not change, but the renderer already modifies perspective and
    ortho projections. The README defers build instructions that exist in another document.
    Publish a feature matrix with stage/character, action tested, build, platform and commit;
    distinguish translation, loading and working in a match. Preserve the historical record of
    `fa257ed` as the result for that commit, without presenting it as validation of the current
    HEAD.

11. **Expand the tests where the code changed.** Cover every new translator with expected
    values, layout/endianness regressions, ImGui lifecycle, persistence, caches and MSAA.
    Compare Debug and Release on deterministic routes. Keep negative tests while a format
    remains unsupported. Record "process exited successfully", "no ASan errors" and "no UBSan
    diagnostics" separately; `fa257ed`'s history already acknowledges 37 UBSan points.

## Validations run

| Check | Result | Limit of the evidence |
| --- | --- | --- |
| `cmake --build --preset host-debug -j2` | Success | Incremental build; there were warnings in the core |
| `ctest --preset host-debug --output-on-failure -j2` | **26/26**, 106.52 s | Includes **227/227** cases in the unit executable; does not test the interactive UI |
| `cmake --build --preset host-sanitize -j2` | Success | Does not by itself exercise the problematic paths |
| CTest sanitize: extract, unit, command-layout, Mario and Link data | **5/5 after an environment adjustment** | The two asset tests initially failed on ptrace/LeakSanitizer; they passed outside the sandbox |
| `--load-archive` under sanitizers for Samus, Yoshi, Kirby, Ice Mountain and Battlefield | Symbols reported as translated | Does not prove correct layout, values or gameplay execution |
| R01/R02 functions with a simulated reader and individual allocations | **Two heap-buffer-overflows under ASan** | Isolated reproduction of the functions, not a crash reproduced during a match |
| Inspection of the local assets' `yakumono_param` blocks | **11/43 non-null blocks fall into the zeroed fallback** | Does not run all 43 stages |
| `npm run play` | Invalid argument in CMake; game not launched | Run in the current Linux environment |
| `run_match` with simulated output containing only STOP | Wrongly returned `ok` | A test of the classifier, without starting a match |

LeakSanitizer does not work in the traced environment of some runs. For the loading probes that
used `ASAN_OPTIONS=detect_leaks=0`, ASan and UBSan stayed active, but leaks were not evaluated.
That adjustment is already used by some of the project's CMake tests. The Mario/Link tests were
repeated outside the sandbox and passed without that environment failure.

In the isolated probes, the original functions and the `copy_bytes` helper were extracted; the
readers were simulated and the arena allocator was replaced with a per-object `calloc`. That
detail is essential to interpreting the difference between those probes' result and normal
asset loading.

Local logs from this review, not committed: `/tmp/melee-review-debug-build.log`,
`/tmp/melee-review-debug-tests.log`, `/tmp/melee-review-sanitize-build.log`,
`/tmp/melee-review-sanitize-tests.log`, `/tmp/melee-review-samus-probe.log`,
`/tmp/melee-review-yoshi-probe.log` and `/tmp/melee-review-npm-play.log`. The sanitize CTest
log preserves the two initial environment failures; the result of the external repeat is the
one recorded in the table above.

## Suggested order of work

1. Fix R01/R02 and add per-object detection in the diagnostic allocator.
2. Redo `yakumono_param`'s selection/materialization and restore the explicit refusal of
   unknown layouts: R03–R05.
3. Fix depth and validate CPU against GPU: R06.
4. Fix option application, events and lifetimes: R07–R12 and R14.
5. Fix the run command and the random test's criteria: R13/R15.
6. Run the broadened matrix in Debug, Release and sanitize; update the public status only with
   what has been demonstrated.

## History coverage

| Commit | Area reviewed |
| --- | --- |
| `fa257ed6d` | Historical sanitizer record; adds no implementation |
| `1d30d447f` | Renderer, video options, presets and window tests |
| `04480d38b` | Scene loop and handling of ticks with no input |
| `8de587128` | Removal of external pacing and VSync |
| `59e34298d` | FPS and interface translation |
| `5b9473bb3` | Unlimited and presentation pacing |
| `f60740a8c` | Organization of the menus' data |
| `e45ca8e4f` | Character/stage translators, Ness, tools and types |
| `5c619906c` | Port README |
| `cd8f479f5` | Yoshi attribute and animation refactor |
| `6704efe5b` | README and project positioning |
| `576cb7734` | ImGui, event integration, unlock and experimental scripts |
| `e40bc4c0f` | Custom textures and vendored libraries |
| `b40c4d13d` | Texture compilation fixes |
| `1d9c8c69e` | Recursive texture discovery |
| `86fa3ef36` | Replacement dimensions |
| `7f45ba3da` | MSAA and anisotropy |
| `c704f182f` | New ImGui controls |
| `5548569c0` | Settings read and write compatibility |
| `f332ddd09` | Depth textures and additional vertex attributes |
| `1348ec4a5` | AObj, Final Destination, texture pointers and UB in Release |

Replacing offset-based accesses with named fields in the menus/HUD, preserving the full pointer
in `HSD_AObjDesc`, using `memcpy` in `fabsf_bitwise` and removing the writes to `sqrt_tmp - N`
are advances worth keeping. No further demonstrated regressions were identified in those
adjustments; approval of those parts remains limited to the tests that were run, with no
validation of the PowerPC build or of every game mode.
