#!/usr/bin/env python3
"""Every raw-offset field access in the port's sources, and whether it is accounted for.

    tools/check_raw_offsets.py [--list]      --list prints the table the doc carries

What it looks for: pointer arithmetic on a numeric offset - `this + 0x20`,
`self + 0x110`, `(char*)ptr + 4` - used to reach a field, rather than a named member.
These sites reproduce retail's bytes and are sometimes the only way to write a function
whose member the header does not model, but they are invisible to a reader, they are
wrong on a 64-bit PC port, and upstream `PrimeDecomp/echoes` rejects them.

The policy, and why it is a file rather than a sentence, is in
`docs/research/raw_offsets.md`: every file that contains one has a section there, with the
count measured here, so a new raw offset in a new file fails this check instead of
accumulating quietly. Counts are per file rather than per line because line numbers move;
the count is what tells you the debt changed.

Deliberately *not* flagged, because they are ordinary code:
  - padding and array extents: `char x_pad0[0x190 - sizeof(CActor)]`
  - allocation sizes: `__nw__FUl(0x160, ...)`
  - anything inside a comment
"""
import pathlib
import re
import sys

# A byte-pointer cast: the reliable signal that a number is being used as a field offset.
BYTE_CAST = re.compile(
    r"reinterpret_cast\s*<\s*(?:const\s+)?(?:char|unsigned\s+char|signed\s+char"
    r"|u?int8_t|s8|u8|uchar)\s*\*\s*>"
    r"|\(\s*(?:const\s+)?(?:char|unsigned\s+char|u?int8_t|s8|u8|uchar)\s*\*\s*\)"
)
# Any cast at all, so `*reinterpret_cast<uint*>(self + 0x110)` is caught as well.
ANY_CAST = re.compile(r"reinterpret_cast|\(\s*(?:const\s+)?(?:u?int\d+_t|float|double|"
                      r"u?int\d+|uint|int|short|char|bool)\s*\*\s*\)")
# The offset itself: a hex literal, or a decimal that is not an array index or a mask.
OFFSET = re.compile(r"\+\s*\(?\s*(0x[0-9a-fA-F]{2,})\s*\)?|\+\s*\(?\s*([1-9][0-9]{1,})\s*\)?")
# What the offset is added to, when the pointer was not cast to a byte type first.
BASE = re.compile(r"(?:this|self|obj|_?this)\s*\+\s*$|\b(?:this|self)\s*\+\s*(?=0x|[1-9])")
# Things that look like offsets but are not field accesses.
IGNORE = re.compile(
    r"\[[^\]]*0x|\[[^\]]*\b(?:i|j|k|n|index|count)\b|__nw__FUl|__nwa|sizeof|offsetof"
    r"|lbl_[0-9A-Fa-f_]+|0x[0-9A-Fa-f]+_|\bR_|k[A-Z]\w*|&\s*0x|\|\s*0x|~\s*0x"
    r"|&\s*~\s*\d|<<\s*\d+\s*[-+)]"          # alignment masks and float exponent bias
)
# The SDK shim layer is a port of Nintendo's code, not our decompilation, and is measured by
# `tools/probe_sources.sh` instead. A raw offset there reproduces the SDK, not our guess.
SKIP_DIRS = ("src/Dolphin", "src/Runtime", "libc", "include/Dolphin", "include/Runtime")

DOC = pathlib.Path(__file__).resolve().parent.parent / "docs" / "research" / "raw_offsets.md"
ROOTS = ("src", "include")


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif text[i] == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def scan(path):
    """One hit per statement that reaches a field through a numeric offset."""
    hits = []
    code = strip_comments(path.read_text(errors="replace"))
    for number, line in enumerate(code.splitlines(), 1):
        if IGNORE.search(line):
            continue
        for stmt in line.split(";"):
            casts = BYTE_CAST.search(stmt) or ANY_CAST.search(stmt)
            if not casts and not BASE.search(stmt):
                continue
            for m in OFFSET.finditer(stmt):
                if BASE.search(stmt[:m.start()]):
                    pass
                elif not casts:
                    continue
                hits.append((number, m.group(1) or m.group(2)))
                break
    return hits


def documented_counts():
    """`## <path>  (N sites)` headings from the policy document."""
    if not DOC.exists():
        return {}
    out = {}
    for line in DOC.read_text().splitlines():
        m = re.match(r"^##\s+`?([\w./-]+)`?\s+\((\d+)\s+sites?\)", line)
        if m:
            out[m.group(1)] = int(m.group(2))
    return out


def main():
    listing = "--list" in sys.argv
    found = {}
    for root in ROOTS:
        base = pathlib.Path(__file__).resolve().parent.parent / root
        for path in sorted(base.rglob("*")):
            if path.suffix in (".cpp", ".c", ".hpp", ".h", ".cp"):
                rel = str(path.relative_to(base.parent))
                if rel.startswith(SKIP_DIRS):
                    continue
                hits = scan(path)
                if hits:
                    found[rel] = hits
    if listing:
        for rel, hits in sorted(found.items()):
            print("%-64s %3d" % (rel, len(hits)))
            for number, off in hits:
                print("      line %-5d +%s" % (number, off))
        print("total: %d raw-offset sites in %d file(s)" % (sum(map(len, found.values())), len(found)))
        return 0

    documented = documented_counts()
    bad = []
    for rel, hits in sorted(found.items()):
        want = documented.get(rel)
        if want is None:
            bad.append("%-64s %3d sites, no section in %s" % (rel, len(hits), DOC.name))
        elif want != len(hits):
            bad.append("%-64s doc says %d, measured %d" % (rel, want, len(hits)))
    for rel in sorted(set(documented) - set(found)):
        bad.append("%-64s documented but has no raw offsets any more - delete the section" % rel)

    if bad:
        print("raw offsets not accounted for:")
        for line in bad:
            print("  " + line)
        print("\npolicy: %s" % DOC)
        return 1
    print("ok: %d raw-offset site(s) in %d file(s), all documented in %s"
          % (sum(map(len, found.values())), len(found), DOC.name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
