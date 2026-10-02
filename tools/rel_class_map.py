#!/usr/bin/env python3
"""Which class owns each function of a REL module, read from the module's vtables.

    python3 tools/rel_class_map.py DarkSamus            # the module's classes and their slots
    python3 tools/rel_class_map.py --summary            # all modules: unmatched functions a vtable names

A module's own functions carry no names (`fn_<id>_<off>`), and the Trilogy disc cannot supply them
(docs/research/trilogy_name_pairing.md). The modules carry no typeinfo either (unlike the DOL, which
tools/vtable_of.py reads): a REL vtable is two zero words, then one pointer per virtual, zero on disc
and filled by ADDR32 relocations, so this walks the relocation table instead of the bytes. A slot
that relocates to the module's own .text is a virtual this module defines; one that relocates to the
DOL is inherited. The base class is the DOL `__vt__` object that agrees with the most inherited
slots, and a slot the module defines overrides the virtual the base has at the same index - which
gives that `fn_` a real name and signature.

The base is a best fit, not a proof: a class deriving from a class that is itself only in the module
shows the nearest DOL ancestor. Slots past the base's length are virtuals the module adds, unnamed.
Non-virtual members are not found at all.
"""
import json
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
RELS = ROOT / "orig/G2ME01/files/RelProd"


def load(mod):
    d = (RELS / f"{mod}.rel").read_bytes()
    mid, = struct.unpack(">I", d[0:4])
    nsec, so = struct.unpack(">II", d[0x0C:0x14])
    secs = [struct.unpack(">II", d[so + 8 * i:so + 8 * i + 8]) for i in range(nsec)]
    impo, imps = struct.unpack(">II", d[0x28:0x30])
    rel = {}  # (section, offset) -> (module id, target section, addend)
    for i in range(impo, impo + imps, 8):
        m, o = struct.unpack(">II", d[i:i + 8])
        sec, off = 0, 0
        while True:
            delta, typ, tsec, add = struct.unpack(">HBBI", d[o:o + 8])
            o += 8
            off += delta
            if typ == 203:
                break
            if typ == 202:
                sec, off = tsec, 0
            elif typ == 1:
                rel[(sec, off)] = (m, tsec, add)
    return d, mid, secs, rel


def dol_vtables():
    """{class name: [slot address ...]} for every `__vt__` object in the DOL, and {address: name}."""
    d = (ROOT / "orig/G2ME01/sys/main.dol").read_bytes()
    segs = [(struct.unpack(">I", d[0x48 + 4 * i:0x4C + 4 * i])[0], struct.unpack(">I", d[4 * i:4 * i + 4])[0],
             struct.unpack(">I", d[0x90 + 4 * i:0x94 + 4 * i])[0]) for i in range(18)]

    def read(addr, n):
        for va, fo, sz in segs:
            if sz and va <= addr and addr + n <= va + sz:
                return d[fo + addr - va:fo + addr - va + n]
        return b""
    names, vts = {}, {}
    for line in (ROOT / "config/G2ME01/symbols.txt").read_text().splitlines():
        m = re.match(r"(\S+) = \.(\w+):0x([0-9A-Fa-f]+);(?:.*size:0x([0-9A-Fa-f]+))?", line)
        if not m:
            continue
        addr = int(m.group(3), 16)
        if m.group(2) in ("text", "init"):
            names[addr] = m.group(1)
        elif m.group(1).startswith("__vt__") and m.group(4):
            raw = read(addr + 8, int(m.group(4), 16) - 8)
            vts[m.group(1)[6:]] = list(struct.unpack(f">{len(raw) // 4}I", raw))
    return vts, names


