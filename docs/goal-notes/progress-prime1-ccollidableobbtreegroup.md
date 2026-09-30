# progress-prime1-ccollidableobbtreegroup

`kind: progress`, `target: WorldFormat/CCollidableOBBTreeGroup`. The unit stays `NonMatching`; no
`flip_test` was run to decide anything.

## Measured result

`build/report.json`, `main/WorldFormat/CCollidableOBBTreeGroup`, before and after. Both figures
were re-measured in this worktree; the "before" column is the judge's own baseline,
`build/goal/judge/report.base.json` (recorded on the branch head), not a number recalled from the
item's `reason`.

| | before | after |
|---|---|---|
| `matched_functions` | **11 / 32** | **17 / 32** |
| `matched_code` | 604 B | 3956 B |
| `matched_code_percent` | 7.57% | 49.57% |
| `fuzzy_match_percent` | 70.69% | 74.34% |

Project-wide, `python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

```
matched  9873 -> 9879   linked 4895 -> 4895   (+6 functions at 100%, 0 units newly linked)
  +100%    main/WorldFormat/CCollidableOBBTreeGroup :: AABoxCollideBoolean__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructure
  +100%    main/WorldFormat/CCollidableOBBTreeGroup :: CastRayInternal__23CCollidableOBBTreeGroupCFRC25CInternalRayCastStructure
  +100%    main/WorldFormat/CCollidableOBBTreeGroup :: CollideMovingAABox__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureRC9CVector3fRdR14CCollisionInfo
  +100%    main/WorldFormat/CCollidableOBBTreeGroup :: CollideMovingSphere__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureRC9CVector3fRdR14CCollisionInfo
  +100%    main/WorldFormat/CCollidableOBBTreeGroup :: SphereCollideBoolean__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructure
  +100%    main/WorldFormat/CCollidableOBBTreeGroup :: SphereCollide__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureR18CCollisionInfoList
no regression
```

`report_diff.py` compares every unit and every function in `report.json` pairwise, not just the
target: **0 units worse and 0 functions worse**, and the only unit that moved is this one.

## Per function

Percentages are objdiff's `fuzzy_match_percent` for that function, re-measured after each build.
"Prime 1's source" records whether Prime 1's spelling was used unchanged.

| function | before | after | Prime 1's source |
|---|---|---|---|
| `SphereCollide__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureR18CCollisionInfoList` | 92.91% | **100%** | matched unchanged apart from declaration order |
| `SphereCollideBoolean__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructure` | 96.20% | **100%** | matched unchanged apart from declaration order |
| `AABoxCollideBoolean__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructure` | 91.69% | **100%** | matched unchanged apart from declaration order |
| `CollideMovingSphere__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureRC9CVector3fRdR14CCollisionInfo` | 95.95% | **100%** | matched unchanged apart from declaration order |
| `CastRayInternal__23CCollidableOBBTreeGroupCFRC25CInternalRayCastStructure` | 97.27% | **100%** | needed a structural edit (Echoes-only filter + virtual call) |
| `CollideMovingAABox__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureRC9CVector3fRdR14CCollisionInfo` | 93.75% | **100%** | matched unchanged apart from declaration order |
| `AABoxCollide__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureR18CCollisionInfoList` | 82.27% | 97.80% | needed a structural edit (the negated plane normals) |
| `Transform__14CRayCastResultFRC12CTransform4f` | 98.90% | 98.90% | matched unchanged; no spelling found that helps |

The three `COBBTreeGroup` constructors, `fn_80253CB4` and the eight unnamed `fn_80254xxx` were not
touched: the constructors are Echoes-only (Prime 1 calls the class
`CCollidableOBBTreeGroupContainer`) and their bodies are already written, and the unnamed functions
have no Prime 1 counterpart at all.

## What actually changed, and why

Prime 1's `src/WorldFormat/CCollidableOBBTreeGroup.cpp` was the starting point for all eight
functions. Six of the eight needed **no logic change at all** - only a reordering of declarations
and one spelling change. Echoes added two things Prime 1 has no equivalent of: the
`WithImplicitMaterials` filter (and its `kFT_Nearly` early-out) at the top of every collide
function, and a filter argument threaded into the tree calls.

### 1. The filter expression, inlined - the change that fixed five functions

Prime 1 has no filter, so it writes:

```cpp
const CCollidableOBBTreeGroup& right = static_cast<...>(collision.GetRight().GetPrim());
const CCollidableSphere& left = static_cast<...>(collision.GetLeft().GetPrim());
const CMaterialFilter filter = collision.GetLeft().GetFilter().WithImplicitMaterials(right.GetMaterial());
```

Adding the filter around that spelling keeps `right` and `left` **live across the
`WithImplicitMaterials` call**, so mwceppc 2.7 spills both into callee-saved registers before the
call and reloads them after. Retail does not: it reloads `left` from `0(r28)` *after* the
`kFT_Never` test, and only ever holds `this` in a saved register. That single difference accounted
for most of the byte-level gap in all five `SphereCollide`/`AABoxCollide`/`CollideMoving` functions.

Putting the `static_cast` **inside** the filter expression, and declaring `left`/`right` after the
early-out, frees both:

```cpp
const CMaterialFilter filter = collision.GetLeft().GetFilter().WithImplicitMaterials(
    static_cast< const CCollidableOBBTreeGroup& >(collision.GetRight().GetPrim()).GetMaterial());
