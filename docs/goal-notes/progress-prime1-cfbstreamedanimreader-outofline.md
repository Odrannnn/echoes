# progress-prime1-cfbstreamedanimreader-outofline

`kind: progress`, target `Kyoto/Animation/CFBStreamedAnimReader` (unit
`main/Kyoto/Animation/CFBStreamedAnimReader`). Run 1 of this item declared the 17 unnamed retail
helpers a wall; that wall is gone.

## Result, measured

`build/report.json`, base `build/goal/judge/report.base.json`:

| | before | after |
|---|---|---|
| unit `matched_functions` | **17 / 51** | **29 / 51** |
| unit `fuzzy_match_percent` | 55.22% | 61.13% |
| unit `matched_code_percent` | 15.00% | 20.49% |
| project `matched_functions` | 10428 | **10440** |

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (`goal_check: PASS
progress-prime1-cfbstreamedanimreader-outofline`): gate green (DOL sha1, 86 RELs, report diff,
wiring, docs claims, port probe), `matched 10428 -> 10440   linked 5048 -> 5048`,
`target rose: 17 -> 29 / 51 functions`, no `asm`. The unit stays `NonMatching`; no `flip_test` was
run and none is claimed.

Diff is two files:

```
  config/G2ME01/symbols.txt                   |   8 +-
  src/Kyoto/Animation/CFBStreamedAnimReader.cpp | 130 +++++++++++++++++++++++++++---
```

No header, no `configure.py`, no `asm`.

## The finding that made all twelve cheap: the report ignores relocation names

Run 1 and run 2 both believed the callers of the `fn_*` helpers were capped, and run 2 recorded it
as the reason the fix was not obvious. It is not true, and it is measurable: `report generate`
ignores a relocation's target name (run 2's own `objdiff-cli diff` note says so on a `Matching`
unit). So **defining retail's function under retail's own name is enough - the call sites do not
have to be routed.** Per-function, nothing else in the unit moved at all:

```
$ python3 - <<'PY'   # baseline report vs new report, same unit
fn_802B03C0   0.00% -> 100.00%      fn_802B0194   0.00% -> 100.00%
fn_802B03A4   0.00% -> 100.00%      fn_802B0170   0.00% -> 100.00%
fn_802B0388   0.00% -> 100.00%      fn_802B0160   0.00% -> 100.00%
fn_802B033C   0.00% -> 100.00%      fn_802B013C   0.00% -> 100.00%
fn_802AE15C   0.00% ->  86.47%
```

Not one caller changed: `VGetTimeRemaining` 90.00%, `__dt__21CFBStreamedAnimReaderFv` 91.19%,
`SetTime__23CFBStreamedPairOfTotals` 87.08%, `HasOffsetData` 54.57%, `IncrementInto` 37.08% -
all identical before and after. **The ten functions the item's reason called "capped on the calls
alone" are not capped**; whatever is left in them is real instruction difference.

## The twelve that reached 100%

### Eight defined in `src/Kyoto/Animation/CFBStreamedAnimReader.cpp`, under retail's `fn_` names

Each is `extern "C"`, each body is retail's body, each carries a comment naming the C++ it is
retail's out-of-line copy of. They are declared at the source position that puts them at retail's
address, because mwcceppc emits in reverse source order - verified with
`tools/check_decl_order.py`, which fails if they are merely appended.

| function | retail bytes | what it is |
|---|---|---|
| `fn_802B03C0` | 52 | `CFBStreamedCompression::GetAnimationDuration()` |
| `fn_802B03A4` | 28 | `CFBBitCompressedDataChannelHeader< 3, 100000, 100000 >::AfterEnd()` |
| `fn_802B0388` | 28 | `CFBBitCompressedDataChannelHeader< 4, 100000, 0 >::AfterEnd()` |
| `fn_802B033C` | 76 | `CFBStreamedCompression::HasScaleData()` |
| `fn_802B0194` | 32 | `< 4, 100000, 0 >::GetBitCount(uint)` |
| `fn_802B0170` | 36 | `< 4, 100000, 0 >::GetInitialValue(uint)` |
| `fn_802B0160` | 16 | `< 3, 100000, 100000 >::GetBitCount(uint)` |
| `fn_802B013C` | 36 | `< 3, 100000, 100000 >::GetInitialValue(uint)` |

`fn_802B0388` and `fn_802B03A4` are byte-identical to each other, which is what identifies them:
all three instantiations add `3 * (Components - (SignComponent < Components))` = 9 to the two-byte
width, and there are exactly two distinct instantiations.

### Four by `config/G2ME01/symbols.txt` rename, no body written at all

