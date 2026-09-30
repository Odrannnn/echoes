# progress-prime1-cfbstreamedAnimReader

`kind: progress`, target `Kyoto/Animation/CFBStreamedAnimReader` (unit `main/Kyoto/Animation/CFBStreamedAnimReader`).

## Result

`matched_functions` on the target unit: **7 -> 15 of 51**. Project total
`matched_functions` **9678 -> 9686** (`build/report.json`, base `build/goal/judge/report.base.json`).
Unit `matched_code_percent` 3.97% -> 10.63%, `fuzzy_match_percent` 48.80% -> 53.23%.
The unit stays `NonMatching`; no `flip_test` was run and none is claimed.

Diff is one file: `src/Kyoto/Animation/CFBStreamedAnimReader.cpp` (36 insertions, 23 deletions).
No header, no `configure.py`, no `asm`.

## The eight functions that reached 100%

Per the item's format - before%, after%, and whether Prime 1's source was used unchanged,
needed a small edit, or did not help. All eight are in this file; none needed a header change.

| function | before | after | how |
|---|---|---|---|
| `VClone__21CFBStreamedAnimReaderCFv` | 99.49% | 100% | **Prime 1 unchanged.** Its `return rstl::ownership_transfer<IAnimReader>(rs_new ...)` explicit wrap; ours relied on the implicit conversion. The only remaining diff was register-allocation order in the epilogue. |
| `VHasOffset__21CFBStreamedAnimReaderCFRC6CSegId` | 60.28% | 100% | **Prime 1 unchanged.** `if (index == ~0u) { return false; } return mTotals.Next().HasOffset(index);` instead of `index != ~0u && ...`. The `&&` form made MWCC materialise a `0/1` in a register. |
| `DoIncrement__23CFBStreamedPairOfTotalsFR47CBitLevelLoader<...>` | 90.87% | 100% | **Prime 1 unchanged.** `const CFBStreamedCompression& source = *mSource;` hoisted into a local; that moves the `lwz r5,12(r3)` load above the `mNextSel` branch. |
| `VGetSegStatementSet__..._RC13CCharAnimTime` | 91.55% | 100% | **Prime 1 unchanged.** Iterate with `CSegIdList::const_iterator end = list.end();` instead of reaching into `list.mSegList` directly; the direct form forced an extra `__pp__...::const_iterator` out-of-line call and a different `this` register. |
| `SetReadTime__21CFBStreamedAnimReaderCFRC13CCharAnimTime` | 91.30% | 100% | **Echoes-specific, not Prime 1.** The `min_val` clamp is Echoes' (Prime 1 has none). The fix was to drop the named `CCharAnimTime readTime` local and pass the temporary inline, so the result moves in `r6` instead of via an sret slot at `r1+8`. |
| `__dt__27CFBStreamedAnimReaderTotalsFv` | 92.31% | 100% | **Prime 1 unchanged.** `if (mBuffer != nullptr) { delete[] mBuffer; }` - retail has the `cmplwi r3,0; beq` before `Free`. |
| `VReverseView__21CFBStreamedAnimReaderFRC13CCharAnimTime` | 38.44% | 100% | **Prime 1 unchanged, adapted to Echoes' types.** `SAdvancementResults(CCharAnimTime::ZeroFlat(), SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()))`. Echoes' `SAdvancementResults` also has a 1-arg ctor that we were using; retail only ever calls the 2-arg `__ct__19SAdvancementResultsFRC13CCharAnimTimeRC18SAdvancementDeltas`. |
| `GetValuesPerChannel__27CFBStreamedAnimReaderTotalsCFv` | 0.00% | 100% | **Echoes-specific, not in Prime 1 at all** (Prime 1 has no scale data, so no `GetValuesPerChannel`). Needed two edits: rewrite the two ternaries as `uchar result = 4; if (...) result += 4;` (retail branches, we produced a branchless `neg/or/srawi/and` chain), and `return static_cast<uchar>(result);` for the trailing `clrlwi r3,r4,24`. |

