# progress-unit-ctextexecutebuffer

`kind: progress`, `target: Kyoto/Text/CTextExecuteBuffer`. Two files changed:
`include/Kyoto/Text/CTextExecuteBuffer.hpp` (`CTextExecuteBuffer::Add`, plus the comment above it)
and `include/rstl/iterator.hpp` (`inline` on `rstl::advance_iterator`, plus the comment above it).
Nothing else touched - no `configure.py`, no `config/`, no `splits.txt`, no `files.cmake`, no
`tools/`, no asm, no `build/goal/` except this notes file.

## Result, measured

| | before (`build/goal/judge/report.base.json`) | after |
|---|---|---|
| unit `matched_functions` | **43 / 46** | **44 / 46** |
| unit fuzzy | 99.46498% | **99.56206%** |
| unit matched code | 84.75177% | **85.992905%** |
| project `matched_functions` | 12478 | **12479** |
| project `linked` | 5863 | 5863 (unchanged, correct for a `progress` item) |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

    matched  12478 -> 12479   linked 5863 -> 5863   (+1 functions at 100%, 0 units newly linked)
      +100%    main/Kyoto/Text/CTextExecuteBuffer :: Add__18CTextExecuteBufferFRCQ24rstl24ncrc_ptr<12CInstruction>
    no regression

`./tools/decomp_build.sh` -> `All:  35.25% fuzzy, 29.05% matched, 12.90% linked (12479 / 28465 functions)`.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`.
`./tools/probe_sources.sh` = `756 files, 0 failed, 0 errors; link: LINKED (290 undefined, 0 duplicates)`.
`python3 tools/check_symbol_names.py` = `checked 525 units; 0 declared names are missing from their object`.
`python3 tools/check_decl_order.py --unit Kyoto/Text/CTextExecuteBuffer` = `ok`.
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`**, every step `ok`, including
`target rose: main/Kyoto/Text/CTextExecuteBuffer: 43 -> 44 / 46 functions`.

Per function, before% -> after%, all measured on this tree:

| function | size | before | after |
|---|---|---|---|
| `Add` | 112 | 92.18 | **100.00** |
| `AddImage` | 508 | 99.09 | 99.09 (unchanged - see the wall) |
| `WrapOneLTR` | 756 | 95.38 | 95.38 (unchanged - see the wall) |

The other 43 functions were already at 100.00% and did not move (`report_diff` says `no regression`).

## The two lines that took `Add` to 100%

`Add` is an inline in `CTextExecuteBuffer.hpp`. Retail's body (0x802B791C, 112 bytes, frame 0x30)
is 28 instructions and holds **three** stack slots holding `mInstructions.begin()` - 0x14(r1) and
0x18(r1) dead, 0x10(r1) the one `rstl::__advance` advances and the one copied to the return slot -
around one out-of-line `bl __advance`. Ours held one (0x10(r1)), was 26 instructions in a 0x20 frame,
and measured 92.18%: the two dead stores plus the frame size are the whole 8-byte gap.

The two dead slots are what an inlined **by-value** iterator helper leaves behind, and the previous
run's note guessed the mechanism was `rstl::advance_iterator` but could not get mwceppc to inline it,
so it recorded a `WALL:`. The missing piece is that **`inline` on `advance_iterator` is what lets
mwceppc inline it here**: without the keyword this function is emitted out of line and `Add` calls
it instead (`78.71%`); with it, the body is inlined into `Add`, the by-value parameter and `result`
become the two dead slots, and `__advance` stays a real `bl` - retail's exact shape.

```cpp
// include/rstl/iterator.hpp
template < typename It, typename S >
inline It advance_iterator(It it, S count) { ... }

// include/Kyoto/Text/CTextExecuteBuffer.hpp
InstList::iterator it = rstl::advance_iterator(mInstructions.begin(), -1);
return it;
```

`Add` is then 28 instructions against retail's 28, instruction for instruction, frame 0x30. Nothing
in the work is dropped: `push_back`, `begin()`, step back one node and return the iterator, exactly
as before.

The keyword is safe because **nothing else in the tree names `advance_iterator`** (`grep` over
`src/` and `include/`: the only hit outside the comment is this one call), so no other translation
unit instantiates it and no other object moves. The full build confirms it: 12478 -> 12479 and
nothing else.

### `Add` spellings measured this run (all on this tree, `rstl::advance` unless stated)

