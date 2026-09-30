# CCollidableSphere (progress, Collision/CCollidableSphere)

## Result

**Both unmatched functions matched. The unit is 17/17 and 100.00% fuzzy, and it still cannot flip**
(`.rodata` is 6 bytes short and `.data` 32 over — both pre-existing, see `unit_fit` below).

| | before | after |
|---|---|---|
| unit fuzzy | 97.84% | **100.00%** |
| unit matched code | 56.82% | **100.00%** |
| **matched functions** | **15 / 17** | **17 / 17** |

```
   95.55%   1652 B  Collide::Sphere_AABox(...)         ->  100.00%
   93.33%    552 B  Collide::Sphere_Sphere(...)        ->  100.00%
```

Tree: `matched 10389 -> 10393, linked 5048 -> 5048`, `All: 31.56% fuzzy, 24.13% matched, 11.83% linked
(10393 / 28465)`. `report_diff.py`: **+4 functions at 100%, 0 units newly linked, no regression** — the
two extras are `CollisionUtil::FilterByClosestNormal` and `CollisionUtil::FilterOutBackfaces`, which
were short of the same thing.

Prime 1's source needed no edits. `diff` of `prime-ref/src/Collision/CCollidableSphere.cpp` against
ours shows only the expected fork differences (`CCollisionInfo`'s extra `0xffff` argument,
`CInternalRayCastStructure::GetRay()` instead of `GetStart()`/`GetNormal()`, `mSphere` for
`x10_sphere`, the `~CCollidableSphere()` definition) — and **neither of the two functions this item
targets differs from Prime 1 at all**. The gap was never the source text; it was one missing
out-of-line function.

## What I changed

Two files, 29 lines. No `configure.py`, no carve, no asm, no `splits.txt`.

1. **`include/Collision/CCollisionInfo.hpp`** — declare retail's out-of-line copy of one
   `CCollisionInfo` under the name retail's map gives it, and route `rstl::construct` for this type
   through it:

   ```cpp
   extern "C" void fn_800D042C(CCollisionInfo* self, const CCollisionInfo& other);
   ...
   namespace rstl {
   inline void construct_impl(void* dest, const CCollisionInfo& src) {
     fn_800D042C(static_cast< CCollisionInfo* >(dest), src);
   }
   }
   ```

2. **`src/Collision/CCollidableSphere.cpp`** — write the helper out: `*self = other`.

`src/Collision/CCollisionInfo.cpp` is the natural home and is unusable: it is
`Object(MatchingFor("G2ME01"), ...)` and 6/6 at 100%, so a second definition there changes
`main.dol` and the build stops hashing to `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. Writing it in
the first unit that needs it is the `fn_80143CD4`-in-`CGameState.cpp` pattern.

### Why `fn_800D042C` is retail's `rstl::construct` and not one unit's accident

Exactly the six units that reach `CCollisionInfoList::Add` reference it in their retail-derived
object — `Collision/CCollidableSphere`, `Collision/CollisionUtil`, `MetroidPrime/CGameCollision`,
`MetroidPrime/CGroundMovement`, `WorldFormat/CCollidableOBBTree`,
`WorldFormat/CMetroidAreaCollider` — and no other object does. Both `Add` call sites in this unit
end in `bl fn_800D042C` with the destination in `r3` and the source in `r4`.

The retail-derived object carries it as an **undefined** symbol, so objdiff compares the *name* in the
instruction, not an address. That is what forces the shim's name. Its bytes in the DOL
(`tools/dol_read.py 0x800D042C 0x64`, and `bl 0x800d042c` confirmed by disassembling the DOL at
`0x80288D6C`) are 12 `lfd`/`stfd` 8-byte copies plus `blr` — a 0x60-byte blit. `*self = other` does
**not** produce those bytes: mwcceppc knows the member types and emits 44 `lwz`/`stw` word moves.
So the shim is behaviourally right and byte-wrong, and this unit does not depend on it being
byte-wrong — see the `NEW:` line at the end.

## The actual bug: a placement `new` leaves a null test behind

`rstl::construct_impl` is `new (dest) T(src)`. mwcceppc 2.7 expands that to *call `operator new`,
test the result against null, then construct*, and **the test survives inlining** — you can see it in
the object:

```
00000c00 <construct_impl<14CCollisionInfo>__4rstlFPvRC14CCollisionInfo>:
     c08:  28 03 00 00   cmplwi  r3,0
     c10:  41 82 00 08   beq     c18
     c14:  48 00 00 01   bl      __ct__14CCollisionInfoFRC14CCollisionInfo
