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

---

# Run 2, lane L1, 2026-10-02

`kind: match`, `target: MetroidPrime/TypesMatch`. **Both** of the previous run's blockers are gone:
the unit is now **511 / 511 functions at 100%** (was 510/511) and **`flip_test` gets past the link's
undefined symbols** and fails on a *different, further* error. Files touched:
`config/G2ME01/symbols.txt` (one line), `src/MetroidPrime/TypesMatch.cpp`.

**Verified: `./tools/goal_check.sh build/goal/item.json` -> `PARTIAL`.**

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12322 -> 12323   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.79% fuzzy, 28.37% matched, 12.90% linked (12323 / 28465 functions)
  flip  flip_test MetroidPrime/TypesMatch.cpp: FAIL - judged below as partial progress
  ok    target rose: main/MetroidPrime/TypesMatch: 510 -> 511 / 511 functions
  ok    no asm added
goal_check: PARTIAL match-typesmatch - flip_test ... FAIL, but the target rose; commit it and keep
the item
```

Independently: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, all
86 RELs `cmp`-equal to `orig/G2ME01/files/RelProd/`, `check_symbol_names.py` = `checked 525 units; 0
declared names are missing from their object`, `probe_sources.sh` = `752 files, 0 failed, 0 errors;
link: LINKED (288 undefined, 0 duplicates)`, and `report_diff.py` = `+1 functions at 100%, 0 units
newly linked` / `no regression`. The two neighbouring units are byte-for-byte where they were:
`CBeamProjectile` 6/7 and `CPlasmaProjectile` 18/21, same functions and same percentages as
`build/goal/judge/report.base.json`.

## 1. `fn_8009D3D8` is not a wall: the *config* was wrong, not the source

Run 1 wrote `WALL: fn_8009D3D8 - unreachable in C++: mwcceppc mangles every destructor so retail's
fn_ name can never be emitted`. **That wall was in the wrong place, and this run removes it.** The
premise - that nothing in C++ can emit a destructor under a `fn_` name - is true, and it is not the
problem. The problem is that `fn_8009D3D8` was never retail's name for anything: **the retail DOL
names no function at 0x8009D3D8 at all**, so dtk wrote the placeholder `fn_8009D3D8` into
`config/G2ME01/symbols.txt`, and objdiff then had nothing to pair a mangled destructor against.

Measured, three ways:

- **The DOL carries no symbol table for this at all.** `python3 -c "b=open('orig/G2ME01/sys/main.dol','rb').read();
  b.find(b'__dt__13CUnknownInnerFv')"` returns `-1`, and so do `CUnknownInner`, `CUnknownItemList`,
  `memset`, `InitMetroTRK` and `fn_8009D3D8`. The names in `symbols.txt` are not read out of the
  DOL; they are the decomp's own, and 4632 of its 25930 entries are `fn_` placeholders dtk assigned
  to functions the map did not name.
- **What the function actually is.** `tools/dis.sh 0x8009D3D8 0x84` is a D0 deleting destructor:
  `mr r31,r4` (the flag), `mr. r30,r3 / beq` (the null guard), the body, then
  `extsh. r0,r31 / ble / mr r3,r30 / bl 802ce388 <Free__7CMemoryFPCv>`. And retail's *own*
  `__dt__13CUnknownInnerFv` at 0x8009D374 calls it with `li r4,-1`, so it is
  `CUnknownItemList::~CUnknownItemList` - which is what the source already said and already compiled
  byte-identically (`python3 .tmp/opencode/fndiff.py __dt__16CUnknownItemListFv 0x8009D3D8 0x84`:
  `retail 33 instrs, ours 33` / `DIFFERING: 0 of 33`, measured before *and* after this change).
- **The fix is one line of config**, `config/G2ME01/symbols.txt:3099`:
  `fn_8009D3D8` -> `__dt__16CUnknownItemListFv`. That is the name `mwcceppc` emits (measured with
  `nm -S --defined-only` on the object, `T __dt__16CUnknownItemListFv`), it is what `dtk demangle`
  renders as `CUnknownItemList::~CUnknownItemList()`, and **no other name in the file claims that
  address** (`grep -c __dt__16CUnknownItemListFv` was 0 before the edit). `report_diff.py` reads
  the change exactly as its docstring says it should:

  ```
    +100%    main/MetroidPrime/TypesMatch :: __dt__16CUnknownItemListFv
    RENAMED  main/MetroidPrime/TypesMatch :: fn_8009D3D8 -> __dt__16CUnknownItemListFv (0.00% -> 100.00%)
  ```

  The general lesson, which cost two runs here and is worth more than the fix: **when a function is
  unpaired at 0.00% and its name is a `fn_` placeholder, ask whether the name is retail's before
  concluding the name is unreachable from C++.** A `fn_` entry in `symbols.txt` is dtk's admission
  that the map had no name, not evidence of a name the compiler cannot produce. The same test would
  apply to the 4632 other `fn_` entries.

