# progress-cvector3d — `Kyoto/Math/CVector3d`

`kind: match`, target `Kyoto/Math/CVector3d`. **The target was already complete when the item was
queued, and the reason's premise is false.** `tools/goal_check.sh build/goal/item.json` → **PASS**.

The only source change is a comment in `include/Kyoto/Math/CPlane.hpp` recording a constraint that
this item's reason would otherwise have broken. No function body, layout or declaration changed.

## Measured position at HEAD (`391c706`, re-measured, not recalled)

`configure.py:864` already reads `Object(Matching, "Kyoto/Math/CVector3d.cpp")`, and
`build/report.json` for `main/Kyoto/Math/CVector3d` reads `fuzzy_match_percent 100.0`,
`matched_functions 13 / 13`, `metadata.complete: true`. `./tools/flip_test.sh Kyoto/Math/CVector3d.cpp`
→ `PASS -> kept as Matching`. The reason was filed by `progress-prime1-cmetroidareacollider` before
its own commit (`4a3bd23`) had landed the `include/Kyoto/Math/CVector3d.hpp` operator declarations
that finished the unit, so the item describes a state that no longer exists.

## The premise is false: `CPlane::GetHeight`'s operand order is already right

`include/Kyoto/Math/CPlane.hpp:23` is `CVector3f::Dot(GetNormal(), pos) - GetConstant()`.
Decisive experiment — change it to `Dot(pos, GetNormal())`, rebuild, measure, revert:

| measure | before | after |
|---|---|---|
| `CPlane::GetClosestPoint` | 100.0 % | **95.71 %** |
| `main/Kyoto/Math/CPlane` | 100.0 %, 4/4, complete | 99.318 %, 3/4 |
| `All:` matched | 10012 | 10010 |

```
$ ./tools/decomp_build.sh Kyoto/Math/CPlane
[1/2] CHECK config/G2ME01/build.sha1
FAILED: [code=1] build/G2ME01/ok
build/G2ME01/main.dol: FAILED
86 files OK
WARNING: 1 computed checksum(s) did NOT match
```

Reverted; `sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.

## Why the inference does not hold: the order is per-callsite scheduling, not source order

`build/G2ME01/main.elf` is **our linked** DOL, so for a `Matching` unit `tools/dis.sh` shows our
bytes, which equal retail. Every site where retail inlines `GetHeight` was disassembled:

| inlined site | addr / size | first multiply | order | our score |
|---|---|---|---|---|
| `CPlane::GetClosestPoint` | 0x802F7728 / 0x54 | `fmuls f0,f3,f6` (f3=normal.y, f6=point.y) | **normal-first** | 100.0 % |
| `Buckets::Insert` | 0x8027249C / 0xF8 | `fmuls f0,f1,f0` (f1=4(r7)=normal.y, f0=4(r3)=pos.y) | **normal-first** | 81.40 % |
| `CFrustumPlanes::SphereInFrustumPlanes` | 0x803022F8 / 0x6C | `fmuls f2,f1,f7` (f1=normal.y, f7=center.y) | **normal-first** | 100.0 % |
| `CFluidPlaneCPU::ClipPolygonToPlane` | 0x8013399C / 0x1B8 | `fmuls f0,f10,f5` (f10=4(r9)=a.y, f5=4(r4)=normal.y) | **point-first** | 0.00 % |

The first three confirm the header. The fourth is the observation the reason was built on — and it is
**point-first from the same header**, and our build already emits the identical instruction there.
So GCC 2.7 chose the `fmuls` operand order per inlining site; it is not a property of the source
expression and cannot be read off one function. `CPlane::ClipLineSegment` (0x802F777C) genuinely
*is* point-first in retail (`fmuls f0,f3,f2`, f3=start.y, f2=normal.y), but it is spelled
separately in `src/Kyoto/Math/CPlane.cpp:12` and is already byte-exact; its order is not the header's.

`Buckets::Insert` was diffed instruction-by-instruction against retail (`.tmp/opencode/insdiff.py`,
`objdump -d` on `build/G2ME01/obj/MetaRender/CCubeRenderer.o` vs `build/G2ME01/src/…`): the
`GetHeight` multiply and both `fmadds` are identical. The 81.40 % is 19 instructions of frame and
register allocation (retail: 80-byte frame, `sData` in callee-saved `r31`, `dcbtct` push-back tail;
ours: 64-byte frame, volatile `r10`/`r11`/`r12`, `xxsel` instead of `xsmaddmsp`).

The remaining `GetHeight` callers were checked and are **not** operand-order problems either:
`CMetroidAreaCollider::ConvexPolyCollision` 96.66 % (already a documented allocation-only wall),
`CDecal::BuildClippedGeometry` 59.01 % (799 retail vs 606 ours instructions),
`CCubeMaterial::EnsureViewDepStateCached` 84.74 % (518 diff lines; the whole function differs).

## Verification

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10012 -> 10012   linked 4896 -> 4896
  ok    check_symbol_names.py
  ok    All:  30.83% fuzzy, 23.11% matched, 11.74% linked (10012 / 28465 functions)
  ok    flip_test Kyoto/Math/CVector3d.cpp: PASS, Object(Matching) in configure.py
goal_check: PASS progress-cvector3d
```

## Lead not filed, and why

`CFluidPlaneCPU::ClipPolygonToPlane` is 440 bytes at **0.00 %** and the whole function is
miscompiled, not the dot product: 182 of our instructions against retail's 110, a 192-byte frame
against 48, ten saved FPRs (`f22`–`f31`) against two, a `divw` from `polygon[(i + 1) % size]`
against retail's branchless `subf`/`andc` clamp, and `lfs f4,0(0)` for the `0.f` literal where
retail has an SDA2 constant at `-25492(r2)`. That is a plausible full-function job, but I did not
measure it above 0 %, so it is not filed as `NEW:` (success would have to mean 100 % to raise the
count). Recorded here so the next run does not rediscover it.

## New queue items

NEW: progress-buckets-insert | progress | MetaRender/CCubeRenderer | Buckets::Insert is 81.40% and the remaining 19-instruction delta is measured and is not the CPlane::GetHeight operand order - retail uses an 80-byte frame, holds sData in callee-saved r31 and ends in the dcbtct push-back tail, ours uses a 64-byte frame with volatile r10/r11/r12 and xxsel instead of xsmaddmsp; InsertPlaneObject is 79.30% and Clear 78.62% in the same class
