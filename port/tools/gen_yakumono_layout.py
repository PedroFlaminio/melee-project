#!/usr/bin/env python3
"""Generate the host's yakumono_param translators from the game's own structs.

Every Gr*.dat carries a public symbol called `yakumono_param`, and each stage
declares its own struct for it inside its `grXXX.c`.  The host therefore cannot
tell the layouts apart by name, and telling them apart by size is what findings
R03-R05 of docs/review-fa257ed.md removed, because several stages share an
extent with incompatible fields.  The stage's identity comes from
`melee_host_stage_current_grkind()` instead, and this script turns each stage's
struct into a translator selected by that GrKind.

The structs live in .c files, so the host cannot include them.  This follows
gen_host_command_layout.py: generate a host copy plus a C check that fails the
build when the copy and the game's struct disagree, and regenerate whenever a
stage's struct changes.

    python3 port/tools/gen_yakumono_layout.py           # rewrite the outputs
    python3 port/tools/gen_yakumono_layout.py --check   # fail if stale
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
GR = REPO / "src/melee/gr"
OUT_H = REPO / "port/src/game/yakumono_param.h"
OUT_C = REPO / "port/src/game/yakumono_param.c.inc"

# GrKind -> the module that owns the stage, and how its yakumono_param is
# declared.  Taken from ground.c's stage_datas[] and each module's own
# declaration; see the mapping in docs/native_port_development.md.
#
#   struct NAME  a named struct in that file
#   anon         a `static struct { ... }* yakumono_param;` in that file
#   opaque       the stage declares `void* yakumono_param` and never reads it
#   absent       the file never mentions yakumono_param
STAGES = [
    (2,  "grcastle.c",      "struct grCastle_YakumonoParam"),
    (3,  "grrcruise.c",     "struct grRCruise_YakumonoParam"),
    (4,  "grkongo.c",       "struct grKongo_YakumonoParam"),
    (5,  "grgarden.c",      "struct grGarden_YakumonoParam"),
    (6,  "grgreatbay.c",    "absent"),
    (7,  "grshrine.c",      "opaque"),
    (8,  "grzebes.c",       "grZe_YakumonoParam"),
    (9,  "grkraid.c",       "struct grKraid_YakumonoParam"),
    (10, "grstory.c",       "struct grStory_YakumonoParam"),
    (11, "gryorster.c",     "absent"),
    (12, "grizumi.c",       "struct grIzumi_YakumonoParam"),
    (13, "grgreens.c",      "struct grGreens_YakumonoParam"),
    (14, "grcorneria.c",    "struct grCorneria_YakumonoParam"),
    (15, "grvenom.c",       "struct grVenom_YakumonoParam"),
    (16, "grpstadium.c",    "anon"),
    (17, "grpura.c",        "opaque"),
    (18, "grmutecity.c",    "struct grMc_YakumonoParam"),
    (19, "grbigblue.c",     "grBb_YakumonoParam"),  # in grbigblue.static.h
    (20, "gronett.c",       "struct grOnett_StageParam"),
    (21, "grfourside.c",    "struct grFourside_YakumonoParam"),
    (22, "gricemt.c",       "struct grIceMt_YakumonoParam"),
    (24, "grinishie1.c",    "struct grInishie1_YakumonoParam"),
    (25, "grinishie2.c",    "struct grInishie2_YakumonoParam"),
    (27, "grflatzone.c",    "struct grFlatzone_YakumonoParam"),
    (28, "groldpupupu.c",   "struct grOldpupupu_YakumonoParam"),
    (29, "groldyoshi.c",    "anon"),
    (30, "groldkongo.c",    "struct grOldKongo_YakumonoParam"),
    (31, "grkinokoroute.c", "anon"),
    (32, "grshrineroute.c", "struct grShrineRoute_YakumonoParam"),
    (33, "grzebesroute.c",  "struct grZebesRoute_YakumonoParam"),
    (34, "grbigblueroute.c", "struct grBigBlueRoute_YakumonoParam"),
    (36, "grbattle.c",      "struct grBattle_YakumonoParam"),
    (37, "grlast.c",        "anon"),
]

# Stages whose block on disc is larger than the struct their grXXX.c declares.
# The excess is trailing and the stage never reads it, so the layout still
# describes every field the stage uses; only the extent check needs the real
# size.  Anything not listed here must match exactly -- a difference is the
# generator and the struct disagreeing, not an unread tail.
DISC_SIZES = {
    # grcastle.c's struct ends at 0x144 with f32 x140; the block is 0x148, one
    # unnamed word past the last field the stage reads.
    2: 0x148,
}

GRKIND_NAMES = {
    2: "Castle", 3: "RCruise", 4: "Kongo", 5: "Garden", 6: "GreatBay",
    7: "Shrine", 8: "Zebes", 9: "Kraid", 10: "Story", 11: "Yorster",
    12: "Izumi", 13: "Greens", 14: "Corneria", 15: "Venom", 16: "PStadium",
    17: "Pura", 18: "MuteCity", 19: "BigBlue", 20: "Onett", 21: "Fourside",
    22: "Icemt", 24: "Inishie1", 25: "Inishie2", 27: "Flatzone",
    28: "OldPupupu", 29: "OldYoshi", 30: "OldKongo", 31: "KinokoRoute",
    32: "ShrineRoute", 33: "ZebesRoute", 34: "BigBlueRoute", 36: "Battle",
    37: "Last",
}

# width on disc (== width on the host for everything but a pointer), and the
# reader call that produces the value.
SCALARS = {
    "f32": (4, "f32"), "float": (4, "f32"),
    "s32": (4, "s32"), "int": (4, "s32"), "enum_t": (4, "s32"),
    "u32": (4, "u32"), "unsigned int": (4, "u32"),
    "s16": (2, "s16"), "short": (2, "s16"),
    "u16": (2, "u16"), "unsigned short": (2, "u16"),
    "s8": (1, "s8"), "char": (1, "s8"), "signed char": (1, "s8"),
    "u8": (1, "u8"), "unsigned char": (1, "u8"), "bool": (1, "u8"),
}
# Composites expanded into scalars.
COMPOSITES = {
    "Vec3": [("x", "f32"), ("y", "f32"), ("z", "f32")],
    "Vec2": [("x", "f32"), ("y", "f32")],
}
POINTER_RE = re.compile(r"\*\s*$")


class Unsupported(Exception):
    pass


def find_struct_body(text: str, spec: str, path: str) -> tuple[str, str]:
    """Return (body, a name for the host copy) for one stage's struct."""
    if spec == "anon":
        m = re.search(r"\}\s*\*\s*yakumono_param\s*;", text)
        if not m:
            raise Unsupported(f"{path}: no anonymous yakumono_param struct")
        end = m.start()
        # Walk back to the `struct {` whose braces close here.
        depth = 0
        start = None
        for j in range(end, -1, -1):
            if text[j] == "}":
                depth += 1
            elif text[j] == "{":
                depth -= 1
                if depth == 0:
                    start = j
                    break
        if start is None:
            raise Unsupported(f"{path}: anonymous struct start not found")
        return text[start + 1:end], None
    name = spec.replace("struct ", "").strip()
    m = re.search(r"(?:typedef\s+)?struct\s+" + re.escape(name) +
                  r"\s*\{", text)
    if not m:
        # A typedef'd struct: `typedef struct { ... } NAME;`
        m2 = re.search(r"typedef struct \{", text)
        if m2 and re.search(r"\}\s*" + re.escape(name) + r"\s*;", text):
            start = m2.end()
            end = text.index("} " + name, start)
            return text[start:end], name
        raise Unsupported(f"{path}: struct {name} not found")
    depth = 0
    i = m.end() - 1
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i + 1:j], name
    raise Unsupported(f"{path}: struct {name} is unterminated")


