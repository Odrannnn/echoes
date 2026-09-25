#!/usr/bin/env python3
"""Derive the REL module load order from the modules themselves.

    python3 tools/gen_module_order.py [--check] [--write-list]

Why this exists. The port needs a game-side module manager to load the 83 modules it does not
compile in, and the retail module descriptor table - which modules exist, where they sit on the
disc, in what order - is not in this repository. `docs/research/rel_module_manager.md` has that
measurement.

The *order*, though, does not need the disc image, and this derives it. Each module's own id is in
its header at 0x00, and its import table is a list of `{u32 id, u32 offset}` entries naming the
modules it links against, where id 0 is the main DOL. So the whole dependency graph is inside the
86 files we already have, and the load order is its topological sort.

Two things this deliberately does NOT do, because both are decisions rather than measurements:

  - it does not invent disc offsets. The files in `orig/G2ME01/files/RelProd/` are extracted
    modules with no record of where they lived on the image, so anything this emits is order and
    dependency only.
  - it does not decide whether the port should read modules from the disc at all. See
    `docs/research/rel_module_manager.md`; generating a table and shipping it is the alternative to
    a disc image, not a replacement for that decision.

The header and import layout are read out of `platform/rel.cpp`'s `Probe` and
`platform/include/port_rel.h`'s `RelImportInfo` - both big-endian, as the cube's format is. If you
change one, change this.

Exit status is 0 on success. With --check, status is 1 if the emitted order in
`docs/research/rel_module_order.md` disagrees with what this run derived, so the file cannot go
stale the way a quoted list does.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RELDIR = ROOT / "orig/G2ME01/files/RelProd"
DOC = ROOT / "docs/research/rel_module_order.md"

K_HEADER_V1 = 0x40
K_MAX_SECTIONS = 64
# The main DOL is import id 0 in every module's table; it is not a module we load.
DOL_ID = 0


def be32(b: bytes, off: int) -> int:
    return struct.unpack_from(">I", b, off)[0]


def read_header(data: bytes):
    """Return (module_id, imp_offset, imp_size, bss_size, version), or None if implausible.

    The bounds are the same ones platform/rel.cpp's Probe applies, so a file this accepts is a file
    the port's own loader would accept.
    """
    if len(data) < K_HEADER_V1:
        return None
    module_id = be32(data, 0x00)
    num_sections = be32(data, 0x0C)
    section_info = be32(data, 0x10)
    version = be32(data, 0x1C)
    bss_size = be32(data, 0x20)
    imp_offset = be32(data, 0x28)
    imp_size = be32(data, 0x2C)
    if version < 1 or version > 3 or not (1 <= num_sections <= K_MAX_SECTIONS):
        return None
    if section_info > len(data) or num_sections * 8 > len(data) - section_info:
        return None
    if imp_offset > len(data) or imp_size > len(data) - imp_offset:
        return None
    if imp_size % 8 != 0:
        return None
    return module_id, imp_offset, imp_size, bss_size, version


def collect():
    """Map every module file to its id, and every module's id to the ids it imports."""
    if not RELDIR.is_dir():
        print(f"gen_module_order: {RELDIR} does not exist", file=sys.stderr)
        sys.exit(2)
    by_id, imports, sizes = {}, {}, {}
    files = sorted(RELDIR.glob("*.rel"))
    if not files:
        print(f"gen_module_order: no .rel files in {RELDIR}", file=sys.stderr)
        sys.exit(2)
    for path in files:
        data = path.read_bytes()
        head = read_header(data)
        if head is None:
            print(f"  {path.name}: header not plausible, skipped", file=sys.stderr)
            continue
        module_id, imp_off, imp_size, bss, version = head
        name = path.stem
        if module_id in by_id and by_id[module_id] != name:
            # Two files claiming one id would make the graph ambiguous, and a
            # wrong import edge is worse than a missing one.
            print(f"  {name}: id {module_id} already claimed by {by_id[module_id]}, skipped",
                  file=sys.stderr)
            continue
        by_id[module_id] = name
        deps = set()
        for i in range(0, imp_size, 8):
            deps.add(be32(data, imp_off + i))
        deps.discard(DOL_ID)
        # The first import entry is the module's *own* id: a module lists itself
        # for the relocations applied to its own image. Measured on Tweaks.rel,
        # whose entries are id=82 (its own moduleId) and id=0 (the DOL). Leaving
        # the self-edge in makes every module a one-node cycle and the topological
        # sort finds nothing ready - which is what it did, for all 86 at once.
        deps.discard(module_id)
        imports[name] = deps
        sizes[name] = (len(data), bss, version)
    return by_id, imports, sizes, len(files)


