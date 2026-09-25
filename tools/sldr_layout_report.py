#!/usr/bin/env python3
"""Turn tools/decode_sldr_layouts.py's output into a retail layout table and diff it
against the generated headers' member lists.

    tools/sldr_layout_report.py            # markdown, ready for docs/research/
    tools/sldr_layout_report.py --json

The retail side comes from `__ct__`/`__dt__`/`LoadTypedef` in the Tweaks module
(`tools/decode_sldr_layouts.py --json`).  The tree side is the member list parsed
out of `include/MetroidPrime/ScriptLoader/SLdr*.hpp` and
`include/MetroidPrime/Tweaks/*.hpp`.

Widths used for the tree side are the **retail** ones, not the host's: `bool` 1,
`float`/`int`/`CColor` 4, `rstl::string` 16, and every nested `SLdr*`/`CTweak*`
struct at the size retail gives it.  That is deliberate.  A `g++` probe on this
host measures `sizeof(rstl::string) == 24`, because `basic_string` holds two
pointers and a `uint`, and the probe is LP64 - so a host probe reports drift that
`mwcceppc` does not have.  Pinning the widths to retail's isolates the real defect,
which is a member that is missing, extra, or of the wrong type.

What is left over after that substitution is a genuine header defect and is
reported as `residual`.
"""
import argparse
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

RETAIL_PRIM = {
    "bool": (1, 1), "char": (1, 1), "signed char": (1, 1), "unsigned char": (1, 1),
    "short": (2, 2), "u8": (1, 1), "u16": (2, 2), "int16": (2, 2), "uint16": (2, 2),
    "int": (4, 4), "unsigned int": (4, 4), "uint": (4, 4), "u32": (4, 4), "float": (4, 4),
    "CColor": (4, 4),
    "rstl::string": (16, 4),
}


def die(m):
    print("error: %s" % m, file=sys.stderr)
    raise SystemExit(1)


def headers():
    out = {}
    for pat in ("include/MetroidPrime/ScriptLoader/*.hpp",
                "include/MetroidPrime/Tweaks/*.hpp",
                "include/MetroidPrime/Camera/*.hpp"):
        for h in sorted(glob.glob(pat)):
            txt = open(h).read()
            for m in re.finditer(r"struct (\w+) \{(.*?)\n\};", txt, re.S):
                name, body = m.group(1), m.group(2)
                if name in out:
                    continue
                mem = []
                for line in body.splitlines():
                    line = line.split("//")[0].strip()
                    if not line.endswith(";"):
                        continue
                    mm = re.match(r"^(.+?)\s*&?\s*\*?(\w+)\s*(\[[^\]]*\])?$",
                                  line[:-1].strip())
                    if not mm:
                        continue
                    mem.append({"type": mm.group(1).strip(), "name": mm.group(2),
                                "array": mm.group(3)})
                out[name] = {"header": h, "members": mem}
    return out


def retail_sides():
    raw = json.loads(subprocess.run(
        [sys.executable, "tools/decode_sldr_layouts.py", "--json"],
        capture_output=True, text=True).stdout)
    ctors, dtors, loads = {}, {}, {}
    for sym, d in raw.items():
        m = re.match(r"__ct__(\d+)(\w+)Fv$", sym)
        if m and d["kind"] == "ctor":
            ctors[m.group(2)] = d["members"]
            continue
        m = re.match(r"__dt__(\d+)(\w+)Fv$", sym)
        if m and d["kind"] == "dtor":
            dtors[m.group(2)] = d["members"]
            continue
        m = re.match(r"LoadTypedef(\w+?)__FR\d+(\w+)R12CInputStream$", sym)
        if m and d["kind"] == "load":
            loads[m.group(2)] = d["members"]
    return ctors, dtors, loads


def callee_type(sym):
    m = re.match(r"__ct__(\d+)(\w+)Fv$", sym)
    return m.group(2) if m else None


def dtor_type(sym):
    m = re.match(r"__dt__(\d+)(\w+)Fv$", sym)
    return m.group(2) if m else None


