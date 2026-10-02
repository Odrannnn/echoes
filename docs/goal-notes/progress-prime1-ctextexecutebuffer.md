# progress-prime1-ctextexecutebuffer

`kind: progress`, `target: Kyoto/Text/CTextExecuteBuffer`. One source file changed:
`src/Kyoto/Text/CTextExecuteBuffer.cpp` (+193 / -19). Nothing else touched; no `configure.py`,
no `config/`, no `tools/`, no `build/goal/` except this notes file.

## Result, measured

| | before (`build/goal/judge/report.base.json`) | after |
|---|---|---|
| unit `matched_functions` | **12 / 46** | **24 / 46** |
| unit fuzzy | 47.92% | **75.89%** |
| project `matched_functions` | 9765 | **9777** |
| project fuzzy | 30.038% | **30.077%** |
| project linked | 4895 | 4895 (unchanged, as expected: the unit stays `NonMatching`) |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

    matched  9765 -> 9777   linked 4895 -> 4895   (+12 functions at 100%, 0 units newly linked)
    ... 12 x "+100%  main/Kyoto/Text/CTextExecuteBuffer :: <name>"
    no regression

`./tools/decomp_build.sh` -> `All: 30.08% fuzzy, 21.97% matched, 11.74% linked (9777 / 28465 functions)`
(fuzzy and matched both rose; linked is unchanged, which is correct for a `progress` item).
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`./tools/probe_sources.sh` = `745 files, 0 failed, 0 errors; link: LINKED (250 undefined, 0 duplicates)`;
`python3 tools/check_symbol_names.py` = `0 declared names are missing from their object`;
`python3 tools/check_decl_order.py --unit Kyoto/Text/CTextExecuteBuffer` = `ok`.
`./tools/gate.sh build/goal/judge/report.base.json` is green on every step except **docs claims**,
which only reports the two derived counts in the `docs/HANDOFF.md` state block
(`matched 9777 / 28465`, `DOL units 8366 / 16726`) - the driver rewrites those, so it is expected
here and I did not touch the doc.

## Per function: before% -> after%, and what Prime 1's source needed