Also raised, but not to 100%: `VAdvanceView` 63.22% -> 78.21% and `VGetAdvancementResults`
59.81% -> 77.33%, from the same 2-arg `SAdvancementResults` / `SAdvancementDeltas` change
(the tail of both was `SAdvancementResults result(t); result.mDeltas.x = ...; return result;`
which compiles to a stack-built object plus three stores; retail passes the deltas to the ctor).

## What did NOT help (measured, so the next run skips it)

- `CFBKeyFrameReductionPerChannel_HeaderForAll::Uint32sForBitCount` in
  `include/Kyoto/Animation/CFBStreamedCompression.hpp`: retail emits a branch
  (`clrlwi. r0,r3,27; srwi r3,r3,5; addi r0,r3,1; bne; mr r0,r3`) where we emit the
  branchless `neg/or/srawi/add`. Rewriting as `(bits >> 5) + ((bits & 31) ? 1 : 0)` changed
  **nothing** (unit stayed 15/51, 50.24% fuzzy). Reverted; the header is untouched.
- Spelling `GetValuesPerChannel`'s return as a plain `return result;` left one instruction
  (`clrlwi r3,r4,24` vs `mr r3,r4`); the explicit `static_cast<uchar>` closed it.
- Hoisting `GetVector`'s offset into a `const uint offset` local in
  `include/Kyoto/Animation/CFBStreamedAnimReader.hpp` to coax retail's
  `add r0,r5,r0; slwi r0,r0,2` form: made the unit *worse* (53.23% -> 53.06% fuzzy).
  Reverted; the header is untouched.
- Prime 1's redundant `if (mNextFrame == mLastFrame) {...} else {...}` with identical arms in
  `CFBFullBodyAspectsForStream::SetTime`: nudged fuzzy 53.23% -> 53.85% but did not move the
  function off 40.07%. Reverted as noise.

## The wall: 17 unnamed retail functions score 0.00% and cannot be matched by name

