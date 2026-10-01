# progress-prime1-cfbstreamedcompression

`Kyoto/Animation/CFBStreamedCompression`, `kind: progress`. One file changed:
`src/Kyoto/Animation/CFBStreamedCompression.cpp`. No header, no `config/`, no asm.

## Result, measured

`build/report.json`, `main/Kyoto/Animation/CFBStreamedCompression`:

| | before | after |
|---|---|---|
| `matched_functions` | **1 / 14** | **2 / 14** |
| `fuzzy_match_percent` | 36.68% | 43.21% |
| `matched_code_percent` | 3.41% | 21.92% |

Per function (`report generate` is the number that counts; see the caveat at the end):

| function | before | after |
|---|---|---|
| `GetRotationsAndOffsets__22CFBStreamedCompressionFUiR12CInputStream` | 91.58% | **100%** |
| `__ct__22CFBStreamedCompressionFR12CInputStreamR12IObjectStore` | 70.67% | **92.18%** |
| `__dt__22CFBStreamedCompressionFv` | 100% | 100% (untouched) |
| the ten `fn_802B0*` / `fn_802B071C` functions | 0.00% | 0.00% (see below) |

`./tools/goal_check.sh build/goal/item.json` → **PASS**
(`matched 10393 -> 10394`, `linked 5048 -> 5048`, `target rose: 1 -> 2 / 14`, no asm, gate green
including the DOL sha1 and all 86 REL hashes).

## What was done, per function

### 1. `GetRotationsAndOffsets` — 91.58% → 100%. One-line fix.

