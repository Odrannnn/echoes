# progress-unit-typesmatch

`kind: progress`, `target: MetroidPrime/TypesMatch`, one file touched:
`src/MetroidPrime/TypesMatch.cpp`. The unit stays `NonMatching`; `flip_test` was not run.

**Verified: `./tools/goal_check.sh build/goal/item.json` -> `PASS`**, run twice (once after the
casts alone, once after the `fn_8009D45C` rename below was added).

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 11779 -> 11782   linked 5727 -> 5727
  ok    check_symbol_names.py
  ok    All:  33.43% fuzzy, 26.46% matched, 12.64% linked (11782 / 28465 functions)
  ok    target rose: main/MetroidPrime/TypesMatch: 503 -> 506 / 511 functions
  ok    no asm added
goal_check: PASS progress-unit-typesmatch
```

Measured position, from `build/report.json` on this tree vs `build/goal/judge/report.base.json`:

| | base | now |
|---|---|---|
| unit matched / total functions | 503 / 511 | **506 / 511** |
| unit fuzzy % | 98.1474 | **98.9783** |
| unit matched-code % | 95.3846 | 95.8556 |
| DOL matched functions | 10231 | 10234 |

`TypesMatch.cpp` is `Object(NonMatching, ...)` in `configure.py`, so `mwldeppc` links the *original*
retail object, not this one (`tools/project.py:1138`, `link_built_obj = obj.completed`). That is why
the DOL sha1 and all 86 RELs are untouched by anything below - and also why none of it counts as a
`Matching` unit yet. `docs/research/decl_order.md` already lists `main/MetroidPrime/TypesMatch`
(499 fns) as permuted, so this unit cannot be flipped until it is reordered; that is not this item's
job and was not touched.

## What landed: three functions, 0.00% -> 100.00%

The three were the *same* defect: the DOL defines both `TCastToPtr<T>(CEntity*)` and
`TCastToPtr<T>(CEntity&)` for a given type id, but the retail map file (read by dtk into
`config/G2ME01/symbols.txt`) spells the **two halves of one pair after two different classes**. Both
halves of each pair load the identical type id, which is the measurement that they are one class:

| id | pointer cast (`__FP7CEntity`) | reference cast (`__FR7CEntity`) | addresses | id loaded |
|---|---|---|---|---|
| 71 | `TCastToPtr<22CScriptPointOfInterest>` | `TCastToPtr<10CUnknown71>` | 0x80099334 / 0x80099358 | 71 / 71 |
| 90 | `TCastToPtr<19CScriptTimeKeyframe>` | `TCastToPtr<10CUnknown90>` | 0x80098CF8 / 0x80098D1C | 90 / 90 |
| 122 | `TCastToPtr<8CMetroid>` | `TCastToPtr<13CMetroidAlpha>` | 0x80098278 / 0x8009829C | 122 / 122 |

`CScriptPointOfInterest` and `CScriptTimeKeyframe` are real Echoes classes (the DOL calls their casts
from `CScanDisplay.cpp` and `CScriptCamera.cpp`/`CScriptPathCamera.cpp`) but **this tree has no header
for either** - both are forward declarations only. `CMetroidAlpha` does have a header, and it is a
pointer-only interface with no `CEntity` base, which is why its cast has to `reinterpret_cast` like
`CScriptPlayerHint`'s already did.

Each pair was therefore split into its two overloads, spelled separately, under retail's own names for
them. Before, `CAST_TO_IMPL(CUnknown71, 71)` emitted both halves as `CUnknown71`; the pointer half
mangled to a name retail does not have, so objdiff could not pair it and reported 0.00%. After, the
pointer half is `TCastToPtr<22CScriptPointOfInterest>` and pairs at 100%.

Four new macros carry this (`CAST_TO_IMPL_PTR`, `_REF`, `_PTR_INCOMPLETE`, `_REF_INCOMPLETE`,
`src/MetroidPrime/TypesMatch.cpp:688-716`); the existing two are untouched and every other cast still
uses them. **Both spellings are emitted**, because the DOL defines both symbols - leaving one out
would trade a 0% for an undefined symbol, not a match.

Per function (before -> after, both from `build/report.json`):

- `TCastToPtr<22CScriptPointOfInterest>__FP7CEntity`, 36 B, 0.00% -> **100.00%**
- `TCastToPtr<19CScriptTimeKeyframe>__FP7CEntity`, 36 B, 0.00% -> **100.00%**
- `TCastToPtr<13CMetroidAlpha>__FR7CEntity`, 48 B, 0.00% -> **100.00%**

A side effect worth keeping: `CScriptDock.cpp:41` (`TCastToPtr<CMetroidAlpha>(actor)`),
`CScanDisplay.cpp:33`, `CScriptCamera.cpp:78` and `CScriptPathCamera.cpp:154` each reference one of
these. All four were previously undefined references in those objects (`nm -u` confirmed
`U TCastToPtr<13CMetroidAlpha>__FR7CEntity` in `CScriptDock.o`); they now resolve.

## Also improved (not a matched function): `fn_8009D45C` 0.00% -> 84.93%

The unit's `CUnknownItemList::~CUnknownItemList` already called a helper the source called
`DestroyUnknownItems`; retail's symbol table calls that function `fn_8009D45C`. objdiff pairs
functions **by name**, so a C++-mangled spelling can never pair with retail's `fn_` name: it scored
0.00% despite the body being nearly right. Renaming it to C linkage (`extern "C" void fn_8009D45C`,
`src/MetroidPrime/TypesMatch.cpp:187-189` and `:929-940`, the same pattern
`src/MetroidPrime/Player/CGameStateBlockDtor.cpp:65` uses for `fn_80004A4C`) made it pair.

Then one codegen lever, from `docs/RUNNING_THE_DECOMP.md:983` ("MWCC hands out callee-saved
registers in declaration order"): retail keeps the loop **bound** in r31 and lets r30 walk the array
(`lwz r31,0(r4)` then `lwz r30,0(r3)`, retail 0x8009D46C/0x8009D474). Spelled as two locals with
`end` declared **first**, MWCC assigns r31 to the bound and r30 to the walker, exactly as retail does.

- `fn_8009D45C`, 108 B: 0.00% (unpaired) -> **84.93%**. Spellings tried:
  - `for (item = *first; item != *last; ++item)` (original, bound re-read each iteration): 0.00%, unpaired
  - `item` then `end` as locals: 83.26%
  - `end` then `item` as locals (**kept**): **84.93%**

The remaining 15% is the four dead stores retail makes at 8(SP)/12(SP) - the two outgoing-argument
copies of the bounds - which MWCC will not produce for a frame this small. That is a real codegen
difference, not a spelling one, and it is the same thing that holds `~CUnknownItemList` (below) short.

## What is left, and why (5 functions, measured)

Ordered by what a next run should look at first.

1. **`fn_8009D3D8`, 132 B, 0.00% (unpaired)** - the *caller* of the above, i.e.
   `CUnknownItemList::~CUnknownItemList`. Same defect as `fn_8009D45C` and the same fix: retail's map
   has no C++ name for it, ours is `__dt__16CUnknownItemListFv`, so objdiff never pairs it. Renaming
   it to C linkage should move it off 0.00% immediately and reveal its true score; the note in the
   source (retail gives each bound a stack home *and* an outgoing-argument copy, four stores where
   MWCC gives one slot each) suggests it will land short of 100% the same way `fn_8009D45C` does.
   **This is the cheapest remaining item and it is not done.**
2. **`fn_80097520`, 32 B, 0.00% (unpaired)** - `0x80097520`, sitting between `~CBeamProjectile`
   (0x800974C0) and the first `TCastToPtr`. Its whole body is `stwu / mflr / stw / bl 0x80032D88 /
   lwz / mtlr / addi / blr` - a bare forward to `fn_80032D88`, which is `if (this != 0) { if
   (*(uchar*)((char*)this + 12) != 0) this->CToken::~CToken(); }`. Same 8-instruction thunk shape as
   `fn_80032D68` (0x80032D68), which is also unnamed and also 0.00% in the `CGameProjectile` unit.
   Nothing in the DOL calls `fn_80097520`; its only caller-side anchor is that it exists. It is
   **not** in `CGameProjectile.cpp`'s object today, so whoever takes it should first establish which
   class's member destructor this is - it looks like an out-of-line destructor of a class whose only
   member is an optional `CToken`, i.e. the `CGameProjectile` member at +0x1F8 whose destructor is
   `fn_80032CB8` -> `fn_80032D0C` -> `fn_80032D68` -> `fn_80032D88`.
3. **`__dt__17CPlasmaProjectileFv`, 516 B, 95.66%** - one difference, measured: at 0x80097490 retail
   calls `~CBeamProjectile` **out of line** (`mr r3,r30 ; li r4,0 ; bl 0x800974C0`), where MWCC inlines
   it (11 instructions including the vtable store and the base call). The rest of the 516 bytes is
   identical. `#pragma noinline` above `~CBeamProjectile` in
   `include/MetroidPrime/Weapons/CBeamProjectile.hpp` was tried and produced **zero** change in any of
   the 2071 units (verified with a per-unit report diff: 0 units changed). `docs/RUNNING_THE_DECOMP.md:976`
   says `#pragma noinline` works above an *out-of-line* destructor; `~CBeamProjectile` is defined
   inline in the class body (`~CBeamProjectile() override {}`), which is probably the difference.
   The edit was reverted - it is in a shared header and bought nothing.
