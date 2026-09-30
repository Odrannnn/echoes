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
