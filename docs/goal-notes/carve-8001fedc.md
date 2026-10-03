# carve-8001fedc — `MetroidPrime/Carve8001FEDC` is `Matching`, 2 functions, 100.00%

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS carve-8001fedc`**, every check `ok`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13497 -> 13499   linked 6545 -> 6547
  ok    check_symbol_names.py
  ok    All:  37.57% fuzzy, 31.01% matched, 13.87% linked (13499 / 28465 functions)
  ok    flip_test MetroidPrime/Carve8001FEDC.c: PASS, Object(Matching) in configure.py
```

Both functions in the item's range matched. Nothing is left in the range unclaimed.

## What I did

Carved `src/MetroidPrime/Carve8001FEDC.c` — `.text 0x8001FEDC..0x8001FF7C`, 0xA0 = 160 bytes,
2 functions — out of dtk's `main/auto_03_8001E070_text` (0x8001E070..0x8001FF7C), and listed it
`Object(Matching, ...)` in `configure.py`. Four files, each placed in address order:

| file | change |
| --- | --- |
| `config/G2ME01/splits.txt` | new entry `MetroidPrime/Carve8001FEDC.c: .text start:0x8001FEDC end:0x8001FF7C`, between `MetroidPrime/CMainFlow.cpp` (ends 0x8001E070) and `MetroidPrime/CCredits.cpp` (starts 0x8001FF7C) |
| `configure.py` | one line, `Object(Matching, "MetroidPrime/Carve8001FEDC.c")`, at line 405 between `CMainFlow.cpp` and `CCredits.cpp` |
| `files.cmake` | `src/MetroidPrime/Carve8001FEDC.c`, beside `Carve8001935C.c` and `Carve8001FF7C.c` |
| `src/MetroidPrime/Carve8001FEDC.c` | new, the two bodies |

Plus `src/MetroidPrime/PortLinkStubs.cpp`: **two** announced empty stand-ins (`stub_8001fedc_0`
for `fn_800E142C`, `stub_8001fedc_1` for `__dt__13CStateManagerFv`) and the header's three
derived counts re-measured — see "the port link" below, that step is what took the undefined count
from 288 back to its baseline 287.

## The twin, and why the two bodies are that short

The seed named
`rstl::rc_ptr<rstl::vector<int, rstl::rmemory_allocator> >::ReleaseData()`
(`ReleaseData__Q24rstl53rc_ptr<Q24rstl36vector<i,Q24rstl17rmemory_allocator>>Fv`, 0x800097C0,
0x50, `build/report.json` `main/MetroidPrime/main`, **100.00%**) as the byte-shape twin of both.
Read against `include/rstl/rc_ptr.hpp:101,117-123` it is the same twenty instructions, only the
two `bl` targets differ:

```
stwu/mflr/stw r0,0x14/stw r31,0xc      0x50-byte frame
mr r31,r3
lwz r4,0x4(r3) / lwz r3,0x0(r4) / subic. r0,r3,1 / stw r0,0x0(r4) / bgt out
  lwz r3,0x0(r31) ; li r4,1 ; bl <dtor>        <- retail's `delete GetPtr()`
  lwz r3,0x4(r31) ; bl Free__7CMemoryFPCv     <- retail's `delete mRefCount`
epilogue
```

`+0x00` is the pointee and `+0x04` an `int*` refcount — retail's `rc_ptr<T>` layout, and
`include/rstl/rc_ptr.hpp:44-52` says why (the refcount is a separate four-byte `CMemory`
allocation, not a `CRefData` control block). Only those two words are modelled.

Callees, which are this copy's own and are the only difference from the twin:

- `fn_8001FEDC` → `__dt__13CStateManagerFv` (0x8004269C, `symbols.txt:1226`, 0x58C), defined by
  our own `build/G2ME01/src/MetroidPrime/CStateManager.o` as `T`, so no stand-in on the DOL.
- `fn_8001FF2C` → `fn_800E142C` (0x800E142C, `symbols.txt:3913`, 0x11C = 284 bytes), **unclaimed**,
  still in dtk's `auto_03_800E122C_text.o`. Declared, never defined here — the same trade
  `src/Collision/Carve8028B728.c` makes for `fn_8028B780`.
- both → `Free__7CMemoryFPCv` (0x802CE388), retail's own name; MWCC's old mangling is
  `[A-Za-z0-9_]` only, so a C declaration names it verbatim (the trick
  `src/MetroidPrime/Carve800E1548.c:77` uses).

## The one spelling that cost the whole run, and it is worth the next lane's time