Our object was 560 bytes against retail's 564 — exactly one instruction short, and the diff showed
which: retail calls `fn_802B02F0` (the list's `AfterEnd()`) *before* `fn_802B0980`
(`GetSumOfBitCounts()`), while we computed the word count first and took `AfterEnd()` afterwards.
Hoisting `uchar* cursor = const_cast< uchar* >(channels->AfterEnd());` above the `wordCount`
expression reproduced the call and, with it, the whole register allocation: the two functions now
match instruction for instruction. Prime 1's source was right on this point (cursor, then count) and
our file had the statements the other way round, so the fix was "match Prime 1", not "port a
spelling".

### 2. The constructor — 70.67% → 92.18%. Three changes, all "follow Prime 1".

* `mRootOffset(CVector3f::Zero())` → `mRootOffset(0.f, 0.f, 0.f)`. `CVector3f::Zero()` made the
  compiler load `sZeroVector__9CVector3f` and emit 8 instructions (`lis`/`lfsu` + three loads and
  stores); retail loads the float literal `0` once and stores it three times. Prime 1 writes
  `mRootOffset(0.f, 0.f, 0.f)`.
* Hoisted `const uint* bytes = GetBytes(channels);` and `const uint keyframes = GetNumKeyframes();`
  above the `CMemoryInputToBitLevelLoader` / `CBitLevelLoader` construction, so the two out-of-line
  calls land where retail has them (retail: `bl fn_802B02F0` then `bl fn_802B071C`, before the
  iterator loop). Prime 1 has the same order.
* Split `const float delta = (current - previous).Magnitude(); previous = current;` into
  `CVector3f difference = current - previous; previous = current; const float delta =
  difference.Magnitude();` (Prime 1's order). This is the one that moved the frame: retail copies
  `previous = current` *before* the `Magnitude` call, so the current vector is dead across it and
  needs no register of its own. In our order it stayed live, which cost three extra `stfd`/`psq_st`
  pairs (f23/f24/f25) and a 0x120 frame against retail's 0xf0. With the copy hoisted, the prologue,
  the keyframe loop and the epilogue all line up.

## What is still not matched, measured

The constructor: 55 of 184 instructions differ; our object is 704 bytes where retail's is 716. The
residue is one root cause with a tail:

* **The channel-header loop keeps its iterator in memory; retail keeps it in registers.** Retail
  holds the `const_iterator` in r3 (mPtr) and r27 (mCount) and never spills; we materialise it at
  `0x1c(r1)`/`0x20(r1)`, reload mPtr at the top of the loop and reload mCount for the condition
  each iteration, and call
  `__pp__Q261TVectorOfVaryingLengthItems<Ui,27CFBStreamedPerChannelHeader>14const_iteratorFv` with
  `r3 = &it` — the address is taken because `TVectorOfVaryingLengthItems::const_iterator::operator++`
  returns `const_iterator&`. That is 5 instructions against retail's, and it pushes the
  `CFBStreamedAnimReaderTotals` local 8 bytes up the frame (ours `0x3c(r1)`, retail `0x34(r1)`), so
  every later `addi r3, r1, 0x3c` / `lbz r0, 0x76(r1)` / `stfs f2, 0x24(r1)` differs too.
  Retail's loop body is three calls — `bl fn_802B0388`, `bl fn_802B03A4`, `bl fn_802B03A4` — where
  ours is one. Those two live in `Kyoto/Animation/CFBStreamedAnimReader.cpp` (0x802B0388 /
  0x802B03A4) and are `fn_`-named in `config/G2ME01/symbols.txt`, so what they are is not yet
  known. Changing `operator++` to return by value is the obvious thing to try; it is a header
  change in a header other units include, so it needs the full gate, not a `fast_try`.
* One extra `mr r3, r28` after the `mRootOffset` init, which shifts retail's r4/r5 temporaries to
  r3/r4 for the rest of the setup (`lwz r4, 0x8(r28)` vs our `lwz r3, 0x8(r28)`). Not localised
  to a statement; it is downstream of the same allocation.
* The tail: retail computes the duration with `lwz r4, 0x8(r28)` + `lfs f1, 0x4(r4)` +
  `bl fn_802B03C0`; we use `mr r4, r28` + `bl __ct__13CCharAnimTimeFf`. One instruction, but the
  reload of `mRotsAndOffs` is a real difference in what is being called.

No `WALL:` line: the constructor moved 70.67% → 92.18% this run, so it has not sat at one score,
and the residue above is measured rather than a set of tried-and-failed spellings. The one spelling
tried and rejected is recorded: `CVector3f::Zero()` for `mRootOffset` (costs 5 instructions and a
`sZeroVector` load).

## The ten `fn_*` functions are 0.00% for a naming reason, not a code reason

`config/G2ME01/symbols.txt:12260-12271` names this unit's internal functions `fn_802B071C`,
`fn_802B0980`, `fn_802B0A20`, `fn_802B0A50`, `fn_802B0B24`, `fn_802B0B54`, `fn_802B0C98`,
`fn_802B0D78`, `fn_802B0DF8`, `fn_802B0E68`, `fn_802B0F08`, while our object emits the real C++
mangled names. The report pairs base and target functions **by name**, so each of those eleven
scores 0.00% however good our code is. Three of them already have a same-size counterpart in our
object (`fn_802B0C98` 224 B = `__ct__26CStandardMultiFormatHeaderFR12CInputStream` 224 B,
`fn_802B0B24` 48 B = `__ct__32CFBStreamedCompressionTimeHeaderFR12CInputStream` 48 B,
`fn_802B0A20` 48 B = `__ct__31CFBStreamedPerChannelHeaderListFR12CInputStream` 48 B), so a
`symbols.txt` rename with `tools/apply_rename.py` is likely to pair several of them at once. **Not
measured** — I did not do it, because it forces a full dtk re-split and a full rebuild, and this
tree was already passing. Filed as `NEW:` below.

The unit also cannot be promoted whatever the percentages: our object emits 20 functions to
retail's 14 (`tools/unit_fit.sh` is the check, and the extra `AfterEnd`/`GetSumOfBitCounts`/
`const_iterator` helpers are the ones to fold in first). It stays `NonMatching`.

## A caveat worth having: the interactive `objdiff diff` and `report generate` disagree

`objdiff-cli diff -1 <ours> -2 <retail>` scored `GetRotationsAndOffsets` at **99.82%** on 5
mismatched instructions while `report generate` scored the same object **100%** and counted it
matched. The 5 are all relocation *names*: `bl fn_802B0C98` vs
`bl __ct__26CStandardMultiFormatHeaderFR12CInputStream`, `lbl_803AEE20` vs `@stringBase0`, and the
`.sdata2` float literals. Confirmed on a unit that is already `Matching`:
`Kyoto/Animation/CSequenceHelper.cpp` is 18/18 in the report, and the interactive diff shows 12
`DIFF_ARG_MISMATCH` on `lbl_80418A18@sda21` vs `@73@sda21`. **The report ignores relocation target
names; the interactive diff does not.** So `objdiff-cli diff` is the right tool for *which
instruction* differs, and `report generate` (or `tools/fast_try.sh`) is the only thing that answers
"did a function get matched". Chasing the interactive diff to 100% here would have meant renaming
five retail symbols for nothing.

Also note the argument order: `-1` is our object (the target) and `-2` is the retail one (the base).
Passing them the other way round still prints a plausible-looking per-function diff, which is how I
first read a "0.00%" function as a code difference.

NEW: match-cfbstreamedcompression-internal-names | progress | Kyoto/Animation/CFBStreamedCompression | the unit's 11 internal functions are `fn_`-named in config/G2ME01/symbols.txt so the report pairs them by name and scores all of them 0.00%; three already have a same-size counterpart in our object, so renaming them with tools/apply_rename.py should pair several at once. Unverified: the rename needs a full dtk re-split and rebuild, which I did not attempt.

---

# Run 2 (lane 8, 2026-10-01). The previous run's `WALL:` reasoning was the blocker; all four
# residual causes were declaration placement, not code. 8 -> 11 of 14.

Two files changed: `include/Kyoto/Animation/CFBStreamedCompression.hpp` and
`src/Kyoto/Animation/CFBStreamedCompression.cpp`. No header layout change, no class layout
change, no `config/`, no asm, no new symbol. Every fix is "follow Prime 1".

## Result, measured (`build/report.json`, before/after from the judge baseline)

`main/Kyoto/Animation/CFBStreamedCompression`:

| | before | after |
|---|---|---|
| `matched_functions` | **8 / 14** | **11 / 14** |
| `fuzzy_match_percent` | 74.18% | **89.62%** |
| `matched_code_percent` | 46.19% | **85.17%** |

`main/Kyoto/Animation/CFBStreamedAnimReader` rose too, as a side effect of the header:
**29 -> 32 of 51**, fuzzy 61.13% -> 67.11%, matched_code 20.49% -> 24.14%.

Global `matched_functions` **11350 -> 11356**; `linked` 5507 -> 5507 (no unit changed state, so
nothing newly-linked, and nothing un-linked).

`./tools/goal_check.sh build/goal/item.json` -> **PASS** (gate green incl. DOL sha1 and all 86
REL hashes; `target rose: 8 -> 11 / 14`; no asm; `check_symbol_names.py` clean).

Per function, `report generate` is the number that counts:

| function | before | after |
|---|---|---|
| `__ct__43CFBKeyFrameReductionPerChannel_HeaderForAll` | 23.98% | **100%** |
| `GetSumOfBitCounts__31CFBStreamedPerChannelHeaderList` | 34.33% | **100%** |
| `GetRotationsAndOffsets__22CFBStreamedCompression` | 100% | 93.33% then **100%** |
| `__ct__22CFBStreamedCompression` (the ctor) | 92.18% | **100%** |
| `__ct__61TVectorOfVaryingLengthItems<Ui,...>` | 33.66% | 38.36% (not matched) |
| `fn_802B0D78` / `fn_802B0DF8` | 0.00% | 0.00% (naming only, see below) |

## The four changes, in the order they were tried and measured

All four were each verified alone with `tools/fast_try.sh`, and the score after each is the
number in that step's row. `fast_try` only rebuilds the named object, so it is safe for a header
change; the full gate at the end is what proves the other units did not move.

### 1. Hoist the word count in `CFBKeyFrameReductionPerChannel_HeaderForAll` -> 23.98% -> **100%**

Ours had `for (uint i = 0; i < Uint32sForBitCount(mBitCount); ++i)`, so MWCC reloaded `mBitCount`
(`lwz r5,0(r3)`) on every iteration and gave up unrolling; ours was 96 bytes. Prime 1 has
`uint words = Uint32sForBitCount(mBitCount);` as a local before the loop. Retail's guard
(`cmplwi r8,0 / ble blelr`, count in `r8`) also shows the count is tested once. With the local,
MWCC unrolls 8x and the object is 324 bytes, matching retail's exactly. One line. **Prime 1's
source matched unchanged.** This was the cheapest win in the item and the previous run never tried
it: its notes describe the function as blocked on a `Uint32sForBitCount` *spelling*, and the
branch-order question there is still settled (it is untouched and still correct).

### 2. Declare `CFBBitCompressedDataChannelHeader`'s methods out of class -> `GetSumOfBitCounts` 34.33% -> **100%** (but cost `GetRotationsAndOffsets`, fixed in step 3)

This is the one the previous run called the wall. It read as a compiler decision ("MWCC inlines
ours into one 272-byte helper") and it is, but the cause is **where the definition sits**, not
what it says. `AfterEnd`, `GetSumOfBitCounts`, `GetInitialValue` and `GetBitCount` were defined in
the class body, which makes them *implicitly* inline. Prime 1 declares all four out of class
behind `NTSC_INLINE`, and `include/types.h:20` defines `NTSC_INLINE` as `inline` only for
`VERSION < VERSION_GM8P_00` — i.e. **empty** for Echoes. So retail's build sees four ordinary
out-of-line template members, and it emits them: the two `AfterEnd` instantiations as real
28-byte functions (`fn_802B0388`/`fn_802B03A4`, in `CFBStreamedAnimReader.o`) and the two
`GetSumOfBitCounts` ones as real functions (112 and 128 bytes). Moving the four definitions below
the class, with no `inline` keyword, reproduces that exactly: `nm` on our object then lists
`AfterEnd__45CFBBitCompressedDataChannelHeader<4,100000,0>CFv` (28 B) and
`GetSumOfBitCounts__50CFBBitCompressedDataChannelHeader<3,100000,100000>CFv` (128 B).

Lesson worth carrying: **`NTSC_INLINE` is not decoration.** On a GM8P+ build it expands to
nothing, so "Prime 1 wrote it out of class behind `NTSC_INLINE`" means "Prime 1's build never
marked it inline", which is a different thing from what the Prime 1 source looks like.

### 3. Move `TVectorOfVaryingLengthItems::AfterEnd()` out of class too -> `GetRotationsAndOffsets` back to **100%**

Step 2 alone traded: `GetSumOfBitCounts` reached 100% but `GetRotationsAndOffsets` fell
100% -> 93.33%, because that change made `AfterEnd` out of line and retail calls it out of line
**twice** there (`fn_802B02F0`, 76 bytes, from `GetRotationsAndOffsets`), while ours was
inlining the walk at both call sites — the 596-vs-564 byte gap. Prime 1 declares
`TVectorOfVaryingLengthItems::AfterEnd` out of class as well, behind the same empty
`NTSC_INLINE`. Doing the same took the unit to **10 / 14** with both matched.

### 4. Declare `CFBStreamedCompression::GetAnimationDuration()` out of class -> the ctor 92.18% -> **100%** (unit 11 / 14)

The previous run's last remaining item, and it was one instruction pair. Ours inlined
`GetAnimationDuration()` and emitted `lwz r4,8(r28) / lfs f1,4(r4) / bl __ct__13CCharAnimTimeFf`;
retail calls `fn_802B03C0` (52 bytes, the out-of-line copy that
`src/Kyoto/Animation/CFBStreamedAnimReader.cpp:48` already provides) as
`mr r4,r28 / addi r3,r1,12 / bl fn_802B03C0`. Prime 1 declares it out of class and defines it in
the .cpp. Moved, and the constructor's 700 instructions match instruction for instruction.

## `fn_802B0D78` and `fn_802B0DF8`: byte-identical, blocked only on a rename

These two score 0.00% for the naming reason the previous run described, and that part of its
notes is still exactly right — but its conclusion ("we emit neither, because MWCC inlines ours
into one 272-byte helper, so there is nothing to pair them with") is now **superseded**. After
steps 2 and 5 we emit both, and both are **byte-identical** to retail: 112 vs 112 B and 128 vs
128 B, and a byte-by-byte comparison of the raw encodings finds **0 differing bytes** in each (28
and 32 instructions). Only the names differ.

Step 5 is what got the sizes: Prime 1's `CFBBitCompressedDataChannelHeader::GetSumOfBitCounts`
walks the payload itself (`sum += data[2]; data += 3;` with a `sum += 1` arm for the sign
component) instead of calling `GetBitCount(i)` in the loop. Ours looped on `GetBitCount`, which
keeps a counted loop and an out-of-line call; retail's two instantiations have the body unrolled
into straight-line `lbz 2(r4) / addi r4,r4,3` pairs. Swapping to Prime 1's form changed nothing
in the report (the functions are still scored 0% by name) but took the sizes to an exact match.

So a `config/G2ME01/symbols.txt` rename of these two — the `NEW:` line the previous run filed —
is now expected to pair two more functions and take the unit to **13 / 14**. That still needs the
full dtk re-split and rebuild, which I did not attempt (config files are out of scope for this
item, and a rename changes every unit's addresses). **No new `NEW:` line is filed for it** — the
existing `match-cfbstreamedcompression-internal-names` item is the right one and this run only
sharpens its expected payoff from "likely to pair several" to "these two are byte-identical and
will pair".

## Spelling rejected this run, with its score

`#pragma inline_max_size(200)` at the top of `CFBStreamedCompression.cpp`, to coax the
per-channel ctor under the inline budget: **11 -> 4 of 14**, fuzzy 89.62% -> 51.17%. The build's
`-pragma "inline_max_size(125)"` is retail's own setting; widening it makes MWCC inline things
retail keeps out of line and breaks seven functions at once. Do not retry. Reverted.

## What is still not matched, measured

`__ct__61TVectorOfVaryingLengthItems<Ui,27CFBStreamedPerChannelHeader>` at **38.36%**, ours
136 B vs retail 212 B. One structural difference remains: retail **inlines**
`CFBStreamedPerChannelHeader`'s constructor into it (the loop body is three out-of-line
`__ct__45`/`__ct__50` calls interleaved with `fn_802B0388`/`fn_802B03A4` `AfterEnd` calls), while
MWCC declines to inline ours and emits a separate `__ct__27CFBStreamedPerChannelHeader` (136 B)
that the TVector ctor calls. Ours is over the 125-byte `inline_max_size` budget that retail's own
build used, and step 5 shows widening that budget is not the answer. What would be needed is a
`CFBStreamedPerChannelHeader` body small enough to fit under 125 bytes — Prime 1's version has
two channels to Echoes' three, so it is not a like-for-like comparison and its shape does not
transfer. **Not a `WALL:`**: the function moved 33.66% -> 38.36% this run, and the residue is a
single characterised cause, not a set of tried-and-failed spellings.

The unit still cannot be promoted: `tools/unit_fit.sh` reports ours 3488 B against retail's
claimed 3048 B, over by 440 (was over by 320 before this run — the honest trade is more real
functions matched for more unclaimed helper bytes emitted). It stays `NonMatching`.

## Two corrections to the previous run's notes, for the record

1. **The `objdiff` caveat is half right and cost a run.** It says `report generate` "ignores
   relocation target names" while the interactive `objdiff diff` does not, and concludes from
   that the interactive diff is the right tool for "which instruction differs". It is the
   opposite: objdiff pairs base and target functions **by name**, so the eleven `fn_`-named
   retail functions score 0.00% no matter how good our code is, and no spelling change to their
   bodies can move that. Diffing our object against retail is the only way to see these two are
   byte-identical.
2. **`unit_fit.sh`'s "over by N bytes" is not a verdict and was read as one.** It rose from 320
   to 440 while three more functions went to 100%. The extras are the out-of-line template
   instantiations retail *also* has (28 + 28 + 112 + 128 B of real code) landing in a unit whose
   `splits.txt` range does not claim them; retail's linker places its copies in
   `CFBStreamedAnimReader.o`. Judged by `report generate` and the gate, not by this number.