`report.json` lists 17 functions in this unit that the retail object defines and ours does not,
all at 0.00%: `fn_802B00AC`, `fn_802B013C`, `fn_802B0160`, `fn_802B0170`, `fn_802B0194`,
`fn_802B01B4`, `fn_802B01E4`, `fn_802B0208`, `fn_802B0298`, `fn_802B02F0`, `fn_802B033C`,
`fn_802B0388`, `fn_802B03A4`, `fn_802B03C0`, `fn_802AE15C`, `fn_802AF2B0`, `fn_802AF308`
(2496 bytes, 31.9% of the unit's `total_code`). Reading their disassembly:

- `fn_802B03C0` (52B) = `CFBStreamedCompression::GetAnimationDuration()`, out of line:
  `lwz r4,8(r4); lfs f1,4(r4); bl __ct__13CCharAnimTimeFf`.
- `fn_802B033C` (76B) = `CFBStreamedCompression::HasScaleData()`, out of line, tail-calls
  `HasScaleData__31CFBStreamedPerChannelHeaderListCFv`.
- `fn_802B01B4` (48B) / `fn_802B01E4` (36B) = the `IAnimSourceInfo` virtual overrides
  `TAnimSourceInfo<CFBStreamedCompression>::GetAnimationDuration()` / `::HasScaleData()`;
  ours emits the same two functions under real names
  (`GetAnimationDuration__41TAnimSourceInfo<22CFBStreamedCompression>CFv` etc.).
- `fn_802B02F0` / `fn_802B0298` / `fn_802B0388` / `fn_802B03A4` = out-of-line iterator helpers
  for `TVectorOfVaryingLengthItems<uint, CFBStreamedPerChannelHeader>`.
- `fn_802B013C`/`fn_802B0160`/`fn_802B0170`/`fn_802B0194` = `CFBStreamedAnimReaderTotals`
  per-channel accessors (stride-3 `lhax`/`lbzx` on `mSegIds` and the flag arrays).
- `fn_802B00AC` (144B) = `TAnimSourceInfo<CFBStreamedCompression>::~TAnimSourceInfo`.
- `fn_802AF2B0` (88B) = `CSegIdToIndexConverter::~CSegIdToIndexConverter`, out of line.
- `fn_802AF308` (600B) = `CFBStreamedPairOfTotals::~CFBStreamedPairOfTotals`, out of line.
- `fn_802AE15C` (68B) = `CFBKeyFrameReductionPerChannel_HeaderForAll::FrameAfter(uint)`, out of line.

**Why this is a wall, not a missed trick.** Retail emitted its inline accessors
out of line; MWCC inlines ours. That is a compiler decision, not a source spelling - and even if
we forced an out-of-line copy, objdiff matches by *symbol name*, so our
`GetAnimationDuration__22CFBStreamedCompressionCFv` would not pair with `fn_802B03C0` and both
would stay 0.00%. Reaching them needs either out-of-lining in the shared headers (which moves
`.text` in `CFBStreamedCompression.cpp` and `CAllFormatsAnimSource.cpp`, the other two TUs that
include `CFBStreamedAnimReader.hpp`) **plus** a `config/G2ME01/symbols.txt` rename via
`tools/apply_rename.py` to give the 17 addresses real names. Both steps are needed; the rename
touches every unit that references those addresses.

This also caps the named functions that are near 100%: `VGetTimeRemaining` (90.00%), and the
`__ct__`/`VAdvanceView`/`VGetAdvancementResults` family, all `bl fn_802B03C0` where we inline
`GetAnimationDuration()`. Same for `__dt__21CFBStreamedAnimReaderFv` (91.19%), which calls
`fn_802AF2B0` and stores `lbl_803B9AE0` where we store `__vt__21CFBStreamedAnimReader`; and
`SetTime__27CFBFullBodyAspectsForStream` (40.07%), which calls `fn_802AE15C` (`FrameAfter`).
`HasOffsetData`/`HasScaleData` (54.57% / 30.71%) call `fn_802B0388` + `fn_802B03A4` and inline
ours as `lhzu`/`lhzx` sequences.

WALL: fn_802B03C0 / 17 unnamed retail functions 0.00% - retail keeps CFBStreamedCompression,
CSegIdToIndexConverter and CFBKeyFrameReductionPerChannel_HeaderForAll accessors out of line and
objdiff pairs by name, so they are unreachable without a symbols.txt rename.

## Remaining named functions, best first (all measured after this change)

```
 93.31  308B  VGetOffset                     accessor strength reduction: retail loads the +4 from an
                                               SDA21 constant, we fold it into `addi r3,r3,16`
 91.19  216B  ~CFBStreamedAnimReader         calls fn_802AF2B0; vtable via lbl_803B9AE0
 90.00   80B  VGetTimeRemaining              calls fn_802B03C0
 81.63  548B  CFBStreamedPairOfTotals::SetTime  mNextSel negate (cntlzw/srwi) hoisting
 78.26  604B  CalculateDown                  CMath::Max inlined by retail, out-of-line call for us
 78.21 1280B  VAdvanceView                   improved but still short
 77.33 1296B  VGetAdvancementResults         improved but still short
 75.10  240B  ~CFBStreamedPairOfTotals       register allocation around mSource
 74.78  304B  ~CFBStreamedAnimReaderTotals   store ordering of mHasOffsetData
 58.62  276B  Allocate                      strength reduction of the five size expressions
 54.57  120B  HasOffsetData                 iterator helpers out of line in retail
 46.72  876B  GetSegStatement               Echoes-only (scale branch); no Prime 1 reference
 40.07  432B  CFBFullBodyAspectsForStream::SetTime  calls fn_802AE15C
 37.86  604B  IncrementInto                  LoadUnsigned out of line in retail
 34.58  548B  ~CFBStreamedAnimReader        Echoes-only ctor (poiData arg); no Prime 1 reference
 31.47  144B  ~CFBFullBodyAspectsForStream   calls fn_802AE15C
 30.74 2492B  VGetSegData                   Echoes-only; no Prime 1 reference
 30.71  124B  HasScaleData                  iterator helpers out of line in retail
  0.00  424B  SetToReadStart                GetInitialValue accessors out of line in retail
```

`VGetSegData`, `GetSegStatement` and `~CFBStreamedAnimReader` are Echoes-only (they carry scale
data and a `CAnimPOIData*` that Prime 1 does not have), so Prime 1's source gives no help on them.

## NEW

None. The 17-fn wall above is a naming/harness cost, not work whose success raises a count, and
the remaining named functions need individual codegen experiments rather than a new lane.

## Verification

All measured on this tree after the final edit:

```
$ ./tools/decomp_build.sh
All:  29.91% fuzzy, 21.73% matched, 11.74% linked (9686 / 28465 functions)

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010     (matches AGENTS.md)

$ python3 tools/check_symbol_names.py
checked 502 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py --unit Kyoto/Animation/CFBStreamedAnimReader
ok: 1 unit(s) checked, none emits its functions out of retail order

$ ./tools/probe_sources.sh
probe: 744 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ git status --porcelain          # only this one file
 M src/Kyoto/Animation/CFBStreamedAnimReader.cpp
```

Per-function diff of the whole project, `build/goal/judge/report.base.json` vs `build/report.json`:
**0 functions worse, 0 functions gone, 0 new**, `matched_functions` 9678 -> 9686.
No `asm` in the added lines.

`tools/unit_fit.sh Kyoto/Animation/CFBStreamedAnimReader.cpp` still reports 25 functions ours
emits that the retail object does not (2496 bytes, all template/COMDAT dtor copies and inline
virtuals) and `.data` over by 20 - **identical before and after this change**, so it is not a
regression, but it is why the unit cannot flip yet even with 15/51.

## One repo-wide observation

`report.json` counts a function as matched only at a fuzzy 100%, and this unit's 17 unreachable
functions plus the 9 extra symbols ours emits are 31.9% of its `total_code`. Note also that our
object defines 60 text symbols where retail defines 51: the extra 9 are exactly the out-of-line
copies of retail's `fn_802B*` helpers under real names. A unit cannot be `Matching` while both
directions are non-empty, whatever the percentages say.

---

# Run 2 (lane 6, 2026-09-30) - append, does not replace the run above

## Result

`matched_functions` on `main/Kyoto/Animation/CFBStreamedAnimReader`: **15 -> 17 of 51**.
Project total **10288 -> 10290** (`build/report.json`, base `build/goal/judge/report.base.json`).
Unit `fuzzy_match_percent` 53.23% -> 55.22%, `matched_code_percent` 10.63% -> 15.00%.
The unit stays `NonMatching`; no `flip_test` was run and none is claimed.

Diff is three files, 3 headers/1 cpp, no `configure.py`, no `asm`:

```
 include/Kyoto/Animation/CFBStreamedAnimReader.hpp | 16 +++++++++++++---
 include/Kyoto/Animation/CFBStreamedCompression.hpp |  9 +++++++--
 src/Kyoto/Animation/CFBStreamedAnimReader.cpp       | 30 +++++++++++++++-----
```

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate, counts, symbol names, `All:`,
target rose, no asm).