def array_count(expr: str, text: str, path: str) -> int:
    """An array bound: a literal, an arithmetic expression, or a #define."""
    expr = expr.strip()
    # Substitute any #define the file declares, repeatedly for nested ones.
    for _ in range(4):
        names = set(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", expr))
        if not names:
            break
        changed = False
        for name in names:
            d = re.search(r"^\s*#define\s+" + re.escape(name) + r"\s+(.+)$",
                          text, re.M)
            if d:
                expr = re.sub(r"\b" + re.escape(name) + r"\b",
                              "(" + d.group(1).split("//")[0].strip() + ")",
                              expr)
                changed = True
        if not changed:
            break
    residue = re.sub(r"0[xX][0-9A-Fa-f]+|\d+", "", expr)
    if re.search(r"[A-Za-z_]", residue):
        raise Unsupported(f"{path}: array bound {expr!r} is not a number")
    if not re.fullmatch(r"[0-9xXa-fA-F+\-*/() ]+", expr):
        raise Unsupported(f"{path}: array bound {expr!r} is not arithmetic")
    try:
        return int(eval(expr, {"__builtins__": {}}, {}))  # noqa: S307
    except Exception as exc:
        raise Unsupported(f"{path}: array bound {expr!r}: {exc}") from exc


FIELD_RE = re.compile(
    r"^\s*(?:/\*[^*]*\*/\s*)?"            # optional offset comment
    r"(?P<type>(?:struct\s+|unsigned\s+|signed\s+)?[A-Za-z_][A-Za-z0-9_]*)"
    r"\s*(?P<ptr>\*?)\s*"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"\s*(?P<arr>(?:\[[^\]]*\])*)\s*;")


def parse_one(typ, name, count, is_ptr, offset, text, path):
    """One field: its record and the offset just past it."""
    typ = " ".join(typ.split())
    if is_ptr:
        offset = (offset + 3) // 4 * 4
        return dict(kind="pointer", name=name, offset=offset, count=count,
                    ctype="void*"), offset + 4 * count
    base = typ.replace("struct ", "")
    if base in SCALARS:
        width, reader = SCALARS[base]
        offset = (offset + width - 1) // width * width
        return dict(kind="scalar", name=name, offset=offset, count=count,
                    width=width, reader=reader, ctype=typ), offset + width * count
    raise Unsupported(f"{path}: field {name} has unsupported type {typ}")


def parse_fields(body: str, text: str, path: str) -> list[dict]:
    """Flatten a struct body into scalar and pointer fields with disc offsets."""
    fields = []
    offset = 0
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("//"):
            continue
        if line.startswith("/*") and line.endswith("*/"):
            continue
        multi = re.match(r"^\s*(?:/\*[^*]*\*/\s*)?"
                         r"((?:struct\s+|unsigned\s+|signed\s+)?"
                         r"[A-Za-z_][A-Za-z0-9_]*)\s+"
                         r"([A-Za-z_][A-Za-z0-9_]*\s*(?:,\s*"
                         r"[A-Za-z_][A-Za-z0-9_]*\s*)+);", line)
        if multi:
            line = None
            for nm in [n.strip() for n in multi.group(2).split(",")]:
                sub, offset = parse_one(multi.group(1), nm, 1, False,
                                        offset, text, path)
                fields.append(sub)
            continue
        m = FIELD_RE.match(line)
        if not m:
            if line in ("{", "}") or line.startswith("#"):
                continue
            raise Unsupported(f"{path}: cannot parse field {line!r}")
        typ = " ".join(m.group("type").split())
        name = m.group("name")
        is_ptr = m.group("ptr") == "*"
        counts = [array_count(c, text, path)
                  for c in re.findall(r"\[([^\]]+)\]", m.group("arr"))
                  if c.strip()] or [1]
        count = 1
        for c in counts:
            count *= c

        if is_ptr:
            # Every pointer in these structs is a colour-animation command
            # script handed to grMaterial_801C9604; the reader materializes it
            # the same way Battlefield's overlays already are.
            align = 4
            offset = (offset + align - 1) // align * align
            fields.append(dict(kind="pointer", name=name, offset=offset,
                               count=count, ctype="void*"))
            offset += 4 * count
            continue

        base = typ.replace("struct ", "")
        if base in SCALARS:
            width, reader = SCALARS[base]
            offset = (offset + width - 1) // width * width
            fields.append(dict(kind="scalar", name=name, offset=offset,
                               count=count, width=width, reader=reader,
                               ctype=typ))
            offset += width * count
        elif base in COMPOSITES:
            offset = (offset + 3) // 4 * 4
            for i in range(count):
                for mname, mtype in COMPOSITES[base]:
                    width, reader = SCALARS[mtype]
                    label = f"{name}_{i}_{mname}" if count > 1 \
                        else f"{name}_{mname}"
                    fields.append(dict(kind="scalar", name=label,
                                       offset=offset, count=1, width=width,
                                       reader=reader, ctype=mtype))
                    offset += width
        elif nested_body(text, base) is not None:
            inner, _ = parse_fields(nested_body(text, base), text, path)
            inner_size = 0
            for f in inner:
                span = f["offset"] + f.get("width", 4) * f["count"]
                if f["kind"] == "composite":
                    span = f["offset"] + 4 * len(f["members"]) * f["count"]
                inner_size = max(inner_size, span)
            inner_size = (inner_size + 3) // 4 * 4
            offset = (offset + 3) // 4 * 4
            for i in range(count):
                base_off = offset + inner_size * i
                for sub in inner:
                    label = f"{name}_{i}_{sub['name']}" if count > 1 \
                        else f"{name}_{sub['name']}"
                    entry = dict(sub)
                    entry["name"] = label
                    entry["offset"] = base_off + sub["offset"]
                    fields.append(entry)
            offset += inner_size * count
        else:
            raise Unsupported(f"{path}: field {name} has unsupported type {typ}")
    return fields, offset


def nested_body(text: str, name: str):
    """The body of a struct or typedef declared in the same file, or None."""
    m = re.search(r"(?:typedef\s+)?struct\s+" + re.escape(name) + r"\s*\{", text)
    if not m:
        m = re.search(r"typedef struct \{", text)
        if not m or not re.search(r"\}\s*" + re.escape(name) + r"\s*;", text):
            return None
        start = m.end() - 1
    else:
        start = m.end() - 1
    depth = 0
    for j in range(start, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:j]
    return None


def declared_offsets(body: str) -> dict[str, int]:
    """The `/* 0x1C */` comments the decomp writes, when present."""
    out = {}
    for line in body.splitlines():
        m = re.match(r"\s*/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*.*?"
                     r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])*\s*;", line)
        if m:
            out[m.group(2)] = int(m.group(1), 16)
    return out