4. **`__dt__15CCollisionActorFv`, 268 B, 78.49%** - three of the four
   `rstl::single_ptr<CCollidable*>` members at +0x2F0/+0x2F4/+0x2FC. Retail destroys each through the
   **vtable** (`lwz r12,24(r12) ; mtctr ; bctrl`, the deleting destructor at vtable slot 6); MWCC emits
   a direct `bl` to a non-virtual destructor for the same three, because `CCollidableOBBTreeGroup`,
   `CCollidableAABox` and `CCollidableSphere` are forward declarations in
   `include/MetroidPrime/CCollisionActor.hpp` at the point the members are declared. The +0x2F8 member
   (`CCollidableSphere`, which *does* have a full definition with `~CCollidableSphere() override`)
   already goes through the vtable and is byte-identical. **The fix is to include the three
   `Collision/*.hpp` headers in `CCollisionActor.hpp`** so their virtual destructors are visible.
   Not attempted here: it is a shared header whose change has to be measured against every unit that
   includes it, which is a bigger job than this item.

## NEW:

None filed. Items 1 and 2 above are inside this item's own target (`MetroidPrime/TypesMatch`) and so
are not re-queued; the driver should requeue `progress-unit-typesmatch` for them rather than take new
`NEW:` lines. `__dt__17CPlasmaProjectileFv` and `__dt__15CCollisionActorFv` are also in this same unit,
so they belong to a requeue of this item too, not to new queue entries.