## Two functions reached 100%

| function | before | after | how |
|---|---|---|---|
| `VGetOffset__21CFBStreamedAnimReaderCFRC6CSegId` | 93.31% | **100%** | `GetVector` now reads its `+4` from a defined symbol instead of a literal (below), and hoists the index into a local. |
| `__ct__27CFBStreamedAnimReaderTotalsFRC22CFBStreamedCompression` | 74.78% | **100%** | `CFBKeyFrameReductionPerChannel_HeaderForAll::Uint32sForBitCount` rewritten as `bits % 32 == 0 ? bits / 32 : bits / 32 + 1`. |

## The finding that moved two functions: a `lbz` from .sdata2 is a *defined* constant

Retail's `GetVector`/`GetScale` (and 12 call sites in `VGetSegData`, 5 in `VAdvanceView`, 1 in
`VGetOffset`) compute the float offset as

```
lbz r5, lbl_8041E3C0@sda21      ; a BYTE constant, value 4
...
mullw r0, r30, r0
add r0, r5, r0                  ; index*stride + 4
slwi r0, r0, 2                  ; * sizeof(float)
```

and our literal `4` compiled to `slwi r3,r0,2; addi r3,r3,0x10` - algebraically the same, one
instruction shorter. `.sdata2` holds **two** 1-byte symbols at 0x8041E3C0/1, both value 4
(retail: `04040000 3f800000 ...`, ours before this change: `3f800000 ...`); that 4-byte lead-in
is 25% of the section and the reason `.sdata2` sat at 82.5%.