if (filter.GetType() == CMaterialFilter::kFT_Never) {
  return false;
}

const CCollidableOBBTreeGroup& right = static_cast<...>(collision.GetRight().GetPrim());
const CCollidableSphere& left = static_cast<...>(collision.GetLeft().GetPrim());
```

Measured, on `SphereCollide` (the first tried): 92.91% -> 95.37% for the reordering alone, and
**100%** once the `right` reference was inlined into the filter. On `SphereCollideBoolean` the
declaration order of `right` and `left` after the filter also matters: `right`-then-`left` reaches
100%, `left`-then-`right` stalls at 99.42% (a 12-instruction difference, all register renaming
between r30 and r31). `SphereCollide` wants the same order, so all five functions use
`right` before `left`.

### 2. `CastRayInternal` - the recursive call is virtual in retail

Echoes' `CCollidableOBBTree::CastRayInternal` is `override`, and retail dispatches through the
vtable (`lwz r12,28(r12) / mtctr r12 / bctrl`); calling it on the concrete local emitted a direct
`bl`. Binding the local to a `CCollisionPrimitive&` **at the call site** - not at the top of the
loop body - restores the indirect call and takes the function to 100%. Declaring the reference
earlier, in the loop body before the `RayAABoxIntersection` test, also produces the indirect call
but costs 20 instructions of register renaming, because the extra live value shifts the whole
callee-saved allocation by one. 97.27% -> 100%.

### 3. `CollideMovingAABox` - a destructor retail never calls

This one needed no change to the function at all. Our build emitted
`addi r3,r1,536 / li r4,-1 / bl __dt__Q220CMetroidAreaCollider22CMovingAABoxComponentsFv` at the
end; retail emits no such call, and `powerpc-eabi-objdump -d build/G2ME01/main.elf | grep -c` for
that destructor returns **0** - nothing in the DOL references it.

The cause is in `include/rstl/reserved_vector.hpp`: `~reserved_vector()` is not trivial, and
`is_trivially_destructible<T>` defaults to `false`, so the teardown is not elided.
`CMetroidAreaCollider::SBoxEdge` holds only PODs plus a user-declared *constructor*, so it genuinely
needs no teardown. Declaring the trait:

```cpp
namespace rstl {
template <>
struct is_trivially_destructible< CMetroidAreaCollider::SBoxEdge > {
  enum { value = true };
};
} // namespace rstl
```

in `include/WorldFormat/CMetroidAreaCollider.hpp` (after the class, since it names `SBoxEdge`;
mwceppc 2.7 rejects a late specialisation - the same constraint `CBSLocomotion.hpp` documents)
takes `CollideMovingAABox` from 98.31% to 100%. This is a shared header, so it was checked
project-wide: the build still reproduces the retail DOL, and `report_diff.py` shows no function
anywhere got worse.

### 4. `AABoxCollide` - the six plane normals

Prime 1 writes `CPlane(max, -rightNormal)`, which goes through
`operator-(const CUnitVector3f&)`. Retail negates in a register (`fneg f31,f7`) off the same
constant it loaded for the positive normal, so it needs **one** `lfs` per axis pair, where Prime
1's spelling needs a second constant load. Writing the negation out longhand, and dropping the
`min`/`max` locals so the box corners are read straight out of `bounds`, matches retail's
register-resident negation:

```cpp
const CUnitVector3f negRightNormal(-rightNormal.GetX(), -rightNormal.GetY(), -rightNormal.GetZ());
...
CPlane planes[6] = {CPlane(bounds.GetMinPoint(), rightNormal),
                    CPlane(bounds.GetMaxPoint(), negRightNormal), ... };
