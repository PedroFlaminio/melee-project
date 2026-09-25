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
STAGE_STATUS = REPO / "port/data/stage_status.json"

with STAGE_STATUS.open(encoding="utf-8") as stage_status_file:
    _stage_status = json.load(stage_status_file)["stages"]
STAGE_KINDS = [stage["kind"] for stage in _stage_status]
STAGE_NAMES = {stage["kind"]: stage["name"] for stage in _stage_status}

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


def repeated_attempts(run_one, repeat: int, fail_fast: bool) -> list[dict]:
    """Run exactly `repeat` times unless the caller explicitly asks to stop."""
    attempts = []
    for _ in range(repeat):
        attempt = run_one()
        attempts.append(attempt)
        if fail_fast and attempt["status"] != "ok":
            break
    return attempts


def summarize_attempts(attempts: list[dict], requested: int) -> dict:
    """Keep a representative status and every underlying attempt."""
    representative = next((a for a in attempts if a["status"] != "ok"),
                          attempts[0])
    report = dict(representative)
    report["requested_attempts"] = requested
    report["completed_attempts"] = len(attempts)
    report["passed"] = sum(1 for attempt in attempts
                           if attempt["status"] == "ok")
    report["attempts"] = attempts
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
                    help="how many times every stage is run; it passes only if all attempts pass")
    ap.add_argument("--fail-fast", action="store_true",
                    help="stop repeating a stage after its first failed attempt")
    ap.add_argument("--no-audio", action="store_true",
                    help="run with MELEE_HOST_AUDIO=0.  Off by default simply because the voices are on in a real run.")
    ap.add_argument("--log-lines", type=int, default=25,
                    help="lines of output kept for a broken run; a\n                          sanitizer report needs more")
    args = ap.parse_args()

    if args.repeat < 1:
        ap.error("--repeat must be at least 1")

    binary = (REPO / args.binary).resolve()
    if not binary.exists():
        raise SystemExit(f"{binary}: build it first")

    kinds = ([int(p) for p in args.stages.split(",")] if args.stages
             else STAGE_KINDS)
    results = []
    broken = []
    for kind in kinds:
        attempts = repeated_attempts(
            lambda: run_stage(binary, args.root, kind, args.frames,
                              args.timeout, args.log_lines, args.no_audio),
            args.repeat, args.fail_fast)
        report = summarize_attempts(attempts, args.repeat)
        results.append(report)
        mark = "ok  " if report["status"] == "ok" else "BAD "
        runs = (f" [{report['passed']}/{report['completed_attempts']}]"
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