No `STALE:` and no `WALL:` line. `fn_8009D45C` was measured at three spellings in this run and is
still moving (0.00% -> 83.26% -> 84.93%), so it is not a wall, and the two destructors were not
measured at several spellings in this run either - one `#pragma noinline` attempt each is not a wall
by the brief's definition.

`docs/HANDOFF.md` shows a diff in this worktree; that is `tools/check_docs_claims.py --write`, run by
`gate.sh` under the judge's `MP_GATE_DOCS_WRITE=1`, not an edit of mine. The brief says the driver
discards edits to that file before judging.
---

# Run 2 (lane `L2`, 2026-10-02): 506 -> 508 / 511

Two functions reached 100%. One of them is the one the previous run called "the cheapest remaining
item and it is not done"; the other is the one it called "a bigger job than this item" - the bigger
job is two `#include`s. The previous run's other two remain walls, and one of them is now proved
impossible in C++ rather than merely hard.

Files touched: `src/MetroidPrime/TypesMatch.cpp`, `include/MetroidPrime/CCollisionActor.hpp`.
The unit stays `NonMatching`; `flip_test` was not run. `docs/HANDOFF.md` shows a diff in this
worktree: that is `tools/check_docs_claims.py --write` under the judge's `MP_GATE_DOCS_WRITE=1`,
not an edit of mine.

