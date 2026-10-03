# carve-800e0efc — `MetroidPrime/Carve800E0EFC` is `Matching`, 2 functions, 100.00%

`./tools/goal_check.sh build/goal/item.json` → **`goal_check: PASS carve-800e0efc`**, every check `ok`:

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 13507 -> 13509   linked 6555 -> 6557
  ok    check_symbol_names.py
  ok    All:  37.58% fuzzy, 31.02% matched, 13.88% linked (13509 / 28465 functions)
  ok    flip_test MetroidPrime/Carve800E0EFC.c: PASS, Object(Matching) in configure.py
```

Both functions in the item's range matched. Nothing in 0x800E0EFC..0x800E0FAC is left unclaimed.

## What I did

Carved `src/MetroidPrime/Carve800E0EFC.c` — `.text 0x800E0EFC..0x800E0FAC`, 0xB0 = 176 bytes,
2 functions — out of dtk's `main/auto_03_800E028C_text` (0x800E028C..0x800E10EC), and listed it
`Object(Matching, ...)` in `configure.py`. Four files, each placed in address order:

| file | change |
| --- | --- |
| `config/G2ME01/splits.txt` | new entry `MetroidPrime/Carve800E0EFC.c: .text start:0x800E0EFC end:0x800E0FAC`, between `MetroidPrime/CActorParameters.cpp` (ends 0x800E028C) and `MetroidPrime/Carve800E10EC.cpp` (starts 0x800E10EC) |
| `configure.py` | one line, `Object(Matching, "MetroidPrime/Carve800E0EFC.c")`, immediately before `Carve800E10EC.cpp` |
| `files.cmake` | `src/MetroidPrime/Carve800E0EFC.c`, between `Player/Carve800DAE94.c` and `Carve800E10EC.cpp` |
| `src/MetroidPrime/Carve800E0EFC.c` | new, the two bodies |

Plus `src/MetroidPrime/PortLinkStubs.cpp`: two announced empty stand-ins and the header's three
derived counts re-derived — see "the port link" below.

## The twin, and why the two bodies are that short

The seed named `rstl::single_ptr<CGameGlobalObjects>::~single_ptr()` —
`__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv`, 0x80006AE0, 0x58 = 88 bytes, emitted by the
`single_ptr<CGameGlobalObjects>` instantiation in `src/MetroidPrime/main.cpp` against retail's
`include/rstl/single_ptr.hpp:39`. Read against
`build/G2ME01/asm/MetroidPrime/main.s:1719-1745` it is the same 22 instructions as each of these,
only the two `bl` displacements different:

```
stwu/mflr/stw r0,0x14/stw r31,0xc      0x10-byte frame
mr r31,r4 ; stw r30,0x8 ; mr. r30,r3 ; beq out      <- the receiver guard is `mr.`
lwz r3,0x0(r30) ; li r4,1 ; bl <T's own destructor>  <- retail's `delete mPtr`, flag 1
extsh. r0,r31 ; ble out                <- the flag is re-tested as a signed halfword
mr r3,r30 ; bl Free__7CMemoryFPCv     <- the receiver is freed, not the pointee
epilogue with `mr r3,r30`             <- every path returns the receiver
```

**I checked the seed's claim rather than trusting it, and a stronger twin exists.** `diff` of
`build/G2ME01/asm/auto_03_800E028C_text.s:947-969` (retail's `fn_800E0EFC`) against
`build/G2ME01/asm/MetroidPrime/Factories/Carve80032674.s:8-30` (the already-`Matching`
`fn_8003271C`) is **empty** — instruction-for-instruction identical, `bl` targets aside. So the
body was written in the shape of the *already-`Matching* sibling carve
`src/MetroidPrime/Factories/Carve80032674.c:92-100`, which is `Matching`
(`configure.py:708`) with `build/report.json` at 3/3 functions and **100.00%**, rather than out of
the seed's naming. That is the cheap case the carve vein records: read the twin's source, write the
same logic in C under the `fn_` name, and only the callees change.

`if (flag > 0)` has to stay **inside** `if (self)` or the `beq` lands on the `extsh.` instead of
the epilogue and the function is a different length. `flag` is `short`, not `int` — `extsh.`.

Callees, this copy's own and the only difference from the twin:

- `fn_800E0EFC` → `__dt__21CDependencyGroupTokenFv` (0x800C07E0, `symbols.txt:3546`, 0x6C, weak), so
  `T` is `CDependencyGroupToken`. `nm build/G2ME01/main.elf` has `800c07e0 W`, out of dtk's
  `build/G2ME01/obj/MetroidPrime/Player/CMorphBall.o` — **not** our compiled object, so nothing
  stale is linked in place of it.
- `fn_800E0F54` → `fn_800E0FAC` (0x800E0FAC, `symbols.txt:3902`, 0x90 = 144 bytes), **unclaimed**,
  still in dtk's `auto_03_800E028C_text.o`. Declared, never defined here — the same trade
  `src/Collision/Carve8028B728.c` makes for `fn_8028B780`. `fn_800E0FAC` is the byte immediately
  above the claim, so it could not join this carve anyway without spanning an unclaimed gap.
