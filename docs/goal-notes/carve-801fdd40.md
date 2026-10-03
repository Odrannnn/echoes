# carve-801fdd40 — `MetroidPrime/ScriptObjects/Carve801FDD40.cpp` (kind: match, FLIPPED)

`fn_801FDD40` (retail `.text:0x801FDD40`, `symbols.txt:8294`, 0x2C = 44 bytes, 11
instructions) carved out of dtk's `auto_03_801FDC88_text` as its own `Matching` unit. The
flip passed; `build/report.json` shows `main/MetroidPrime/ScriptObjects/Carve801FDD40`
`complete: true`, `matched_functions 1 / 1`, `fuzzy_match_percent 100.0`.

## Measured result

| | before | after |
| --- | --- | --- |
| `matched_functions` | 13640 | **13641** |
| `linked` (Matching + in link) | 6688 | **6689** |
| DOL units `main/*` | 11702 | **11703** |
| `total_functions` | 28465 | **28465** (unchanged) |
| port link undefined / duplicates | 287 / 0 | **287 / 0** (unchanged) |

`./tools/goal_check.sh build/goal/item.json`:

```
goal_check: item carve-801fdd40 (match) target=MetroidPrime/ScriptObjects/Carve801FDD40
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13640 -> 13641   linked 6688 -> 6689
  ok    check_symbol_names.py
  ok    All:  37.71% fuzzy, 31.14% matched, 14.01% linked (13641 / 28465 functions)
  ok    flip_test MetroidPrime/ScriptObjects/Carve801FDD40.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-801fdd40
```

`sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
`./tools/flip_test.sh MetroidPrime/ScriptObjects/Carve801FDD40.cpp` -> `PASS -> kept as
Matching`, `kept: 1 / 1 failed: 0 skipped: 0`. `probe: 1041 files, 0 failed, 0 errors; link:
LINKED (287 undefined, 0 duplicates)`. `tools/unit_fit.sh` -> `44 claimed / 44 ours / 44
retail, fits`, `no extra functions`. `python3 tools/check_symbol_names.py` -> `0 declared names
are missing`. `python3 tools/check_decl_order.py` and `check_files_cmake.py` clean.
`build/gate-diff.log` records the carve as a clean split: `SPLIT
main/auto_03_801FDCAC_text: 17 function(s) moved into
main/MetroidPrime/ScriptObjects/Carve801FDD40, main/auto_03_801FDD6C_text (exact count match -
a split, not a loss)`.

## The four carve files

1. `src/MetroidPrime/ScriptObjects/Carve801FDD40.cpp` (new, 1 function)
2. `config/G2ME01/splits.txt`: `MetroidPrime/ScriptObjects/Carve801FDD40.cpp: .text start:0x801FDD40
   end:0x801FDD6C`, between `Carve801FDC88.c` (ends 0x801FDCAC) and `Carve801FEA98.c`, i.e. in
   address order. No gap spanned: `fn_801FDCAC..fn_801FDD40` below and `fn_801FDD6C..fn_801FEA98`
   above both stay retail's.
3. `configure.py:882` `Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDD40.cpp")`, one
   line, right after `Carve801FDC88.c`.
4. `files.cmake:873` `src/MetroidPrime/ScriptObjects/Carve801FDD40.cpp`, same place.

No `PortLinkStubs.cpp` duplicate: `stub_carve801fdc88_0` stubs `fn_801FDCAC`, which this claim
does not touch.

## What the function is

`TToken<T>::GetIObjObjectFor(const rstl::auto_ptr<T>&)` - `include/Kyoto/TToken.hpp:24-27`
verbatim, `return TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(obj);`, with the callee here
being the unclaimed `fn_801FDD6C` (`symbols.txt:8295`, 0x9C). Retail names this copy nothing
(`fn_801FDD40` placeholder), so the symbol is reproduced verbatim under `extern "C"`.