**Verified: `./tools/goal_check.sh build/goal/item.json` -> `PASS`**, run three times (after
`fn_80097520`, after the header change, and again after the header comment was added).

```
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 12266 -> 12268   linked 5863 -> 5863
  ok    check_symbol_names.py
  ok    All:  34.66% fuzzy, 28.02% matched, 12.90% linked (12268 / 28465 functions)
  ok    target rose: main/MetroidPrime/TypesMatch: 506 -> 508 / 511 functions
  ok    no asm added
goal_check: PASS progress-unit-typesmatch
```

| | base (report.base.json) | now |
|---|---|---|
| unit matched / total | 506 / 511 | **508 / 511** |
| unit fuzzy % | 98.97833 | **99.33014** |
| unit matched-code % | 95.855576 | **97.03297** |
| DOL matched functions | 12266 | **12268** |

Independently: `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`,
`python3 tools/check_symbol_names.py` = `checked 525 units; 0 declared names are missing from their
object`, `./tools/probe_sources.sh` = `752 files, 0 failed, 0 errors; link: LINKED (287 undefined,
0 duplicates)`. The DOL hash is untouched because `TypesMatch.cpp` is `Object(NonMatching, ...)`,
so `mwldeppc` links the original retail object (`tools/project.py:1138`, `link_built_obj =
obj.completed`) - which is also why the undefined reference added below is inert.

## 1. `fn_80097520`: 0.00% (unpaired) -> 100.00%, 32 bytes

The previous run measured it and stopped: *"Nothing in the DOL calls `fn_80097520`; its only
caller-side anchor is that it exists. It is not in `CGameProjectile.cpp`'s object today, so whoever
takes it should first establish which class's member destructor this is."* The class does not have
to be established to write it, because the whole of retail's 32 bytes is the frame and the forward:

```
stwu r1,-16(r1) / mflr r0 / stw r0,20(r1) / bl 0x80032D88
lwz r0,20(r1) / mtlr r0 / addi r1,r1,16 / blr
```

No vtable store and no `extsh`/deleting tail, so the class is not polymorphic and the destructor is
the entire object; r4 is never written, so the deleting flag goes straight through. Spelled
`extern "C" void fn_80097520(void* self, int deletingFlag) { fn_80032D88(self, deletingFlag); }`
(`src/MetroidPrime/TypesMatch.cpp:485-494`) that is byte-identical, all eight instructions.

**The class name is still unknown and that is a real gap.** `fn_80032D88` (0x80032D88) is
`if (self) { if (self[0xC] != 0) self[0xC]->CToken::~CToken(); }` - a `destroy<T>` - and
`docs/goal-notes/progress-cgp-doorbranch.md` already identifies 0x80032D88 as
`destroy<CImpactVisorEffect::SParticleEffect>` and 0x80032D68 as `construct<...>`, so this is very
likely another template instantiation whose symbol the retail map does not name, in a class this
tree does not have. Because `mwcceppc` mangles destructors and template instantiations, **no
spelling of it can carry the symbol `fn_80097520` except a hand-written forwarder**, which is what
this is. Placed after `TYPES_MATCH_IMPL(CBeamProjectile, ...)` (retail 0x800974C0, its immediate
lower neighbour) rather than in descending-address position: the file is already permuted, and
`python3 tools/check_decl_order.py --unit main/MetroidPrime/TypesMatch` still prints
`would break on a flip`, so the unit cannot flip on this run whatever the order. It pairs because
objdiff pairs by name, and a `progress` item is judged on `report.json`, not on the flip.

