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
