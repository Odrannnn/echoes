# progress-unit-cguitextsupport

`Kyoto/Text/CGuiTextSupport`, one bounded slice: three functions taken to 100% by source-shape
changes only. The unit stays `NonMatching`; no config, no carve, no asm.

## Measured before and after

| | before | after |
|---|---|---|
| unit matched_functions | 22 / 79 | **25 / 79** |
| unit fuzzy | 42.73% | 43.18% |
| `All:` matched | 11842 / 28465 | **11845 / 28465** |
| linked | 5727 | 5727 (unchanged) |

Baseline measured from the clean tree before the first edit, not recalled from `item.json`
(the queue said 22/79; that was right). Judge: `./tools/goal_check.sh build/goal/item.json`
-> `PASS`, every `ok` line including `gate.sh` and `target rose: 22 -> 25 / 79`.

## The three, per function

Sources are `src/Kyoto/Text/CGuiTextSupport.cpp`; spellings were tried with
`tools/try_batch.py`, which counts differing *instructions* against
`build/G2ME01/obj/Kyoto/Text/CGuiTextSupport.o`.

### 1. `SetWordWrap__15CGuiTextSupportFb` - 83.85% -> **100%** (52 B, 13 instrs)

Only the operand order of the comparison. Ours loaded the member into r5 and the normalised
parameter into r0; retail loads the member into **r0** and the normalised parameter into **r5**
(`clrlwi r5,r4,24` / `lbz r0,248(r3)`).

`if (mProperties.mWordWrap != wordWrap)` -> `if (wordWrap != mProperties.mWordWrap)`.

Tried and unchanged (all 3 differing instrs): the baseline, `this->ClearRenderBuffer()`, a local
copy of the member, `!(a == b)`. Both operand orders of the `this->` form behaved the same.

### 2. `SetText__15CGuiTextSupportFRCQ24rstl66basic_string<c,...>b` - 80.38% -> **100%** (104 B, 26 instrs)

Retail materialises the converted string at r1+8, then **copy-constructs a second wstring at
r1+24** and passes *that* to the wide overload. Passing the temporary directly gives one wstring
and a 32-byte frame instead of 48.

```cpp
rstl::wstring wtext = CStringExtras::ConvertToUNICODE(text);
SetText(wtext, multipage);
```

Tried: the direct temporary (16 differing), `rstl::wstring(CStringExtras::ConvertToUNICODE(text))`
as the argument (9 - the copy happens in the wrong place), both parenthesised and unparenthesised
named-local forms (both 0). So the difference is "named local vs temporary argument", not the
spelling of the conversion.

### 3. `Render__15CGuiTextSupportCFv` - 90.05% -> **100%** (152 B, 38 instrs)

Retail builds a `CVector3f` in the outgoing argument slot and calls the **vector** overload
`CTransform4f::Scale(CVector3f const&)`; ours called the three-float overload, which is a leaf
call taking f1/f2/f3 and gives a 160-byte frame against retail's 176.

```cpp
CGraphics::SetModelMatrix(oldModel * CTransform4f::Scale(CVector3f(1.f, 1.f, -1.f)));
```

Tried: the three-float form (18 differing), the vector form with a named temporary (0, same code).

## What I did not get to, and what I measured on it

These are the closest remaining ones. All are **layout or one-instruction-shape** differences, not
missing work, so I stopped rather than burn the item on them.

**`CheckAndRebuildRenderBuffer__15CGuiTextSupportCFv`, 93.15%, 336 B - 5 differing instrs.**
Everything matches except where the `return false` block sits. Retail branches *over* the body to
the `li r3,0` at 0x8027c258 and reaches the tail by falling through; ours materialises
`li r3,0; b <epilogue>` inline right after the test and jumps over the body. Same 83/84
instructions, two block layouts. Tried, all 5-6 differing: baseline, no braces on the
`if (!_GetIsTextSupportFinishedLoading())`, `== false`, `!(...)`, a `bool loaded` local, an
`else` wrapping the rest of the body, and `goto not_loaded` (5 - best, still 5). This is MWCC's
block ordering on an early `return` from inside a nested `if`; I could not find the spelling that
moves the block.

**`_GetIsTextSupportFinishedLoading__15CGuiTextSupportCFv`, 77.65%, 264 B - 6 differing instrs.**
Two separate shape differences, and I fixed neither:
- retail normalises `IsFinishedLoading()` with `mr r31,r3` and returns it raw; ours adds
  `clrlwi. r0,r3,24` / `beq` / `li r31,1` because the `&&` forces a bool register;
- the tail `return !mAssets.empty()` is `cmpwi r3,0` / `bne` in retail (a test on `mAssets.mCount`)
  but compiles to `neg`/`or`/`srwi` for us, i.e. a *value* computation, not a test. Restructuring
  the tail as `if (...) return true; return false;` did not change it. So the difference is in how
  `vector::empty()` is expressed for MWCC here, not in the caller's control flow - and
  `rstl::vector::empty()` is shared, so I did not touch it.
  Best of 14 spellings: 6 differing (`if (!font.IsLoaded()) return false;` + `mAssets.size() != 0`).