**`li r4, 1` before the destructor `bl` is a parameter, not decoration.** Declaring
`__dt__13CStateManagerFv(void*)` with one parameter compiles to 0x4C bytes, not 0x50: mwcceppc
emits no `li r4,1` and **omits the `bgt` displacement change too**, so the first function ends at
0x8001FF28 and the second starts there instead of 0x8001FF2C. With two parameters
(`void* self, int deleting`, called as `(__dt__13CStateManagerFv(self->x0_ptr, 1)`) it is 0x50 and
byte-exact.

How that presented, and it is worth stating because the diagnostics are all misleading:

- **objdiff said 100.00% and `unit_fit.sh` said `fits`.** Both compare per function by name, so a
  whole-unit size error of 4 bytes is invisible to them; `unit_fit.sh` reported
  `.text claimed 160 ours 160 retail 160 fits` **after** the fix and would have said the same
  thing before it (0x4C + 0x50 = 0x9C ≠ 0xA0, so it disagrees, but it is not the tool that
  catches a 4-byte skew between two functions in one unit).
- **`build/G2ME01/main.dol`'s sha1 stayed `6ef9b491…` and still does.** `ninja`'s `ok` target
  (`dtk shasum -c config/G2ME01/build.sha1`) reported **zero FAILED** with the broken unit in the
  tree, because that target only re-checks files ninja considers up to date and
  `build/G2ME01/main.dol` had not been relinked. The tell was elsewhere: **all 86 RELs went
  FAILED**, and `AIMannedTurret.rel` differed from `orig/G2ME01/files/RelProd/AIMannedTurret.rel`
  in 421 bytes (byte 25604 onward) — `dtk rel make` re-resolves every module's imports against
  `main.elf`'s symbol table, so one address 4 bytes off moves them. **`nm build/G2ME01/main.elf`
  then showed `8001ff28 T fn_8001FF2C` where retail has `8001ff2c`**, which is the whole
  diagnosis in one line.
- `check_decl_order.py` was clean throughout (`37 permuted, all 37 accounted for`), correctly: the
  unit was in the right order, just the wrong size.

So: **a DOL sha1 that does not move is not evidence a carve is right** (the carve vein's own
"100.00% and must stay NonMatching" note, one level down), and the REL sha1s are what actually
caught it. A carve should be checked with `nm … | grep <fn_>` on `main.elf`, or by diffing the two
REL runs — the cheap version is `tools/carve_diff.sh <start> <size> <ours.o>`, which reports
`retail: N instructions / ours: M` and is loud about a size mismatch.

## The port link: 287 -> 288 -> 287, and why it needed two stubs

`tools/link_check.sh`'s STRICT gate is *growth*, not the total, so the carve had to add nothing.
It first added **one** symbol and failed:

```
link_check: STRICT FAIL - regression gate: 288 undefined against a baseline of 287 (GREW), 0 duplicate(s)
  NEW  __dt__13CStateManagerFv
```

`fn_800E142C` was already answered by the new `stub_8001fedc_0`. `__dt__13CStateManagerFv` was
not, and the reason is worth writing down: **the port cannot spell a MWCC-mangled destructor name
at all.** The DOL build gets `__dt__13CStateManagerFv` from `src/MetroidPrime/CStateManager.cpp`
(`nm` on its object: `00001b04 T __dt__13CStateManagerFv`), but a host compiler mangles the same
`~CStateManager()` as `_ZN13CStateManagerD1Ev`, so the retail spelling is undefined on a host link
no matter which unit asks for it. `stub_8001fedc_1` names it and does nothing — which costs
nothing, because `src/MetroidPrime/CStateManager.cpp:954` already writes an **empty**
`CStateManager::~CStateManager() {}`. Same situation as `stub_80004438_0`'s `fn_8000447C` and
`reachstub_616`'s `__dt__20SLdrEditorPropertiesFv` (`src/MetroidPrime/PortReachStubs.cpp:1900`),
which is the existing precedent for a `__dt__` stand-in.

After it: `link_check: STRICT PASS - regression gate: 287 undefined against a baseline of 287
(no growth), 0 duplicate(s), 0 compile error(s), linker_ran=1`.

## Measured, not recalled

