# match-typesmatch

`kind: match`, `target: MetroidPrime/TypesMatch`, **lane `L8`, 2026-10-02**. Files touched:
`include/MetroidPrime/Weapons/CBeamProjectile.hpp`, `src/MetroidPrime/TypesMatch.cpp`. The unit
stays `NonMatching`; `flip_test` was run and failed (measured, see below).

**Verified: `./tools/goal_check.sh build/goal/item.json` -> `PARTIAL`**, i.e. the gate is clean and
the target rose, which the brief says the judge commits as partial progress.

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12296 -> 12297   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.73% fuzzy, 28.29% matched, 12.90% linked (12297 / 28465 functions)
  flip  flip_test MetroidPrime/TypesMatch.cpp: FAIL - judged below as partial progress
            #   undefined: 'FreeUnknownItem(void*,bool)'
  ok    target rose: main/MetroidPrime/TypesMatch: 508 -> 509 / 511 functions
  ok    no asm added
goal_check: PARTIAL match-typesmatch - flip_test MetroidPrime/TypesMatch.cpp: FAIL, but the
target rose; commit it and keep the item
```

Independently: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`python3 tools/check_symbol_names.py` = `checked 525 units; 0 declared names are missing from their
object`, and `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json` =
`+1 functions at 100%, 0 units newly linked` / `no regression`.

| | base (`build/goal/judge/report.base.json`) | now |
|---|---|---|
| unit matched / total | 508 / 511 | **509 / 511** |
| unit fuzzy % | 99.33014 | **99.41805** |
| unit matched-code % | 97.03297 | **99.05808** |
| DOL matched functions | 12296 | **12297** |
| linked units | 5863 | 5863 (unchanged) |

## What landed: `__dt__17CPlasmaProjectileFv`, 95.66% -> 100.00%, 516 bytes

**This closes the item that
`docs/goal-notes/progress-unit-typesmatch.md` run 1 called "one difference, measured" and run 2
called "unchanged and not retried".** The difference was that retail calls `~CBeamProjectile`
**out of line** where mwcceppc inlines it. Run 1 tried `#pragma noinline` above
`~CBeamProjectile` in the header and measured **zero** change in any of the 2071 units, and
recorded "`~CBeamProjectile` is defined inline in the class body, which is probably the
difference". That diagnosis was right, and the fix is the obvious next step: **take the body out of
the class body.** Retail's symbol table puts `__dt__15CBeamProjectileFv` at **0x800974C0**, inside
`MetroidPrime/TypesMatch`'s own range (0x800972BC..0x8009D644) and immediately after
`__dt__17CPlasmaProjectileFv` at 0x800972BC - so retail's own translation unit defines it, which is
where ours has to define it too.

- `include/MetroidPrime/Weapons/CBeamProjectile.hpp:15-22`: `~CBeamProjectile() override {}` ->
  `~CBeamProjectile() override;`, with a comment saying why and why the definition must not move.
- `src/MetroidPrime/TypesMatch.cpp:61-76`: `CBeamProjectile::~CBeamProjectile() {}`.

Measured on the object, word for word (`python3 .tmp/opencode/cmp.py`, a 30-line objdiff of our
`.o` against dtk's `build/G2ME01/obj/MetroidPrime/TypesMatch.o`, branch targets and relocation
operands normalised):

```
__dt__17CPlasmaProjectileFv    5 differing instrs (retail 129, ours 134)
   +cmplwi  r30,0 / +beq <t> / +lis r4,0 / +addi r0,r4,0 / +stw r0,0(r30)     <- the inlined base dtor
-> after the change
__dt__17CPlasmaProjectileFv    0 differing instrs (retail 129, ours 129)   *** MATCH ***
__dt__15CBeamProjectileFv      0 differing instrs (retail  24, ours  24)   *** MATCH ***
```

**`inline` on the out-of-class definition is the one thing that must not be there.** Measured:

| spelling of the definition | `__dt__15CBeamProjectileFv` symbol | `__dt__17CPlasmaProjectileFv` |
|---|---|---|
| in the class body (`{}`) | `W` (weak) | 5 differing instrs, 95.66% |
| out of class, `inline` | `W` (weak) | 5 differing instrs, 95.66% |
| out of class, `#pragma noinline` + `inline` | `W` (weak) | 5 differing instrs, 95.66% |
| **out of class, plain** | **`T` (strong)** | **0 differing instrs, 100%** |