def toposort(by_id, imports):
    """Kahn's algorithm over the import graph, with ties broken by module id.

    Ties are inevitable - many modules import only the DOL - so the order within a
    tier is by id, which is stable and reproducible rather than dependent on
    directory order.
    """
    unknown = set()
    edges = {}
    for name, deps in imports.items():
        resolved = set()
        for d in deps:
            if d in by_id:
                resolved.add(by_id[d])
            else:
                unknown.add(d)
        edges[name] = resolved

    order, remaining = [], dict(edges)
    while remaining:
        ready = sorted(n for n, ds in remaining.items() if not (ds & set(remaining)))
        if not ready:
            # A cycle. Report it rather than emitting an order that cannot work.
            stuck = sorted(remaining)
            return order, stuck, unknown
        for n in ready:
            order.append(n)
            del remaining[n]
    return order, [], unknown


def render(by_id, imports, sizes, order, cycles, unknown, total_files):
    id_of = {n: i for i, n in by_id.items()}
    lines = [
        "# The REL module load order, derived from the modules themselves",
        "",
        "Generated by `tools/gen_module_order.py`. **Do not hand-edit**: run the tool, or",
        "`--check` will fail the gate.",
        "",
        "This is the part of the module manager that does **not** need a disc image. Each module's",
        "own id is in its header at `0x00` and its import table is a list of `{u32 id, u32 offset}`",
        "entries naming what it links against, so the dependency graph is inside the 86 files in",
        "`orig/G2ME01/files/RelProd/`. What is *not* here is any disc offset: these files are",
        "extracted modules with no record of where they lived. See `rel_module_manager.md`.",
        "",
        f"- modules found: **{len(imports)}** of {total_files} `.rel` files",
        f"- ids referenced by an import but not present in the directory: "
        f"**{len(unknown)}**" + (f" ({', '.join(str(u) for u in sorted(unknown))})"
                                  if unknown else " (none - every import resolves)"),
        f"- dependency cycles: **{len(cycles)}**" + (f" ({', '.join(cycles)})" if cycles else ""),
        "",
        "## The order",
        "",
        "`imports` counts dependencies on other modules, excluding the main DOL, which every module",
        "imports and which is the executable rather than something to load.",
        "",
        "| # | module | id | imports | file bytes | bss bytes | version |",
        "| ---: | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for i, name in enumerate(order, 1):
        n, bss, ver = sizes[name]
        ndeps = len(imports[name] - {DOL_ID})
        lines.append(f"| {i} | `{name}` | {id_of[name]} | {ndeps} | {n} | {bss} | {ver} |")
    if cycles:
        lines += ["", "## Modules in a dependency cycle", "",
                  "These cannot be ordered against each other and the runtime will reject them:", ""]
        for name in cycles:
            lines.append(f"- `{name}` imports: "
                         + (", ".join(f"`{d}`" for d in sorted(imports[name])) or "(nothing)"))
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true",
                    help="fail if the committed document disagrees with this run")
    ap.add_argument("--write-list", action="store_true", help="rewrite the document")
    args = ap.parse_args()

    by_id, imports, sizes, total = collect()
    order, cycles, unknown = toposort(by_id, imports)
    text = render(by_id, imports, sizes, order, cycles, unknown, total)

    if args.write_list:
        DOC.write_text(text)
        print(f"wrote {DOC.relative_to(ROOT)}: {len(order)} modules ordered, "
              f"{len(cycles)} cycles, {len(unknown)} unresolved import ids")
        return 0

    if not DOC.exists():
        print(f"gen_module_order: {DOC} does not exist; run with --write-list", file=sys.stderr)
        return 1
    if DOC.read_text() != text:
        print("gen_module_order: docs/research/rel_module_order.md is stale.", file=sys.stderr)
        print("  Re-run with --write-list, in the same commit as whatever moved it.", file=sys.stderr)
        return 1
    print(f"rel_module_order: {len(order)} modules, unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