A `lbz` off a 1-byte `.sdata2` symbol is what MWCC emits for a **`static const uchar` member
with an out-of-line definition** - the reference is Prime 1's
`CRainSplashGenerator::SSplashLine::skInitialWidth` (`prime-ref/include/...:26` declares it,
`prime-ref/src/MetroidPrime/CRainSplashGenerator.cpp:16` defines it, and retail `lbz`s it in
`__ct__...SRainSplash`). So `CFBStreamedAnimReaderTotals` now has

```cpp
static const uchar skQuatFloats;   // defined in the .cpp as 4
static const uchar skTransFloats;  // defined in the .cpp as 4
```

`GetVector` uses `skQuatFloats`, `GetScale` uses `skQuatFloats + (mHasOffsetData ? skTransFloats
: 0)` - which is exactly retail's two distinct byte symbols. Our `.sdata2` now has retail's
`04040000 3f800000` lead-in and still fits 40/40 bytes. Lesson for the whole project: **a
`.sdata2`/`.sdata` byte constant is a `static const char`-typed symbol, not a literal**, so a
literal and a symbol are not interchangeable even when the value is the same.

## Spellings measured this run (the ones a next run should not repeat)

`Uint32sForBitCount(uint bits)` - retail branches, so the whole ctor missed until the right one:

| spelling | ctor score |
|---|---|
| `bits / 32 + (bits % 32 != 0)` (was in the tree) | 74.78% - branchless `neg/or/srawi/add` |
| `bits / 32 + (bits % 32 ? 1 : 0)` | 74.78% (identical codegen) |
| `bits / 32 + (bits & 31 ? 1 : 0)` | 74.78% (identical codegen) |
| `{ const uint whole = bits / 32; if (bits % 32 != 0) return whole + 1; return whole; }` | 62.95% - `beq` + in-place `addi` |
| `bits % 32 ? bits / 32 + 1 : bits / 32` | 93.29% - right branch, wrong arm order (`beq`+`addi`) |
| `bits / 32 + (bits % 32 ? 1 : 0)` with `whole`/`rest` locals | 93.29% (same) |
| **`bits % 32 == 0 ? bits / 32 : bits / 32 + 1`** | **100%** - `addi r0,r3,1; bne; mr r0,r3` |

The last two rows are the lesson: **for a two-arm select MWCC's block layout follows the arm
order**, so inverting the ternary (and nothing else) is what produced retail's `bne`.

`GetVector`/`GetScale` in `CFBStreamedAnimReader.hpp`:

- `mComputedFloats + index * mValuesPerChannel + skQuatFloats` (no local): `VGetOffset`
  **89.42%** - the `lbz` appears but `slwi`/`addi` are still distributed.
  `const uint offset = index * mValuesPerChannel + skQuatFloats; return *(...)(mComputedFloats +
  offset);` -> **100%**.
- `GetScale` as `uint offset = skQuatFloats + (mHasOffsetData ? skTransFloats : 0)`: MWCC
  if-converts it to `neg/or/srawi` (branchless), `GetSegStatement` 51.08%.
- `GetScale` as `const uint offset = index * mValuesPerChannel + skQuatFloats +
  (mHasOffsetData ? skTransFloats : 0)`: `GetSegStatement` **43.65%** (worse - the index multiply
  moves after the select). `uint offset = skQuatFloats; if (mHasOffsetData) { offset +=
  skTransFloats; }` -> **51.82%**, and it is the form that emits retail's `cmplwi/beq/lbz/add`.

`Allocate` (58.62% -> **75.64%**), three independent fixes, all visible in retail's disassembly:

- `floatsSize` is rounded up like every other sub-buffer: retail computes `4 - (floats & 3)`, we
  had a hard `+ 4`.
- the three flag arrays are summed as `flagsSize + flagsSize + flagsSize`, not `flagsSize * 3`;
  retail builds the sum in a register with two `add`s and a rematerialised copy.
- the five member pointers come from `mBuffer + offset` with a **running `uint offset`**, not
  chained through the previous member. Chaining gives 69.64%; writing each as
  `mBuffer + shortsSize + flagsSize + flagsSize + flagsSize` gives **67.09%** (worse - MWCC
  duplicates the sum and pushes `this` out of a volatile register).