`inline` is exactly the token that makes the comdat weak *and* puts the call back inline, so a plain
out-of-class definition is the only spelling that does both halves of what retail needs. The
`#pragma noinline` row also re-measures run 1's negative result in the new form: it works on the
out-of-line destructor (`docs/RUNNING_THE_DECOMP.md:1027`) but cannot beat `inline`.

**Blast radius: three translation units, and in fact none of them changes.** Only
`src/MetroidPrime/TypesMatch.cpp`, `src/MetroidPrime/Weapons/CBeamProjectile.cpp` and
`src/MetroidPrime/Weapons/CPlasmaProjectile.cpp` include the header (`grep -rl CBeamProjectile.hpp
src/ include/`), and **no REL module claims any of them**. More than that, neither of the other two
gains so much as an undefined reference, because neither emits a `CBeamProjectile` vtable - their own
`__vt__15CBeamProjectile` / `__vt__17CPlasmaProjectile` is an **undefined reference in both**, and
`__vt__15CBeamProjectile` (0x758) and `__vt__17CPlasmaProjectile` (0x698) are *defined in
TypesMatch.o*. Measured: `nm -u` on both objects before and after, and on the whole unit's
undefined list before and after - **100 undefined symbols in both, and `diff` of the two sorted
lists is empty**. So the two neighbouring units' objects are unchanged in every respect that matters,
and their report rows are byte-identical (`CBeamProjectile` 6/7 and `CPlasmaProjectile` 18/21, same
fuzzy percentages, before and after). The DOL sha1 and all 86 RELs are untouched - the unit is
`NonMatching`, so `mwldeppc` links the original retail object (`tools/project.py:1138`).

## Why the flip still fails, measured on the base tree too

`./tools/flip_test.sh MetroidPrime/TypesMatch.cpp` fails at the **link**, with three undefined
symbols, none of them mine:

```
#   undefined: 'FreeUnknownItem(void*,bool)'
#   undefined: 'SOutOfLineMember::~SOutOfLineMember()'   (twice)
```

Both are **declared in this unit and defined nowhere in the tree**: `grep -rn FreeUnknownItem
src/ include/` and `grep -rn SOutOfLineMember src/ include/` return only `TypesMatch.cpp` itself.
`git show HEAD:src/MetroidPrime/TypesMatch.cpp` has them too (lines 75, 184), so they predate this
run.

**Measured, not inferred: `git stash`, then `flip_test` on the clean base tree fails with the
identical three errors.** So the flip was out of reach before this change and still is; this run
does not make it worse and does not cause it.

The two symbols are also *deliberate* placeholders, and the file says so. `FreeUnknownItem` stands
for retail's `0x8024ec88`, which `tools/dis.sh 0x8009D45C 0x6C` shows is
`bl 8024ec88 <__dt__8COBBTreeFv>` - `COBBTree::~COBBTree()` with the deleting flag, i.e. a real
`delete` of the item's `x4_ptr`. `SOutOfLineMember` is the 0x10-byte member of
`CScriptDamageableTrigger` at 0x190. **Both are one-line definitions away from the flip**, and both
need the class's real type to be written honestly; neither is a wall, just work this item did not
have room for. That is where the next run on this unit should start, ahead of the two functions
below: **until the link succeeds, no amount of percentage on `fn_8009D45C` can flip the unit.**

Also measured: `python3 tools/check_decl_order.py --unit main/MetroidPrime/TypesMatch` still prints
497 more permuted entries, so the file is not in descending retail order either - a second,
independent flip blocker, unchanged by this run.

## What is left, and why (2 functions, both measured this run)

1. **`fn_8009D3D8`, 132 B, 0.00%, unpaired** - `CUnknownItemList::~CUnknownItemList`. Two
   independent blockers, either of which alone stops it, both inherited from run 2 of
   `progress-unit-typesmatch` and neither re-tried here because both are settled against:
   - **The name is unreachable.** mwcceppc mangles every destructor, so no C++ spelling of a
     destructor emits the unmangled `fn_8009D3D8` that retail's symbol table has, and objdiff pairs
     by name. Run 2 measured this: `extern "C" ~CUnknownItemList();` is `Error: illegal storage
     class`, and C linkage on a class is ignored. I did not re-derive it.
   - **The body is a wall** (below).
2. **`fn_8009D45C`, 108 B, 84.93%** - the same wall, in the callee. Unchanged by this run.

WALL: fn_8009D45C and fn_8009D3D8's body - retail emits a home *and* an outgoing-argument copy for
each of the two bounds (4 stores at 12/8/16/20(SP) in the caller, 2 at 8/12(SP) in the callee) and
mwcceppc collapses them to 2 in every spelling; 18 measured this run on top of run 2's 14, best
result 4 differing instructions out of 27.