- both → `Free__7CMemoryFPCv` (0x802CE388) under retail's own name; MWCC's old mangling is
  `[A-Za-z0-9_]` only, so a C declaration names it verbatim (the trick
  `src/MetroidPrime/Carve800E1548.c:77` uses).

**The `li r4, 1` on the destructor call is a parameter, not decoration**, the lesson
`docs/goal-notes/carve-8001fedc.md` paid for: both `extern` declarations take
`(void* self, int deleting)` and are called with `1`. A one-parameter declaration drops the `li` and
makes each function 0x54 instead of 0x58, which would permute the unit. I did not re-measure that
here — it is inherited, and I state it as such in the source comment.

## Which two `single_ptr`s these are, measured rather than inferred

Both are called as **member** teardowns with `li r4,-1`, never with the deleting flag, which is what
a `.~member()` looks like:

- `fn_800E0EFC` at 0x800E0ECC from `fn_800E0E7C` (0x800E0E7C, 0x80) as `addi r3,r30,0x4 ; li r4,-1`,
  and the same instruction at 0x800E0ECC from `fn_800E0BE4` (0x800E0BE4, 0x240).
- `fn_800E0F54` at 0x800E0D0C from `fn_800E0BE4` as `addi r3,r30,0x3c ; li r4,-1`.

So `fn_800E0EFC` is a `rstl::single_ptr<CDependencyGroupToken>` destructor at +0x04 of that class
and `fn_800E0F54` one at +0x3C. Both callers are in dtk's `auto_03_800DFA60_text.o` /
`auto_03_800E028C_text.o`, so no `bl` we own moves. The same method is used in
`src/MetroidPrime/Factories/Carve80032674.c:36-51`.

## The port link: 287 -> 289 -> 287, two announced stand-ins

`tools/link_check.sh`'s STRICT gate measures **growth**, not the total, so the carve had to add
nothing. First run:

```
link_check: STRICT FAIL - regression gate: 289 undefined against a baseline of 287 (GREW),
            0 duplicate(s), 0 compile error(s), linker_ran=1
```

`link_check` named 5 symbols as new to the gap, but **only two of them are mine** —
`__dt__21CDependencyGroupTokenFv` and `fn_800E0FAC`, the two externs the carve declares. The other
three (`CExplosion::CExplosion(...)`, `__ct__21SCarve801FECACElementFRC21SCarve801FECACElement`,
`__dt__24CSpawnSystemKeyframeDataFv`) come from `CScriptPickup.cpp`, `Carve801FECAC.cpp` and
`Carve8032F2B8.c`, are all present in `build/goal/judge/undef.base.txt`, and were already there
before this change: `build/goal/judge/record-link.log` at the recorded head `0c95f33f` says
`unchanged from baseline (287 undefined, 0 duplicates)`. **So the tree was at 287 with those three
already missing, and "5 symbols ADDED" was the diff against `docs/research/port_link_baseline.txt`'s
`sym` list, which is not the same set as `undef.base.txt`.** The count, not the name list, is the
gate.

`stub_carve800e0efc_0` (for `fn_800E0FAC`) and `stub_carve800e0efc_1` (for
`__dt__21CDependencyGroupTokenFv`) brought it back:

```
probe: 934 files, 0 failed, 0 errors; link: LINKED (287 undefined, 0 duplicates)
```

The second one is the `__dt__` case `carve-8001fedc` established: the **host** cannot spell a
MWCC-mangled destructor name at all — a host compiler mangles the same destructor
`_ZN21CDependencyGroupTokenD1Ev` — and nothing in `src/` defines `CDependencyGroupToken`'s
destructor body anyway, so the stand-in loses no port behaviour.

## Measured, not recalled

- `main.dol` `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`; all 86 RELs `cmp`-equal to
  `orig/G2ME01/files/RelProd/` (checked individually).
- `nm build/G2ME01/main.elf` has `800e0efc T fn_800E0EFC`, `800e0f54 T fn_800E0F54`,
  `800e0fac T fn_800E0FAC` — the two carve addresses are the second measurement that the two
  functions are each 0x58 and did not skew.
- `build/report.json`: `total_functions` **28465** (unchanged — the claim moved, it did not add),
  `matched_functions` **13507 -> 13509**, `linked` **6555 -> 6557**, `total_units` **2343 -> 2345**,
  `fuzzy_match_percent` 37.580322 -> 37.583015.
- `main/MetroidPrime/Carve800E0EFC`: `.text` 176 B, **2 / 2 functions at 100.00%**.
- `tools/flip_test.sh MetroidPrime/Carve800E0EFC.c`: `PASS -> kept as Matching`,
  `kept: 1 / 1   failed: 0   skipped: 0`.
- `tools/unit_fit.sh MetroidPrime/Carve800E0EFC.c`: `.text claimed 176 ours 176 retail 176 fits`,
  `no extra functions: our object defines only what the retail unit object does`.