def sources_for(module: str) -> str:
    """The module's own text plus the headers a stage keeps its struct in."""
    stem = module[:-2]
    parts = []
    for name in (module, stem + ".h", stem + ".static.h", "types.h"):
        f = GR / name
        if f.exists():
            parts.append(f.read_text())
    return "\n".join(parts)


def collect() -> tuple[list[dict], list[tuple[int, str, str]]]:
    done, skipped = [], []
    for grkind, module, spec in STAGES:
        path = GR / module
        if spec in ("absent", "opaque"):
            done.append(dict(grkind=grkind, module=module, mode=spec,
                             name=GRKIND_NAMES[grkind]))
            continue
        text = sources_for(module)
        try:
            body, name = find_struct_body(text, spec, module)
            fields, size = parse_fields(body, text, module)
        except Unsupported as exc:
            skipped.append((grkind, module, str(exc)))
            continue
        declared = declared_offsets(body)
        for f in fields:
            want = declared.get(f["name"])
            if want is not None and want != f["offset"]:
                skipped.append((grkind, module,
                                f"{f['name']}: computed 0x{f['offset']:X}, "
                                f"decomp says 0x{want:X}"))
                break
        else:
            align = 1
            for f in fields:
                align = max(align, 4 if f["kind"] == "pointer" else f["width"])
            size = (size + align - 1) // align * align
            done.append(dict(grkind=grkind, module=module, mode="struct",
                             disc_size=DISC_SIZES.get(grkind, size),
                             name=GRKIND_NAMES[grkind],
                             struct=f"melee_host_yakumono_{GRKIND_NAMES[grkind].lower()}",
                             fields=fields, size=size, source=spec))
    return done, skipped


