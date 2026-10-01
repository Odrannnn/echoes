# match-cfbstreamedcompression

`kind: match`, target `Kyoto/Animation/CFBStreamedCompression`.

## Result

**PARTIAL** — `./tools/goal_check.sh build/goal/item.json` prints
`goal_check: PARTIAL match-cfbstreamedcompression - flip_test ... FAIL, but the target rose`.
Every other check is `ok`, including `target rose: ... 13 -> 14 / 14 functions`.

| | before | after |
|---|---|---|
| **unit matched functions** | **13 / 14** | **14 / 14** |
| unit fuzzy | 97.49% | **100.00%** |
| unit matched code | 93.04% | **100.00%** |
| **All: matched functions** | 11416 / 28465 | **11417 / 28465** |
| All: linked | 5523 | 5523 (unchanged, as it must be: no unit flipped) |

The unit is **fully matched at the function level and cannot be flipped**, for a reason that has
nothing to do with this unit — see "The flip" below.

## Files changed

- `include/Kyoto/Animation/CFBStreamedCompression.hpp` — two definitions moved out of their class
  bodies, plus the comment blocks that record why (a stale prediction at the top is corrected).
- `src/Kyoto/Animation/CFBStreamedCompression.cpp` — the one moved definition, placed above the
  constructor so decl order stays descending by retail offset.

No `config/`, no `configure.py`, no `splits.txt`, no asm, no `#pragma inline_max_size`. Not one
instruction of behaviour changed: the two definitions have identical bodies to the ones they
replaced. Both placements follow Prime 1's own header
(`prime-ref/include/Kyoto/Animation/CFBStreamedCompression.hpp:96` and `:112` declare the
`CFBBitCompressedDataChannelHeader` constructor out of class).

## What the change was, and why the previous three runs could not see it

The 14th function, `__ct__61TVectorOfVaryingLengthItems<Ui,27CFBStreamedPerChannelHeader>FR12CInputStream`,
sat at **63.96%**, ours 136 bytes against retail's 212. The structural difference: retail inlines
the element constructor `CFBStreamedPerChannelHeader` into the loop body; we emitted it as a
separate 140-byte weak function and `bl` it.

Three earlier notes (all in `docs/goal-notes/`) each concluded this was an **inline-budget**
problem and stopped at the threshold:

- `progress-prime1-cfbstreamedcompression.md` run 2: "Ours is over the 125-byte `inline_max_size`
  budget … step 5 shows widening that budget is not the answer", filed as not-a-`WALL` because the
  cause was characterised but never fixed.
- `progress-prime1-cfbstreamedcompression-internal-names.md` run 2: swept
  126/130/134/138/140/141/144/148/152/160/170/180/200 and found "the vector ctor's size **never
  moves off 136** at any value up to 200 — `inline_max_size` is not what keeps the element ctor
  out of line." That was the right measurement and the right conclusion, and it is this run's
  starting point.

**The budget was never the thing.** The element ctor was out of line because
`CFBBitCompressedDataChannelHeader`'s constructor was defined *in its class body*, which makes it
**implicitly inline**. The element ctor calls all three instantiations, so it absorbed them,
grew past 125, and *that* is what stopped the element ctor itself being a candidate. It is a
cascade, and raising the budget cannot fix a cascade: it feeds the fire.

Re-measured, sweeping `inline_max_size` in the `.cpp` (element-ctor size in brackets):

| budget | element ctor | vector ctor | unit |
|---|---|---|---|
| 125 (project default) | 0x8c (140) | 0x88 (136) | 13 / 14 |
| 174 and below | 0x8c (140) | 0x88 (136) | 13 / 14 → 8 / 14 as other functions break |
| **180-220** | **0x2b4 (692)** | 0x88 (136) | 6 / 14 |
| 240-500 | 0x2b4 (692) | 0x88 (136) | 4 / 14 |
| 700 | GONE (inlined) | 0x2f8 (760) | 4 / 14 |
| 1000 | GONE | GONE | 3 / 14 |

The 140 -> 692 jump at 174 is the bug in one number: the budget inlined the sub-header
constructors *into the element ctor*, so the callee got much bigger exactly when the caller
needed it small. The previous run's sweep stopped at 200, below the 700 where inlining finally
happens and long above the 174 where it starts hurting. **There is no budget in the 181-699 range
that works, which is why the sweep looked like a wall.**

The fix is to remove the sub-header constructor from the candidate set entirely. Out of class
with no `inline` keyword it is never inlined, so the element ctor stays 140 bytes, stays under
the default 125-budget threshold for the *call*, and MWCC inlines it into the vector ctor on its
own. **No pragma is needed at all** — the unit builds on `configure.py`'s
`-pragma "inline_max_size(125)"`, and the result is the same at budgets 125, 126, 130, 140 and
200 (measured).

## The second move: `GetNumKeyframes`

Getting the element ctor inlined made the unit go *down* to 12 / 14: `GetNumKeyframes` fell to
0.00% and the constructor that calls it to 90.49%. Same cause, one class over —
`CFBStreamedCompression::GetNumKeyframes` was defined in its class body, hence implicitly inline,
hence never emitted; objdiff pairs by name and scores an absent function 0.00%, and the caller
loses its `bl`. Retail calls it as a real 48-byte function (`fn_802B071C`, `bl` at **+0xf4** in
the constructor — the 0x2f0 figure is a miscalculation on my part, corrected in the comment
before committing).