This object already emits retail's bytes for these four **under the real C++ names**; verified
instruction for instruction (`build/binutils/powerpc-eabi-objdump -dr` on our object and on
`build/G2ME01/obj/Kyoto/Animation/CFBStreamedAnimReader.o`, relocation targets normalised) before
touching anything. So `tools/apply_rename.py` gave retail's `fn_` address the name its body has,
which is what `tools/report_diff.py`'s own docstring calls one of the two cheapest improvements:

```
fn_802AF2B0 -> __dt__22CSegIdToIndexConverterFv                              88 B
fn_802B00AC -> __dt__41TAnimSourceInfo<22CFBStreamedCompression>Fv         144 B
fn_802B0208 -> LoadUnsigned__47CBitLevelLoader<28CMemoryInputToBitLevelLoader>FUi  144 B
fn_802B0298 -> LoadSigned__47CBitLevelLoader<28CMemoryInputToBitLevelLoader>FUi    88 B
```

`apply_rename.py` reported `renamed 4/4`; none of the four target names existed in
`symbols.txt` and no source file referenced any of the eight names, so no other unit moved. dtk
re-splits on the change (its `depfile` covers `symbols.txt`); `./tools/decomp_build.sh` afterwards
is a 10-second rebuild.

**This is the generalisable part of the item: for an unpaired 0.00% function, check whether our
object already contains those bytes under a real name before writing a body for it.** The same
trick is still unclaimed in `Kyoto/Animation/CFBStreamedCompression.cpp`, whose `fn_802B0*`
functions run-2 of the neighbouring item listed as "0.00% for a naming reason".

## Spellings measured, so the next run does not repeat them

**Indexed vs folded address** (`GetBitCount` / `GetInitialValue`): retail uses
`mulli r4,r4,3` / `addi r0,r4,+4` / `lbzx r3,r3,r0`. Read as `self[component * 3 + 4]` MWCC folds
the base into the index - `mulli r0,r4,3` / `add r3,r3,r0` / `lbz r3,4(r3)` - and scores 68.75%.
`const uint offset = ...; return self[offset];` is identical (68.75%). Only routing the address
through a **pointer local** reproduces the indexed load:

```
const uchar* result = self + component * 3 + 4;   // 68.75% -> 100%
```

This is run 2's `GetVector`/`skQuatFloats` trick again, now confirmed for `lbzx` as well as
`add`: **MWCC only emits the indexed form when the index expression is a pointer value at the
load.** Two functions fixed by it here, and it is worth trying on any remaining `lbzx`/`lhax`
mismatch in the project.

**Ternary arm order** (`AfterEnd`): `width != 0 ? this + 11 : this + 2` gives `beq` and 99.00%;
`width == 0 ? this + 2 : this + 11` gives retail's `bne` and 100%. Same as run 2's
`Uint32sForBitCount` result, now confirmed for pointer arms.

**A 32-bit constant compared against a `uint`** (`GetInitialValue`, sign component 100000): retail
guards with `addis r0, r4, -1` + `cmplwi r0, 34464`, i.e. it tests `component == 34464`, 100000
truncated to 16 bits. Writing `component == 34464` gives a single `cmplwi r4, 34464` and 88.33%;
**writing `component == 100000` is what produces retail's two instructions** (100%) - MWCC does the
truncation itself. Do not hard-code the truncated value.

**Parameter order against a two-word class return** (`fn_802B03C0`): retail saves its unused
`CCharAnimTime` argument to r31 and dereferences its second argument in r4. Declared
`(const CCharAnimTime&, const CFBStreamedCompression*)`, `self` lands in **r5** (`lwz r4, 8(r5)`,
99.62%) because the reference is allocated after the two-word return slot. Declared
`(const CFBStreamedCompression&, const CCharAnimTime&)` it lands in r4 and matches exactly. Also
measured: by-value `CCharAnimTime`, `const CFBStreamedCompression&` second, a single parameter,
and three parameters - all put the dereferenced argument in r5 or r6.

## `fn_802AE15C` - 0.00% -> 86.47%, five instructions short

`CFBKeyFrameReductionPerChannel_HeaderForAll::FrameAfter(uint)`, defined as
`return header.FrameAfter(frame);`. Retail and we compute the same value two different ways:

| | retail | ours |
|---|---|---|
| word index | `rlwinm r6,r4,29,3,29` = `(frame >> 3) & 7` | `srwi` + mask |
| bit mask | `clrrwi r0,r4,5` + `subf r0,r0,r4` = `frame % 32` | `clrlwi r0,r4,27` |

