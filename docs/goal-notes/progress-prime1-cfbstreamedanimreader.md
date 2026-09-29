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
