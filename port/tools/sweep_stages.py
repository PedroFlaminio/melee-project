#!/usr/bin/env python3
"""Enter a VS match on every stage the select screen offers, and say which ones break.

The route walks the real menus to the stage select and then forces the stage
with `FRAME:STAGE=KIND`, which is the force_stage_id the game's own Training
and Tournament modes set.  Steering the cursor instead would only reach the
squares a save unlocks, and would need the icon layout; this reaches every
square, which is the point -- the stage dimension is where the crashes are.

A stage passes when the match scene (0x02) comes up and the route survives to
its stop frame.  Anything else is reported: a stage the host refuses by name, a
signal, or a run that never leaves the select screen.

    python3 port/tools/sweep_stages.py                      # every stage
    python3 port/tools/sweep_stages.py --stages 14,24,31    # a few
    python3 port/tools/sweep_stages.py --binary build/host-sanitize/port/melee-pc
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]

# The stage kinds mnStageSel_803F06D0 offers, in the select screen's order.
# Kept here rather than parsed so the sweep runs without the game's headers;
# --stages overrides it when the table changes.
STAGE_KINDS = [4, 11, 5, 12, 13, 14, 8, 16, 2, 17, 7, 22, 6, 15, 9, 18,
               10, 24, 3, 23, 19, 20, 25, 27, 31, 32, 28, 29, 30, 0]

STAGE_NAMES = {  # StKind, from src/melee/gr/types.h
    0: "Dummy/random", 2: "Fountain of Dreams", 3: "Pokemon Stadium",
    4: "Princess Peach's Castle", 5: "Kongo Jungle", 6: "Brinstar",
    7: "Corneria", 8: "Yoshi's Story", 9: "Onett", 10: "Mute City",
    11: "Rainbow Cruise", 12: "Jungle Japes", 13: "Great Bay",
    14: "Hyrule Temple", 15: "Brinstar Depths", 16: "Yoshi's Island",
    17: "Green Greens", 18: "Fourside", 19: "Mushroom Kingdom",
    20: "Mushroom Kingdom II", 22: "Venom", 23: "Poke Floats", 24: "Big Blue",
    25: "Icicle Mountain", 27: "Flat Zone", 28: "Dream Land N64",
    29: "Yoshi's Island N64", 30: "Kongo Jungle N64", 31: "Battlefield",
    32: "Final Destination",
}

# The menu route to the stage select, with two ports taking Fox.  The stage
# select opens on frame 384; STAGE lands a few frames later and the match
# follows two frames after that.
ROUTE_TO_STAGE_SELECT = [
    "120:START", "160:DOWN", "200:A", "240:A",
    "300-315:SY=127", "300-315:SY=127@2", "320:A", "320:A@2",
    "330-337:SX=127", "330-333:SX=-127@2",
    "345-354:SY=127", "345-354:SY=127@2", "360:A", "360:A@2",
    "380:START",
]
FORCE_FRAME = 400
MATCH_SCENE = "0x02"
SELECT_SCENE = "0x09"


def run_stage(binary: pathlib.Path, root: str, kind: int, frames: int,
              timeout: int, log_lines: int = 25,
              no_audio: bool = False) -> dict:
    """Play one stage and classify how it went."""
    route = [*ROUTE_TO_STAGE_SELECT, f"{FORCE_FRAME}:STAGE={kind}",
             f"{FORCE_FRAME + frames}:STOP"]
    argv = [str(binary), "--run-modes", root, "0", "3", *route]
    report = {"stage": kind, "name": STAGE_NAMES.get(kind, "?"),
              "route": " ".join(route)}
    try:
        done = subprocess.run(argv, cwd=REPO, capture_output=True, text=True,
                              timeout=timeout,
                              env={"MELEE_HOST_AUDIO":
                                       "0" if no_audio else "1",
                                   "PATH": "/usr/bin:/bin",
                                   "HOME": str(pathlib.Path.home()),
                                   "LANG": "C", "LC_ALL": "C",
                                   # The boot and the scenes do not free what
                                   # they allocate, so LeakSanitizer would end
                                   # every sanitized run with code 1; the ctest
                                   # suite runs the same way.
                                   "ASAN_OPTIONS": "detect_leaks=0",
                                   "UBSAN_OPTIONS": "print_stacktrace=1"})
    except subprocess.TimeoutExpired:
        report.update(status="timeout", detail=f"no exit within {timeout}s")
        return report

    out = done.stdout + done.stderr
    report["exit"] = done.returncode
    scenes = re.findall(r"scene (0x[0-9A-Fa-f]{2}) from frame (\d+)", out)
    report["scenes"] = [s for s, _ in scenes]
    reached_match = any(s == MATCH_SCENE for s, _ in scenes)

    # A refusal names its own cause, so it beats the bare signal the OSPanic
    # that follows it turns into.
    # ASan's SUMMARY is the authoritative line: it names the fault and where.
    # UBSan's "runtime error" lines only print, and several benign ones fire
    # early in every run -- matching those first would hide the real fault.
    asan = re.search(r"SUMMARY: AddressSanitizer: (\S+) (\S+?)(?::\d+:\d+)? "
                     r"in (\S+)", out)
    refusal = re.search(r"cannot translate (\w+): (.*)", out)
    if asan:
        where = asan.group(2).split("/melee/")[-1]
        report.update(status="sanitizer",
                      detail=f"{asan.group(1)} in {asan.group(3)} ({where})")
    elif refusal:
        report.update(status="refused",
                      detail=f"{refusal.group(1)}: {refusal.group(2)}"[:160])
    elif done.returncode < 0:
        import signal as _signal
        name = _signal.Signals(-done.returncode).name
        where = "in the match" if reached_match else "loading the stage"
        report.update(status="signal", detail=f"{name} {where}")
    elif "OS panic" in out:
        line = next((l.strip() for l in out.splitlines() if "OS panic" in l), "")
        report.update(status="panic", detail=line[:160])
    elif not reached_match:
        report.update(status="no-match",
                      detail="never left the stage select")
    elif done.returncode != 0:
        report.update(status="exit", detail=f"exit code {done.returncode}")
    else:
        report.update(status="ok", detail="")
    # Keep the tail of the output for anything that did not simply work.
    if report["status"] != "ok":
        report["output"] = "\n".join(out.splitlines()[-log_lines:])
    return report


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--binary", default="build/host-release/port/melee-pc")
    ap.add_argument("--root", default="assets-local")
    ap.add_argument("--stages", help="comma separated stage kinds")
    ap.add_argument("--frames", type=int, default=240,
                    help="drawn frames of match before the route stops")
    ap.add_argument("--timeout", type=int, default=300)
    ap.add_argument("--results", help="write one JSON line a stage here")
    ap.add_argument("--repeat", type=int, default=1,
                    help="how many times a stage must load before it counts.  A route freezes the clock, so a stage that loads only sometimes is reading something that moves with the address layout -- setarch -R makes such a run repeatable while you chase it.")
    ap.add_argument("--no-audio", action="store_true",
                    help="run with MELEE_HOST_AUDIO=0.  Off by default simply because the voices are on in a real run.")
    ap.add_argument("--log-lines", type=int, default=25,
                    help="lines of output kept for a broken run; a\n                          sanitizer report needs more")
    args = ap.parse_args()

    binary = (REPO / args.binary).resolve()
    if not binary.exists():
        raise SystemExit(f"{binary}: build it first")

    kinds = ([int(p) for p in args.stages.split(",")] if args.stages
             else STAGE_KINDS)
    results = []
    broken = []
    for kind in kinds:
        attempts = []
        for _ in range(max(1, args.repeat)):
            attempts.append(run_stage(binary, args.root, kind, args.frames,
                                      args.timeout, args.log_lines,
                                      args.no_audio))
            if attempts[-1]["status"] != "ok":
                break
        report = next((a for a in attempts if a["status"] != "ok"),
                      attempts[0])
        report["attempts"] = len(attempts)
        report["passed"] = sum(1 for a in attempts if a["status"] == "ok")
        results.append(report)
        mark = "ok  " if report["status"] == "ok" else "BAD "
        runs = (f" [{report['passed']}/{report['attempts']}]"
                if args.repeat > 1 else "")
        print(f"{mark}{kind:>3} {report['name']:<24} {report['status']:<8}"
              f"{runs} {report['detail'][:64]}", flush=True)
        if report["status"] != "ok":
            broken.append(report)

    if args.results:
        with open(args.results, "w", encoding="utf-8") as out:
            for report in results:
                out.write(json.dumps(report) + "\n")

    print(f"\n{len(results) - len(broken)} of {len(results)} stages entered a match")
    if broken:
        print("broken:")
        for report in broken:
            print(f"  {report['stage']:>3} {report['name']:<24} "
                  f"{report['status']}: {report['detail'][:90]}")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
