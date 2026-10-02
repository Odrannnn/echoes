# progress-unit-cmayaspline

`kind: progress`, target `Kyoto/Math/CMayaSpline`, unit stays `NonMatching`.

**24 -> 25 matched functions** in the unit (`build/report.json`, measured after a real
`ninja` rebuild of the object). Whole-tree `matched_functions` **12332 -> 12333**; linked
5863 -> 5863 (unchanged); DOL sha1 `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` unchanged.
`./tools/goal_check.sh build/goal/item.json` -> `PASS progress-unit-cmayaspline`, all 7 checks
ok (gate.sh incl. DOL sha1 + 86 RELs + report diff + docs claims + port probe, matched count
rose, no asm, no judge path touched).

One file changed: `src/Kyoto/Math/CMayaSpline.cpp`, `CMayaSpline::CreateFor` only
(now lines 476-506 with the explanatory comment). No header, no `include/` change.

## What changed and why

`CreateFor` went **81.93% -> 100.00%** (110 instructions, byte-identical). It needed two
independent spelling fixes; both are recorded as a comment above the function.

### 1. `FLT_MAX` must be a literal, not libc's `(*(float*)__float_max)`

`libc/float.h:8-10` is `extern int __float_max[]; #define FLT_MAX (*(float*)__float_max)`, so
`mwcceppc` materialises the *address* of that global and negates at runtime:

```
lis r3,0 / addi r8,r3,0   (&__float_max, reloc)
lfs f2,0(r8) / fneg f1,f2
```

Retail has the negated literal in `.sdata2` (`lbl_8041EBF4` = `ff7fffff`, `lbl_8041EBF8` =
`7f7fffff`) and loads it in one `lfs f1,lbl_8041EBF4`. Spelling the limits as
`-3.40282347e+38f` / `3.40282347e+38f` reproduces that. Side effect: the object no longer
references `__float_max` (it was the only user in this unit).