READERS = {
    "f32": ("mh_f32", "melee_host_hsd_reader_f32(reader, {off})"),
    "s32": ("mh_s32", "(mh_s32) melee_host_hsd_reader_u32(reader, {off})"),
    "u32": ("mh_u32", "melee_host_hsd_reader_u32(reader, {off})"),
    "s16": ("mh_s16", "(mh_s16) melee_host_hsd_reader_u16(reader, {off})"),
    "u16": ("mh_u16", "melee_host_hsd_reader_u16(reader, {off})"),
    "s8":  ("mh_s8",  "(mh_s8) melee_host_hsd_reader_u8(reader, {off})"),
    "u8":  ("mh_u8",  "melee_host_hsd_reader_u8(reader, {off})"),
}


def emit_struct(stage) -> str:
    """The host's copy of one stage's struct."""
    lines = [f"/* {stage['module']}: {stage['source']} */",
             f"struct {stage['struct']} {{"]
    for f in stage["fields"]:
        arr = f"[{f['count']}]" if f["count"] > 1 else ""
        if f["kind"] == "pointer":
            lines.append(f"    /* 0x{f['offset']:03X} */ void* {f['name']}{arr};")
        else:
            lines.append(f"    /* 0x{f['offset']:03X} */ "
                         f"{READERS[f['reader']][0]} {f['name']}{arr};")
    lines.append("};")
    return "\n".join(lines)