- `tools/carve_diff.sh 0x800E0EFC 0xB0 build/G2ME01/src/MetroidPrime/Carve800E0EFC.o`:
  `retail: 44 instructions, 176 bytes` / `ours: 44 instructions, 176 bytes`, `differing
  instructions: 4` — the four are the **two `bl` targets in each function**, which the tool cannot
  resolve in an unlinked `.o`. Instruction and byte counts agree per function.
- `tools/check_symbol_names.py`: `checked 588 units; 0 declared names are missing`.
- `python3 tools/check_decl_order.py --all`: `1205 unit(s) checked, 37 permuted, all 37 accounted
  for in decl_order.md` — the 37 are pre-existing and listed there; the new unit is not among them.
  (`--unit` reports `0 unit(s) checked`; the tool ignores `--unit` on this tree, so the whole-tree
  run is what covers a new file. Definitions here are descending, `fn_800E0F54` then
  `fn_800E0EFC`.)
- `./tools/probe_sources.sh`: `probe: 934 files, 0 failed, 0 errors`.
- `src/MetroidPrime/PortLinkStubs.cpp` header counts re-derived after the edit, not carried:
  `grep -cE 'asm\("'` **205 -> 207**, `^extern "C" void stub_[A-Za-z0-9_]*\(\) asm\(` **195 -> 197**,
  `^extern "C" char stub_data_*` unmoved at **10**; total supplied **203 -> 205**.

  **The paragraph this replaced said 202 / 194 / 202 and was already three low on the parent commit
  `0c95f33f`**, which measures 205 / 195 / 205 (`git show HEAD:src/MetroidPrime/PortLinkStubs.cpp`
  | grep -c). The new paragraph says so and re-derives from the tree; it does not adjust the old
  figure. That is the same drift `AGENTS.md`'s docs rule is about, in a file the generator owns.

`docs/HANDOFF.md` and `docs/RUNNING_THE_DECOMP.md` appear in `git status` because `gate.sh` runs
with `MP_GATE_DOCS_WRITE=1` and rewrote their derived counts. Those are the gate's output; the driver
discards them, and I did not edit them.

## Files touched

- `src/MetroidPrime/Carve800E0EFC.c` (new, 137 lines)
- `config/G2ME01/splits.txt` (+3, after line 533)
- `configure.py` (+1, before line 728)
- `files.cmake` (+1, line 688)
- `src/MetroidPrime/PortLinkStubs.cpp` (+2 stubs and their comments at the tail; header block
  lines 7-21)

No `asm` body was added: the two `asm("…")` lines are the alias labels `PortLinkStubs.cpp` already
uses ~195 times, and both bodies are C. No commit, per the brief.

## Notes worth keeping

- **When the seed's twin is matched but a *byte-identical sibling* is also matched, take the
  sibling.** Comparing the two matched units' asm before writing anything is one `diff` and it
  turns a guess about retail semantics into a copy of code that is already at 100.00%.
- **`link_check`'s "N symbols this change ADDED" is not the growth.** It is a name-set difference
  against `docs/research/port_link_baseline.txt`, and that file's `sym` list is not the same set as
  `build/goal/judge/undef.base.txt`. The gate is the **count** line. Read both, and check which
  files the names come from before deciding what your change added.
- **`PortLinkStubs.cpp`'s header counts drift silently** when a lane adds a stub and the file is
  union-merged. Re-measure all three terms with `git show HEAD:` before editing the paragraph.

## For the next run

Nothing is blocked. In the same unclaimed range (`auto_03_800E028C_text`, 0x800E028C..0x800E10EC,
still entirely dtk's above 0x800E0FAC), all with their shapes measured from
`build/G2ME01/asm/auto_03_800E028C_text.s`:

- `fn_800E0FAC` (0x800E0FAC, 0x90) — this carve's own callee, retail's unclaimed 144-byte
  destructor: null-receiver early return, a member at +0x38 through `fn_800E103C` with flag -1, a
  `bool` at +0x30 gating a `CQuitGameScreen*` at +0x34 through `__dt__15CQuitGameScreenFv`, then
  `__dt__6CTokenFv` on the receiver itself with flag 0, the flag test and `Free`. Carving it would
  retire `stub_carve800e0efc_0` and, if it is the tail of this run's claim, extend it.
- `fn_800E103C` (0x800E103C) — the other callee inside `fn_800E0FAC`, unmeasured here.
- `fn_800E0E7C` (0x800E0E7C, 0x80) — the caller immediately below this claim; it releases members
  through `__dt__12CActorLightsFv` and `__dt__10CModelDataFv` behind `addic.` guards, so it has the
  same twin shape but a longer member list.
- `fn_800E0BE4` (0x800E0BE4, 0x240 = 576 bytes) — the 0x240-byte teardown that calls both of this
  carve's functions at `self+0x04` and `self+0x3c`. Too big for one carve but its member list is
  readable straight off the bytes and it is the only thing that establishes what the two
  `single_ptr`s belong to.
- `fn_800E108C` (0x800E108C, ends 0x800E10EC) — the top of the range, a loop over a 0x20-byte
  element calling `fn_800E10EC` (already `Matching`, `Carve800E10EC.cpp`).

NEW: none. Nothing found here is a blocker, and the `li r4,1` rule is a codegen lesson rather than
work whose success raises a count.