| spelling of the body | `Add` |
|---|---|
| `it = begin(); rstl::advance(it, -1); return it;` (the head) | 92.18 |
| `it = begin(); InstList::iterator end = it; advance(end, -1); return end;` | 92.18 |
| `it = begin(); it = advance_iterator(it, -1); return it;` | 87.96 |
| `it = begin(); advance_iterator(it, -1); return it;` (result discarded) | 88.00 |
| `it = begin(); advance_iterator(it, -1); InstList::iterator e = it; return e;` | 88.00 |
| `return advance_iterator(begin(), -1);` | 78.71 |
| `it = advance_iterator(begin(), -1); return it;` **without the `inline`** | 88.00 |
| `it = begin(); __advance(it, -1, bidirectional_iterator_tag()); return it;` | 92.07 |
| `it = begin(); advance(it, -1); advance(it, 0); return it;` | 70.86 |
| `it = end(); advance(it, -1); return it;` | 92.14 |
| `InstList::iterator it; it = begin(); advance(it, -1); return it;` | 78.79 |
| `it = advance_iterator(begin(), -1); return it;` **with the `inline`** | **100.00** |

The last two rows are the pair that matters: the source shape is the same in both, and the only
difference is the keyword in `rstl/iterator.hpp`. So the earlier runs' "MWCC will not inline ours"
was a keyword, not a limit - the `#pragma inline_max_size(110)` at the top of
`src/Kyoto/Text/CTextExecuteBuffer.cpp` is not what stands in the way.

## The two functions still open, with the evidence

**`AddImage` 99.09%** - 12 bytes of 508, and the only difference is which physical register each of
the four live values gets. Retail: `li r28,0 / mr r29,r28` (`wrap`, then `tooWide` initialised from
it), widths in r27/r26. Ours: `li r27,0 / li r26,0` (MWCC folds the copy into a second `li`), widths
in r28/r29. Everything after, including the r28/r29 reuse for the baseline and height, is identical.
Measured this run, all worse or equal: declaration order swapped **99.09**; `bool tooWide;` then
`tooWide = wrap;` **99.09**; the word-count test split into a nested `if` **99.09**; the width test
split into a nested `if` **99.09**; redundant parentheses on both tests **99.09**; operands of the
width sum swapped **99.06**; `!= 0` instead of `> 0` **99.06**; `tooWide = <the comparison>` instead of
a nested assignment **96.05**. The earlier runs' list is in
`docs/goal-notes/progress-prime1-ctextexecutebuffer.md` (single reused `wrap`, `> 1`, an extra
`const int` local, folded `&&`, `goto`, a nested-`if` and a flat-`&&` form). Every spelling in those
lists plus the nine measured this run leaves those four assignments alone.

WALL: AddImage__18CTextExecuteBufferFRC13CFontImageDef 99.09% - the residual is the physical
register numbers of four values (retail r28/r29 for the bools and r27/r26 for the widths, MWCC the
other way round) and nothing else; nineteen source spellings measured this run and in the three
earlier runs all leave those four assignments alone.

**`WrapOneLTR` 95.38%** - the whole gap is one contiguous hunk: retail keeps
`mCurrentLine->GetWidth()` in r24 and `mCurrentBlock->GetOutputWidth()` in r28 **across** the
`MoveWordLTR()` call and reuses both in the `rem` estimate (`subf r4,r24,r28`), while mwceppc
rematerialises the member loads and reloads them (`lwz r5,8(r5) / lwz r4,12(r4) / subf r4,r5,r4`).
Naming them as locals is what forces register allocation, and it makes things worse - measured this
run: both `const int lineWidth` + `const int blockWidth` used in all three places **93.24**; the same
non-`const` **93.24**; the two declared in the other order **93.24**; `blockWidth` only **91.96**;
`lineWidth` only **91.99**. The earlier runs measured an inline `rstl::min_val`/`max_val` variant
**95.38** (same as head), explicit `if`/`else` clamps **93.45**, a `const int estimate` local
**95.09**, and hoisted widths **95.44** ("noise", and it renumbers r24 to `this`).

WALL: WrapOneLTR__18CTextExecuteBufferFPCwi 95.38% - retail keeps the line width and block output
width in r24/r28 across the MoveWordLTR() call and mwceppc rematerialises the loads instead; hoisting
them into named locals, which is the only lever, renumbers the whole frame and measures 91.96-93.24.

**`tools/unit_fit.sh Kyoto/Text/CTextExecuteBuffer.cpp`**: 86 functions present in ours but not in
the retail unit object, 8892 bytes; sections over the claimed range by 8908 bytes. Unchanged in kind
from the previous run's 89 functions / 9360 bytes / 9160 bytes, and the unit cannot flip while 2 of
46 functions are short of 100% anyway.

NEW: none. Both remaining gaps are register allocation inside functions this run measured, which is
what this notes file is for, and the target for either would be this same unit.

## A note for the next run of this unit

`inline` on a template function in a shared header is worth re-testing before recording a wall. The
`WALL:` in the third run of `docs/goal-notes/progress-prime1-ctextexecutebuffer.md` reads "MWCC will
not inline ours ... bare or with the inline keyword removed"; the keyword had to be *added* to
`advance_iterator`, and `rstl::advance` - the helper the old spelling used - never needed it. An
out-of-line callee that retail inlines is not always an inline-size problem.