def classes(mod, dol=None):
    """[(base class or None, score, vtable (sec, off), [(kind, target, overridden name) ...])]

    A REL vtable has no typeinfo: two zero words, then the slots. kind is 'own' (this module's
    .text) or 'dol' (inherited). The base is the DOL vtable agreeing with the most inherited slots.
    """
    d, mid, secs, rel = load(mod)
    vts, names = dol or dol_vtables()
    text = next(i for i, (o, _) in enumerate(secs) if o & 1)
    out = []
    for sec in range(len(secs)):
        base, size = secs[sec][0] & ~1, secs[sec][1]
        if sec == text or not base:
            continue
        o = 0
        while o + 12 <= size:
            if (sec, o) in rel or (sec, o + 4) in rel or d[base + o:base + o + 8] != bytes(8) \
                    or (sec, o + 8) not in rel:
                o += 4
                continue
            slots, p = [], o + 8
            while (sec, p) in rel:
                m, ts, add = rel[(sec, p)]
                if m == mid and ts != text:
                    break
                slots.append(("own", add) if m == mid else ("dol", add))
                p += 4
            if len(slots) >= 2:
                best, score = None, 0
                for cls, v in vts.items():
                    sc = sum(1 for i, (k, a) in enumerate(slots) if k == "dol" and i < len(v) and v[i] == a)
                    if sc > score or (sc == score and sc and len(v) > len(vts[best])):
                        best, score = cls, sc
                bv = vts.get(best, [])
                out.append((best, score, (sec, o), [
                    (k, a, names.get(bv[i]) if k == "own" and i < len(bv) else names.get(a) if k == "dol" else None)
                    for i, (k, a) in enumerate(slots)]))
            o = max(p, o + 4)
    return mid, out


def unmatched(report, mod):
    """{.text offset: (name, size)} of the module's functions below 100%."""
    out = {}
    for u in report["units"]:
        if not u["name"].startswith(mod + "/"):
            continue
        base = None
        m = re.search(r"auto_(?:fn_\d+_|\d+_)([0-9A-Fa-f]+)_text$", u["name"])
        if m:
            base = int(m.group(1), 16)
        for f in u.get("functions", []):
            m = re.match(r"fn_\d+_([0-9A-F]+)$", f["name"])
            if m:
                off = int(m.group(1), 16)
            elif base is not None:
                off = base + int(f.get("metadata", {}).get("virtual_address", 0) or 0)
            else:
                continue
            if f.get("fuzzy_match_percent", 0) < 100:
                out[off] = (f["name"], int(f["size"]))
    return out


def main():
    report = json.loads((ROOT / "build/report.json").read_text())
    dol = dol_vtables()
    if sys.argv[1:] == ["--summary"]:
        rows, tot, hit, named, cls = [], 0, 0, 0, 0
        for p in sorted(RELS.glob("*.rel")):
            mod = p.stem
            _, cs = classes(mod, dol)
            un = unmatched(report, mod)
            own = {a: n for _, _, _, sl in cs for k, a, n in sl if k == "own"}
            h = [a for a in own if a in un]
            nm = sum(1 for a in h if own[a])
            rows.append((len(h), nm, len(un), len(cs), mod))
            tot += len(un); hit += len(h); named += nm; cls += len(cs)
        for h, nm, n, c, mod in sorted(rows, reverse=True):
            print(f"{mod:28} {c:3} vtables  {h:4} of {n:4} unmatched fn_ are slots, {nm:4} override a named virtual")
        print(f"total: {cls} vtables, {hit} of {tot} unmatched fn_ functions are vtable slots, "
              f"{named} of them override a named DOL virtual")
        return 0
    mod = sys.argv[1]
    mid, cs = classes(mod, dol)
    un = unmatched(report, mod)
    for base, score, (sec, off), slots in cs:
        own = [a for k, a, _ in slots if k == "own"]
        print(f"vtable sec {sec} +0x{off:X}: {len(slots)} slots, {len(own)} defined here "
              f"({sum(1 for a in own if a in un)} unmatched); base "
              + (f"{base} ({score} inherited slots agree)" if base else "unknown"))
        for i, (k, a, n) in enumerate(slots):
            if k == "dol":
                print(f"  [{i:3}] inherited  {n or hex(a)}")
            else:
                st = f"unmatched, 0x{un[a][1]:X} bytes" if a in un else "matched"
                print(f"  [{i:3}] fn_{mid}_{a:X}  {st}" + (f"  overrides {n}" if n else "  (new virtual)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
