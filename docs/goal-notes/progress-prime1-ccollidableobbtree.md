# progress-prime1-ccollidableobbtree

`kind: progress`, `target: WorldFormat/CCollidableOBBTree`. The unit stays `NonMatching`; the flip
was not attempted (the item says not to use `flip_test.sh` to decide). Prime 1's source was read at
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/WorldFormat/CCollidableOBBTree.cpp`
and adapted to this tree's headers and member names only - no Prime 1 header was copied, no class
layout was changed.

## Result, measured

`build/report.json`, unit `main/WorldFormat/CCollidableOBBTree`:

| | before | after |
|---|---|---|
| `matched_functions` | **9 / 26** | **14 / 26** |
| `matched_code` | 924 | 2472 |
| `matched_code_percent` | 7.2164946 | 19.306467 |
| `fuzzy_match_percent` | 18.900969 | 27.800688 |
| `total_code` / `total_data` | 12804 / 104 | 12804 / 104 (unchanged) |

Whole build, from `./tools/decomp_build.sh` and `tools/report_diff.py` against
`build/goal/judge/report.base.json`:

```
matched  9868 -> 9873   linked 4895 -> 4895   (+5 functions at 100%, 0 units newly linked)
no regression
```

`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
No `asm` was added; the diff touches `src/WorldFormat/CCollidableOBBTree.cpp` only -
`configure.py`, `config/`, `files.cmake`, `tools/`, `docs/` and `build/goal/` are untouched.
`python3 tools/check_symbol_names.py` -> `checked 503 units; 0 declared names are missing`.
`python3 tools/check_decl_order.py --unit WorldFormat/CCollidableOBBTree` -> ok.
`powerpc-eabi-nm -u` on the object: the undefined-symbol set is byte-identical to the HEAD object,
so the port's link gap does not move.

## Per function: before%, after%, and what Prime 1's source did

| function | before | after | Prime 1's source |
|---|---|---|---|
| `AABoxCollision` | 82.09 | **100** | matched **after one small edit** |
| `SphereCollision` | 81.13 | **100** | matched **after one small edit** |
| `AABoxCollisionMoving` | 80.86 | **100** | matched **after one small edit** |
| `SphereCollisionMoving` | 82.95 | **100** | matched **after one small edit** |
| `LineIntersectsOBBTree(node, info)` | 62.19 | **100** | matched **unchanged** + the null guard |
| `LineIntersectsOBBTree(left, right, info)` | 0.70 | 99.91 | needed 3 small edits; instruction stream is now identical |
| `TransformPlane` | 93.80 | 99.80 | needed 1 small edit; 2-load scheduling residue |
| `AABoxCollideWithLeaf` | 0.93 | 0.93 | **blocked** - needs `GetTriangle` |
| `SphereCollideWithLeaf` | 1.49 | 1.49 | **blocked** - needs `GetTriangle` |
| `AABoxCollisionBoolean` | 1.14 | 1.14 | **blocked** - needs `GetTriangle` |
| `SphereCollisionBoolean` | 1.46 | 1.46 | **blocked** - needs `GetTriangle` |
| `LineIntersectsLeaf` | 1.97 | 1.97 | **blocked** - needs `GetTriangle` |
| `AABoxCollideWithLeafMoving` | 0.27 | 0.27 | **blocked** - needs `GetTriangle` + index accessors |
| `SphereCollideWithLeafMoving` | 0.15 | 0.15 | **blocked** - same |
| `CacheTree` / `CacheSphere` / `CacheAABox` | 1.47 / 0.73 / 1.14 | unchanged | Echoes-only, no Prime 1 counterpart |

### The four recursive `*Collision` / `*CollisionMoving` traversals: 81-83% -> 100%

The previous source was a modern rewrite: early `return`, `++mTries`, `if (x && ...)` short-circuits.
Prime 1's source is the `bool ret = false;` + nested `if`/`else` shape. Prime 1's shape is right
**except** that Echoes's `COBBTree::CNode::GetLeftNode()`/`GetRightNode()` return `const CNode*`,
not a reference, and retail null-checks both children before recursing. Disassembly of
`AABoxCollision` (`0x802526CC`) confirms it:

```
80252700:  li      r27,0                    ; ret = false
80252704:  lwz     r3,20(r3)                ; this->mTries
80252710:  stw     r0,20(r28)
80252714:  bl      OBBIntersectsBox
8025271c:  beq     802527e4                 ; else -> mMisses += 1
80252724:  stb     r0,76(r29)               ; node.SetHit(true)
80252728:  lbz     r0,60(r29)               ; node.IsLeaf()
80252768:  lwz     r4,64(r29) / cmplwi r4,0 / beq   ; if (node.GetLeftNode())
802527a4:  lwz     r4,68(r29) / cmplwi r4,0 / beq   ; if (node.GetRightNode())
802527e4:  lwz     r3,24(r28) ; stw -> mMisses += 1  ; the else arm
802527f0:  mr      r3,r27                   ; return ret
```

