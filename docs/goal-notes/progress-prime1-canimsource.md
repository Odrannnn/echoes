# progress-prime1-canimsource - `Kyoto/Animation/CAnimSource`

Unit: `main/Kyoto/Animation/CAnimSource`, 20 functions, stays `NonMatching`.
**`matched_functions` 10 -> 13.** `All:` 9950 -> 9953 functions (22.79% -> 22.80% matched);
fuzzy total and `linked` (4896) unchanged.

Two files touched, both mine, nothing under `tools/`, `docs/research/` or `build/goal/`:
`include/Kyoto/Animation/CAnimSource.hpp` and `src/Kyoto/Animation/CAnimSource.cpp`.

## Per-function result

| function | before | after | Prime 1's source |
| --- | --- | --- | --- |
| `GetRotationsAndOffsets` | 92.74% | **100%** | matched after one edit (the allocation argument) |
| `CopyRotationsAndOffsets` | 48.76% | **100%** | matched after rewriting the loops to Prime 1's shape |
| `GetSize` | 56.25% | **100%** | **no Prime 1 counterpart** (Echoes-only); matched by rewriting the `+` chain as an accumulator |
| `CalcAverageVelocity` | 81.91% | 98.54% | matched to 98.54%; last 1.5% is register allocation only (see WALL) |
| `GetOffset` | 40.92% | 44.26% | blocked (needs `fn_802A2C78`) |
| `GetRotation` | 35.34% | 36.00% | blocked (needs `fn_802A2C78`) |
| `GetSegStatement` | 32.99% | 37.16% | **no Prime 1 counterpart**; big gap, see NEW below |
| `GetSegData` | 20.64% | **18.36%** | no Prime 1 counterpart; the only score that fell |
| `fn_802A2C78` | 0.00% | 0.00% | Prime 1 has it as `clamp_zero_to_one` - inline asm, see the blocker |

Only 8 functions' scores moved anywhere in the whole build, all of them in this unit - the
header edit reached no other unit.

## What each edit was, and why

