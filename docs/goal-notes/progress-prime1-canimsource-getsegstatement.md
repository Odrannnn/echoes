# progress-prime1-canimsource-getsegstatement - `Kyoto/Animation/CAnimSource`

Unit: `main/Kyoto/Animation/CAnimSource`, 20 functions, stays `NonMatching`.
**`matched_functions` 13 -> 15.** `All:` 10040 -> 10042 matched functions.
Fuzzy total 30.963% -> 30.970%, linked 4908 -> 4908 (unchanged).

Two files touched, both mine, nothing under `tools/`, `docs/research/` or `build/goal/`:
`include/Kyoto/Animation/CAnimSource.hpp` and `src/Kyoto/Animation/CAnimSource.cpp`.

The item asked for `GetSegStatement`; reaching 100% on it also took **`CalcAverageVelocity`
from 98.54% to 100%**, which the previous run had written off as a register-allocation wall.
Both count.

## Per-function result

| function | before | after |
| --- | --- | --- |
| `GetSegStatement` | 37.16% | **100.00%** |
| `CalcAverageVelocity` | 98.54% | **100.00%** |
| `GetRotation` | 36.00% | 36.90% |
| `GetSegData` | 18.36% | 18.95% |
| `GetOffset` | 44.26% | 44.22% (the only score that fell, 0.04 points) |
| `GetSegStatementSet` | 27.60% | 27.60% (unchanged) |
| `fn_802A2C78` | 0.00% | 0.00% (still blocked, needs inline asm) |

## 1. `GetSegStatement`'s branch shape was the whole 37% -> 86% step

The previous notes recorded ours at 456 bytes against retail's 1084 and called the gap
"the branch shape recovered from the disassembly". It is: retail's function is **not**
`if (HasRotation) {...} if (HasOffset) {...}` with the `SampleRotation`/`SampleVector`
helpers. Retail duplicates the two `Has*` blocks **three times**, once per branch of the
threshold test. `objdump -dr` of 0xd10..0x114c shows the three copies, and each copy is
followed by a `b`/`beq` to a *different* target rather than falling through, which is the
signature of three separate tails:

- 0xd6c `bl HasRotation` ... 0xdec `bl HasOffset` ... 0xe80 `b 1128` (end) - branch taken
  when `1.f - weight < kInterpolationThreshold`, i.e. weight ~ 1, so it samples `nextFrame`.
- 0xe8c `bl HasRotation` ... 0xf0c `bl HasOffset` ... 0xfa0 `b 1128` - the `weight <
  kInterpolationThreshold` case, samples `frame`.
- 0xfa4 `bl HasRotation` -> `bl Slerp` ... 0x1050 `bl HasOffset` -> inline lerp - the general
  case, and the only one that calls anything.

So the threshold test belongs in `GetSegStatement` itself, not in the two helpers. Spelling
it as one `if / else if / else` over `kInterpolationThreshold`, each arm holding both `Has*`
blocks, is what took the function from 456 to exactly retail's 1084 bytes and 37.16% to
85.82%. The helpers still serve `GetSegStatementSet` and `GetSegData`, which is why they stay.

Note the first arm samples **`nextFrame`**, not `frame` - the old `SampleRotation` returned
`b` on that branch, and this preserves that. The second arm samples `frame`.

## 2. `StartForFrame` must be a ternary, not an `if`

This is the change the previous run recorded as a settled result and it is wrong, so it is
worth stating precisely. The old form was:

```cpp
if (mNumFrames - 1 < frame) { frame = mNumFrames - 1; }
return mStorage.get() + frame * (mRotationsPerFrame * 4 + mOffsetsPerFrame * 3);
```

and the notes reported `if (mNumFrames - 1 < frame)` = 98.54% as the best of three spellings
of the *clamp*. That comparison was measuring the wrong thing. The comparison mwcc has to
emit is the same either way; what changes is **which loads it may hoist above the
conditional branch**. Retail's block (0xd78..0xda8) does this:

```
lwz r4,116(r26)   # mNumFrames        <- loads first
mr  r0,r29        # frame
lwz r3,124(r26)   # mOffsetsPerFrame
addi r6,r4,-1     # mNumFrames - 1
lwz r4,120(r26)   # mRotationsPerFrame
mulli r3,r3,3
lwz r5,112(r26)   # mStorage
slwi r4,r4,2
cmplw r6,r29      # <- the compare, after all four loads
add  r3,r4,r3
bge  da8
mr   r0,r6
mullw r4,r3,r0
```

Every load is above the branch. With the `if` statement, mwcc sinks them below it:

```
lwz r3,116(r26); mr r7,r29; addi r0,r3,-1; cmplw r0,r29; bge; mr r7,r0
644: lwz r4,124(r26); lwz r5,120(r26); ...   <- below the branch
```

The `if` gives the loads a side edge they cannot cross; the conditional operator does not.
Written as `(last < frame ? last : frame)` inside the multiply, the same source produces
retail's schedule. Measured, all with the same `GetSegStatement`:

| `StartForFrame` | GetSegStatement | CalcAverageVelocity |
| --- | --- | --- |
| `if` + `frame * (stride)` (old) | 37.16% | 98.54% |
| `if` + `stride * frame` | 85.88% | 98.64% |
| `if` + `(stride) * (ternary)` - **shipped** | 96.72% | **100.00%** |
| hoisting `stride` and `base` into locals before the `if` | 74-76% | 84-88% |