This is **not a new finding** - the same workaround and the same reasoning are already in
`src/MetroidPrime/PathFinding/CPathFindArea.cpp:19`, `src/Kyoto/Particles/CElementGen.cpp:27`,
`src/MetroidPrime/CSteeringBehaviors.cpp:10` and `src/MetroidPrime/Player/CPlayerVisor.cpp:20`.
It is a general rule: *any* `FLT_MAX` in this tree needs the literal, and `FLT_EPSILON` does
not (it is already a constant - this unit's `EvaluateAt` matched on it).

### 2. One `const float&` argument per local, four locals

`CMayaSplineKnot`'s `flagA`/`flagB` are literal `2` here, so the two `const float&` tangent
arguments are provably unread inside the callee. Passing the literal `0.f` lets mwcceppc emit
`li r6,0 / li r7,0` and drop the temporaries. Retail instead materialises a distinct `0.f`
stack slot for **each** of the four arguments:

```
1st knot: stfs f0,20(r1) / addi r6,r1,20 / addi r7,r1,16 / stfs f0,16(r1)
2nd knot: addi r6,r1,12 / addi r7,r1,8  / stfs f0,12(r1) / stfs f0,8(r1)
```

i.e. four distinct objects, at 20, 16, 12, 8 - MWCC's reverse declaration order, which also
fixes which slot each argument gets. A non-const local is required; `const float zero = 0.f`
folds straight back to `li`.

## Measurements (every number read from `build/report.json` after a real rebuild)

`CreateFor` spellings tried, in the order run:

| spelling | score |
|---|---|
| original (`0.f, 0.f` literals, `-FLT_MAX, FLT_MAX`) | 81.93% |
| omit the two tangent args (use the header defaults) | 81.93% |
| one shared non-const `float zero` for both calls | 77.56% |
| two non-const locals, shared across both calls | 85.16% |
| four non-const locals declared up front | 86.23% |
| four non-const locals, two declared before each call | 88.41% |
| `FLT_MAX` -> literals only | 89.24% |
| four locals (two per call) + `FLT_MAX` literals | **100.00%** |

## Spellings tried and measured elsewhere this run (so the next run skips them)

Harness: `.tmp/opencode/try.py` (patch -> `ninja build/G2ME01/src/Kyoto/Math/CMayaSpline.o`
-> `objdiff-cli report generate` -> per-function table -> restore), `.tmp/opencode/cndiff.py`
(aligned instruction diff of one function against `build/G2ME01/obj/Kyoto/Math/CMayaSpline.o`),
`.tmp/opencode/keep.py` (same but leaves the patch in place for diffing). All under `.tmp/`,
untracked, not part of the change.

**`EvaluateAt` (97.33%)** - with the target now 10 instructions of pure register naming
(`lfs f4,28(r31)` retail vs `lfs f1,28(r31)` ours, and the three uses of that value). MWCC
coalesces the case-2 `min` local into f1 because it is the return register; retail keeps it in
f4 and emits `fmr f1,f4`. Nothing tried this run moved it off the f4/f1 split:

| spelling | score |
|---|---|
| current (`if/else` + `max`/`min` locals) | 97.33% |
| `const float&` for both locals | 97.33% |
| `const float center` | 97.33% |
| non-`const` locals | 97.33% |
| `const float eps = FLT_EPSILON` local | 97.33% |
| `0.f < center` | 97.15% |
| swap `min`/`max` declaration order | 97.24% |
| comma declaration `min, max` | 97.24% |
| no `else`, plain `return min;` | 97.33% |
| `const float clamped = min; return clamped;` | 97.33% |
| `amplitude = min; return amplitude;` | 97.33% |
| drop `case 0: break;` | 95.91% |
| both locals hoisted, no `else` (`if (center <= 0.f) return min;`) | 96.05% |
| no locals, `mMinAmplitude`/`mMaxAmplitude` inline, `const float&` | 95.12% |
| `volatile float center` | 80.09% |

**`FilterLeftIntersections` (43.05%) / `FilterRightIntersections` (70.65%)** - the gap is not
a wrong statement. Retail **inlines the whole of `IsSegmentConstant`** into both (246 vs our
131 instructions in `FilterLeft`), including both `GetTangents` calls and the four stack
`CVector2f` slots; the `atEndpoint` test in the source is already exactly retail's
(`IsEpsilon(time,start) || IsEpsilon(time,end)`, then `start <= time && time <= end` - read
off `0x803285D0+0x60..0x803285D0+0xC4`, with `0.002f` at `.sdata2+0x04` and `0.0f` at
`.sdata2+0x08`). Removing the second `IsEpsilon` makes `FilterLeft` **worse** (40.90%), so the
current source is right. `#pragma inline_max_size(200|400|700|1200)` around the two filters
changed nothing at all - with `-inline deferred,noauto` mwcceppc only inlines functions the
source marks `inline`, and an out-of-line definition in the `.cpp` is never inlined.

**`IsSegmentConstant` (96.86%)** - 140 instructions in both objects; the only differences are
three adjacent-pair scheduler swaps (`stfs f0,32(r1)` / `mulli r30` order, `li r4,0` before or
after `stfs f0,24(r1)`, and `add r5,...`/`add r3,...` order). Pure scheduling; the previous
run's pointer/reference rewritings were all far worse (77-80%), so the current form is best.

**`__ct__11CMayaSplineFRC...vector...iiiff` (94.33%)** - same instruction sequence, but retail's
five stack temporaries sit at 8, 12, 16, 20, 24(r1) and ours at 12, 16, 20, 24, 28: the whole
temp block is 4 bytes higher in a frame of the same size (80 B), and the `less<CMayaSplineKnot>`
byte is loaded early instead of late. Nothing source-level found to shift it.

**`FindIntersections` (96.62%)** - retail's dedup loop folds the bound to `end()-1` and compares
`next <= end-1` in 3 instructions; ours computes `end` and does `next < end` in 4
(`add r3,r31,r0 / addi r0,r3,4 / cmplw`). Same 89-vs-91 instruction count otherwise.

**`FindSegmentExtrema` (97.32%)** - an `r29`/`r30`/`r31` rotation plus interleaving of
`mKnots` loads with the `extrema` store; nothing else.

**`CalculateHermiteCoefficients` (82.19%)** - retail spills six floats to `32(r3)`..`52(r3)`
(the `reserved_vector<CVector2f,4>`'s spare inline capacity) where we keep them in
callee-saved FP registers, so retail's frame is 112 B against our 96 B. A register-allocation
wall, not a statement difference.

**`CalculateTangents` (87.35%), `FindControlPoints` (89.59%), `EvaluateAtUnclamped` (65.58%)**
- not attempted this run beyond reading the diffs. `EvaluateAtUnclamped`'s frame is 112 B in
retail against 80 B in ours (retail keeps `nextKnotIndex`/`segmentKnown`/`points` addressable
on the stack) and retail tests `time <= knots[0].time` where we test `time < ...` - worth a
fresh look, it is the largest single gap left in the unit.

**`fn_8032AE0C` (168 B) and `fn_8032AA78` (176 B)** - still unpairable, as the previous run
found: retail's linker kept only the *unnamed* copies of
`iter_swap<pointer_iterator<CMayaSplineKnot>,...>` and
`uninitialized_copy<pointer_iterator<...>, CMayaSplineKnot*>`, so objdiff has nothing in our
object to pair them against. Ours emits them as **local** (`t`) symbols at `0x2EA4` / `0x3218`
where retail's are global (`T`); the first is byte-identical to retail `fn_8032AA78`. Not
reachable without renaming, which would be a fake.

## Not reached, and why

The unit now emits six functions retail does not (`__dt__` for four `reserved_vector`
instantiations, `resize<reserved_vector<float,8>>`, `__ct__<vector>(const vector&)`,
`sort<float*>`, `reserve<vector<CMayaSplineKnot>>`, `swap<CMayaSplineKnot>`, `__sort3<float>`,
`__insertion_sort<float*>`) and references two retail does not (`Free__7CMemoryFPCv`,
`allocate__rmemory_allocator`), while retail keeps `sort<float*>`,
`resize<reserved_vector<float,8>>` and `__ct__<vector>(const vector&)` undefined here. That
emitted-symbol-set mismatch is why the unit is far from flippable even at 87% fuzzy, and it is
a bigger job than any single function.

WALL: CreateFor 100% - reached, not a wall
WALL: EvaluateAt 97.33% - min/max register split f4 vs f1, MWCC coalesces the returned local into f1; 14 spellings tried this run, none moved it
WALL: IsSegmentConstant 96.86% - three adjacent-pair scheduler swaps, no statement difference left
WALL: FindIntersections 96.62% - loop bound folded to end()-1 by retail, not reproducible from the source
WALL: __ct__11CMayaSplineFRC...vector...iiiff 94.33% - five stack temporaries sit 4 bytes higher in an identically sized frame
WALL: FilterLeftIntersections 43.05% - retail inlines all of IsSegmentConstant; -inline deferred,noauto never inlines an out-of-line definition
WALL: FilterRightIntersections 70.65% - same cause
WALL: CalculateHermiteCoefficients 82.19% - retail spills to the vector's spare inline capacity, 112 B frame vs 96 B
WALL: CalculateTangents 87.35% - not attempted this run
WALL: FindControlPoints 89.59% - r27..r31 rotation
WALL: EvaluateAtUnclamped 65.58% - 112 B vs 80 B frame, different first comparison; needs a fresh reading of retail's cache logic

## Notes for the next run

- **`FLT_MAX` is a trap in this tree.** Any unit still spelling it as `FLT_MAX` is missing
  retail's folded literal. Grep `src/` for `FLT_MAX` and check the object's undefined list for
  `__float_max`; if it is there, the limit has to be written as a literal. `FLT_EPSILON` is
  fine as-is.
- **Two spellings, one function.** `CreateFor` needed the literal *and* four separate
  `const float&` locals. The tell for the second: retail's stack slots are at 20/16 and 12/8 -
  count retail's `stfs`/`addi r6`/`addi r7` triples, and give each argument its own
  non-`const` local declared in the order that reverses into the slot order you see.
- **`-inline deferred,noauto` is the wall for Filter\*.** `rstl`/`Kyoto` helpers defined in a
  `.cpp` are never inlined, whatever `#pragma inline_max_size` says. Getting
  `IsSegmentConstant` inlined means moving its definition into the header (so it is `inline`
  and still gets an out-of-line copy, as retail has) - that touches `include/` and is a bigger
  change than this item.
- `docs/HANDOFF.md` in this worktree is dirty because `./tools/goal_check.sh` runs
  `gate.sh` with `MP_GATE_DOCS_WRITE=1` and rewrote the derived counts (12333 / DOL 10785).
  The driver discards it; it is not part of the change.