- `main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs `cmp`-equal to
  `orig/G2ME01/files/RelProd/` (checked individually, the flat filenames).
- `build/report.json`: `total_functions` **28465** (unchanged — the claim moved, it did not add),
  `matched_functions` **13497 -> 13499**, `linked` **6545 -> 6547**, `total_units` 2335.
- `main/MetroidPrime/Carve8001FEDC`: `.text` 160 B, **2 / 2 functions at 100.00%**,
  `fn_8001FEDC` at 0x8001FEDC (80 B) and `fn_8001FF2C` at 0x8001FF2C (80 B) — the two addresses
  are the second measurement of the `li r4,1` fix.
- `tools/check_symbol_names.py`: `checked 587 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --all`: `1200 unit(s) checked, 37 permuted, all 37 accounted
  for` — the 37 are pre-existing and listed in `decl_order.md`; the new unit is not among them.
  (`--unit MetroidPrime/Carve8001FEDC.c` reports `0 unit(s) checked`; the tool ignores `--unit` on
  this tree, so the whole-tree run is what covers a new file.)
- `tools/unit_fit.sh MetroidPrime/Carve8001FEDC.c`: `.text claimed 160 ours 160 retail 160 fits`,
  `no extra functions: our object defines only what the retail unit object does`.
- `tools/carve_diff.sh 0x8001FEDC 0xA0 build/G2ME01/src/MetroidPrime/Carve8001FEDC.o`: the only
  4 differing instructions are the **two `bl` targets in each function**, which the tool cannot
  resolve in an unlinked `.o` (`bl 800e142c` vs `bl 80 <fn_8001FF2C+0x30>`). The linked result is
  the flip, not this. Instruction and byte counts match: 20/20 and 80/80 per function.
- `./tools/probe_sources.sh`: `probe: 929 files, 0 failed, 0 errors`.
- `src/MetroidPrime/PortLinkStubs.cpp` header counts re-derived after the edit, not carried:
  `grep -cE 'asm\("'` **202 -> 204**, `^extern "C" void stub_[A-Za-z0-9_]*\(\) asm\(` **192 -> 194**,
  `^extern "C" char stub_data_` unmoved at **10**; total supplied **201 -> 202**.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` appear in `git status` because `gate.sh` runs
with `MP_GATE_DOCS_WRITE=1` and rewrote their derived counts. Those are the gate's output; the
driver discards them, and I did not edit them.

## Files touched

- `src/MetroidPrime/Carve8001FEDC.c` (new, 122 lines)
- `config/G2ME01/splits.txt` (+3, after line 111)
- `configure.py` (+1, line 405)
- `files.cmake` (+1, line 633)
- `src/MetroidPrime/PortLinkStubs.cpp` (+2 stubs at ~line 1595, header block lines 7-22)

No `asm` body was added: the two `asm("…")` lines are the alias labels `PortLinkStubs.cpp` already
uses ~194 times, and both bodies are C. No commit, per the brief.

## Notes worth keeping

- **Both twins in one carve are the cheap case** and this item confirms it end to end: read the
  twin's source, write the same logic in C under the `fn_` name, and the whole range flips. The
  only thing that was not free was the parameter count on the destructor call — a detail invisible
  to every per-function tool in the tree.
- **The carve vein's "100.00% and must stay NonMatching" case has a sibling**: a carve can be
  100.00% per function, `Matching`, `flip_test` PASS and *still* be four bytes wrong if one
  function's size is off, because `flip_test` checks the sha1 and the sha1 target had not been
  relinked in my incremental builds. Rebuild the RELs (`ninja build/G2ME01/ok`) after any carve
  before believing it.

## For the next run

Nothing is blocked here. In this same hole (`auto_03_8001E070_text`, 0x8001E070..0x8001FEDC and
above), all still unclaimed and all with their shapes measured from
`build/G2ME01/asm/auto_03_8001E070_text.s`:

- `fn_800E142C` (0x800E142C, 0x11C) — this carve's own callee, retail's unclaimed 284-byte
  destructor: null-receiver early return, four `CGuiFrameLoader` members at
  +0x4C/+0x50/+0x54/+0x58 each through `__dt__15CGuiFrameLoaderFv` behind a null test, a `CToken`
  at +0x48 through `__dt__6CTokenFv` then `Free__7CMemoryFPCv`, and `fn_800E1548` / `fn_800E163C`
  on +0x5C / +0x80. Carving it would retire `stub_8001fedc_0`.
- `fn_800E1548` (0x800E1548, 0x50) — **already written** in
  `src/MetroidPrime/Carve800E1548.c`, but that file is not in `configure.py` or `splits.txt`, so
  the range is still dtk's. A one-line claim plus an `Object` is likely a flip.
- `fn_8001FDA4` (0x8001FDA4, 0x138 = 312 bytes) — the function immediately below this claim and
  the boundary that stopped the carve; it and `fn_8001FD50`/`fn_8001FD68` (its `ReleaseData` call
  sites) are the next run's twin to read.
- `fn_8001FD24`/`fn_8001FD8C` (the other `rc_ptr<T>::ReleaseData` copies that
  `grep -rn 'bl fn_8001FF2C' build/G2ME01/asm/` finds in `auto_03_801F36E0_text.s:1020-1208`, six
  callers) — the same shape as this item's pair, and a `port`-side caller of both.

NEW: none. Nothing found here is a blocker, and the `li r4,1` lesson is a codegen rule rather than
work whose success raises a count.