**Twin, measured on the disc, not recalled.** With
`python3 tools/dol_read.py 0x801FDD40 0x2c orig/G2ME01/sys/main.dol` and the same at
0x80031A00, this range and
`GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSkinnedModel>`
(0x80031A00, 0x2C, `symbols.txt:931`) are the same 11 words **including the `bl` word
`48 00 00 19`** - both because each callee sits 0x2C past its `bl`
(0x801FDD54+0x18=0x801FDD6C, 0x80031A14+0x18=0x80031A2C). The twin's listing
(`build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2564-2578`) is **our** compile, so
it only confirms; the disc is the evidence. The seed's file path for the twin is right about
where its disassembly lives and wrong about its being matched - 0x80031A00 is still inside dtk's
unclaimed `0x8002F7A8..0x80031E60` run.

The already-matched twins in this tree are `fn_801EF784`
(0x801EF784, `src/MetroidPrime/Carve801EF730.cpp:175-178`) and `fn_8028EC50`
(0x8028EC50, `src/Kyoto/Animation/CAnimCharacterSet.cpp:136-138`); the body below is that
spelling with this copy's callee.

**The sret convention, measured off retail's only caller.** `fn_801FD37C` (0x801FD37C, 0xA4) is
the sole caller (`grep -rn 'bl fn_801FDD40' build/G2ME01/asm/` -> one hit,
`auto_03_801FCE40_text.s:402`). It sets `addi r3,r1,0x8 / addi r4,r1,0x10` before its `bl` at
0x801FD3AC, writes `stw r4,0x14(r1)` and `stb r0,0x10(r1)` *before* the call, and reads
`lwz r3,0xc(r1)` after. So r3 is a hidden **8-byte** result slot at `r1+8`, the argument is the
8-byte `rstl::auto_ptr` at `r1+0x10` (its +0/+4 are `mHas`/`mItem`, written before the call), the
read after is the result's **+4** word, and the slots do not overlap (LR is at `r1+0x24`).

## Two things the seed got wrong, and what they cost

1. **"plain C so the `fn_` names do not mangle" -> the unit must be a `.cpp`.** `extern "C"`
   keeps the names verbatim just as well. The 8-byte class return is what produces the `mr
   r31,r3` + `stw/lwz r31` pair, and a plain-C spelling cannot express it: a `.c` function
   returning an 8-byte POD is **8 instructions** and never touches r31, because MWCC hands an
   8-byte POD back in `r3:r4`. Retail returns 8 bytes (`{bool mHas; T* mItem;}`, measured above),
   so the honest spelling is the class `rstl::auto_ptr`, and only C++ has the class. A 12-byte
   plain-C struct does emit the 11 instructions, but it describes an ABI retail does not have
   and makes the two caller slots overlap - rejected.
2. **"claim exactly this range and nothing else" also constrains the port link.** `fn_801FDD6C`
   is unclaimed, so referencing it from `files.cmake` makes it a *new* undefined symbol and
   `tools/link_gap.py` fails the gate on a symbol not in
   `docs/research/port_link_gap_list.md`. Fixed the same way as `src/MetroidPrime/Carve801EF730.cpp:197-215`
   and `src/MetroidPrime/Carve801FDC88.c`: an announced, empty-bodied
   `#ifndef __MWERKS__` stand-in in the carve file itself. Guard is `__MWERKS__`, not `TARGET_PC`
   - the matching build must take the symbol from dtk's own object, or the host build would
   define it twice. Measured: port link stayed at **287 undefined, 0 duplicates** and
   `probe_sources.sh` still reports `0 failed`.

Reusable, non-obvious: **for a 0x2C-byte `mr r31,r3 / bl / lwz r31` frame, the return type alone
decides whether it matches** - an 8-byte class return emits 11 instructions, an 8-byte POD return
emits 8. Same finding as `Carve801EF730.cpp:99-110`, re-measured here rather than inherited.

`fn_801FDD6C`'s own 0x9C bytes are **not** claimed here. It is 39 instructions and the same
shape as `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>F...` at 0x80031A2C
(0x9C) - measured, the two differ in 10 of 39 instructions and all 10 are scheduling, `bl`
displacement or data address - so it is a `GetNewDerivedObject` and is a job of its own.
