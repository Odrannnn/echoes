# carve-8025fe8c — `Weapons/Carve8025FE8C`

`kind: match`, target `Weapons/Carve8025FE8C`. **PASS** — `tools/goal_check.sh build/goal/item.json`
exits 0, `flip_test.sh` kept the unit.

## What I did

Carved the one function the item named, `fn_8025FE8C` (0x8025FE8C..0x8025FEAC, 0x20 = 32 bytes,
8 instructions), out of dtk's unclaimed `auto_03_8025F468_text` as a new `Matching` unit
`src/Weapons/Carve8025FE8C.c`, with the four files a carve requires:

- `src/Weapons/Carve8025FE8C.c` — new, plain C, header in the style of
  `src/MetroidPrime/Carve80004438.c` / `src/Dolphin/Carve8038A7DC.c`.
- `configure.py:647` — `Object(Matching, "Weapons/Carve8025FE8C.c"),`, one line, between
  `Weapons/CCollisionResponseData.cpp` and `Weapons/Carve8026023C.cpp`.
- `config/G2ME01/splits.txt:2260-2261` — `.text start:0x8025FE8C end:0x8025FEAC`, in address
  order.
- `files.cmake:1119-1122` — `src/Weapons/Carve8025FE8C.c`, with the reason it is a `.c`.

## Measured

- `build/report.json`: `main/Weapons/Carve8025FE8C` **1 / 1**, `complete: True`,
  `fuzzy_match_percent 100.0`. `matched` **13628 -> 13629**, `linked` **6676 -> 6677**,
  `total_functions` **28465 before and after** the `splits.txt` edit.
- `tools/flip_test.sh Weapons/Carve8025FE8C.c`: `PASS -> kept as Matching` (the acceptance
  test). DOL sha1 and all 86 RELs held.
- `tools/carve_diff.sh 8025FE8C 20 build/G2ME01/src/Weapons/Carve8025FE8C.o`: 8 instructions /
  32 bytes on both sides, 1 differing — the `bl` displacement, which is a relocation in an
  unlinked object (`4b ff ff 69` resolved against 0x8025FE8C). Expected, and the flip is what
  proves it.
- `tools/goal_check.sh build/goal/item.json`: **PASS**, every line ok — gate.sh (DOL sha1, 86
  RELs, per-function diff, wiring, docs claims, port probe), counts, `check_symbol_names.py`
  (0 missing), `All: 37.70% fuzzy, 31.14% matched, 14.01% linked (13629 / 28465 functions)`.
- Port link unchanged: `1029 files, 0 failed`, `287 undefined, 0 duplicates`,
  `279 MISSING symbol(s), all accounted for in port_link_gap_list.md`.
- `python3 tools/check_files_cmake.py`: every configured DOL object is in `files.cmake` or
  excluded with a reason. `python3 tools/check_symbol_names.py`: 611 units, 0 missing.
- `python3 tools/check_decl_order.py --unit Weapons/Carve8025FE8C.c`: nothing to compare (a
  one-function file cannot be permuted). `python3 tools/check_raw_offsets.py` and
  `check_docs_claims.py` pass inside the gate.

**The split did to the auto unit, measured after the build** (re-measure, do not recall):
`auto_03_8025F468_text` was `# 0x8025F468..0x8026023C | size: 0xDD4` with 15 functions; it is now
`# 0x8025F468..0x8025FE8C | size: 0xA24` with **10**, plus a new `auto_03_8025FEAC_text` with
**4**, plus this unit's own **1**. 15 in, 15 out. A claim in the middle of an auto unit splits it
in two, the same mechanism `docs/goal-notes/carve-8022a0c0.md` records.

## The body, and why it is what it is

`fn_8025FE8C` is `stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) / bl fn_8025FE00 / lwz
r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr`. A byte-shape twin of `fn_80004438`
(`src/MetroidPrime/Carve80004438.c:91-97`, `Matching`) and of `fn_80004C4C` (0x80004C4C, `Matching`)
— the same eight instructions with the `bl` target changed, confirmed on `main.elf` for all three.

Two things are **not** copied from the twin, and both are load-bearing:

1. **The twin is `rstl::destroy`; this copy is not.** Its callee `fn_8025FE00`
   (0x8025FE00, `symbols.txt:10670`, `size:0x8C`) range-checks a class id
   (`subis r0,r3,0x4450` / `cmplwi r0,0x534d`, i.e. `[0x4450, 0x978D)`), returns `li r3,0x0` on a
   miss, and on a hit runs `__nw__FUlPCcPCc(0x60, lbl_803ADD68, 0)`, the 0x6C-byte
   `fn_80262CD0`, then `fn_8025F4F4(new, stream, arg)` (0x8025F4F4, `size:0x4A4`), returning the
   new pointer. A class-id-checked object factory.
2. **The return type is a pointer, not `void`.** `grep -rn 'bl fn_8025FE8C' build/G2ME01/asm/`
   finds exactly one call site, 0x8026020C, and it does `mr r0,r3` at 0x80260210 to pass the
   result on. `void` would compile to the same eight bytes, but it is a signature retail's own
   call site contradicts.

**A measured detail worth keeping, because it decides the argument types.** `fn_8025FE00` calls
`GetClassID__20CParticleDataFactoryFR12CInputStream` with **r3 and r4 untouched**, so its r3 is
the stream, not a receiver. That is settled off a `Matching` unit rather than assumed:
`GetModel__20CParticleDataFactoryFR12CInputStreamP11CSimplePool` (0x802E14E4,
`src/Kyoto/Particles/CParticleDataFactory.cpp`, 38/38) does `mr r3,r30` before the very same
call — the signature difference of a `static` — and `fn_8025FE00` has no such move.

## Notes for the next lane

- `fn_8025FE00` (0x8C) and `fn_8025FEAC` (0x90, sets `__vt__31CObjOwnerDerivedFromIObjUntyped`
  then `__vt__4IObj`) are the unclaimed neighbours and are **not** claimed by this item. Nothing
  here claims they are decompiled.
- No `PortLinkStubs.cpp` entry had to go: `fn_8025FE8C`/`fn_8025FE00` appear nowhere in
  `src/` or `include/` (grepped), so there was no duplicate. The port-side `fn_8025FE00` is an
  announced empty stand-in in the carve's own `#ifndef __MWERKS__` block, the arrangement
  `src/Weapons/Carve8026023C.cpp:215-239` already uses for the same run's callees.
- `NEW:` lines: none. Nothing new is blocked — the item's function reached 100% and the unit
  flipped.
