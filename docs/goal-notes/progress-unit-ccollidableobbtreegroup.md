# progress-unit-ccollidableobbtreegroup

## Result

`WorldFormat/CCollidableOBBTreeGroup` **18 -> 21 / 32 functions matched**. Three
functions taken to 100%. `tools/goal_check.sh build/goal/item.json` -> **PASS**
(matched 11705 -> 11708, linked 5698 unchanged, DOL sha1 and all 86 RELs still hold).

## What changed

One file, `src/WorldFormat/CCollidableOBBTreeGroup.cpp`: `push_back` -> `push_back_unsafe`
at the six fill sites in the three `COBBTreeGroup` constructors, plus one extra
`rstl::auto_ptr<COBBTree>` temporary in the `(const CVector3f&, const CVector3f&)` ctor.

Per function (before% -> after%):

| function | before | after |
| --- | --- | --- |
| `__ct__13COBBTreeGroupFR12CInputStream` (552 B) | 70.63% | **100%** |
| `__ct__13COBBTreeGroupFRQ24rstl19auto_ptr<8COBBTree>` (452 B) | 71.35% | **100%** |
| `__ct__13COBBTreeGroupFRC9CVector3fRC9CVector3f` (536 B) | 69.85% | **100%** |

## Why `push_back_unsafe`

This is the measured result, not a guess. `rstl::vector::push_back` in
`include/rstl/vector.hpp:78` expands to a capacity check plus an out-of-line
`reserve` call; `push_back_unsafe` (line 86) is just `construct(mItems + mCount++, in)`.

Retail's fill loops contain **no capacity check at all**. `tools/dis.sh 0x8025429c 0x20`
(the `CInputStream` ctor's push site) reads:

```
802542a0:  lwz     r3,4(r28)      # mCount
802542b0:  slwi    r0,r3,3        # count * sizeof(auto_ptr)
802542b4:  addi    r3,r3,1
802542c0:  add.    r4,r4,r0       # mItems + count
802542cc:  beq                   # the construct() null guard
```

There is no `cmpw r0,r5` against `mCapacity`, no `bl reserve`. Our `push_back`
spelling emitted all of it (`cmpw`/`blt`/`li r4,4`/`bl reserve`), which is exactly the
30% of each ctor that was missing.

`push_back_unsafe` is **not** a work-deleting shortcut, which is the reviewer's rule 7
risk. Every `reserve` that made the unchecked fill legal is still in the source and
still in the object: `mTrees.reserve(obbCount)` before the read loop,
`mAabbs.reserve(mTrees.size())` / `reserve(1)` before the box loop. Only the
redundant per-element re-check went away, and only because retail does not have it.

## Why the extra `auto_ptr` temporary in the CVector3f ctor

Retail (`tools/dis.sh 0x80253e10 0x218`) materialises `BuildOrientedBoundingBoxTree`'s
return value at `0x18(r1)` and then copies it through a **second** `auto_ptr` at
`0x10(r1)` before the store:

```
80253ea0:  lbz     r7,24(r1)     # return temp .mHas
80253ea8:  lwz     r6,28(r1)     # return temp .mItem
80253eac:  stb     r5,24(r1)     # return temp .mHas = false   <- auto_ptr copy steals
80253ec4:  stb     r7,16(r1)     # second temp .mHas
80253ec8:  stw     r6,20(r1)     # second temp .mItem
```

Two live `auto_ptr`s, both with their destructors emitted (`80253ee0` and `80253ef8`).
The explicit `rstl::auto_ptr<COBBTree>(...)` gives exactly that. Measured the
alternatives, all worse or equal:

- named local `newTree`, reserve first: 81.60%
- named local `newTree`, declared before reserve: 69.81%
- named local via direct-init `newTree(...)`: 60.91%
- **explicit temporary (kept): 100%**

## The three remaining ctors are done; what is left

`AABoxCollide...R18CCollisionInfoList` sits at 97.80%, 748 B. The diff is **only** float
register allocation inside the six-`CPlane` construction block - every store target and
every arithmetic relation matches, the compiler just picked a different `f`-register
rotation. Spelled four ways, measured:

| spelling | score |
| --- | --- |
| named `min`/`max` locals bound before the planes | 89.56% |
| negated normals as literals `(-1.f, -0.f, -0.f)` etc. | 82.23% |
| six named `CPlane` locals then a braced array copy | 96.18% |
| normals declared up/forward/right instead of right/forward/up | 97.80% (unchanged) |
| `const CAABox bounds` | 97.80% (unchanged) |
| **original array-brace form (kept)** | **97.80%** |

WALL: AABoxCollide__23CCollidableOBBTreeGroupFRC27CInternalCollisionStructureR18CCollisionInfoList 97.80% - remaining diff is float-register rotation in the CPlane[6] block; five source spellings tried, none changed it.

`CacheTree` is at 1.00%, 400 B, still a stub. Not a spelling problem - it calls
`fn_802585B0`, `fn_80258078` (0x4E8 B) and `fn_80258560`, which live at
`0x80258078`-`0x802585B0`. Those addresses are in an **unclaimed gap**: the nearest
`CCollisionPrimitiveData.cpp` claim ends at `0x80257af8` and the next unit
(`Weapons/CProjectileWeapon.cpp`) starts after them, so no unit in `splits.txt` owns
them and there is no source to decompile them from. `CCollisionCacheWriter`'s own
methods (`ReserveTriangles`, `AddTriangle`, in `src/WorldFormat/CMetroidAreaCollider.cpp:917,925`)
are also still TODO stubs. Recovering `CacheTree` needs that machinery first.

## Two byte-identical functions the unit is not being credited for

Comparing our object's `.text` against the retail object's function by function, two of
the nine unnamed `fn_*` retail functions are already **byte-identical** to functions our
object emits under a different name:

- `fn_802547A4` (68 B) == our `uninitialized_copy<rstl::pointer_iterator<rstl::auto_ptr<COBBTree>,...>>`
- `fn_8025447C` (164 B) == our `__ct<13COBBTreeGroup>__16CFactoryFnReturnFP13COBBTreeGroup`

objdiff pairs by name, so these score 0% and the 21/32 count does not include them.
Renaming them in `config/G2ME01/symbols.txt` would credit them, but I tried it and backed
it out: `python3 tools/check_symbol_names.py` then reports

```
WorldFormat/CCollidableOBBTreeGroup.cpp: __ct<13COBBTreeGroup>__16CFactoryFnReturnFP13COBBTreeGroup
is declared but CCollidableOBBTreeGroup.o does not define it
```

because the retail object carries the old name until the unit is `Matching`, which is the
documented way a rename breaks all 86 REL links. Left `symbols.txt` untouched.

## Gates

- `tools/goal_check.sh build/goal/item.json` -> PASS (target rose 18 -> 21, no asm, no judge path)
- `tools/fast_try.sh WorldFormat/CCollidableOBBTreeGroup` -> 80.04% fuzzy, 70.83% matched code, 21/32
- `python3 tools/check_symbol_names.py` -> 0 declared names missing
- `docs/HANDOFF.md` in the working tree is the judge's own state-block rewrite, not mine.