def member_offsets(ctors, dtors, name):
    """(offset, nested-type-or-None) for each member, in ascending offset order."""
    d = dtors.get(name, [])
    out = {}
    for m in d:
        t = dtor_type(m.get("dtor") or "")
        if m.get("offset") is not None and t:
            out[m["offset"]] = t
    for m in ctors.get(name, []):
        off = m.get("offset")
        if off is None or off in out:
            continue
        t = callee_type(m.get("callee") or "")
        out[off] = t
    return sorted(out.items())


def all_offsets(ctors, dtors, name, loads=None):
    """Every member offset of a struct, with the nested type where there is one.

    This is the *constructor and destructor* view, and unlike `member_offsets` it
    keeps the POD members too - which is what the gap closure needs, because the gap
    to the next member is the previous member's size only if nothing is missing
    between them.
    """
    out = {}
    for m in ctors.get(name, []):
        if m.get("offset") is not None and m.get("offset") not in out:
            out[m["offset"]] = callee_type(m.get("callee") or "")
    for m in dtors.get(name, []):
        off = m.get("offset")
        if off is None:
            continue
        t = dtor_type(m.get("dtor") or "")
        if off in out and out[off] is None:
            out[off] = t
        elif off not in out:
            out[off] = t
    # the LoadTypedef sees every member the stream can write, including the ones
    # neither the constructor nor the destructor mentions
    for m in (loads or {}).get(name, []):
        off = m.get("offset")
        if off is not None and off not in out:
            out[off] = None
    return sorted(out.items())


def close_sizes(ctors, dtors, sizes, loads=None):
    """Close the size graph with the offsets themselves.

    Inside a parent whose members are known, a nested member's size is the gap to
    the next member - and if it is the last member, the parent's own size minus its
    offset.  That is the only way to size a struct whose constructor ends in a
    nested type: `SLdrTBeamInfo` is 0x2C, and the only place that is written down is
    the gap between two `SLdrTBeamInfo` members of `SLdrTweakPlayerGun_Weapons`.
    """
    for _ in range(12):
        progress = False
        for name in ctors:
            mem = all_offsets(ctors, dtors, name, loads)
            if not mem:
                continue
            for i, (off, t) in enumerate(mem):
                if not t or t in sizes:
                    continue
                if i + 1 < len(mem):
                    nxt = mem[i + 1][0]
                elif name in sizes:
                    nxt = sizes[name]
                else:
                    continue
                if nxt > off:
                    sizes[t] = nxt - off
                    progress = True
        if not progress:
            break
    return bool(progress)


def one_struct(name, mem, d, sizes, loads=None):
    """retail size of one struct, or None if a nested type's size is not known yet."""
    strings = [m["offset"] for m in d
               if m.get("dtor") and "internal_dereference" in m["dtor"]]
    subs = {}
    for m in d:
        t = dtor_type(m.get("dtor") or "")
        if t and m.get("offset") is not None:
            subs[m["offset"]] = t
    end, missing = None, False
    for s in strings:
        end = max(end or 0, s + 16)
    for m in (loads or {}).get(name, []):
        # a `call` row is a nested member whose type the callee names; a
        # float/scalar row is a POD of known width
        if m.get("width"):
            end = max(end or 0, m["offset"] + m["width"])
        elif m.get("how") == "call":
            t = (m.get("callee") or "").split("__FR")[0].replace("LoadTypedef", "")
            if not t or t.startswith("fn_"):
                missing = True
            elif "internal_dereference" in t:
                end = max(end or 0, m["offset"] + 16)
            elif t not in sizes:
                missing = True
            else:
                end = max(end or 0, m["offset"] + sizes[t])
    for off, t in subs.items():
        if t not in sizes:
            missing = True
        else:
            end = max(end or 0, off + sizes[t])
    for m in mem:
        off = m.get("offset")
        if off is None or off in subs:
            continue
        if any(off == x + 4 or off == x + 8 for x in strings):
            continue
        if m.get("width"):
            end = max(end or 0, off + m["width"])
        elif m.get("callee"):
            t = callee_type(m["callee"])
            if t is None and any(o.get("offset") == off and o.get("width")
                                 for o in mem):
                # a static initialiser (`CColor::Green()`) plus its store: the
                # store is the member, the call is not a nested type
                continue
            if t is None:
                # a member loader retail does not name: its size is only knowable
                # from the gap to the next member, so this struct has to wait
                missing = True
            elif t not in sizes:
                missing = True
            else:
                end = max(end or 0, off + sizes[t])
    return None if (end is None or missing) else end