Three spellings written out in the extern body rather than calling the inline header method, all
**worse** than the 86.47% the inline call gives:

```
(frame >> 3) & 7,  1u << (frame - (frame & ~31u))      61.47%
frame / 32,        1u << (frame - frame / 32 * 32)     62.06%
(frame & 0xFF) >> 5, 1u << (frame & 31)                 67.06%
```

So retail's `%` keeps the `clrrwi`/`subf` pair and our `%` does not; that is a difference inside
`CFBKeyFrameReductionPerChannel_HeaderForAll::FrameAfter` itself, i.e. a **header** edit, which
would move `CFBStreamedCompression.cpp` and `CAllFormatsAnimSource.cpp` (`Matching`, 11/11). Not
attempted. The definition is kept at 86.47% because it is better than 0.00% and costs nothing.

## Still open, measured, for the next run

- **`fn_802B02F0`, 76 B, 0.00%** - `TVectorOfVaryingLengthItems< uint,
  CFBStreamedPerChannelHeader >::AfterEnd()`. Retail's loop body is `bl fn_802B0388` /
  `bl fn_802B03A4` / `bl fn_802B03A4`; ours inlines the three nested `AfterEnd()`s and keeps the
  loop counter in r30 (retail uses r31 and counts down). Reaching it means
  `CFBStreamedPerChannelHeader::AfterEnd()` in `include/Kyoto/Animation/CFBStreamedCompression.hpp`
  calling the out-of-line copies, which is a **shared-header change**: `CAllFormatsAnimSource.cpp`
  is `Matching` (11/11), so its `.text` has to be checked before keeping it. Not attempted.
- **`fn_802B01B4` (48 B) and `fn_802B01E4` (36 B), both 0.00%** - the `IAnimSourceInfo` overrides
  `TAnimSourceInfo< CFBStreamedCompression >::GetAnimationDuration()` / `::HasScaleData()`. Both
  bodies are `lwz rX, 16(rX)` then a tail call, and **retail's `mSource` is at +16 while ours is at
  +4** (vptr at 0). This is blocked on the class layout, not on codegen: it needs whatever puts
  `IAnimSourceInfo`'s state back to Echoes' offsets, which is a much larger item.
- **`fn_802AF308`, 600 B, 0.00%** - run 1 called it `~CFBStreamedPairOfTotals`. **That is wrong.**
  It opens by filling the object with -1: `li r3,-1` / `mtctr r0` / 24 `stw r3,off(r5)` over 96
  bytes, from a 48-byte frame. It is a constructor or an in-place reset, not a destructor - ours,
  `__dt__23CFBStreamedPairOfTotalsFv`, is 128 bytes and calls two member destructors. Not
  identified; do not spend a run assuming it is a destructor.
- `tools/unit_fit.sh` still reports **21 functions ours emits that retail's unit object does not**
  (2028 bytes, all template/COMDAT destructor copies) and `.text` 14156 against a claimed 14000 -
  **over by 156, where the baseline was 212 short.** So this change moved the unit closer to its
  claimed range from the other side, and it still cannot flip. `.sdata2` still fits 40/40, `.data`
  still over by 20, `.rodata` still short by 1 - all unchanged.

## NEW

None filed. The three functions left at 0.00% need a shared-header out-lining, a class-layout
change and an unidentified 600-byte body respectively - none of which is an hour of work, and the
first would put a `Matching` unit's `.text` at risk.

## Verification, all on this tree after the last edit

```
$ ./tools/goal_check.sh build/goal/item.json
  ok    no judge-owned path touched
  ok    gate.sh (includes DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
  ok    counts: matched 10428 -> 10440   linked 5048 -> 5048
  ok    check_symbol_names.py
  ok    All:  31.67% fuzzy, 24.24% matched, 11.83% linked (10440 / 28465 functions)
  ok    target rose: main/Kyoto/Animation/CFBStreamedAnimReader: 17 -> 29 / 51 functions
  ok    no asm added
goal_check: PASS progress-prime1-cfbstreamedanimreader-outofline

$ sha1sum build/G2ME01/main.dol
6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

$ ./tools/probe_sources.sh
probe: 752 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)

$ python3 tools/check_symbol_names.py
checked 505 units; 0 declared names are missing from their object

$ python3 tools/check_decl_order.py
ok: 972 unit(s) checked, 31 permuted, all 31 accounted for in decl_order.md

$ python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
matched 10428 -> 10440   linked 5048 -> 5048   (+12 functions at 100%, 0 units newly linked)
no regression

$ git status --porcelain
 M config/G2ME01/symbols.txt
 M src/Kyoto/Animation/CFBStreamedAnimReader.cpp
```