```

82.27% -> 97.80%, and the instruction count now matches retail exactly (187 vs 187). What is left
is float register allocation and scheduling only.

## Spellings measured and rejected

Recorded so the next run does not repeat them. `diff` is the count of differing instructions
against `build/G2ME01/obj/WorldFormat/CCollidableOBBTreeGroup.o` with branch targets and relocation
operands normalised away, which is what `tools/try_batch.py` counts; the percentage is the real
objdiff number and is the one that decides.

**`SphereCollide`** (baseline 92.91%, diff 9)

| spelling | objdiff | diff |
|---|---|---|
| `left`/`right` declared before the filter | 92.91% | 9 |
| `left` declared after the filter, `right` before | 95.37% | 5 |
| `left` after the filter, `right` after, `right` inlined in the filter | **100%** | 0 |
| material bound to a local first, reused in the loop | 92.70% | 28 |
| no `left` binding at all (re-cast at each use) | 94.84% | 10 |

**`SphereCollideBoolean`** (baseline 96.20%, diff 15)

| spelling | objdiff | diff |
|---|---|---|
| `right` inlined in the filter, `right` then `left` | **100%** | 0 |
| `right` inlined in the filter, `left` then `right` | 99.42% | 12 |
| `right` inlined, `left` kept but declared first | 95.92% | 14 |
| no `left` binding | 96.21% | 21 |

**`CastRayInternal`** (baseline 97.27%, diff 5)

| spelling | objdiff | diff |
|---|---|---|
| direct call on the concrete local | 97.27% | 5 |
| `CCollisionPrimitive&` bound at the call site | **100%** | 0 |
| `CCollisionPrimitive*` bound at the call site | **100%** | 0 |
| `CCollisionPrimitive&` bound at the top of the loop body | 97.13% | 20 |
| `static_cast` written inline in the call | 97.27% | 5 |
| `CInternalRayCastStructure` hoisted to a named local | 97.13% | 20 |
| `CRayCastResult localResult` by value instead of by const-ref | 97.67% | 71 |
| Prime 1's source, with no filter at all | 76.66% | 109 |

**`AABoxCollide`** (baseline 82.27%, diff 150)

| spelling | objdiff | diff |
|---|---|---|
| Prime 1's exact source (`-rightNormal`, `min`/`max` locals) | 85.05% | 150 |
| negated normals as six named locals | 88.76% | 80 |
| six negated `CUnitVector3f` literals in the initialiser | 88.76% | 80 |
| `bounds.GetMinPoint()` inline, no `min`/`max` locals | 89.19% | 81 |
| negation written longhand, `min`/`max` locals kept | 89.56% | 48 |
| negation written longhand, **no** `min`/`max` locals | **97.80%** | 57 |
| `-CUnitVector3f(1.f, 0.f, 0.f)` (operator on a literal) | 85.05% | 150 |
| `-rightNormal` into named locals via the operator | 85.13% | 143 |
| `CVector3f::Right()/Left()/Up()/Down()/Forward()/Back()` | 56.52% | 158 |
| `planes` declared before `obb` | 66.70% | 191 |
| `CPlane planes[6];` then six `planes[n] = ...` statements | build failure | - |

**`Transform`** (baseline 98.90%, diff 7) - all measured, none better than the baseline:

| spelling | objdiff | diff |
|---|---|---|
| Prime 1's source (baseline) | 98.90% | 7 |
| `CUnitVector3f unitNormal(...)` local, then `CPlane` | 98.90% | 7 |
| `CPlane plane(...)` local, then assign | 98.90% | 7 |
| `CUnitVector3f(normal, CUnitVector3f::kN_No)` | 85.31% | 22 |
| `const CUnitVector3f normal = xf.Rotate(...)` | 85.31% | 22 |
| separate `CVector3f n2` round-trip | 98.90% | 7 |
| copy-construct the unit vector from the rotated one | 69.92% | 28 |

## What is still open

- **`Transform__14CRayCastResultFRC12CTransform4f`, 98.90%, 7 instructions.** The whole difference is
  a register swap inside the inlined `CPlane` constructor: retail loads `8(r1)` into f3 and
  `16(r1)` into f5, ours loads them the other way round, and the two `fmadds`/`stfs` pairs follow.
  The arithmetic is identical; nothing but allocation differs. Every spelling tried above leaves it
  at exactly 7. The lever is `CVector3f::Dot` in `include/Kyoto/Math/CVector3f.hpp`, which is shared
  by the whole project - changing it needs its own measurement, not a guess.
- **`AABoxCollide`, 97.80%, instruction count already equal to retail's (187 vs 187).** Only float
  register allocation and scheduling remain. Retail reuses f7/f6 for the two loaded constants and
  derives both negated normals with `fneg` into f31/f29; ours loads the constants into f8/f7 and
  negates into f12/f13, and the subsequent `fmuls`/`fmadds` chain is ordered differently.
- **`CacheTree`, 1.00%.** Still the declared stub, untouched. It is not one of the item's eight
  functions and the shared collision-cache writer is still unknown.
- The three `COBBTreeGroup` constructors sit at ~70% and the eight `fn_80254xxx`/`fn_80253CB4` at
  0%. The constructors are Echoes-only; the unnamed functions have no Prime 1 counterpart.

## Gates

Run in this worktree, all clean:

```
sha1sum build/G2ME01/main.dol                 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      749 files, 0 failed, 0 errors; LINKED, 0 duplicates
python3 tools/check_symbol_names.py           0 missing names
./tools/decomp_build.sh                       All: 30.44% fuzzy, 22.40% matched, 11.74% linked
                                             (9879 / 28465 functions)
all 86 RELs                                   cmp-equal to orig/G2ME01/files/RelProd/, 0 differ
```

`python3 tools/check_docs_claims.py` reports only the two `docs/HANDOFF.md` state-block counts
(`9879 / 28465` and `8468 / 16726`), which this item moved and which the judge rewrites from the
tree; no doc was edited here, per the brief.