### The dead-store wall, re-measured this run (so the next run skips these)

`fn_8009D45C` is 25 instructions for us and 27 for retail. The whole difference is the frame
(`stwu r1,-16` against `-32`) and two stores retail makes at 8(SP) and 12(SP) of the two bounds,
which nothing ever reads. 18 bodies measured with the unit's own compile line, scored
instruction-by-instruction against `tools/dis.sh 0x8009D45C 0x6C`:

| body | differing instrs (of 27) |
|---|---|
| `end` then `item` locals, `!=` loop (**kept**, the current spelling) | 11 |
| one local, bound re-read from `*last` each iteration | 18 |
| `const` locals / `const` params | 11 / 11 |
| `volatile` locals | 27 (three reloads per iteration) |
| `volatile` params | 8 (retail's *parameters* homed at 8/12, not the bounds) |
| `uchar* const*` / `SUnknownItem**` params | 11 |
| 2-element array of bounds, loop reads the array | 9, but 32 instructions for retail's 27 |
| 2-element array + two extra locals | 11 |
| `SUnknownItem**` struct holding the two bounds, `&` of its fields | 11 |
| four locals, two of them duplicates, loop over the duplicates | 11 (mwcceppc coalesces them) |
| nested-block declaration order | 11 |
| early `return` on a null bound | 18 |
| the address of each bound stored to a `volatile` sink | **4** - closest, but the stores land in the wrong slots (`stw r0,8` and `stw r30,8`) and it adds `li r0,0` |
| `SUnknownItemRef` wrapper class with a user copy ctor, operator SUnknownItem* | BUILD FAIL (see below) |
| `typedef uchar*& ItemSlot;` parameters, passed as `first, last` not `&first, &last` | 11, and the **caller is unchanged too** - see below |

Two results are worth keeping because they correct an earlier record:

- **`typedef uchar*& ItemSlot;` compiles.** mwcceppc's diagnostic prints the parameter type as
  `unsigned char *&`, so a reference-to-pointer parameter is spellable after all, and the previous
  run's "cannot be spelled at all" is superseded: it is spellable and *also* useless, because
  mwcceppc drops the reference in the body too - with `ItemSlot` parameters, `first` still compiles
  as a `uchar*` and the callee still emits `lwz r31,0(r4)`, byte-identical to the pointer-parameter
  version. Both the callee and `CUnknownItemList::~CUnknownItemList` came out unchanged (11 and the
  same 31 words). Reverted; the typedef is not in the tree.
- **A `SUnknownItemRef`-style wrapper class does not build at all**: `SUnknownItemRef const end(
  *last ); ... item != p` fails to compile, because `operator SUnknownItem*` on a `const` object is
  not enough for mwcceppc to compare two of them. The idea was that a class-typed local forces a
  stack home; it is worth one more attempt by a run that fixes the comparison (e.g. comparing the
  wrapped pointers through a non-converting accessor), because it is the only mechanism found so
  far that produces homes at all.

The general rule, unchanged and re-confirmed: **MWCC hands the callee the address of a register
resident local and never materialises an outgoing-argument copy for it.** Every construct that would
force the copy - a reference parameter (whether spelled directly or through a typedef), a conversion
needing a temporary, a class-typed local - either is dropped by the front end or compiles away.
`fn_8009D45C` and `fn_8009D3D8` are, on this toolchain, only reachable by a wrapper class whose
copy semantics mwcceppc cannot see through, if at all.

## NEW:

None filed. All three remaining blockers are inside this item's own target
(`main/MetroidPrime/TypesMatch`), so they belong to a requeue of `match-typesmatch` and not to new
queue entries. In the order a next run should take them:

1. define `FreeUnknownItem` and `SOutOfLineMember::~SOutOfLineMember` (three undefined symbols, the
   only thing between the unit and a link that succeeds at all);
2. reorder the file into descending retail offset (`check_decl_order.py`, 497 entries);
3. the dead-store wall above, and `fn_8009D3D8`'s unreachable name.

No `STALE:`. The `WALL:` line above is new for this item and was measured in this run, over 18
spellings, not copied.

`docs/HANDOFF.md` shows a diff in this worktree: that is `tools/check_docs_claims.py --write` under
the judge's `MP_GATE_DOCS_WRITE=1`, not an edit of mine. The brief says the driver discards edits to
that file before judging.