**1. `GetRotationsAndOffsets` (92.74 -> 100).** Ours precomputed a `words` local from the
*member* fields `mRotationsPerFrame`/`mOffsetsPerFrame`, so mwcc had to reload them from the
object (`lwz r3,0xc(r4); lwz r4,0x10(r4)`) after the two `stw`s and passed the reloaded values
to `DataSizeInBytes`. Retail passes the values it already has in registers
(`mr r4,r0` then `bl DataSizeInBytes`). Prime 1's form - `DataSizeInBytes(rotations.size()/numFrames,
offsets.size()/numFrames, numFrames)/4 + 1` written inline in the `rs_new uint[...]` - is what
removes the reload. Size 216 -> 212 bytes, exact.

**2. `CopyRotationsAndOffsets` (48.76 -> 100).** Not a percentage tweak, a shape change. Ours
indexed `rotations[channel * numFrames + frame]`; retail indexes `rotations[frame + rotation]`
where `rotation` is a separate accumulator that steps by `numFrames` while `i` counts
0..rotationsPerFrame (`add r4,r31,r10; slwi r0,r4,4`, one `add`+`slwi` per channel). Prime 1's
nested-loop spelling - `for (int rotation = 0; i < rotationsPerFrame; rotation += numFrames, i++)`
and `for (int offset = 0; offset < offsetsPerFrame; offset++, i += numFrames)` - reproduces it
byte for byte, including the unrolled-by-4 inner body (848 -> 616 bytes, exact). `numFrames` had
to become `const uint` and the parameter `buffer` -> `buf` only for signature fidelity; neither
changes the code.

**3. `CalcAverageVelocity` (81.91 -> 98.54).** Two changes, both from Prime 1:
`1.f / mDuration.GetSeconds()` hoisted into `invDuration` and `distance *= invDuration` at the end
instead of `mAverageVelocity = distance / mDuration.GetSeconds()`. Retail has one `fdivs` at the
top and one `fmuls` at the bottom, which is also why retail keeps four FP registers (f28 distance,
f29 invDuration, f30 the 0.0f of `close_enough`, f31 the epsilon) against our three - and that
alone moved the frame size from 372 to retail's 396.

**4. `GetSize` (56.25 -> 100).** Echoes-only, no Prime 1 reference, so this came from the
disassembly. Retail computes `addi r31,r5,0x84` first and then three `+=`, i.e. it starts the
accumulator at `sizeof(CAnimSource)` and adds the three channel-map sizes, and only then calls
`GetFrameSizeInBytes` and adds `mFrameCount * result`. Ours was one `return` expression and mwcc
folded it as `a + b + c + (mFrameCount*frameSize) + 0x84`, needing r28/r29 and a 0x20 frame
instead of 0x10. Rewriting as `uint totalSize = sizeof(CAnimSource); totalSize += ...;` is exact.
The member offsets it reads are 0x1c/0x2c/0x3c (the three channel-map `size()`s), 0x10
(`mFrameCount`) and 0x6c (`&mStorage`, for the `GetFrameSizeInBytes` call).

**5. `include/Kyoto/Animation/CAnimSource.hpp` - `RotationAndOffsetStorage`'s accessors.** This
is an Echoes addition; Prime 1 has no clamp at all, but its *index arithmetic* is the same code.
Two edits, both load-bearing:
- `GetOffset` / `GetRotation` need the `StartForFrame` result in a named local before the offset is
  added, as in Prime 1. Ours wrote `StartForFrame(frame) + mRotationsPerFrame * 4 + channel * 3`
  in one expression, and mwcc folded it to `channel * 12`; retail computes `channel * 3` once
  (`mulli r31,r0,3`, hoisted out of the loop in `CalcAverageVelocity`) and scales the sum later
  (`slwi r5,r3,2`). The named `const uint offset` is what stops the fold.
- `StartForFrame`'s clamp must compare against `mNumFrames - 1`, not `mNumFrames`. Retail hoists
  `subi r6,r3,-1` once and shares it across both clamps in a loop body (`cmplw r6,r5; bge +8`),
  where `if (frame >= mNumFrames)` makes mwcc compare the raw `mNumFrames` and subtract afterwards.
  Measured: `if (mNumFrames - 1 < frame)` = 98.54%, `if (frame > mNumFrames - 1)` = 98.28% (wrong
  branch polarity), `if (frame >= mNumFrames)` = 92.46%.
  Behaviour note: with `mNumFrames == 0` the new form no longer clamps (the old one clamped to
  `0xFFFFFFFF`). That is retail's own behaviour and `mNumFrames == 0` cannot reach these paths.

## The blocker: `fn_802A2C78` is retail's `clamp_zero_to_one`, written with inline asm

`fn_802A2C78` is 28 bytes at the very end of `.text`:

```
lfs  f0, zero ; lfs f2, one
fsel f0,f1,f1,f0 ; fsubs f1,f1,f2 ; fsel f0,f1,f2,f0 ; fmr f1,f0 ; blr
```

That is exactly Prime 1's `static float clamp_zero_to_one(register const float v)`, and Echoes'
retail object has only those two `fsel` instructions in the whole unit - the primitive is never
inlined. `objdump -dr` shows **four `bl fn_802A2C78` call sites**: 0xe8 (`GetSegData`), 0xad4
(`GetSegStatementSet`), 0x1394 (`GetRotation`), 0x1508 (`GetOffset`).

So five of the seven remaining functions are blocked behind a file-static function that cannot be
written without an `asm` block, and `goal_check.sh` fails any item whose added lines contain
`\basm\b` or `__asm`:

```
git diff -U0 HEAD -- src include | grep -E '^\+' | ... | grep -nE '\basm\b|__asm'
```

I did not work around it. `include/Kyoto/Math/CMath.hpp` already has `CMath::FastFSel`, an
`asm`-based float select, and calling it would put no `asm` in my diff - but a caller would still
not produce a *named out-of-line* `fn_802A2C78` with a `bl` to it, so it would not match these
functions anyway, and reaching for it reads as buying bytes rather than decompiling.

Also note for whoever lifts this: `fn_802A2C78` sits immediately after
`DataSizeInBytes__24RotationAndOffsetStorageFUiUiUi` in `.text`. mwcceppc emits definitions in
reverse source order, so it must be the **first** function defined in the file, and our three
`namespace {}` helpers (`GetFrameAndWeight`, `SampleRotation`, `SampleVector`) currently occupy
exactly that slot after it. The unit cannot flip until they are gone or inlined.

## Codegen notes worth keeping

- mwcc 2.7 turns `static_cast<uint>(someFloat)` into a `bl __cvt_fp2unsigned` and then converts
  the result back out of a *double* holding `2^52 + value`: `lis r0,0x4330; stw r0,8(r1)` (176.0f,
  which is `2^52`'s high word) followed by `stw <frame>,0xc(r1)`, then `lfd f1,8(r1); fsubs f1,f1,
  lbl_8041E358@sda21`. That is the `0x43300000` word in `.sdata2`. Reading a decompilation that
  does not have it makes the float arithmetic look impossible; it is just mwcc's float-to-integer
  idiom.
- `close_enough(float, float)` is `fabs(a-b) < Real32::Epsilon()` with
  `Real32::Epsilon() == 1e-5f` (0x3727c5ac) - confirmed against `.sdata2+4` in the retail object.
  The only real `.sdata2` difference left in this unit is that we materialise `1e-4f`
  (`vector3_epsilon()`) at `+0xc` where retail has `0.0f`.

WALL: CalcAverageVelocity 98.54% - last 1.5% is only register allocation (r4/r5 for the clamped
`frame-1`, and mwcc 2.7 spills to r9 where 1.3.2 reused the now-dead r5); the whole instruction
sequence is otherwise identical, and `if (frame >= mNumFrames)`, `int channel` vs `uint channel`,
`const uint prev = frame - 1`, and named `&` temporaries for the two `GetOffset` results were all
tried and none changed it (the temporaries made it *worse*, 93.69%).

NEW: progress-prime1-canimsource-getsegstatement | progress | Kyoto/Animation/CAnimSource | the
only remaining function in this unit that does not call fn_802A2C78, so the only one still
reachable without inline asm: Echoes-only (no Prime 1 counterpart), currently 37.16%, ours 456
bytes against retail's 1084, so it needs the branch shape recovered from the disassembly.

## Verification (all run in this worktree)

```
./tools/decomp_build.sh main/Kyoto/Animation/CAnimSource
  All:  30.63% fuzzy, 22.80% matched, 11.74% linked (9953 / 28465 functions)
  main/Kyoto/Animation/CAnimSource: 49.05% fuzzy, 24.92% matched (13 / 20 functions)
python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json   -> exit 0
  matched 9950 -> 9953   linked 4896 -> 4896 ; 1 drop, in a unit that was not Matching
python3 tools/check_symbol_names.py   -> checked 503 units; 0 declared names are missing
./tools/probe_sources.sh              -> 749 files, 0 failed, 0 errors; LINKED (250 undefined)
sha1 build/G2ME01/main.dol            -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
config/G2ME01/build.sha1              -> 87/87 files match (DOL + all 86 RELs)
python3 tools/check_decl_order.py --unit Kyoto/Animation/CAnimSource -> ok
git status: only include/Kyoto/Animation/CAnimSource.hpp and src/Kyoto/Animation/CAnimSource.cpp
asm check from goal_check.sh: no asm added
```

`flip_test.sh` was not run: the unit is 13/20 and cannot flip while five functions need
`fn_802A2C78`.