`mMisses` is incremented in the **`else`** arm, not on the early-return path - so the early-return
rewrite was wrong in behaviour, not only in codegen. All four bodies now read

```cpp
  bool ret = false;
  mTries += 1;
  if (obb.OBBIntersectsBox(node.GetOBB())) {
    node.SetHit(true);
    if (node.IsLeaf()) {
      if (AABoxCollideWithLeaf(...)) ret = true;      // or the sphere/moving variant
    } else {
      if (node.GetLeftNode() && AABoxCollision(...)) ret = true;
      if (node.GetRightNode() && AABoxCollision(...)) ret = true;
    }
  } else {
    mMisses += 1;
  }
  return ret;
```

`GetLeftNode()` null-checked and dereferenced is the only Echoes-specific edit; the parameter name
(`aabb`/`dir`/`dOut` -> `box`/`direction`/`time`) is this tree's and is codegen-neutral.

### `LineIntersectsOBBTree(const CNode*, CRayCastInfo&)` - 62.19% -> 100%

Prime 1's source verbatim, with this tree's pointer-based children:

```cpp
  if (!node) {
    return false;
  }
  float t;
  bool ret = false;
  mTries += 1;
  if (node->GetOBB().LineIntersectsBox(info.GetRay(), t) && t < info.GetMagnitude()) {
    if (node->IsLeaf() == true) {
      if (LineIntersectsLeaf(*node->GetLeafData(), info) == true) ret = true;
    } else {
      if (LineIntersectsOBBTree(node->GetLeftNode(), node->GetRightNode(), info) == true) ret = true;
    }
    node->SetHit(true);
  } else {
    mMisses += 1;
  }
  return ret;
```

The previous source was wrong in two measured ways: it returned early so `node->SetHit(true)` was
reached on a different path, and it used `info.GetMagnitude() <= time` where retail branches on
`t >= info.GetMagnitude()` after the box test (`fcmpo cr0,f1,f0 ; bge` -> miss). The null guard is
retail's: `mr. r29,r4 ; bne` at `0x8024fbe4` returns 0 for a null node **before** `mTries` is touched.

### `LineIntersectsOBBTree(const CNode*, const CNode*, CRayCastInfo&)` - 0.70% -> 99.91%

Three edits to Prime 1's source, each read off the disassembly at `0x8024F8A8`:

1. **`mTries += 2` must come before the first box test** (retail: the `beq` for the null `n0` sits
   *after* `stw r0,20(r27)`).
2. **`float t0 = 0.f; float t1 = 0.f;` are explicitly zero-initialised.** Retail loads one 0.0f
   literal (`lfs f0,-17932(r2)`) and stores it to both frame slots. The 1-arg overload's `t` has no
   such store, so this is source, not compiler determinism.
3. **The two hit flags are direct initialisations, not `= false; if (...) = true;`.** Retail holds
   four registers: `r31` = `ret`, `r25`/`r24` = the two hit flags, `r26` = a *shared temporary* for
   the `LineIntersectsBox` return value. With Prime 1's spelling the temp and the flag collapse into
   one register (94.33%); with

   ```cpp
   const bool intersects0 = n0 && n0->GetOBB().LineIntersectsBox(info.GetRay(), t0) == true &&
                            t0 < info.GetMagnitude();
   const bool intersects1 = n1 && n1->GetOBB().LineIntersectsBox(info.GetRay(), t1) == true &&
                            t1 < info.GetMagnitude();
   ```

   the whole prologue matches, including the `stmw r24,16(r1)` register window. `n0 &&` / `n1 &&`
   are Echoes-only (retail `beq` at `0x8024f8ec` and `cmplwi r29,0` at `0x8024f92c`); Prime 1 has no
   such guard. The rest of the body is Prime 1 unchanged, `&nX->GetLeftNode()` becoming `nX->GetLeftNode()`.

Verified by diffing the two disassembly streams as normalised `objdump -d` mnemonics (branch targets
masked): retail 201 lines vs ours 199, and the only differences are the `lfs f0,-17932(r2)` /
`lfs f0,0(0)` relocation form. **The
residual 0.09% is that relocation**, not codegen - the object has to reference the shared 0.0f
literal through `R_PPC_EMB_SDA21`, whose final displacement depends on where the linker places it,
so it cannot be made to match from the source.

### `TransformPlane` - 93.80% -> 99.80%

Prime 1's source with one edit: the `CUnitVector3f` must be built from its three components, not
copied from the vector.