The added `extern "C" void fn_80032D88(void*, int);` declaration (`:191-196`) is an undefined reference
in `TypesMatch.o` (`nm -u` confirms `U fn_80032D88`). That is safe here and only here: the unit is
`NonMatching`, so its object never reaches the link - the port's undefined count, the DOL sha1 and
all 86 RELs are all unchanged, which the gate measured.

## 2. `__dt__15CCollisionActorFv`: 78.49% -> 100.00%, 268 bytes

The previous run called this "a bigger job than this item" and was wrong about the size of the job.
Its diagnosis was exactly right: retail destroys `mSpherePrimitive` (+0x2FC) and
`mObbTreeGroupPrimitive` (+0x2F4) through the **vtable** and mwcceppc emitted a direct `bl` because
those two classes were forward declarations at the point the members are declared. Two includes fix
it, and nothing else:

```cpp
#include "Collision/CCollidableSphere.hpp"          // +0x2FC
#include "WorldFormat/CCollidableOBBTreeGroup.hpp"  // +0x2F4, and COBBTreeGroup for +0x2F0
```

`include/MetroidPrime/CCollisionActor.hpp:4,10`, with a comment saying why they must stay (removing
them moves the function straight back to 78.49%). Notes on what each one buys, measured on the
object:

- `CCollidableAABox` (+0x2F8) already arrived complete - `MetroidPrime/CPhysicsActor.hpp:15`
  includes `Collision/CCollidableAABox.hpp` - which is why exactly one of the four members was
  already correct and the previous run could see it.
- `Collision/CCollidableSphere.hpp` alone gets +0x2FC onto the vtable.
- `WorldFormat/CCollidableOBBTreeGroup.hpp` gets **two** members: +0x2F4 onto the vtable, and
  +0x2F0 from `addic./beq/lwz/bl` (4 instructions, `single_ptr<COBBTreeGroup>` with `COBBTreeGroup`
  incomplete) onto retail's `addi r3,r30,752 / li r4,-1 / bl __dt__13SUnknownOuterFv` (3). That is
  because it is the header that pulls `WorldFormat/COBBTreeGroup.hpp` in.
- The four forward declarations at `:21-24` are now redundant but were left alone: they are
  harmless, and deleting them would widen the diff for no measured gain.

The five translation units that include this header (`TypesMatch.cpp`, `CCollisionActor.cpp`,
`CCollisionActorManager.cpp`, `CBallCamera.cpp`, `CScriptTrigger.cpp`) all rebuilt and the report
diff shows no function anywhere worse - `goal_check`'s `counts:` and `gate.sh`'s report diff both
say so, and `All:` fuzzy/matched percentages are unchanged at 34.66% / 28.02%.

## 3. `fn_8009D3D8` cannot be renamed: mwcceppc has no C-linkage destructor

The previous run's cheapest remaining item was "rename it to C linkage". Measured this run: **there
is no way to give a destructor the symbol `fn_8009D3D8`**, and it is not a source-shape problem.

```
extern "C" ~CUnknownItemList();      -> Error: illegal storage class
extern "C" class CA { ~CA(); };      -> emits __dt__2CAFv   (C linkage ignored)
class CB {...} inside extern "C" {}  -> emits __dt__2CBFv   (C linkage ignored)
extern "C" void f(uchar*& p)         -> *p is parsed as uchar, i.e. the & is dropped
```

The last one matters as the general rule: **mwcceppc does not support a reference to a pointer**, so
the one construct that would have produced retail's four stores at `fn_8009D3D8` (a `T*&` parameter
forces the caller to materialise a reference slot per argument) cannot be spelled at all.

The alternative - keeping `~CUnknownItemList` and adding a second `extern "C"` forwarder - emits a
function retail's object does not define, which `tools/unit_fit.sh` exists to prevent, so it was
not done. `fn_8009D3D8` stays unpaired at 0.00%. Its caller-side shape is right except for the two
missing stores; see the wall below.

## 4. What is left, and why (3 functions, measured)

1. **`fn_8009D3D8`, 132 B, unpaired (0.00%)** - see above; the name is unreachable and the body is
   a wall. Two independent blockers, either of which alone stops it.