```

Retail's inlined `CCollisionInfoList::Add` has no such test — it ends in a bare `bl` — because its
copy is a call to a function the compiler has no body for. Routing `construct` through a declared
`extern "C"` helper removes the test *and* is what makes `Add` small enough to inline at all. With
the test in place, `Add` is left outlined (a 44-byte weak `Add` plus a 72-byte `push_back` in the
object) and both target functions sit 3-6 instructions short.

## Spellings measured, all on this unit with `tools/fast_try.sh`

Baseline `Sphere_Sphere` 93.33% / `Sphere_AABox` 95.55%.

| spelling | result |
|---|---|
| `#pragma inline_max_size(126…300)` | no change; `Add` still outlined |
| `#pragma inline_max_size(350/400)` | `CastRayInternal` 100 -> **55.18**; targets unmoved |
| `#pragma inline_max_size(450…600)` | targets **fall** to 75.67 / 63.19; `CastRayInternal` 55.18 |
| `#pragma inline_max_size(0)` | unit collapses to 6.72% fuzzy, 3/17 (0 disables inlining) |
| `#pragma inline force` / `forceinline` / `on` / `inline_ratio(100)` | no effect at all |
| `inline` keyword on `Add` + max_size 125…300 | still 3 call sites; needs ~600, and 600 regresses the others |
| `Add` written as placement new + `++mList.mCount` | `Add`'s body is right but it is still outlined, and placement new adds the `addic.`/`beq` pair |
| `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CCollisionInfo)` | **does not compile** — the macro redeclares `is_trivially_destructible`, which `CCollisionInfo.hpp` already specialises: `Error: struct/union/enum/class tag 'is_trivially_destructible' redefined` |
| the same idea with `construct_impl` written out by hand | compiles; `Add` still outlined — the implicit `operator=` is not an inline candidate |
| declare `CCollisionInfo(const CCollisionInfo&)` out of line, keep `mList.push_back(info)` | 93.33 -> **97.75**, 95.55 -> **98.49**; still one `addic.`/`beq` short. Also needs a definition, and `CCollisionInfo.cpp` is linked |
| declare **both** the copy ctor and `operator=` out of line, `Add` assigns `mList[i] = info` | targets reach **100%**, but `CollideMovingAABox` 100 -> **64.06** and `CollideMovingSphere` 100 -> **72.06** here, and `CCollidableAABox`'s two 100% `CollideMoving*` would regress the same way. Dead end |
| **`rstl::construct_impl` -> declared out-of-line `fn_800D042C`** | **17/17, 100.00%, no regression** |

Two lessons worth keeping: **mwcceppc 2.7's placement `new` emits a null test that inlining does not
remove**, and **`inline_max_size` is not the lever for a class member in a header** — the cost that
put `Add` over the threshold was the callee, and raising the threshold takes unrelated functions with
it.

## `unit_fit` — the unit was not flippable before and is not flippable now

```
                      baseline                          after
.text      claimed 5104  ours 5660  over by 556   claimed 5104  ours 5552  over by 448
.rodata    claimed   24  ours   18  SHORT by 6     (unchanged)
.data      claimed  204  ours  236  over by 32      (unchanged)
extras     6 functions, 312 B                      5 functions, 448 B
```

The extras go from `__ct__14CCollisionInfoFRC14CCollisionInfo`, `push_back<reserved_vector<...>>` and
`Add` to a single `fn_800D042C` (180 B), and `.text` gets 108 bytes closer. The unit was already
unreachable for a flip (`.rodata` short, `.data` over) and still is; `.data`'s 32 bytes are the
`jumptable_803B9180` and `.sdata`'s 4 the `gap_09_804189DC_sdata`, neither of which this item
touches.

## Verified

```
tools/goal_check.sh build/goal/item.json   PASS progress-prime1-ccollidablesphere
  ok  no judge-owned path touched
  ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok  counts: matched 10389 -> 10393   linked 5048 -> 5048
  ok  check_symbol_names.py
  ok  All:  31.56% fuzzy, 24.13% matched, 11.83% linked (10393 / 28465 functions)
  ok  target rose: main/Collision/CCollidableSphere: 15 -> 17 / 17 functions
  ok  no asm added
sha1sum build/G2ME01/main.dol              6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
tools/report_diff.py <judge base> build/report.json
                                           +4 functions at 100%, 0 units newly linked, no regression
python3 tools/check_symbol_names.py        505 units; 0 declared names are missing from their object
```

`docs/HANDOFF.md` is rewritten by the gate's own `check_docs_claims.py --write` (10389 -> 10393,
DOL units 8841 -> 8845); I reverted it, since the driver discards edits to that file and the judge
recomputes those numbers from the tree.

## Open question, filed as `NEW:`

`fn_800D042C` is real code, 0x64 bytes, and it is currently undefined in **six** units' worth of call
sites while sitting unimplemented in a seventh unit's split. Matching it is a separate job and its
spelling is the open question: retail emits `lfd`/`stfd`, and every member-wise spelling tried here
emits `lwz`/`stw`.

NEW: match-cmorphball-fn800d042c | match | MetroidPrime/Player/CMorphBall | fn_800D042C (0x64 B, in this
unit's split, currently unimplemented so its 12 lfd/stfd 8-byte copies + blr are unreproduced) is the
out-of-line CCollisionInfo copy every CCollisionInfoList::Add calls; mwcceppc lowers *self = other to
44 lwz/stw moves, so the spelling that yields retail's lfd/stfd blit is the open question