| spelling | score |
|---|---|
| `CUnitVector3f(normal, CUnitVector3f::kN_No)` (was in the tree) | 93.80 |
| `CUnitVector3f(normal.GetX(), normal.GetY(), normal.GetZ())` | **99.80** |
| same, as a named `CUnitVector3f` local | 99.80 |
| `CVector3f::Dot(normal, transformed)` (args swapped) | 99.29 |
| unit vector built *before* the dot | 70.34 |
| `CUnitVector3f(x, y, z, CUnitVector3f::kN_No)` (4-arg) | 93.80 |

That one edit also removes an extra local: the old copy ctor left a fourth `CVector3f` on the frame,
which is why the frame was `-128` against retail's `-112`. What is left is two `lfs` in swapped
order in the tail - retail emits `lfs y, lfs x, fmuls, lfs z` and we emit `lfs y, lfs z, fmuls, lfs x`,
same registers, same stores. That is register scheduling, and four spellings could not move it.

## What is blocked, and why (do not re-try these)

The five leaf-walking functions and the two moving leaf-walking functions cannot be written in this
tree at all: they need `CCollisionPrimitiveData::GetTriangle`, which is **declared at
`include/WorldFormat/CCollisionPrimitiveData.hpp:22-23` and defined nowhere**. There is no
`src/WorldFormat/CCollisionPrimitiveData.cpp` and no unit for it, so a call is an undefined symbol
and `tools/link_gap.py` fails the gate on an undocumented gap.

Retail's disassembly names the callees: `GetTriangle(ushort)` is `fn_80257A14` (called by
`LineIntersectsLeaf`, `0x8024f7d8`) and `GetTriangle(ushort, const CTransform4f*)` is `fn_80257800`
(called by `SphereCollideWithLeaf`, `0x8025224c`, and by the two `*CollisionBoolean` bodies). Their
call shapes match this tree's declarations exactly - `bl fn_80257A14` with
`r3 = &temp, r4 = mTree, r5 = index`, and `bl fn_80257800` with
`r3 = &temp, r4 = mTree, r5 = index, r6 = &xf` - so the declarations are right and only the
definitions are missing.

This is the same blocker already recorded, with the same evidence, in
`docs/goal-notes/progress-prime1-cmetroidareacollider.md` (section "Functions left at 0-2% and why:
`CCollisionPrimitiveData::GetTriangle` is undefined"), which filed
`NEW: progress-cmetroidareacollider | match | WorldFormat/CCollisionPrimitiveData | ...`.
**No new `NEW:` line is filed here - it would be the same item.** When that one lands, this unit's
seven leaf-walking functions (`LineIntersectsLeaf`, `AABoxCollideWithLeaf`, `SphereCollideWithLeaf`,
`AABoxCollisionBoolean`, `SphereCollisionBoolean`, and the two `*CollideWithLeafMoving`) become
writable, and Prime 1's source for them is already known to fit the call shapes.

`AABoxCollideWithLeafMoving` and `SphereCollideWithLeafMoving` need three further accessors that do
not exist on `COBBTree` in this tree and are not declared anywhere:
`GetTriangleVertexIndices`, `GetTriangleEdgeIndices`, `GetVertMaterial`, `GetEdgeMaterial`, plus the
`kMT_NoEdgeCollision` material bit. Those are separate retail functions inside `COBBTree` and are
part of the same carve.

`CacheTree`, `CacheSphere` and `CacheAABox` are Echoes additions (they build the packed
`CCollisionCache` / `CCollisionCacheWriter` that Prime 1 has no equivalent of) and stay as they
were; they are outside this item's remit.

## Lessons worth keeping

- **Echoes's `COBBTree::CNode::GetLeftNode()` returns a pointer, Prime 1's returns a reference, and
  Echoes null-checks both children before recursing.** Copying Prime 1's four traversal bodies
  verbatim would have produced a *behaviourally* different function (it would dereference null) that
  still compiled. The measured diff - `lwz r4,64(r29) ; cmplwi r4,0 ; beq` - is the evidence.
- **`mMisses` is incremented in the `else` arm of the box test, not on an early-return path.** The
  existing early-return rewrite moved the increment into the miss path only for the *first* failure
  reason, which is the same code but a different function shape.
- **When a decomp stores an intermediate `float` that the callee takes by reference, look for the
  `lfs <literal>` + `stfs` pair in the prologue.** `LineIntersectsOBBTree`'s `t0`/`t1` are
  initialised; the 1-arg overload's `t` is not. That difference is the whole of the 0.7% -> 99.91%
  jump.
- **A `bool` written as `= false; if (c) x = true;` is a different register allocation from
  `const bool x = c;`** in this compiler, and it is visible in the saved-register window
  (`stmw r24,16(r1)` vs `stmw r25,20(r1)`). Worth trying before blaming the compiler version.
- **A residual under 1% that is one `R_PPC_EMB_SDA21` displacement is not a source defect.**
  `LineIntersectsOBBTree(left, right)` and `TransformPlane` are both in that state, and both have
  identical instruction streams to retail.