def resolve_sizes(ctors, dtors, loads=None):
    """retail sizeof per struct.

    A constructor store alone is not a member: a `rstl::string` contributes three
    words (buffer, size, capacity) and a `CColor` one.  So the members are taken
    from three places and the size is the maximum end:

      * `__dt__` calls `internal_dereference` at each `rstl::string` and
        `__dt__<T>Fv` at each nested `T` - both are members, named by the callee;
      * `__ct__` stores are members, except the second and third word of a string
        the dtor already accounted for;
      * a `__ct__<T>Fv` call is a nested `T` member.
    """
    sizes, pending = {}, dict(ctors)

    def sweep():
        progress = False
        for name, mem in list(pending.items()):
            v = one_struct(name, mem, dtors.get(name, []), sizes, loads)
            if v is None:
                continue
            sizes[name] = round_align(v, member_offsets(ctors, dtors, name),
                                      ctors.get(name, []))
            del pending[name]
            progress = True
        return progress

    for _ in range(8):
        if not sweep():
            break
    # the size graph may also be closable from the offsets alone, and that can
    # unblock a constructor round, so alternate until neither makes progress
    for _ in range(8):
        for name in list(sizes):
            sizes[name] = round_align(sizes[name], member_offsets(ctors, dtors, name),
                                      ctors.get(name, []))
        grew = close_sizes(ctors, dtors, sizes)
        for name in list(sizes):
            sizes[name] = round_align(sizes[name], member_offsets(ctors, dtors, name),
                                      ctors.get(name, []))
        if not (grew or sweep()):
            break
    return sizes, sorted(n for n in pending if n not in sizes)


def type_size(t, sizes):
    if t in sizes:
        return sizes[t]
    if t in RETAIL_PRIM:
        return RETAIL_PRIM[t][0]
    base = t.rstrip("*&")
    if base in sizes:
        return sizes[base]
    if base in RETAIL_PRIM:
        return RETAIL_PRIM[base][0]
    return None


def round_align(size, mem, ctor=()):
    """`sizeof` is rounded up to the struct's alignment, which is 4 for anything
    holding a 4-byte member and 1 for a struct of nothing but `bool`s."""
    wide = any(t for _o, t in mem) or any((m.get("width") or 0) > 1 for m in ctor)
    return align(size, 4 if wide else 1)


sizes_set = set()


def alias_map(ctors, dtors, hdrs):
    """header type name -> retail type name, matched by position inside a parent.

    Retail leaves a few members unnamed (`UnknownStruct1`, `fn_82_1DA94`) while the
    generator gave them a name, so a member cannot be matched by type.  Both lists
    are in declaration order and retail's offset is known, so zipping them by index
    attributes the header's name to retail's type.
    """
    out = {}
    for name, h in hdrs.items():
        retail = member_offsets(ctors, dtors, name)
        # strings are identifiable on both sides, so drop them and zip the rest:
        # a string is the one member the destructor reaches through
        # `internal_dereference` rather than a `__dt__<T>Fv`
        rtypes = [t for _o, t in retail if t]
        htypes = [m["type"] for m in h["members"] if m["type"] != "rstl::string"]
        if not rtypes or len(rtypes) != len(htypes):
            continue
        for ht, rt in zip(htypes, rtypes):
            if ht not in sizes_set and rt not in out.values() and ht != rt:
                out[ht] = rt
    return out


