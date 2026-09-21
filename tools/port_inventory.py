#!/usr/bin/env python3
"""Inventory native-port blockers without modifying the decompilation tree."""

from __future__ import annotations

import argparse
import json
import re
from collections import Counter
from pathlib import Path
from typing import Iterable


PLATFORM_PREFIXES = (
    "AI",
    "AR",
    "AX",
    "CARD",
    "DSP",
    "DVD",
    "EXI",
    "GX",
    "OS",
    "PAD",
    "SI",
    "VI",
)

FUNCTION_PATTERN = re.compile(
    rf"\b((?:{'|'.join(PLATFORM_PREFIXES)})[A-Za-z0-9_]+)(?=\s*\()"
)
POINTER_CAST_PATTERN = re.compile(
    r"\((?:u32|s32|unsigned\s+long|signed\s+long)\)\s*"
    r"(?:[A-Za-z_&*]|\([^)]*\*[^)]*\))"
)
MMIO_PATTERN = re.compile(r"\b0x(?:80|CC)[0-9A-Fa-f]{6}\b")
LAYOUT_PATTERN = re.compile(
    r"\b(?:ASSERT_SIZE|ASSERT_OFFSET)\s*\(|STATIC_ASSERT\s*\(\s*offsetof"
)
MUST_MATCH_PATTERN = re.compile(r"^\s*#\s*if[^\n]*\bMUST_MATCH\b", re.MULTILINE)
ASM_DEFINITION_PATTERN = re.compile(r"^\s*asm\s+[A-Za-z_]", re.MULTILINE)


def source_files(root: Path, relative_roots: Iterable[str]) -> list[Path]:
    result: list[Path] = []
    for relative_root in relative_roots:
        directory = root / relative_root
        if not directory.exists():
            continue
        result.extend(path for path in directory.rglob("*") if path.suffix in {".c", ".h"})
    return sorted(set(result))


def scan_group(root: Path, files: list[Path]) -> dict[str, object]:
    api_calls: Counter[str] = Counter()
    prefixes: Counter[str] = Counter()
    totals: Counter[str] = Counter()
    samples: dict[str, list[str]] = {
        "pointer_to_32_bit_cast": [],
        "fixed_or_mmio_address": [],
    }

    for path in files:
        text = path.read_text(encoding="utf-8", errors="replace")
        relative = path.relative_to(root).as_posix()
        totals["lines"] += text.count("\n") + (not text.endswith("\n"))
        totals["must_match_blocks"] += len(MUST_MATCH_PATTERN.findall(text))
        totals["layout_assertions"] += len(LAYOUT_PATTERN.findall(text))
        totals["asm_definitions"] += len(ASM_DEFINITION_PATTERN.findall(text))

        pointer_casts = list(POINTER_CAST_PATTERN.finditer(text))
        totals["pointer_to_32_bit_casts"] += len(pointer_casts)
        mmio_addresses = list(MMIO_PATTERN.finditer(text))
        totals["fixed_or_mmio_addresses"] += len(mmio_addresses)

        for category, matches in (
            ("pointer_to_32_bit_cast", pointer_casts),
            ("fixed_or_mmio_address", mmio_addresses),
        ):
            for match in matches:
                if len(samples[category]) >= 20:
                    break
                line = text.count("\n", 0, match.start()) + 1
                samples[category].append(f"{relative}:{line}")

        for function in FUNCTION_PATTERN.findall(text):
            api_calls[function] += 1
            prefix = next(
                candidate
                for candidate in PLATFORM_PREFIXES
                if function.startswith(candidate)
            )
            prefixes[prefix] += 1

    return {
        "files": len(files),
        **dict(sorted(totals.items())),
        "platform_api": {
            "unique_functions": len(api_calls),
            "occurrences_by_prefix": dict(sorted(prefixes.items())),
            "functions": dict(sorted(api_calls.items())),
        },
        "samples": samples,
    }


def build_inventory(root: Path) -> dict[str, object]:
    assembly_files = sorted(
        path.relative_to(root).as_posix()
        for base in (root / "src", root / "extern" / "dolphin")
        if base.exists()
        for path in base.rglob("*")
        if path.suffix.lower() == ".s"
    )

    return {
        "schema_version": 2,
        "assembly_files": assembly_files,
        "game_and_baselib": scan_group(root, source_files(root, ("src",))),
        "dolphin_sdk": scan_group(
            root,
            source_files(root, ("extern/dolphin/include", "extern/dolphin/src")),
        ),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="repository root (default: inferred from this script)",
    )
    parser.add_argument("--output", type=Path, help="write JSON to this path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    inventory = build_inventory(root)
    rendered = json.dumps(inventory, indent=2, sort_keys=True) + "\n"
    if args.output is None:
        print(rendered, end="")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