Prime 1's `src/Kyoto/Text/CTextExecuteBuffer.cpp` (read-only clone at
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref`) is the reference throughout.
ECGC 2.7 vs GCC 1.3.2 made no difference anywhere that is still short of 100% - every remaining
gap below is a source-shape or register-allocation difference, not a compiler-version one.

| function | size | before | after | Prime 1's source |
|---|---|---|---|---|
| AddLineSpacing | 180 | 95.56 | **100.00** | needed edit (named local, see lesson 1) |
| AddLineExtraSpace | 184 | 95.65 | **100.00** | needed edit (same) |
| AddCharacterExtraSpace | 184 | 95.65 | **100.00** | not in Prime 1 at all; same edit applied by hand |
| AddPushState | 172 | 94.81 | **100.00** | needed edit (same) |
| AddPopState | 224 | 92.86 | **100.00** | needed edit (same) |
| AddFont | 460 | 96.52 | **100.00** | matched unchanged except the local (lesson 1) |
| StartNewWord | 220 | 89.29 | **100.00** | needed edit (same) |
| AddStringFragment | 128 | 93.75 | **100.00** | needed edit: `int i = 0;` must sit *above* the `if`, not inside it |
| AddString | 368 | 1.09 | **100.00** | **verbatim**, only the member names renamed (`mCurLine`->`mCurrentLine` etc.) |
| BuildRenderBuffer | 260 | 13.63 | **100.00** | **verbatim**, `AUTO(it,..)` -> `InstList::const_iterator it = ..` |
| BuildRenderBufferPage | 340 | 8.13 | **100.00** | **verbatim**, same iterator spelling change |
| BuildRenderBufferPages | 500 | 5.12 | **100.00** | Prime 1 **does not** compile here: Echoes replaces `buffer.HasSpaceAvailable(CVector2i(0,0), extent)` with `(*it2)->IsLineInstruction() && state.GetY() > extent.GetY()`. Everything else verbatim. |
| AddImage | 508 | 28.20 | 94.91 | Prime 1 needed two edits: `TestLargestImage` takes `GetHeight()` (not `GetMonoHeight()`), and the word-count test is `> 0` (not `> 1`) |
| WrapOneLTR | 756 | 1.03 | 95.38 | Prime 1 **does not** compile here: Echoes seeds `rem` from `(blockWidth - lineWidth) / font->GetMonoWidth() * 2` clamped to `[1, len]` instead of from `len`. Everything else verbatim. |
| StartNewLine | 268 | 58.01 | 58.01 | unchanged - blocked, see below |
| MoveWordLTR | 408 | 78.85 | 78.85 | unchanged - blocked, see below |
| Add | 112 | 78.71 | 78.71 | inline in the header, not in Prime 1's cpp |

Everything else in the unit is unchanged and no function anywhere got worse (`report_diff` says
`no regression`, and none of the 46 rows above moved down).

## Codegen lessons (the reusable part)

1. **`Add(rs_new X(...))` destroys the `rstl::ncrc_ptr<CInstruction>` at the end of the statement;
   a named local destroys it at the end of the function.** Retail (and Prime 1) always used the
   named local, and every function that also does a `mState.Set...()` after the `Add` shows retail
   emitting the state write *before* the `rstl::rc_ptr<CInstruction>` release call
   (`stfs f31,144(r31)` then `addi r3,r1,12; bl <ReleaseData>`), while the inlined form emits the
   release first. Rewriting `Add(rs_new X(...))` into
   `const rstl::ncrc_ptr< CInstruction > instruction = rs_new X(...); Add(instruction);`
   took **five functions to 100% in one edit** (AddLineSpacing, AddLineExtraSpace,
   AddCharacterExtraSpace, AddPushState, AddPopState) and fixed AddFont, StartNewWord and
   AddStringFragment's remaining bytes too. The release call is a *local* `bl` in both cases, so
   the objdiff relocation name does not have to agree.
2. **`rstl::min_val(len, est)` then `rstl::max_val(1, rem)`** reproduces retail's two clamps
   exactly - `cmpw est,len; bge; mr res,est` followed by `cmpwi res,1; li 1; ble; mr res,1`-style
   sequences. Hand-written `if (est >= len) rem = len; else rem = est; if (rem <= 1) rem = 1;`
   compiles to the *inverted* branches (`blt` / `bgt`) and does **not** match. Same trick as
   `rstl::max_val` in `TerminateLineLTR`, which was already 100% before this item.
3. A counter declared *inside* the `if` that tests it is initialised after the branch; retail
   initialises before it. `AddStringFragment` needed `int consumed = 0;` hoisted above the
   `if (mCurrentBlock->GetTextDirection() == kTD_Horizontal)`.
4. `AddFont` needed only lesson 1 - Prime 1's `(*mState.GetFont())->GetMonoWidth()` is the same
   thing as this repo's `mState.GetFont()->GetMonoWidth()` (Echoes' `TToken::operator->`), so no
   adaptation was needed.

## What is still blocked, with the evidence

**`StartNewLine` (58.01%) and `MoveWordLTR` (78.85%) are stuck on MWCC inlining
`CLineInstruction`'s constructor.** Retail calls an out-of-line copy that lives *in this same
object*: `bl 0x802B7C74` (`fn_802B7C74`, 80 bytes, reported at 0.00% because our object never
defines it). Our object inlines the 52-byte member-initialisation at both call sites instead
(`stw r0,4(r3) ... stw r6,40(r3); stb r8,48(r3)`, 11 stores), which is what `CLineInstruction.hpp`'s
in-class constructor produces today. `tools/unit_fit.sh` shows the retail object *does* contain
that function, so the bytes are retail's; only the inline decision differs. Fixing it means
changing `include/Kyoto/Text/CLineInstruction.hpp`, and that unit is `Matching` at **9/9** today, so
it is a separate item with its own regression risk - I did not touch it here.

**`Add` (78.71%)** is an inline in `CTextExecuteBuffer.hpp` (`push_back` + `advance_iterator(begin(), -1)`).
Retail's body calls a list helper, then reads the node pointer back out of the list head and stores
it to the return slot. Not attempted beyond reading the disassembly.

**`AddImage` 94.91% / `WrapOneLTR` 95.38%** - both are register allocation only, and the spellings
tried are listed so the next run does not repeat them:

- `AddImage`, best 94.91%: `bool wrap = false; if (mState.IsWordWrapping() && mCurrentLine->GetWidth() + image.GetWidth() > mCurrentBlock->GetOutputWidth()) { wrap = mCurrentLine->GetWordCount() > 0; }`.
  Retail materialises *two* booleans with `li r28,0 / mr r29,r28 ... li r29,1 ... li r28,1` and
  keeps them in callee-saved registers; ours folds the comparison into a normalised
  `xor/srawi/and/subf/srwi` sequence and saves only r27-r31. Also tried, all measured: single
  `wrap` variable reused as in Prime 1 **82.51%**; same with `> 1` -> `> 0` **82.72%**; two
  variables `tooWide` + `wrap` **93.45%**; declaration order swapped **93.45%**; `!= 0` instead of
  `> 0` **93.45%**; extra `const int width` local **85.58%**; `wrap = false` with `&&` folded
  manually **86.18%**; `goto` form **90.73%**.
- `WrapOneLTR`, best 95.38%: inline `rstl::min_val`/`max_val` (the four-instruction difference is
  in the second word-wrap `if`; retail holds the line width in r24 and the block width in r28
  across the `MoveWordLTR()` call and reuses them for the estimate, we reload them). Also tried:
  explicit `if`/`else` clamps **93.45%**, nested `max_val(1, min_val(len, est))` **95.38%** (same
  as the flat form), `const int estimate` local **95.09%**, hoisted `lineWidth`/`blockWidth`
  locals **worse** (unit 72.03% and the first `if` also changed registers).

**`tools/unit_fit.sh Kyoto/Text/CTextExecuteBuffer.cpp`**, before -> after: `.text` claimed 9024,
ours **11080 -> 17936** (over by 2056 -> 8912); functions present in ours but not in the retail
unit object **56 -> 101**, 6328 -> 10732 bytes. The growth is COMDAT weak template code that
`BuildRenderBuffer*` now pulls in (`__ct__Q24rstl36vector<b,...>`, `__ct__17CTextRenderBufferFRC...`,
`__dt__16CFontRenderStateFv`, ...). Retail keeps the same code, but dtk's split put it in other
units, so our object is further from the claimed range than before. The unit could not flip anyway
(it was 2056 bytes over with 24 of 46 functions unmatched), and the judge for a `progress` item is
`report.json`'s per-function exact matches, so this is recorded rather than hidden.

## Leads for the next run (not filed as `NEW:` - same target unit)

- `GetWidth__13CFontImageDefCFv` and `GetHeight__13CFontImageDefCFv` (0x802B889C / 0x802B8920, 132
  bytes each) are reported **unnamed-target inside this unit's range** and sit at 0.00% because
  `include/Kyoto/Text/CFontImageDef.hpp` declares them but `src/Kyoto/Text/CFontImageDef.cpp` never
  defines them, and `Kyoto/Text/CFontImageDef.cpp` is `Matching` at 5/5 today. `AddImage` now calls
  both, so making them available would plausibly add 2 more matched functions here - but doing it
  in the .cpp would change a `Matching` unit's object and break the build, and the fix belongs to
  whichever unit dtk's split really attributes those 264 bytes to. Worth one `unit_fit`/
  `range_owner` check before spending a lane on it.
- `fn_802B7098` (124 bytes) is `__dt__16CFontRenderStateFv` as a weak COMDAT copy; our object
  emits it at exactly 124 bytes since `BuildRenderBuffer` went in, it is only unpaired because
  `config/G2ME01/symbols.txt` has no name at 0x802B7098. A rename there is a one-line config change
  I did not make because it is a `config/` edit, not source, and the item asked for decompilation.

---

# Run of 2026-10-02 (lane 7), re-measured on top of the two earlier runs

Three files changed: `include/Kyoto/Text/CLineInstruction.hpp`,
`src/Kyoto/Text/CTextExecuteBuffer.cpp`, `config/G2ME01/symbols.txt`. No
`configure.py`, no `files.cmake`, no `splits.txt`, no `tools/`, no asm.

## Result, measured

| | before (`build/goal/judge/report.base.json`) | after |
|---|---|---|
| unit `matched_functions` | **26 / 46** | **38 / 46** |
| unit fuzzy | 78.82% | **95.18%** |
| project `matched_functions` | 12386 | **12398** |
| project linked | 5863 | 5863 (unchanged, correct for a `progress` item) |

`python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json`:

    matched  12386 -> 12398   linked 5863 -> 5863   (+12 functions at 100%, 0 units newly linked)
    ... 12 x "+100%  main/Kyoto/Text/CTextExecuteBuffer :: <name>"
    13 x "RENAMED ... (0.00% -> 100.00%)", 1 x "(0.00% -> 33.29%)"
    no regression

`./tools/decomp_build.sh` -> `All: 35.02% fuzzy, 28.69% matched, 12.90% linked (12398 / 28465 functions)`.
`sha1sum build/G2ME01/main.dol` = `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`;
`./tools/probe_sources.sh` = `752 files, 0 failed, 0 errors; link: LINKED (289 undefined, 0 duplicates)`;
`python3 tools/check_symbol_names.py` = `0 declared names are missing from their object`;
`python3 tools/check_decl_order.py --unit Kyoto/Text/CTextExecuteBuffer` = `ok`.
`./tools/goal_check.sh build/goal/item.json` -> **`goal_check: PASS`** (every step ok, including
`target rose: main/Kyoto/Text/CTextExecuteBuffer: 26 -> 38 / 46 functions`).

## The previous run's "wall" on StartNewLine / MoveWordLTR was not a wall

Both earlier runs recorded that these two were blocked because MWCC inlines
`CLineInstruction`'s constructor at both call sites while retail calls an out-of-line copy
(`fn_802B7C74`, 80 bytes, in this same object), and that the fix "belongs to
`include/Kyoto/Text/CLineInstruction.hpp`, a `Matching` unit". That is the whole of the
obstacle, and it is reachable: `CTextExecuteBuffer.cpp` is the **only** TU that constructs a
`CLineInstruction` (grep over `src/` + `include/`: `CLineInstruction.cpp`, `CTextInstruction.cpp`,
`CImageInstruction.cpp`, `CWordInstruction.cpp` include the header but never construct one).
So the constructor was declared in the header and defined in `CTextExecuteBuffer.cpp`,
between `StartNewLine` and `MoveWordLTR` (that position is what the reverse-source-order rule
in `docs/goal-unit-prompt.md` requires). `Kyoto/Text/CLineInstruction.cpp` is unchanged and
still MatchingFor 9/9; nothing that is linked references the symbol.

Per function, this run (before% -> after%), all measured on this tree:

| function | before | after | how |
|---|---|---|---|
| `MoveWordLTR` | 78.85% | **97.01%** | out-of-line ctor + `static_cast<bool>(mImageBaseline)`; 408/408 bytes now |
| `StartNewLine` | 58.01% | **83.27%** | out-of-line ctor + `mImageBaseline ? true : false`; 268 bytes now (was 292) |
| `AddImage` | 94.91% | **99.09%** | two bools, `tooWide` then `wrap`; 508/508 bytes now |
| `Add` | 78.71% | 78.71% | unchanged, see below |
| `WrapOneLTR` | 95.38% | 95.38% | unchanged, see below |

### Codegen notes for the next run

- **Retail loads `mImageBaseline` (0xC4) *first*, before the other five ctor arguments.** With a
  bare `mImageBaseline` MWCC sinks the `lbz` to just before the `bl`; `MoveWordLTR` measures
  90.20% that way against retail's 78.85% with the ctor inlined. `static_cast<bool>(...)` /
  `x ? true : false` forces the load early and reaches 97.01%, at the cost of three extra
  `neg / or / srwi` (MWCC re-normalises the byte to a canonical bool; retail does not).
  Measured and rejected: named local read at the top of `MoveWordLTR` (r30 spill, 93.14%);
  named local read just before the `rs_new` (84.61%); `const bool` on the parameter (90.20%
  plain / 97.01% with the ternary); `int` parameter (97.01% but the mangled name changes to
  `...Fiiii...`, which breaks the 0x802B7C74 name).
- **`AddImage` uses two bools.** Retail (0x802B8664-0x802B8700): `li r28,0 / mr r29,r28`,
  the width test sets `r29`, `clrlwi. r0,r29,24 / beq`, the word-count test sets `r28`, and
  `StartNewLine` is called only on `r28`. That is
  `bool wrap = false; bool tooWide = wrap; if (width-test) tooWide = true;
  if (tooWide && GetWordCount() > 0) wrap = true; if (wrap) StartNewLine();` — 99.09%, sizes
  equal. Declaring `tooWide` first measures the same 99.09%. Measured and rejected on top of
  that: single reused `wrap` (82.51% in the earlier run), `> 1` instead of `> 0` (82.72%),
  `!= 0` (93.45%), nested `if` only (96.32%), one flat `&&` chain (86.35%).
  What is left is register numbering only: retail puts the bools in r28/r29 and the two
  widths in r27/r26, we put the bools in r27/r26 and the widths in r28/r29, and retail emits
  `mr r29, r28` where we emit `li r26, 0` (MWCC constant-folds `bool tooWide = wrap;`, retail
  did not). 12 bytes of 508.
- **`StartNewLine`'s frame is 0x30 in retail and 0x20 in ours** (268 vs 236 bytes) and retail
  carries four extra instructions (`stw r3,0x18(r1)`, `lwz r4,0(r3)`, `addi r0,r4,1`,
  `stw r0,0(r3)`, `addi r3,r1,0xc`, `bl fn_80025CC0`) around the second insert. That is
  `StartNewWord`'s refcount bump materialised in the caller instead of behind `Add()`'s
  inlined `push_back`, so closing it needs a different decomposition of `StartNewWord`/`Add`,
  not a spelling change.

## The 15 unnamed retail functions were the real ceiling

This is the finding the next run should not have to rediscover. The unit's 46 functions include
**15 that retail's own map file left unnamed** (`fn_802B6F84`, `fn_802B6FFC`, `fn_802B7070`,
`fn_802B7098`, `fn_802B798C`, `fn_802B79B4`, `fn_802B7A24`, `fn_802B7C34`, `fn_802B7C74`,
`fn_802B8850`, `fn_802B8CF8`, `fn_802B8DB8`, `fn_802B8DF8`, `fn_802B8EE8`, `fn_802B903C`).
objdiff pairs functions **by name**, so all 15 sat at 0.00% with no `fuzzy_match_percent` at all
however exact our bytes were - and no source edit can fix that, because `config/G2ME01/symbols.txt`
is the only thing that names a retail function. Both earlier runs saw this (`fn_802B7098` was
recorded as a "one-line config change I did not make"); the previous run's note also claimed
retail's object contains only 56 functions we lack, and that our object grew - both true, and
irrelevant to the ceiling.

Measured here: **12 of those 15 are byte-identical to a function our own compiler emits** for
the same entity. `build/G2ME01/obj/Kyoto/Text/CTextExecuteBuffer.o` is the retail object (dtk
`split`); `build/G2ME01/src/...` is ours (the mwcc object); objdiff's `-1/--target` is the retail
one, which is the opposite of what the field names in `objdiff.json` suggest. Renaming an entry
can only ever *reveal* a difference, never hide one, so the renames below are measurements, not
assertions: eleven reach 100.00% and one reaches 33.29%, and that one is left in because 33.29
is what our `rstl::list` really produces for it.

Each name was assigned by **caller matching**, not by guessing from the bytes: the retail
function's callers in `build/G2ME01/asm/Kyoto/Text/CTextExecuteBuffer.s` were matched against the
callers of each candidate in our object, and only then written into `symbols.txt`.

| retail placeholder | -> name | called by (retail) | after |
|---|---|---|---|
| `fn_802B798C` | `push_back__Q24rstl66list<...ncrc_ptr<12CInstruction>...>FRC...` | `Add` | 100% |
| `fn_802B79B4` | `do_insert_before__Q24rstl66list<...ncrc_ptr<12CInstruction>...>` | `fn_802B798C`, `fn_802B7C34` | 33.29% |
| `fn_802B7C34` | `insert__Q24rstl66list<...ncrc_ptr<12CInstruction>...>` | `MoveWordLTR` | 100% |
| `fn_802B7098` | `__dt__16CFontRenderStateFv` | the three `BuildRenderBuffer*` | 100% |
| `fn_802B8850` | `__ct__17CImageInstructionFRC13CFontImageDef` | `AddImage` | 100% |
| `fn_802B8CF8` | `__ct__17CBlockInstructionFiiii14ETextDirection14EJustification22EVerticalJustification` | `BeginBlock` | 100% |
| `fn_802B8DB8` | `clear__Q24rstl66list<...ncrc_ptr<12CInstruction>...>Fv` | `Clear` | 100% |
| `fn_802B8DF8` | `erase__Q24rstl66list<...ncrc_ptr<12CInstruction>...>` | `fn_802B8DB8` | 100% |
| `fn_802B8EE8` | `__advance<Q34rstl66list<...ncrc_ptr<12CInstruction>...>8iterator,i>__4rstlF...` | `Add` | 100% |
| `fn_802B903C` | `do_erase__Q24rstl66list<...ncrc_ptr<12CInstruction>...>` | `fn_802B8DF8` | 100% |
| `fn_802B7070` | `push_back__Q24rstl52list<17CTextRenderBuffer,...>FRC17CTextRenderBuffer` | `BuildRenderBufferPages` | 100% |
| `fn_802B6FFC` | `insert<Q34rstl52list<17CTextRenderBuffer,...>14const_iterator>__4rstlF...` | `fn_802B6F84` | 100% |
| `fn_802B7C74` | `__ct__16CLineInstructionFiii14EJustification22EVerticalJustificationb` | `StartNewLine`, `MoveWordLTR` | 100% |

Two honest caveats. First, several of these bodies are identical across instantiations
(`push_back` for two different list types, for instance), so the *bytes* cannot distinguish
them; the caller chain above is what pins each one down, and it is unique for every row.
Second, `report_diff.py` prints its RENAMED rows in sorted order, so its `old -> new` arrows
do not line up with the addresses - read the table, not the arrows.

Two are still unnamed and still 0.00%, because we emit nothing mnemonically identical:
`fn_802B7A24` (120 bytes, `create_node` for the instruction list; ours is a different size) and
`fn_802B6F84` (120 bytes, called only by `BuildRenderBufferPages`). `do_insert_before` at 33.29%
is a real difference: our `do_insert_before` for `list<ncrc_ptr<CInstruction>>` is 176 bytes
against retail's 112.

None of these twelve symbols is defined by any linked object (`find build/G2ME01/src -name '*.o'`
+ `nm` for each: only `CTextExecuteBuffer.o`, which `build.ninja` marks `linked False`), so the
linker script gains no duplicate; `main.dol` still hashes to the retail sha1, which is the gate
that would catch a rename that changed anything.

## Still open, with the evidence

- **`Add` (78.71%)** is an inline in `CTextExecuteBuffer.hpp` (`push_back` + `advance_iterator`).
  Retail's body is 28 instructions and stores the `__advance` result to the return slot through a
  stack-local iterator pair; ours is 23 and passes `this` where retail passes a local. Not
  attempted beyond reading the disassembly. Prime 1 has no `Add` in its .cpp either.
- **`WrapOneLTR` (95.38%)**: the four-instruction difference is register allocation in the two
  word-wrap tests - retail keeps the line width in r24 and the block width in r28 across the
  `MoveWordLTR()` call and reuses them; we reload through r5/r4. Hoisting `lineWidth`/`blockWidth`
  into locals was already measured as worse by the previous run, and I did not repeat it.
- **`AddImage` 99.09% / `MoveWordLTR` 97.01% / `StartNewLine` 83.27%** - register allocation and,
  for `StartNewLine`, frame size only; the per-function spellings tried are in the sections above.
- `tools/unit_fit.sh Kyoto/Text/CTextExecuteBuffer.cpp`: 89 functions in ours but not in the
  retail unit object, 9360 bytes, sections over the claimed range by 9160 bytes. That is the
  COMDAT weak template code `BuildRenderBuffer*` drags in; unchanged in kind from the previous
  run's measurement, and the unit cannot flip while 8 of 46 functions are short of 100%.

NEW: none. The remaining gaps in this unit are register allocation inside functions this run
measured, which is not the kind of item a lane should be handed, and `fn_802B6F84`/`fn_802B7A24`
need a new disassembly reading rather than a queue entry.