So the previous run's `WALL: CalcAverageVelocity 98.54% - last 1.5% is only register
allocation` was a **spelling artefact, not a wall**. The register allocation it complained
about was the symptom of the branch shape being wrong; fixing the shape fixed the
allocation. Worth remembering generally: "the remaining diff is only register allocation"
is a claim about the *spelling* that should be re-tested against a different spelling
before it is written down as a wall.

Also measured, all worse: hoisting `const uint stride` or `const uint* base = mStorage.get()`
into locals *before* the clamp (74-76%), and writing the clamp as a helper function
`ClampFrame` (77.58%).

## 3. The `Slerp` and `Lerp` arguments must be named locals

The last 3.4 points, in two steps. In the general arm, the address computations for the two
sampled frames are interleaved by mwcc, and retail's are not: it computes the `frame`
pointer completely (`mullw`/`slwi`/`add`/`add`), *then* the `nextFrame` one, then loads them
into r4 and r5 and calls. Passing the two `GetRotation(...)` calls straight into
`Slerp(a, b, weight)` as nested calls gets the pointer arithmetic in the wrong order.

Binding them to named `const CQuaternion&` / `const CVector3f&` locals first forces the
order:

```cpp
const CQuaternion& a = mStorage.GetRotation(channel, frame);
const CQuaternion& b = mStorage.GetRotation(channel, nextFrame);
statement.Set(CAnimMathUtils::Slerp(a, b, weight));
```

`GetSegStatement` 96.72% -> 97.60% with the Slerp locals, -> **100.00%** with the Lerp ones.
This is the same lever the previous run recorded for `GetRotation`/`GetOffset` in edit 5 of
its notes ("the named `const uint offset` is what stops the fold"); it applies to the
sampler arguments too.

After both, `diff` of our `GetSegStatement` against retail's, ignoring addresses, is empty:
**the instruction streams are byte-for-byte identical**, and the function is exactly 1084
bytes.

## The one score that fell

`GetOffset` 44.26% -> 44.22%, from the `StartForFrame` change reaching it. It is one
instruction out of 118 and `report_diff.py` classes it as a signal, not a failure (the unit
is `NonMatching`), and it could not be recovered: 12 further spellings of `GetOffset` were
tried (`channel * 3 + mRotationsPerFrame * 4`, offset-first inside `StartForFrame`, the
offset as a single expression, offset as `uint` not `const uint`) and every one either left
`GetSegStatement` below 100% or pushed `GetOffset` further down (35.4-42.5%). `GetOffset`
is blocked behind `fn_802A2C78` regardless - see below - so its percentage is a signal on a
function that cannot be finished either way.

## Still blocked: the same `fn_802A2C78`

`fn_802A2C78` is retail's `clamp_zero_to_one`, 28 bytes of two `fsel`s that need an inline
`asm` block, which `goal_check.sh` refuses. Five of the unit's seven remaining functions
(`GetOffset`, `GetRotation`, `GetSegStatementSet`, `GetSegData`, and `fn_802A2C78` itself)
call it, so **the unit cannot flip until that is lifted**, and this item could not have
flipped it. The blocker is unchanged from the previous run and is not re-derived here; see
`docs/goal-notes/progress-prime1-canimsource.md`. One update to its note: it says
`fn_802A2C78` sits after `DataSizeInBytes...` in `.text` and so must be defined first in the
file, with the three `namespace {}` helpers moved. `GetSegStatement` no longer uses
`SampleRotation`/`SampleVector`, so two of those three have one caller each left
(`GetSegStatementSet` and `GetSegData`) - whoever lifts this is slightly closer than the
note suggests.

## Codegen notes worth keeping

- **`if` vs `?:` around a load is a codegen difference, not a style one.** mwcc 2.7 will not
  hoist a `lwz` above a conditional branch that came from a statement, but will above one
  that came from a conditional operator. Same semantics, different schedule. This is the
  single most reusable thing found this run.
- mwcc evaluates nested call arguments right-to-left. Retail's `Slerp`/`Lerp` call sites
  have the *first* argument's address computed first, so a named local is needed, not just
  a different operator.

## Verification (all run in this worktree)

```
./tools/decomp_build.sh main/Kyoto/Animation/CAnimSource
  All:  30.97% fuzzy, 23.24% matched, 11.76% linked (10042 / 28465 functions)
  main/Kyoto/Animation/CAnimSource: 58.42% fuzzy, 44.58% matched (15 / 20 functions)
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json  -> exit 0
  matched 10040 -> 10042, linked 4908 -> 4908
  +100%  CalcAverageVelocity__11CAnimSourceFv
  +100%  GetSegStatement__11CAnimSourceCFRC6CSegIdUiUifR13CSegStatement
  1 WORSE (GetOffset 44.26 -> 44.22), a signal in a NonMatching unit, does not fail
sha1sum build/G2ME01/main.dol   -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
sha1sum -c config/G2ME01/build.sha1 -> 87/87 OK (DOL + all 86 RELs)
python3 tools/check_symbol_names.py  -> checked 503 units; 0 declared names are missing
./tools/probe_sources.sh  -> 749 files, 0 failed, 0 errors; LINKED (250 undefined)
python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimSource -> ok
git status: only include/Kyoto/Animation/CAnimSource.hpp and src/Kyoto/Animation/CAnimSource.cpp
asm check from goal_check.sh: no asm added
objdump diff of GetSegStatement vs retail: identical instruction stream, 1084 bytes both
```

`flip_test.sh` was not run: the unit is 15/20 and cannot flip while five functions need
`fn_802A2C78`.