**`AddText__15CGuiTextSupportFRCQ24rstl66basic_string<w,...>b`, 72.40%, 188 B - 8 differing instrs.**
The `push_back` tail. Ours re-checks capacity inline (`cmpw r0,r5` / `blt`, then a `bl` to
`reserve`); retail has no check - it stores into `mItems[mCount]` directly, because
`reserve(size + 1)` on the line above already guaranteed the capacity. `push_back_unsafe` is the
obvious candidate and gets to 8 (it removes the check) but still differs in where the null test
and the `mCount` store land. The rest of the function, including the `psq_st f31` frame and the
`max_val` select, already matches. Tried: `push_back`, `push_back_unsafe`, `push_back_unsafe` with
a named pair, `resize` + `operator[]`.

**`ClearRenderBuffer__15CGuiTextSupportFv`, 70.04%, 100 B - 8 differing instrs.**
Retail calls an out-of-line copy of `optional_object<CTextRenderBuffer>::clear()`
(`fn_8027C0B0`) that tests the valid flag and calls the destructor; ours inlines the whole thing
(lbz / cmplwi / beq / call dtor / stb). This is an MWCC inline-size decision on
`rstl::optional_object::clear()`, shared across the tree.

**`CheckAndRebuildTextBuffer__15CGuiTextSupportCFv`, 61.89%, 592 B - 81 differing instrs.**
The real gap. Retail converts the four colour channels through **floating point**
(`psq_l` pairs, `fmuls` by 255 and by 1/255, four `fctiwz`, then `stfd`/`lwz` to pick the integer
out) where ours does four `lbz`. So retail's `CTextColor(...)` argument is not
`GetRedu8()`-style byte extraction - it is `(uchar)(GetRed() * 255.f)` per channel. That is a
change to how the colours are spelled in this function, and it is the highest-value next step in
this unit; it needs the `1/255` and `255` constants to be the same ones retail loads
(`-17120(r2)` and `-17136(r2)`), which I did not identify.

**`GetCurrentPageRenderBuffer__15CGuiTextSupportCFv`, 32.27%, 104 B - 6 differing instrs.**
Purely the walk loop. Retail keeps a separate induction variable and a plain `cmpw`/`bne`
bottom-tested loop; MWCC unrolls ours 8x with `srwi`/`mtctr` because it can derive the trip count
from `mPageCounter`. `do { ++it; ++i; } while (i != mPageCounter)` gets to 6 (it stops the unroll)
but leaves the `lwz r3,3308(r3)` hoist on the wrong side. Tried: 20 spellings - `for` with `!=`,
`<`, `> 0` and post-decrement, `while (i++ != n)`, `std::advance`, a raw `node*` walk, a
nested `if (it != mPages.end())`, `do/while` with the increment in either order, unsigned counters,
and `get_pointer()` instead of `&*it`.

**`__ct__` (92.35%, 508 B) and `__dt__` (89.36%, 188 B)** were not worked on. `__dt__` is 5
differing instrs, all of it an inline-vs-out-of-line call to the `optional_object` destructor, the
same lever as `ClearRenderBuffer` above.

## Next step for this unit

Two independent levers, both in shared headers, which is why I left them:
1. `rstl::optional_object<CTextRenderBuffer>::clear()` / `~optional_object` - making those
   out-of-line copies is worth `ClearRenderBuffer` (70%) and `__dt__` (89%) together.
2. How a `CColor`'s channels are turned into `CTextColor` - the float path in
   `CheckAndRebuildTextBuffer` is worth the most bytes in the unit (592 B at 62%).

Both would touch headers other units include, so each needs its own item with the full gate, not a
drive-by edit from here.

## Verified

- `./tools/goal_check.sh build/goal/item.json` -> `PASS progress-unit-cguitextsupport`, all `ok`,
  including `no judge-owned path touched`, `no asm added`, `gate.sh`, and
  `target rose: 22 -> 25 / 79`.
- `sha1sum build/G2ME01/main.dol` -> `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010` (unchanged).
- `flip_test.sh Kyoto/Text/CGuiTextSupport.cpp` **FAILS**, as it must: this is a `progress` item,
  the unit has 54 unmatched functions and stays `NonMatching`. Not a regression - the flip test
  fails on this unit before my change too. No `flip_test` result is claimed.
- Diff is 3 hunks in one file, `src/Kyoto/Text/CGuiTextSupport.cpp` (+10 / -3). `docs/HANDOFF.md`'s
  state block moved by `tools/sync_state_block.py` during the gate; that is the tool's own doing and
  the judge rewrites it from the tree.
- Not committed.
