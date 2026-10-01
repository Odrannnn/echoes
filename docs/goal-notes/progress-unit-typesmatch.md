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