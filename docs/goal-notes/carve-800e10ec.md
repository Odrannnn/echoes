# carve-800e10ec — `MetroidPrime/Carve800E10EC`, four functions, `Matching`

`kind: match`. Carved 0x800E10EC..0x800E122C (0x140 = 320 bytes, 4 functions) out of
`main/auto_03_800DFA60_text` as one `Matching` unit, all four functions. **The judge passed.**

`./tools/goal_check.sh build/goal/item.json` printed, verbatim:

```
goal_check: item carve-800e10ec (match) target=MetroidPrime/Carve800E10EC
goal_check: baseline .../wt-mp2-goal-L12/build/goal/judge/report.base.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12523 -> 12527   linked 5896 -> 5900
  ok    check_symbol_names.py
  ok    All:  35.33% fuzzy, 29.17% matched, 12.92% linked (12527 / 28465 functions)
  ok    flip_test MetroidPrime/Carve800E10EC.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-800e10ec
```

## The files

| file | line | entry |
| --- | --- | --- |
| `src/MetroidPrime/Carve800E10EC.cpp` | new (126 lines) | 4 functions, `extern "C"`, descending by address: `fn_800E1188` (l. 98), `fn_800E1130` (l. 110), `fn_800E110C` (l. 120), `fn_800E10EC` (l. 124); the single raw-offset site is `+0x10` at l. 112 |
| `configure.py` | 665 | `Object(Matching, "MetroidPrime/Carve800E10EC.cpp"),` — between `CConsoleOutputWindowCtor.cpp` (0x800D63F0) and `CAudioStateWinCtor.cpp` (0x800E25C4), i.e. in ascending address order |
| `config/G2ME01/splits.txt` | 432-433 | `MetroidPrime/Carve800E10EC.cpp:` / `.text start:0x800E10EC end:0x800E122C`, on one line pair, no gap inside the claim. Inserted between `CSimpleShadow.cpp` (ends 0x800DFA60) and `CWorldShadow.cpp` (starts 0x800E17E4). `total_functions` is still **28465** (`./tools/decomp_build.sh`'s `All:` line and `build/report.json`) |
| `files.cmake` | 443 | `src/MetroidPrime/Carve800E10EC.cpp`, between `Carve800DAE94.c` and `Carve800E39D0.c` |
| `docs/research/raw_offsets.md` | 901-end | new `## \`src/MetroidPrime/Carve800E10EC.cpp\` (1 site)` section, and the measured total moved 168/72 -> **170 sites in 74 files** (see below) |

No `PortLinkStubs.cpp` entry was needed and none was added: none of the four `fn_` names appears
there, and the port now *defines* all four from the same body (see "Port", below).

## What the four functions are, and the evidence

One weak-COMDAT teardown group for a class with a `rstl::vector<rstl::wstring>` at +0x10:

- `fn_800E10EC` (0x20) = `rstl::destroy<X>(X*)`, the header's `{ destroy_impl(in); }`: one call
  and nothing else. **Evidence, not a twin guess:** the function immediately below the claim,
  `fn_800E108C` (0x800E108C, 0x60, still in the auto unit), is a loop over a run of 0x20-byte
  elements - count at +0, items at +4 (`lwz r0,0x0(r29)`, `addi r31,r29,4`), `addi r31,r31,0x20`
  - that calls `fn_800E10EC` with each element's address in r3, which is `destroy(&*cur)`'s call
  shape and fixes the element type at 0x20 bytes.
- `fn_800E110C` (0x24) = `rstl::destroy_impl<X>(X*)`, 9 instructions, no trait test (X is not
  trivially destructible), `in->~X()` = `fn_800E1130(in, -1)`.
- `fn_800E1130` (0x58) = `X::~X()` with the deleting flag: `addi r3,r30,0x10 / li r4,-1 /
  bl fn_800E1188`, then `Free(this)` when `extsh. r0,r31` is positive. Its callers are retail's:
  two stack objects 0x20 bytes apart in `fn_80244314` (`addi r3,r1,0x94` / `addi r3,r1,0xB4`, both
  flag -1, `auto_03_80243ED4_text.s` 0x80244760 / 0x8024476C).
- `fn_800E1188` (0xA4) = `rstl::vector<rstl::wstring>`'s **deleting** destructor: iterate
  `[mItems(+0xC), + mCount(+0x4) * 0x10)`, `internal_dereference__Q24rstl66basic_string<w,...>Fv`
  per element, `Free(mItems)`, `Free(this)` when the flag is positive. Caller: a `+0xFC` member in
  `MetroidPrime/CSlideShow.s` 0x80190570 (`li r4,-1`).

The class itself is not named in `symbols.txt`. The seeder's twins were `<c>`-instantiation twins
of the *shape*: `__sys_free`, `destroy_impl<11CTweakValue>`, `__dt__24CSpawnSystemKeyframeDataFv`
and `__dt__Q24rstl110vector<Q24rstl66basic_string<c,...>>Fv` (the last at 0x8000971C, 0xA4, the
same bytes as ours apart from the one `bl`).

## The one deviation from the seeder's brief: a `.cpp`, and why

The item asked for `src/MetroidPrime/Carve800E10EC.c`. It is a `.cpp`, for two **measured**
reasons, and the four functions still do not mangle (`extern "C"` definitions, so objdiff pairs
`fn_800E10EC` etc. exactly as before).

1. **A `.c` file cannot name the callee at all.** `fn_800E1188` calls
   `internal_dereference__Q24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>Fv`,
   and `<`, `,`, `>` are not valid in a C identifier. Measured: a `.c` probe declaring it as
   `extern void internal_dereference__Q24rstl66basic_string<w,...>Fv(void*);` fails to compile -
   `undefined identifier 'internal_dereference__Q24rstl66basic_string'`. `RUNNING_THE_DECOMP.md`
   already states the rule ("a C or `extern "C"` identifier can carry any mangled name **made of
   identifier characters**"); this callee is not made of them. Naming it through
   `~rstl::basic_string` (the header's `{ internal_dereference(); }`) costs no declaration at all.
2. **The bytes want the header's own spelling.** The four dead stack stores at
   0x800E11C0..0x800E11CC are the `pointer_iterator` temporaries `rstl::destroy(begin(), end())`
   passes by value. A raw pointer loop spells the same arithmetic and is **16 bytes short**, frame
   0x20 instead of 0x30, 34 differing instructions (measured, `tools/carve_diff.sh` on both
   compiles). `rstl::destroy(v->begin(), v->end())` is `~vector()`'s body verbatim.

## The register allocation was the one real block, and it is spelling-sensitive

First spelling (`if (v)` on a typed local) produced instructions +45 and +47 as
`mr. r29,r3` / `mr r28,r4` against retail's `mr r29,r4` / `mr. r28,r3` - the same code with
`self` and the flag in each other's register, **9 differing instructions**. Ten more spellings
(`if (v != 0)`, `v == 0` early return, `reinterpret_cast`, a typed parameter, `int flag`, a
`const short deleting = flag` copy, splitting the two `if`s, `v` vs `self` in the `free`) gave the
same 9; only guarding on the **raw parameter** (`if (self) { WStringVector* v = ...; }`) reproduces
retail's assignment, at 0 differing instructions once `Free__7CMemoryFPCv` was declared
`extern "C"` (declared without it, the call mangles to `Free__7CMemoryFPCv__FPCv` and would have
been undefined at link).

## Verification, measured

- `tools/carve_diff.sh 800E10EC 140 <obj>`: 80 instructions both sides, 320 bytes both sides; the
  only differing words are the 6 link-filled `bl` displacements, and each of the object's 7
  relocations names exactly the symbol retail's `bl` calls (`fn_800E110C`, `fn_800E1130`,
  `fn_800E1188`, `Free__7CMemoryFPCv` x3, `internal_dereference__...<w>...Fv`).
- `tools/unit_fit.sh MetroidPrime/Carve800E10EC.cpp`: `.text claimed 320, ours 320, retail 320,
  fits; no extra functions`. The object's only section is `.text` (0x140) plus `.comment`, so the
  claim carries no `.rodata`/`.sdata2` the linker would have to place.
- `tools/flip_test.sh MetroidPrime/Carve800E10EC.cpp`: `PASS -> kept as Matching`,
  `kept: 1 / 1 failed: 0 skipped: 0`; `sha1sum build/G2ME01/main.dol` =
  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all 86 RELs unchanged (the gate's `hashes vs
  config.yml` step is `ok`).
- `build/report.json`: `main/MetroidPrime/Carve800E10EC` `complete: true`, `matched_functions 4 /
  4`, `matched_code_percent 100.0`, each of the four at `fuzzy_match_percent 100.0`. Total
  `matched 12523 -> 12527`, `linked 5896 -> 5900`, `total_functions 28465` unchanged.
- The auto unit split 52 = **36** (`auto_03_800DFA60_text`) + **4** (ours) + **12**
  (`auto_03_800E122C_text`), and neither auto unit reports `matched_functions`, so nothing matched
  was lost by the split.
- `python3 tools/check_decl_order.py --unit main/MetroidPrime/Carve800E10EC`: ok, 1 unit checked,
  none permuted. `check_symbol_names.py`: 526 units, 0 missing names.
  `check_files_cmake.py`, `check_module_wiring.py`, `gen_module_order.py --check`: all ok, module
  order unchanged (86 modules).
- Port: `./tools/probe_sources.sh` = `764 files, 0 failed, 0 errors; link: LINKED (291 undefined,
  0 duplicates)` - the same 291 as the judge's baseline, so the new unit opened nothing and
  duplicated nothing. It needs no `TARGET_PC` guard: the port defines `Free__7CMemoryFPCv` in
  `src/Kyoto/Alloc/PortMwccNew.cpp:34` and the element teardown binds to `rstl_strings.cpp`'s host
  specialization, and every call site of all four functions is 32-bit assembly, so no host code
  ever reaches the DOL offsets.
- `tools/check_raw_offsets.py` after the doc edit: `ok: 170 raw-offset site(s) in 74 file(s), all
  documented in raw_offsets.md`. The unit's one site is `+0x10` in `fn_800E1130` - Kind A, an
  opaque receiver (an unnamed class, all four definitions anonymous `fn_`); `mCount` / `mItems` /
  the 0x10 stride go through the modelled `rstl::vector`.

## Two measurement corrections worth keeping

- **The raw-offsets total in `docs/research/raw_offsets.md` was already stale by one file and one
  site before this item.** `python3 tools/check_raw_offsets.py` measured **169 sites in 73 files**
  on the tree the line was committed on while the prose read 168 in 72; the doc's 73 file headings
  summed to 169. That is the drift that file's own paragraph records five times - the tool compares
  per-file heading counts and never the summary. The line now reads the measured 170 in 74 and says
  the previous figure was 169 in 73. `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` also appear
  modified in `git status`: those are the judge's own `check_docs_claims.py --write` count updates
  (matched/linked/DOL-units/probe-count), not edits of mine.
- **`unit_fit.sh` is not a carve verdict and neither is `carve_diff`:** both were green on the
  first spelling whose register allocation was wrong. `flip_test.sh` is the one that decides.

## Notes for the next run

No `NEW:` items - the item is complete, no `WALL:`, no `STALE:`. Nothing was left half-edited;
the only files changed are the five above. The `auto_03_800E122C_text` unit that this carve
created (12 functions, 0x800E122C..0x800E17E4) now starts exactly at `fn_800E122C`,
`vector<CToken>`'s 0xA8 deleting destructor - the same kind of group this carve took, and the
obvious next carve in this gap, with `fn_800E108C` (0x800E108C, 0x60, the element-run teardown
loop that calls `destroy<X>` per element) and the rest of the 36-function head the ones below it.

---

## Run 2 (lane 12, 2026-10-02, `wt-mp2-goal-L12`): re-derived from scratch, landed, judge PASS

**The run-1 work above was not in the tree.** Measured at the start of this run: `git status`
clean, `src/MetroidPrime/Carve800E10EC.*` absent, `git log --all -- src/MetroidPrime/Carve800E10EC.cpp`
empty (no ref anywhere has it), `build/goal/judge/report.base.json` matched **12537** with no
`Carve800E10EC` unit, and `git log --all -- docs/goal-notes/carve-800e10ec.md` empty. So this was
not `STALE:` - every part of the carve was written again on this tree, and the four functions
matched on the **first** compile. Run 1's notes were used as a recipe, not trusted as measurements;
everything quoted below is from this tree, this run.

Files (same four-file carve, same unit name `MetroidPrime/Carve800E10EC`):

| file | line | entry |
| --- | --- | --- |
| `src/MetroidPrime/Carve800E10EC.cpp` | new, 90 lines | `extern "C"` definitions descending by address: `fn_800E1188` (l. 58), `fn_800E1130` (l. 73), `fn_800E110C` (l. 86), `fn_800E10EC` (l. 90) |
| `configure.py` | 666 | `Object(Matching, "MetroidPrime/Carve800E10EC.cpp"),`, before `Carve800E1548.c` (0x800E1548), ascending |
| `config/G2ME01/splits.txt` | 435-436 | `.text start:0x800E10EC end:0x800E122C`, between `CSimpleShadow.cpp` (ends 0x800DFA60) and `Carve800E1548.c`; `total_functions` still **28465** |
| `files.cmake` | 444 | `src/MetroidPrime/Carve800E10EC.cpp`, before `Carve800E1548.c` |

No `PortLinkStubs.cpp` duplicate existed (grep: none of the four names is there), and no
`docs/research/raw_offsets.md` edit was needed - see the next paragraph.

### Two deliberate differences from run 1's recipe

1. **No raw-offset site.** Run 1 passed `this + 0x10` as a raw offset and had to document it,
   moving that doc's measured total to 170 sites in 74 files. The receiver is modelled instead as
   `struct CWideStringListHolder { uchar mUnknown00[0x10]; rstl::vector< rstl::wstring > mStrings; }`,
   so `fn_800E1130` calls `fn_800E1188(&self->mStrings, -1)` and the member's own offset supplies
   the `addi r3,r30,0x10`. Measured after the change: `check_raw_offsets.py` = **169 sites in 73
   files, untouched doc, all documented** (169/73 is this tree's count, not run 1's 170/74). The
   struct is a layout model of an unnamed class, and only its +0x10 member is ever read - the
   evidence is `fn_800E108C` (below), not a guess.
2. **The register allocation needed no iteration this time.** The spelling that reproduced retail
   on the first compile is the one the matched twins in `src/MetroidPrime/CGameArea.cpp:2355-2365`
   already use: `extern "C" rstl::vector<rstl::wstring>* fn_800E1188(rstl::vector<rstl::wstring>* self,
   int flag)` with `if (self != nullptr) { rstl::destroy(self->begin(), self->end());
   CMemory::Free(self->mItems); if (static_cast<short>(flag) > 0) CMemory::Free(self); } return self;`
   (and the same `int flag` / `static_cast<short>(flag) > 0` in `fn_800E1130`). `rstl::destroy(begin(),
   end())` is still load-bearing (it is what emits the four dead `pointer_iterator` stores);
   `CMemory::Free` is used instead of a declared `Free__7CMemoryFPCv`.

### The `.c`-vs-`.cpp` deviation, re-measured on this tree

The item asked for `.c`. Three measurements, all this run, all with the unit's own ninja flags
(`build/G2ME01`'s `mwcc_sjis` rule `cflags`, the same ones `tools/probe_cc.sh` uses) plus
`-lang=c`:

- `#include "rstl/vector.hpp"` in a `.c` does not compile:
  `include\rstl\allocator_auto_ptr.hpp ... 6: namespace rstl { / Error: ^^^^`.
- The element destructor **cannot be named in C at all**:
  `extern void internal_dereference__Q24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>Fv(void*);`
  gives `Error: illegal use of 'void'` at the first `<` - that callee is the one `bl` at 0x800E11E0.
- A raw-pointer C loop with a placeholder callee (right members, right 0x10 stride) measures
  **37 differing instructions** against retail's 0xA4 bytes (`tools/carve_diff.sh 800E1188 0xA4`),
  frame too small: the bytes want the header's `destroy(begin(), end())` temporaries.

So the unit is a `.cpp` with `extern "C"` on all four definitions; `nm` shows the plain symbols
(`fn_800E10EC`, `fn_800E110C`, `fn_800E1130`, `fn_800E1188`), so objdiff pairs them exactly as it
would a `.c`.

### Verification, measured on this tree

- `tools/carve_diff.sh 800E10EC 140 build/G2ME01/src/MetroidPrime/Carve800E10EC.o`: retail 80
  instructions / 320 bytes, ours 80 / 320, **7 differing instructions and all 7 are `bl`s**
  (`fn_800E110C`, `fn_800E1130`, `fn_800E1188`, `Free__7CMemoryFPCv` x3,
  `internal_dereference__...<w>...Fv`). `objdump -r` lists exactly those 7 relocations.
- `tools/unit_fit.sh MetroidPrime/Carve800E10EC.cpp`: `.text claimed 320, ours 320, retail 320,
  fits; no extra functions`. The object is `.text` + `.comment` only.
- `tools/check_decl_order.py --unit main/MetroidPrime/Carve800E10EC`: ok, none permuted.
- `tools/flip_test.sh MetroidPrime/Carve800E10EC.cpp`: `PASS -> kept as Matching`, kept 1/1.
- `./tools/goal_check.sh build/goal/item.json`, verbatim:

```
goal_check: item carve-800e10ec (match) target=MetroidPrime/Carve800E10EC
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12537 -> 12541   linked 5907 -> 5911
  ok    check_symbol_names.py
  ok    All:  35.36% fuzzy, 29.20% matched, 12.93% linked (12541 / 28465 functions)
  ok    flip_test MetroidPrime/Carve800E10EC.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS carve-800e10ec
```

- `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs
  unchanged (goal_check's gate step). `check_files_cmake.py`, `check_module_wiring.py`,
  `gen_module_order.py --check` (86 modules, unchanged), `check_docs_claims.py` ("docs claims agree
  with the tree"), `check_symbol_names.py` (526 units, 0 missing) all ok.
- `build/report.json`: `main/MetroidPrime/Carve800E10EC` 4/4 functions, `matched 12537 -> 12541`,
  `total_functions` 28465 unchanged. The old auto unit split 45 = **36** (`auto_03_800DFA60_text`,
  0x800DFA60..0x800E10EC) + **4** (ours) + **5** (`auto_03_800E122C_text`, 0x800E122C..0x800E1548);
  neither auto unit reports matched functions, so nothing matched was lost.
- This tree's layout differs from run 1's: `MetroidPrime/Carve800E1548.c` (0x800E1548..0x800E163C)
  is already a `Matching` carve here, so the leftover runs are 0x800DFA60..0x800E10EC (36 functions,
  `fn_800E108C` last - the 0x20-byte element-run loop that calls `destroy<X>`) and
  0x800E122C..0x800E1548 (5 functions, starting at `vector<CToken>`'s 0xA8 deleting destructor).
  Those are the next carves in this gap; no `NEW:` filed, since `goal_seed.py --only carve` plans
  them.

No `WALL:`, no `STALE:`, no `NEW:`. No commit (the driver commits). The only files this run
changed are the four in the table above (plus the two judge-rewritten docs named below); `docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` show as modified in
`git status` because goal_check's own docs-claims step rewrote the derived counts (matched/linked/
DOL/probe 766 -> 767), not because of an edit of mine.