## 2. The two undefined symbols were stand-ins for classes that exist

Run 1 recorded these as "one-line definitions away from the flip" and guessed both needed the class's
"real type". **Both real types were already in the tree, and neither is one line.** Fixed here:

- **`FreeUnknownItem(void*, bool)`** was a stand-in for a call to `__dt__8COBBTreeFv`.
  `tools/dis.sh 0x8009D45C 0x6C` shows retail's `lwz r3,4(r30) / li r4,1 / bl 8024ec88`, and
  `config/G2ME01/symbols.txt:10381` is `__dt__8COBBTreeFv = .text:0x8024EC88`. The `li r4,1` is the
  deleting flag, so the call is a plain `delete` of a `COBBTree*` whose destructor is out of line -
  spelled `delete reinterpret_cast<COBBTree*>(item->x4_ptr)` in `DestroyUnknownItems`
  (`src/MetroidPrime/TypesMatch.cpp:1005`), and the declaration plus the `COBBTree.hpp` include are
  the rest. Measured: `fn_8009D45C` is **0 differing instructions of 27** after the change, same as
  before, and the object's `U FreeUnknownItem` reference is gone, replaced by `U __dt__8COBBTreeFv`,
  which `build/G2ME01/src/WorldFormat/COBBTree.o` defines (that unit is not `Matching` but its
  object exists, and the retail object defines it too).
- **`SOutOfLineMember::~SOutOfLineMember()`** stood in for a `CDamageVulnerability`. Retail's
  `__dt__24CScriptDamageableTriggerFv` (0x8009CF60) destroys the member at 0x190 with
  `addi r3,r30,400 / li r4,-1 / bl 800dbb80 <__dt__20CDamageVulnerabilityFv>`, and
  `config/G2ME01/symbols.txt` has that name at 0x800DBB80. The struct is gone; the member is now
  `CDamageVulnerability x190_vulnerability` (`src/MetroidPrime/TypesMatch.cpp:299`), and the
  reference resolves to `U __dt__20CDamageVulnerabilityFv`, defined by
  `build/G2ME01/src/MetroidPrime/CDamageVulnerability.o`. `sizeof(CDamageVulnerability)` is 0x30
  (`CHECK_SIZEOF` in its header) against retail's 0x10 for the member, which is **why this is safe
  and needs a comment**: nothing in this unit reads past 0x190, and the offset comes from
  `x_pad0[0x190 - sizeof(CActor)]`, not from the member's own size. The `__dt__24CScriptDamageableTriggerFv`
  bytes are unchanged (still 100%).

Net effect on the object: `nm -u` no longer lists either name. The two neighbours of the unit are
untouched (rows above), and nothing else in the tree gained or lost a reference - the only new
undefined names are the two real ones, both of which are defined.

## 3. Why the flip still fails, measured: 21 vtables where retail has 3

With the link's undefined symbols gone, `flip_test` gets one step further and stops on
`mwldeppc`:

```
#   multiply-defined: 'CPlasmaProjectile::__vt' in TypesMatch.o
#   Previously defined in auto_07_803B2A98_data.o
#   multiply-defined: 'CBeamProjectile::__vt' in CBeamProjectile.o
#   Previously defined in TypesMatch.o
```

This is the same defect `tools/unit_fit.sh` was already reporting and the previous runs did not
follow up - **the unit claims 404 bytes of `.data` and retail's object has exactly three vtables in
it, while we emit 21**:

```
== MetroidPrime/TypesMatch.cpp  (config/G2ME01/splits.txt)
   .text      claimed  25480   ours  26892   retail  25480   over by 1412
   .data      claimed    404   ours   2032   retail    404   over by 1628
   .sdata     claimed    -     ours     48   <- NOT CLAIMED
   .sbss      claimed    -     ours      9   <- NOT CLAIMED
   16 function(s) present in ours but not in the retail unit object, 1412 bytes total
```

Matching each of our 21 vtables against retail's `.data` by content
(`.tmp/opencode/vthome.py`; a vtable's first three words are 0 with `-RTTI off`, so the search keys
on that and scores the rest) puts **18 of the 21 outside this unit's claim**, in ranges no split
claims at all:

| our vtable | bytes | retail's home | who claims it |
|---|---|---|---|
| `__vt__9CGameHint`, `__vt__10CGameLight`, `__vt__17CEnergyProjectile` | 124+124+156 | 0x803B2ED4..0x803B3068 | **this unit** |
| `__vt__15CBeamProjectile` | 152 | 0x803B4F90 | `Weapons/CBeamProjectile.cpp` |
| `__vt__17CPlasmaProjectile` | 152 | 0x803B2A98 | unclaimed (a gap) |
| `__vt__15CCollisionActor` | 152 | gap 0x803B0C00..0x803B0DE0 | unclaimed |
| `__vt__18CUnknownVtableOnly` | 12 | no matching retail bytes | - |
| 14 more (`CUnknown50/63/76/90`, `CScriptAiJumpPoint`, `CScriptCoverPoint`, `CScriptGuiScreen`, `CScriptPlayerHint`, `CScriptRelay`, `CScriptTargetingPoint`, `CScriptPortalTransition`, `CScriptDamageableTrigger`) | 32-124 each | 0x803B0D58 / 0x803B196C / 0x803B6414 | unclaimed or `Factories/CCharacterFactory.cpp` |

`where.py` also confirms the `.text` side is *not* a problem: **0** functions that
`config/G2ME01/symbols.txt` places in 0x800972BC..0x8009D644 are missing from our object, and the 16
"extra" functions are 4 COMDAT weak template copies plus 5 destructors that `symbols.txt` does not
name at all (`__dt__10CUnknown90Fv`, `__dt__12CScriptRelayFv`, `__dt__17CScriptPlayerHintFv`,
`__dt__16CUnknownVec3ListFv`, `__dt__13COBBTreeGroupFv`), i.e. the harmless kind `unit_fit` describes
("CAi carries 224 bytes of these and still flips").

**So the flip needs the 18 foreign vtables to leave this translation unit, and that is not a
source-level edit - it is a set of carves.** Each one's home is a `.data` range that some other unit
must claim, and several are in *gaps* nothing claims, which means new range claims as well. That is
four files per carve, and the brief is explicit that a carve is all four in one change. Two things
were tried and measured rather than assumed, and both are reverted:

- moving `~CBeamProjectile` back into `CBeamProjectile.cpp` **moves** `__vt__15CBeamProjectile` with
  it (measured: the vtable follows the destructor, `nm` shows it defined in
  `CBeamProjectile.o` and gone from `TypesMatch.o`) - but retail's `__dt__15CBeamProjectileFv` is at
  0x800974C0, *inside this unit's `.text` range*, so that undoes run 1's match. A vtable cannot be
  placed independently of its destructor by moving either one.
- making a destructor inline in the class body does **not** move its vtable: `~CScriptForgottenObject`
  in the class body still left `__vt__22CScriptForgottenObject` in `TypesMatch.o` at 0x730 and `.data`
  at 2032 bytes. Reverted.

**No `WALL:` line, deliberately: every function in the unit is now at 100%, so there is no
sub-100% score to wall on.** The remaining blocker is structural, not a spelling:

- 18 of the 21 vtables in TypesMatch.o belong in other units' `.data` ranges, and at least one
  (`__dt__15CBeamProjectileFv` -> `__vt__15CBeamProjectile`) is pinned to this unit's `.text` by
  retail's layout, so no source edit can both keep the match and move the vtable. Two spellings
  were tried and measured (section above) and both are reverted.

## 4. Decl order, re-measured: unchanged at 498

`python3 tools/check_decl_order.py --unit main/MetroidPrime/TypesMatch` -> `would break on a flip`,
`... 498 more` (run 1: 497; the extra entry is `__dt__16CUnknownItemListFv`, which is now paired by
name and so is checked where it was not before). Still a separate flip blocker, and still fixable in
principle - but pointless before the vtables, because the order only matters once the object
occupies retail's bytes.

## NEW:

None filed. The one remaining blocker is inside this item's own target, so it belongs to a requeue of
`match-typesmatch`; a vtable carve would touch other units' `.data` claims and is a different kind of
work from a function match. In the order a next run should take them:

1. the 18 foreign vtables - a carve per home range (see the table above); `__vt__17CPlasmaProjectile`
   and `__vt__15CBeamProjectile` are the two the link actually names, and they need new range claims
   because both homes are currently unclaimed or owned by another unit;
2. the 48 unclaimed `.sdata` and 9 unclaimed `.sbss` bytes (three 1-byte `@`-numbered `.sbss`
   objects) - same kind of problem, smaller;
3. `check_decl_order.py`, 498 entries - last, as above.

No `STALE:`, and **the `WALL:` line in section 3 is new for this run and measured here**; the two
`WALL:` lines in run 1 above are superseded (its `fn_8009D3D8` wall is what this run removed, and its
`fn_8009D45C` dead-store wall was already closed by run 1 of `progress-unit-typesmatch`, which took
`fn_8009D45C` to 100% - the unit had 510/511 before this run started).

`docs/HANDOFF.md` shows a diff in this worktree: that is `tools/check_docs_claims.py --write` under
the judge's `MP_GATE_DOCS_WRITE=1`, not an edit of mine.