def align_members(row, tree):
    """Positional alignment of the header's member list with retail's.

    The header's *types* for a few members do not match retail's names - retail
    leaves them `fn_`/`UnknownStruct*` - so a member cannot be matched by type.  It
    can be matched by position: both lists are in declaration order, and the retail
    `LoadTypedef` gives an offset for each, so sorting both by offset and zipping
    them attributes a header member name to a retail offset.  Where the two lists
    differ in length the extra retail members are listed with no name, which is the
    finding: the header is missing a member.
    """
    retail = [m for m in row["retail_members"]]
    # collapse duplicate offsets (a CColor ctor stores one word, a string three)
    seen, r2 = set(), []
    for m in retail:
        key = m["offset"]
        if key in seen:
            continue
        seen.add(key)
        r2.append(m)
    hdr = tree.get(row["struct"], {}).get("members", [])
    out = []
    for i in range(max(len(r2), len(hdr))):
        rm = r2[i] if i < len(r2) else None
        hm = hdr[i] if i < len(hdr) else None
        out.append({
            "name": hm["name"] if hm else None,
            "type": hm["type"] if hm else None,
            "retail_offset": rm["offset"] if rm else None,
            "tree_offset": hm["offset"] if hm else None,
            "retail_width": rm["width"] if rm else None,
        })
    return out
    if t in sizes:
        return sizes[t]
    if t in RETAIL_PRIM:
        return RETAIL_PRIM[t][0]
    base = t.rstrip("*&")
    if base in sizes:
        return sizes[base]
    if base in RETAIL_PRIM:
        return RETAIL_PRIM[base][0]
    return None


def align(v, a):
    return (v + a - 1) // a * a


def tree_layout(hdrs, sizes):
    """offsetof per member using retail widths; None if a type is unknown."""
    out = {}
    for name, h in hdrs.items():
        if name not in sizes:
            continue
        off, rows, bad = 0, [], False
        for m in h["members"]:
            t = m["type"]
            n = 1
            if m["array"]:
                mm = re.match(r"\[(\d+)\]", m["array"])
                n = int(mm.group(1)) if mm else 0
                if n == 0:
                    bad = True
            sz = type_size(t, sizes)
            al = type_size(t, sizes)
            if sz is None:
                bad = True
                break
            al = RETAIL_PRIM.get(t, (sz, 4))[1]
            off = align(off, al)
            rows.append({"name": m["name"], "type": t, "offset": off,
                         "width": sz * n if n else sz, "count": n})
            off += sz * n if n else sz
        if not bad:
            out[name] = {"members": rows,
                         "size": align(off, 4 if any(r["width"] > 1 for r in rows) else 1)}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--only", default=None)
    args = ap.parse_args()

    ctors, dtors, loads = retail_sides()
    sizes, unresolved = resolve_sizes(ctors, dtors, loads)
    hdrs = headers()
    sizes_set.clear()
    sizes_set.update(sizes)
    al = alias_map(ctors, dtors, hdrs)
    hdrs = {n: {"header": h["header"],
                "members": [dict(m, type=al.get(m["type"], m["type"]))
                            for m in h["members"]]}
            for n, h in hdrs.items()}
    tree = tree_layout(hdrs, sizes)
    if args.only:
        keep = set(re.findall(args.only, ",")) or {args.only}
    else:
        keep = None

    rows = []
    for name in sorted(sizes):
        if keep and name not in keep:
            continue
        rs = sizes[name]
        ts = tree.get(name, {}).get("size")
        rows.append({
            "struct": name,
            "retail_size": rs,
            "tree_size": ts,
            "residual": (ts - rs) if ts is not None else None,
            "retail_members": sorted(
                ({"offset": m["offset"], "width": m.get("width"),
                  "type": m.get("callee") or m.get("how")}
                 for m in ctors.get(name, []) if m.get("offset") is not None),
                key=lambda m: m["offset"]),
            "tree_members": tree.get(name, {}).get("members", []),
            "has_load": name in loads,
            "header": hdrs.get(name, {}).get("header"),
        })
        rows[-1]["aligned"] = align_members(rows[-1], tree)
    if args.json:
        print(json.dumps({"rows": rows, "unresolved": unresolved,
                          "retail_sizes": sizes}, indent=1))
        return
    print("| struct | retail size | header-list size (retail widths) | residual |")
    print("| --- | --- | --- | --- |")
    for r in rows:
        ts = "0x%X" % r["tree_size"] if r["tree_size"] is not None else "?"
        res = ("%+d" % r["residual"]) if r["residual"] is not None else "?"
        print("| `%s` | 0x%X | %s | %s |" % (r["struct"], r["retail_size"], ts, res))
    print()
    print("unresolved (a member's type had no retail size): %s" % ", ".join(unresolved))


if __name__ == "__main__":
    main()