def emit_translator(stage) -> str:
    """The function that fills one stage's struct from the disc bytes."""
    name = f"yakumono_{stage['name'].lower()}"
    out = [f"static void* {name}(MeleeHostHsdReader* reader, mh_u32 root)",
           "{",
           f"    struct {stage['struct']}* out = melee_host_hsd_reader_allocate(",
           f"        reader, sizeof(*out), _Alignof(struct {stage['struct']}));",
           "    mh_u32 target;",
           "",
           "    if (out == NULL) {",
           "        return NULL;",
           "    }",
           "    (void) target;",
           "    /* The block on disc has to be the size this layout describes;",
           "     * a mismatch means the stage's struct moved under the",
           "     * generator, not that the data is unusual. */",
           f"    if (melee_host_hsd_reader_extent(reader, root) != 0x{stage['disc_size']:X}) {{",
           "        char message[96];",
           "",
           "        (void) snprintf(message, sizeof(message),",
           f"                        \"{stage['name']} yakumono_param is 0x%X \"",
           f"                        \"bytes, this layout expects 0x{stage['disc_size']:X}\",",
           "                        melee_host_hsd_reader_extent(reader, root));",
           "        melee_host_hsd_reader_fail(reader, message);",
           "        return NULL;",
           "    }"]
    for f in stage["fields"]:
        for i in range(f["count"]):
            idx = f"[{i}]" if f["count"] > 1 else ""
            if f["kind"] == "pointer":
                off = f["offset"] + 4 * i
                out += [
                    f"    if (melee_host_hsd_reader_pointer(reader, "
                    f"root + 0x{off:X}, &target)) {{",
                    f"        out->{f['name']}{idx} =",
                    f"            melee_host_hsd_reader_command_stream(reader, target);",
                    "    } else {",
                    f"        out->{f['name']}{idx} = NULL;",
                    "    }"]
            else:
                off = f["offset"] + f["width"] * i
                expr = READERS[f["reader"]][1].format(off=f"root + 0x{off:X}")
                out.append(f"    out->{f['name']}{idx} = {expr};")
    out += ["    return melee_host_hsd_reader_failed(reader) ? NULL : out;",
            "}"]
    return "\n".join(out)