Its definition had to move to the `.cpp`, not to the bottom of the header: this header is
included by many units and a non-template out-of-class definition in it is emitted by every one,
which mwldeppc rejects as multiply-defined (measured — `CMetaAnimPlay.o` and
`CAllFormatsAnimSource.o` each carry their own copy). It is placed **above** the constructor
because mwcceppc emits definitions in reverse source order and mwldeppc keeps `.text` order
verbatim; `python3 tools/check_decl_order.py --unit Kyoto/Animation/CFBStreamedCompression` says
`ok: none emits its functions out of retail order`. Getting this wrong is invisible to objdiff
and to `unit_fit.sh` and only shows up as a few permuted bytes in the module hash — the prompt
warns about exactly this, and the flip test is what catches it.

## Verification

```
python3 tools/check_decl_order.py --unit Kyoto/Animation/CFBStreamedCompression
    ok: 1 unit(s) checked, none emits its functions out of retail order
./tools/fast_try.sh Kyoto/Animation/CFBStreamedCompression
    main/Kyoto/Animation/CFBStreamedCompression: 100.00% fuzzy, 100.00% matched code, 14/14 functions
python3 tools/check_symbol_names.py    checked 514 units; 0 declared names are missing
./tools/decomp_build.sh                All: 32.80% fuzzy, 25.55% matched, 11.98% linked (11417 / 28465)
sha1sum build/G2ME01/main.dol          6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/goal_check.sh build/goal/item.json                     PARTIAL, all other checks ok
```

**The vector constructor is byte-for-byte retail's.** `objcopy -O binary --only-section=.text` on
both objects, then a byte compare of the two 0xd4 ranges (retail at `.text`+0x65c, ours at
+0x714): **0 differing bytes out of 212.** It was 136 bytes and 63.96% before.

The shared header is not a hazard here: `Kyoto/Animation/CFBStreamedAnimReader` measures
**32 / 51, fuzzy 67.11%, unchanged** (identical to its pre-change figures), so no other unit moved.

## The flip

`./tools/flip_test.sh Kyoto/Animation/CFBStreamedCompression.cpp` **FAILs**, and cannot pass:

```
### mwldeppc.exe Linker Error:
#   undefined: 'CFBStreamedAnimReaderTotals::skQuatFloats'
```

This is **pre-existing and unrelated to this change**. Measured directly: I stashed this unit's
edit, restored the header and `.cpp` to `HEAD`, and ran the flip test on the untouched tree — the
same error, verbatim. The previous run's notes record the same thing. `configure.py` is not
touched, so nothing is claimed beyond the count and the unit correctly stays `NonMatching`; per the
goal-unit prompt this is judged as partial progress on a `match` item.

`tools/unit_fit.sh` reports `.text` over the claimed range by 376 bytes, listing six functions
present in ours and not in the retail unit object. All six are COMDAT weak template
instantiations (`__dt__Q24rstl12auto_ptr<Ui>Fv`, `__dt__Q24rstl14single_ptr<Ui>Fv`,
`AfterEnd__61TVectorOfVaryingLengthItems<...>`, `AfterEnd__45/50CFBBitCompressedDataChannelHeader`),
which retail's own linker also discards — so this number is not a verdict and `flip_test` is.

`flip_test.sh` rewrote `docs/HANDOFF.md` as a side effect; I reverted it (`git checkout --
docs/HANDOFF.md`) because the prompt forbids editing that file.

## Lesson worth carrying

**An in-class definition is not a neutral placement.** It is *implicitly inline*, which makes it
a candidate, which changes the size of whatever contains it, which changes that container's own
eligibility. Two functions here were moved out of their class bodies for exactly this reason and
nothing else changed. This is the same fact as `progress-prime1-cfbstreamedcompression-internal-names.md`
run 2's `NTSC_INLINE` finding ("`NTSC_INLINE` is not decoration"), one class over and with a
different symptom: there, defining in class made retail's out-of-line functions vanish; here, it
made a *third* function stop being inlinable.

The generalisable form: **when a size threshold will not move a call site, check whether the
callee is a candidate at all, and check what is being inlined *into* the callee.** A sweep that
reports "the caller's size never moves" is measuring the wrong thing — the fix is upstream of the
threshold, and no value of it will work.

## NEW:

NEW: fix-cfbstreamedanimreader-skquatfloats | progress | Kyoto/Animation/CFBStreamedAnimReader | `CFBStreamedAnimReaderTotals::skQuatFloats` and `::skTransFloats` are defined in src/Kyoto/Animation/CFBStreamedAnimReader.cpp:10-11 as ordinary globals in .sdata2, but mwldeppc reports `undefined: 'CFBStreamedAnimReaderTotals::skQuatFloats'` when main.elf is linked, which makes tools/flip_test.sh fail for Kyoto/Animation/CFBStreamedCompression on a clean tree and probably blocks any flip in that neighbourhood; the symbol IS in the object (`nm` shows `D skQuatFloats__27CFBStreamedAnimReaderTotals`) and two objects reference it (CFBStreamedAnimReader.o, CFBStreamedCompression.o), so this is a definition-vs-reference naming or COMDAT-visibility mismatch, not a missing definition. Unverified beyond that - I did not fix it, it is another unit's problem.