Still not 100%: only register allocation is left (`stmw r26`/`lmw r26` over 6 saved registers vs
our 4 individual `stw`/`lwz`, and the `add` operand order). Not reachable from the source.

## The run-1 WALL is wrong - `fn_` names DO pair

Run 1 recorded:

> WALL: fn_802B03C0 / 17 unnamed retail functions 0.00% - ... objdiff pairs by name, so they are
> unreachable without a symbols.txt rename.

The premise is false, measured on this tree:

```
$ python3 -c "...count report entries named fn_* ..."
retail fn_ entries: 16625 with score: 1216
```

1216 of them carry a fuzzy score and 121 are at **100%** - e.g.
`main/MetroidPrime/main :: fn_80009864` 100.0%, `main/MetroidPrime/Player/CGameStateBlockDtor ::
fn_80004A4C` 100.0%. This repo already uses `fn_XXXXXXXX` as real C++ function names
(`src/MetroidPrime/CModelDataModelSlots.cpp:103`, 484 files use the prefix). So the 17 functions are
**not** blocked by a naming/harness problem: they are reachable by declaring the out-of-line
copies MWCC currently inlines under the *same* `fn_` names, so the symbol objdiff pairs with
matches and the callers' `bl` becomes the same relocation.

Identified from the retail disassembly, in retail address order:

| retail symbol | body |
|---|---|
| `fn_802AE15C` (68B) | `CFBKeyFrameReductionPerChannel_HeaderForAll::FrameAfter(uint)` |
| `fn_802AF2B0` (88B) | `~CSegIdToIndexConverter` |
| `fn_802AF308` (600B) | `~CFBStreamedPairOfTotals` |
| `fn_802B00AC` (144B) | `~TAnimSourceInfo<CFBStreamedCompression>` |
| `fn_802B013C` (36B), `fn_802B0160` (16B), `fn_802B0170` (36B), `fn_802B0194` (32B) | `CFBStreamedAnimReaderTotals` per-channel accessors (stride-3 `lhax`/`lbzx` over `mSegIds` and the flag arrays) |
| `fn_802B01B4` (48B), `fn_802B01E4` (36B) | the `IAnimSourceInfo` overrides `TAnimSourceInfo<CFBStreamedCompression>::GetAnimationDuration()` / `::HasScaleData()` |
| `fn_802B0208` (144B) | the out-of-line `CFBStreamedCompression` accessor set |
| `fn_802B0298` (88B), `fn_802B02F0` (76B) | out-of-line helpers of `TVectorOfVaryingLengthItems<uint, CFBStreamedPerChannelHeader>` |
| `fn_802B033C` (76B) | `CFBStreamedCompression::HasScaleData()`, tail-calls `fn_802B0388`+`fn_802B03A4` |
| `fn_802B0388` (28B), `fn_802B03A4` (28B) | that vector's `const_iterator::operator*` and `operator++`; retail calls `fn_802B03A4` twice per loop iteration |
| `fn_802B03C0` (52B) | `CFBStreamedCompression::GetAnimationDuration()` |

`HasOffsetData` shows the shape: retail's loop is `bl fn_802B0388; bl fn_802B03A4; bl fn_802B03A4`
where ours calls `__pp__Q261TVectorOfVaryingLengthItems<Ui,27CFBStreamedPerChannelHeader>14const_iteratorFv`
and inlines `operator[]`. So the out-of-lining has to be in
`rstl`/`CFBStreamedCompression.hpp`, which `CAllFormatsAnimSource.cpp` also includes - **that
unit is `Matching` (11/11)**, so check its `.text` did not move before keeping any such change.
Not attempted this run: it is a shared-header refactor, not a spelling.