def emit(done) -> tuple[str, str]:
    structs = [d for d in done if d["mode"] == "struct"]
    banner = ("/* Generated by port/tools/gen_yakumono_layout.py from the\n"
              " * stage structs in src/melee/gr.  Do not edit; rerun the\n"
              " * generator when a stage's yakumono_param struct changes.\n"
              " *\n"
              " * Each stage declares its own struct for the `yakumono_param`\n"
              " * symbol every Gr*.dat carries, so the layout is chosen by the\n"
              " * stage's GrKind, never by the block's size: several stages\n"
              " * share an extent with incompatible fields, which is what\n"
              " * findings R03-R05 of docs/review-fa257ed.md removed. */\n")

    head = [banner, "#ifndef MELEE_HOST_YAKUMONO_PARAM_H",
            "#define MELEE_HOST_YAKUMONO_PARAM_H", "",
            "#include <melee_host/types.h>", ""]
    for s in structs:
        head += [emit_struct(s), ""]
    head += ["#endif", ""]

    body = [banner]
    for s in structs:
        body += [emit_translator(s), ""]
    body += [
        "/* The stage being loaded picks the layout.  Ground_801C0754 sets",
        " * stage_info.grkind before the archive read that runs this. */",
        "static void* stage_yakumono_param_by_kind(MeleeHostHsdReader* reader,",
        "                                          mh_u32 root, int grkind)",
        "{",
        "    switch (grkind) {"]
    for s in structs:
        body.append(f"    case {s['grkind']}: /* {s['name']} */")
        body.append(f"        return yakumono_{s['name'].lower()}(reader, root);")
    for s in done:
        if s["mode"] == "opaque":
            body.append(f"    case {s['grkind']}: /* {s['name']}: declares "
                        f"yakumono_param as void* and never reads it */")
        elif s["mode"] == "absent":
            body.append(f"    case {s['grkind']}: /* {s['name']}: never "
                        f"mentions yakumono_param */")
    body += [
        "        /* Nothing dereferences the block, so the bytes are handed",
        "         * over as they are rather than guessed at. */",
        "        return melee_host_hsd_reader_payload(",
        "            reader, root, melee_host_hsd_reader_extent(reader, root));",
        "    default:",
        "        break;",
        "    }",
        "    /* No stage is being loaded -- an archive parsed on its own, as",
        "     * the tests do -- or the stage has no layout here.  A block that",
        "     * is entirely zero means the same under every layout, so it still",
        "     * translates; anything else needs that stage's struct. */",
        "    {",
        "        const mh_u32 size = melee_host_hsd_reader_extent(reader, root);",
        "        mh_u32 at;",
        "",
        "        for (at = 0; at < size; at += 4) {",
        "            if (melee_host_hsd_reader_u32(reader, root + at) != 0) {",
        "                break;",
        "            }",
        "        }",
        "        if (size != 0 && at >= size) {",
        "            void* block = melee_host_hsd_reader_allocate(",
        "                reader, size, _Alignof(mh_u32));",
        "            if (block != NULL) {",
        "                memset(block, 0, size);",
        "            }",
        "            return block;",
        "        }",
        "    }",
        "    melee_host_hsd_reader_fail(",
        '           reader, "yakumono_param has no layout for this stage");',
        "    return NULL;",
        "}", ""]
    return "\n".join(head), "\n".join(body)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    done, skipped = collect()
    if skipped and not args.report:
        for grkind, module, why in skipped:
            print(f"unsupported: {grkind} {module}: {why}", file=sys.stderr)
        return 2
    header, source = emit(done)
    if args.check:
        stale = [f for f, text in ((OUT_H, header), (OUT_C, source))
                 if not f.exists() or f.read_text() != text]
        if stale:
            for f in stale:
                print(f"{f.relative_to(REPO)} is stale; rerun "
                      f"port/tools/gen_yakumono_layout.py", file=sys.stderr)
            return 1
        print("yakumono layouts are up to date")
        return 0
    if not args.report:
        OUT_H.write_text(header)
        OUT_C.write_text(source)
        print(f"wrote {OUT_H.relative_to(REPO)} and {OUT_C.relative_to(REPO)}")
    if args.report:
        ok = [d for d in done if d["mode"] == "struct"]
        print(f"{len(ok)} stages parsed, "
              f"{len([d for d in done if d['mode'] != 'struct'])} need no layout, "
              f"{len(skipped)} unsupported")
        for grkind, module, why in skipped:
            print(f"  skip {grkind:>3} {module:<20} {why}")
        for d in ok:
            ptr = sum(1 for f in d["fields"] if f["kind"] == "pointer")
            print(f"  ok   {d['grkind']:>3} {d['module']:<20} "
                  f"{len(d['fields'])} fields, 0x{d['size']:X} bytes"
                  + (f", {ptr} pointers" if ptr else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