2. **`fn_8009D45C`, 108 B, 84.93%** and **`fn_8009D3D8`'s body** are the same wall - see below.
3. **`__dt__17CPlasmaProjectileFv`, 516 B, 95.66%** - unchanged and not retried: the previous run
   measured `#pragma noinline` above `~CBeamProjectile` and it changed **zero** of the 2071 units
   (it is defined inline in the class body), so one attempt is not several and there is no new
   spelling in this run to try.

WALL: fn_8009D3D8 and fn_8009D45C - retail emits a home *and* an outgoing-argument copy for each of
the two bounds (4 stores: 12(SP), 8(SP), 16(SP), 20(SP) in the caller; 8(SP), 12(SP) in the callee)
and every spelling measured in this run collapses them to 2.

WALL: fn_80097520's class name is unknown (see 1), so the function is right but unlabelled - a later
run that identifies the class can give it a real C++ spelling, though the `fn_80097520` symbol will
then have to move to a fresh forwarder or the pairing has to be given up.

## The dead-store wall, spelled out so the next run does not repeat it

Retail `fn_8009D3D8` (0x8009D3D8) versus ours, word for word, ignoring relocated fields:

| retail | ours | note |
|---|---|---|
| `addi r3,r1,20` / `addi r4,r1,12` | `addi r3,r1,8` / `addi r4,r1,12` | arg1's slot |
| `add r5,r5,r0` | `add r0,r5,r0` | which register the bound lands in |
| `stw r5,12(r1)` | `stw r0,12(r1)` | follows from the row above |
| `stw r5,8(r1)` | `stw r0,8(r1)` | follows |
| `stw r0,16(r1)`, `stw r0,20(r1)` | **absent** | the two extra copies |
| `lwz r3,12(r30)` for the free | was `lwz r3,8(r1)` | **fixed this run** |

Retail frees the *member* (`lwz r3,12(r30)`), not the local, so `CMemory::Free(xC_items)` is right and
`CMemory::Free(first)` is wrong; that one row is settled. The other five are not: **14 spellings**
(all measured with the unit's own compile line through wibo + mwcceppc, scored word-by-word against
`tools/dis.sh 0x8009D3D8 0x84`) produce a 31-word function every single time, against retail's 33.
`uchar**` vs `uchar* const*` vs `SUnknownItem**` vs `void**` parameters; `uchar*` vs
`SUnknownItem*` locals and members; `const` and `volatile` locals; `last`/`first` and
`first`/`last`/`end` declaration order; the bounds in an inner scope; an extra `uchar* last = end;`
(the only one that changed the length, to 30 words); `Free(first)` vs `Free(xC_items)`;
`reinterpret_cast<void**>(&first)` at the call. Best score 21/33 words different, and no spelling
produced a single one of the two missing stores. The remaining difference is register allocation
(`add r5` vs `add r0`) plus two dead stores, which is the brief's definition of a wall.

The general rule worth keeping: **MWCC passes the address of a local straight to the callee and
never makes an outgoing-argument copy of it**, so retail's four stores in this shape are not
reachable by spelling the two arguments as `&local`. Anything that would normally force the copy -
a reference parameter, a conversion needing a temporary - is either dropped by the parser
(`T*&`) or compiled away, as measured.

## NEW:

None filed. All three remaining functions are inside this item's own target
(`main/MetroidPrime/TypesMatch`), so they belong to a requeue of `progress-unit-typesmatch` and not
to new queue entries. `fn_80097520`'s class identity and `fn_80032D88`'s definition both live in
`MetroidPrime/Weapons/CGameProjectile`, whose unpaired functions `docs/goal-notes/
progress-cgp-doorbranch.md` already enumerates; that unit, not this one, is where the ladder
belongs.

No `STALE:`. No new `WALL:` line for `fn_8009D45C` is claimed separately from the one above: the
two functions fail on the identical construct, and this run measured the caller's spelling, which is
where the construct lives.