The rest of this unit is capped by the same thing. Callers of `fn_*`, with today's score:
`VGetTimeRemaining` 90.00% (`fn_802B03C0`), `__dt__21CFBStreamedAnimReader` 91.19%
(`fn_802AF2B0`), `SetTime__23CFBStreamedPairOfTotals` 87.08% (`fn_802B02F0`),
`__ct__23CFBStreamedPairOfTotals` 75.10%, `SetTime__27CFBFullBodyAspectsForStream` 40.07% and
`__ct__27CFBFullBodyAspectsForStream` 31.47% (`fn_802AE15C`), `IncrementInto` 37.08%
(`fn_802B0160/0194/0208/0298/0388/03A4`), `HasOffsetData` 54.57% / `HasScaleData` 30.71%
(`fn_802B0388`+`fn_802B03A4`), `__ct__21CFBStreamedAnimReader` 39.10%,
`VAdvanceView` 78.21% / `VGetAdvancementResults` 77.33% (`fn_802B03C0`).

## Still open, and not yet measured here

- `VGetSegData__...RC13CCharAnimTime` 2492B, 30.74% -> 32.33%: the largest single function and it
  calls **no** `fn_*`, so it is not capped. Its diff is register allocation plus 176 bytes of
  structure we do not emit. Worth its own item.
- `CalculateDown` 78.26%: retail converts `values[1..3]` to float with **`psq_l f0, 0x2(r31)`**
  (a paired single load), while we emit the `lha` / `xoris 0x8000` / `stw` / `lfd` idiom that
  retail itself also uses for the *offset* branch. Same function, two conversions: this is a
  register-pressure difference inside the loop, and no source spelling was found for it.
- `SetTime__27CFBFullBodyAspectsForStream` 40.07%: retail's Prime-1 `if (mNextFrame == mLastFrame)
  {...} else {...}` with identical arms still did not move it (run 1 measured that); do not spend
  a run on it again.

## Regression inside the unit (a signal, not a failure)

`IncrementInto` 37.86% -> 37.08%. `tools/report_diff.py` prints it as `WORSE` and passes,
because it is in a unit that was not `Matching` in the baseline; the judge passed. The `Uint32sForBitCount`
sites inside it now match exactly (`clrlwi./srwi/addi/bne/mr r0`); the 0.78% is a different
`addi r0, r3, 0x8` in the loop.

## Verification (all on this tree, after the last edit)

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10288 -> 10290   linked 5043 -> 5043
  ok    check_symbol_names.py
  ok    All:  31.27% fuzzy, 23.62% matched, 11.83% linked (10290 / 28465 functions)
  ok    target rose: main/Kyoto/Animation/CFBStreamedAnimReader: 15 -> 17 / 51 functions
  ok    no asm added
goal_check: PASS progress-prime1-cfbstreamedanimreader

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

$ ./tools/probe_sources.sh
probe: 750 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ python3 tools/check_decl_order.py --unit Kyoto/Animation/CFBStreamedAnimReader
ok: 1 unit(s) checked, none emits its functions out of retail order

$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched 10288 -> 10290   linked 5043 -> 5043   (+2 functions at 100%, 0 units newly linked)
no regression
```

`tools/unit_fit.sh Kyoto/Animation/CFBStreamedAnimReader.cpp`: `.text` 13788 vs 14000 claimed
(SHORT by 212, was 236), `.sdata2` 40/40 **fits**, `.data` still over by 20, and the same 25
extra symbols (2496 bytes of template/COMDAT dtor copies and inlined virtuals) as the baseline.
The unit still cannot flip; this item does not claim otherwise.

## NEW

NEW: progress-prime1-cfbstreamedanimreader-outofline | progress | Kyoto/Animation/CFBStreamedAnimReader | out-line retail's 17 unnamed helpers under their own fn_ names (fn_802B03C0 = CFBStreamedCompression::GetAnimationDuration, fn_802B0388/fn_802B03A4 = TVectorOfVaryingLengthItems<uint, CFBStreamedPerChannelHeader>::const_iterator's * and ++, fn_802B0298/02F0, fn_802B00AC/013C/0160/0170/0194/01B4/01E4/0208/033C, fn_802AE15C = FrameAfter, fn_802AF2B0/fn_802AF308 = the two destructors): run 1's wall was wrong, objdiff pairs fn_ names (1216 of 16625 in report.json have a score, 121 at 100%) and this repo already defines fn_XXXXXXXX functions; ~11 more functions are capped on the calls alone, but check CAllFormatsAnimSource (Matching) .text first
