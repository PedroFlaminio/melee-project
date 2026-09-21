# Contributing

This repository is a native PC port of Super Smash Bros. Melee. It is no longer a
decompilation project: there is no PowerPC build, no matching requirement, and no
`configure.py`. Everything is built with CMake for the host.

## Project language

**English.** Documentation, code comments, commit messages and pull request descriptions
are written in English. Some documents were translated from Portuguese on 2026-09-21; new
text should not reintroduce Portuguese.

## Layout

| Path | What it holds |
| --- | --- |
| `src/melee`, `src/sysdolphin` | the game's own C source, compiled as-is for the host |
| `extern/dolphin` | GameCube SDK headers and the SDK sources the port compiles |
| `port/src`, `port/include` | the platform layer: OS, GX, VI, DVD, PAD, AX, assets, renderer |
| `port/tests` | unit tests and the scripted match routes |
| `port/tools` | Python helpers for audio checks, generated tables and keyboard-driven runs |
| `tools` | asset extraction (`melee_extract.py`), the port inventory and the style checker |
| `config/GALE01/symbols.txt` | address → symbol map for `main.dol`, used when porting data tables |

`src/melee` and `src/sysdolphin` are kept whole even though `port/CMakeLists.txt` compiles
only part of them today; the port takes on more of those files over time.

## Building and testing

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

Before opening a pull request, also check the sanitizer build. Note that its CTest run does
not fail on a UBSan report — it only prints one, so read the output:

```sh
cmake --build --preset host-sanitize
ctest --preset host-sanitize -V | grep 'runtime error'
```

Changes that affect rendering, audio or match flow should come with a test. The existing
scripted routes under `port/tests` drive the game through real scenes with `--run-modes`
and compare the result, and are the model to follow.

## Style

- C17 for `.c`, C++20 for `.cpp`; warnings are errors in the debug preset.
- `clang-format` and `editorconfig` are enforced in CI. Run `pre-commit run --all-files`
  locally (`pip install -r reqs/dev.txt`).
- `python3 tools/check/main.py --fix` applies the source checks that catch raw literals
  where a named constant is meant. Run `clang-format` afterwards.
- Match the surrounding code. Comments explain why something is done, not what the line
  does; several files in `port/src` carry notes about GameCube behaviour that the host has
  to reproduce, and those are worth keeping accurate.

## Commits and pull requests

- One logical change per commit, with a subject in the imperative mood
  ("Implement indirect texture support in SDL GL renderer").
- Say in the body what was measured, not just what was written — the status documents are
  built from measured results.
- Update `docs/native_port_status.md` and `docs/port-mvp-progress.md` when a change moves
  the port forward; those two documents are how work is handed over between sessions.

## Assets

Never commit game assets. `assets-local/`, `textures/` and any extracted disc content are
ignored, and the repository must stay free of Nintendo-owned data. Contributors supply
their own dump of the game.
