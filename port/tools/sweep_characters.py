#!/usr/bin/env python3
"""Enter a VS match with every character, and say which ones break.

The route walks the real menus to the stage select with two ports taking
Fox, then rewrites both ports' choices with `FRAME:CHAR=KIND,PORT,LEVEL`
and forces Battlefield with `FRAME:STAGE=31`.  Steering the character select
cursor would only reach the 14 characters a save unlocks and would need the
icon layout; this reaches all 26.

Two sweeps, each a column of port/data/character_status.json:

  vs-cpu      the character as a level 9 CPU against a level 9 CPU Mario, so
              both fight: specials, items and projectiles get exercised, not
              just the load.
  kirby-copy  a level 9 CPU Kirby against the character.  The match loads
              Kirby's copy of every opponent's neutral special
              (ftDataKirbyCopy<X>), which the first sweep never reaches.

A character passes when the match scene comes up and the route survives to
its stop frame.  Several routes fail only on some runs, so use --repeat 3 to
count a character only if it passes every time.

    python3 port/tools/sweep_characters.py --repeat 3 --update
    python3 port/tools/sweep_characters.py --mode kirby-copy --characters 5,9
    python3 port/tools/sweep_characters.py --binary build/host-sanitize/port/melee-pc
"""

from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import sys

from sweep_stages import (FORCE_FRAME, REPO, ROUTE_TO_STAGE_SELECT,
                          repeated_attempts, run_route, summarize_attempts)

CHARACTER_STATUS = REPO / "port/data/character_status.json"
STAGE_BATTLEFIELD = 31
CKIND_KIRBY = 4
CKIND_MARIO = 8
CPU_LEVEL = 9
MODES = {"vs-cpu": "vs_cpu", "kirby-copy": "kirby_copy"}


def load_status() -> dict:
    with CHARACTER_STATUS.open(encoding="utf-8") as status_file:
        return json.load(status_file)


def character_route(tested: int, opponent: int, frames: int) -> list[str]:
    """Port 1 takes `tested`, port 2 `opponent`, both level 9 CPUs."""
    set_frame = FORCE_FRAME - 2
    return [*ROUTE_TO_STAGE_SELECT,
            f"{set_frame}:CHAR={tested},1,{CPU_LEVEL}",
            f"{set_frame}:CHAR={opponent},2,{CPU_LEVEL}",
            f"{FORCE_FRAME}:STAGE={STAGE_BATTLEFIELD}",
            f"{FORCE_FRAME + frames}:STOP"]


def write_status(document: dict, column: str, results: list[dict]) -> None:
    by_kind = {report["character"]: report["status"] == "ok"
               for report in results}
    for character in document["characters"]:
        if character["kind"] in by_kind:
            character[column] = by_kind[character["kind"]]
    document["measured_on"] = datetime.date.today().isoformat()
    lines = ["{", f'  "schema_version": {document["schema_version"]},',
             f'  "measured_on": {json.dumps(document["measured_on"])},',
             f'  "note": {json.dumps(document["note"])},',
             '  "characters": [']
    characters = document["characters"]
    for index, character in enumerate(characters):
        comma = "," if index + 1 < len(characters) else ""
        lines.append(
            f'    {{ "kind": {character["kind"]}, '
            f'"name": {json.dumps(character["name"])}, '
            f'"vs_cpu": {json.dumps(character["vs_cpu"])}, '
            f'"kirby_copy": {json.dumps(character["kirby_copy"])} }}{comma}')
    lines += ["  ]", "}", ""]
    CHARACTER_STATUS.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--binary", default="build/host-debug/port/melee-pc")
    ap.add_argument("--root", default="assets-local")
    ap.add_argument("--mode", choices=sorted(MODES), default="vs-cpu")
    ap.add_argument("--characters", help="comma separated character kinds")
    ap.add_argument("--frames", type=int, default=1800,
                    help="drawn frames of match before the route stops")
    ap.add_argument("--timeout", type=int, default=600)
    ap.add_argument("--repeat", type=int, default=1,
                    help="runs per character; it passes only if all pass")
    ap.add_argument("--fail-fast", action="store_true")
    ap.add_argument("--results", help="write one JSON line a character here")
    ap.add_argument("--update", action="store_true",
                    help="record the results in character_status.json")
    ap.add_argument("--log-lines", type=int, default=25)
    args = ap.parse_args()

    if args.repeat < 1:
        ap.error("--repeat must be at least 1")
    binary = (REPO / args.binary).resolve()
    if not binary.exists():
        raise SystemExit(f"{binary}: build it first")

    document = load_status()
    names = {c["kind"]: c["name"] for c in document["characters"]}
    kinds = ([int(p) for p in args.characters.split(",")]
             if args.characters else sorted(names))

    results = []
    broken = []
    for kind in kinds:
        if args.mode == "vs-cpu":
            tested, opponent = kind, CKIND_MARIO
        else:
            tested, opponent = CKIND_KIRBY, kind
        route = character_route(tested, opponent, args.frames)
        attempts = repeated_attempts(
            lambda: run_route(binary, args.root, route, args.timeout,
                              args.log_lines),
            args.repeat, args.fail_fast)
        report = summarize_attempts(attempts, args.repeat)
        report.update(character=kind, name=names.get(kind, "?"),
                      mode=args.mode)
        report["detail"] = report["detail"].replace("loading the stage",
                                                    "loading the match")
        results.append(report)
        mark = "ok  " if report["status"] == "ok" else "BAD "
        runs = (f" [{report['passed']}/{report['completed_attempts']}]"
                if args.repeat > 1 else "")
        print(f"{mark}{kind:>3} {report['name']:<18} {report['status']:<9}"
              f"{runs} {report['detail'][:70]}", flush=True)
        if report["status"] != "ok":
            broken.append(report)

    if args.results:
        with open(args.results, "w", encoding="utf-8") as out:
            for report in results:
                out.write(json.dumps(report) + "\n")
    if args.update:
        write_status(document, MODES[args.mode], results)

    print(f"\n{len(results) - len(broken)} of {len(results)} characters "
          f"passed ({args.mode})")
    if broken:
        print("broken:")
        for report in broken:
            print(f"  {report['character']:>3} {report['name']:<18} "
                  f"{report['status']}: {report['detail'][:90]}")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
