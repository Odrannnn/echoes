# progress-prime1-csavegamescreen — MetroidPrime/CSaveGameScreen

Lane 8, 2026-09-30. Ported four of Prime 1's functions and turned `SetUIColors` from a TODO into
real code. The unit stays `NonMatching` (it is 16/24 functions and carries data a neighbouring
unit owns), as a `progress` item requires. No commit made; no judge-owned path touched.

## Result, measured (`build/report.json` vs `build/goal/judge/report.base.json`)

    unit main/MetroidPrime/CSaveGameScreen
      matched_functions   11 / 24  ->  16 / 24
      fuzzy                   37.64  ->  48.90
      matched_code            14.77  ->  41.70
    project: All: 30.29% fuzzy, 22.17% matched, 11.74% linked (9829 / 28465 functions)
             matched 9824 -> 9829, linked 4895 -> 4895

    +100%  main/MetroidPrime/CSaveGameScreen :: SelectUIType__15CSaveGameScreenCFv
    +100%  main/MetroidPrime/CSaveGameScreen :: Update__15CSaveGameScreenFf
    +100%  main/MetroidPrime/CSaveGameScreen :: __ct__15CSaveGameScreenF12ESaveContextUx
    +100%  main/MetroidPrime/CSaveGameScreen :: Draw__15CSaveGameScreenCFv
    +100%  main/MetroidPrime/CSaveGameScreen :: SetUIColors__15CSaveGameScreenFv
    no regression          (python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json)

Per function, retail size in bytes, before -> after, and how Prime 1's source fared:

| function | B | before | after | Prime 1's source |
|---|---|---|---|---|
| `SelectUIType` | 340 | 77.74% | **100%** | **unchanged, one edit**: Prime 1 writes the `EError` chain as sequential `if`s; the Echoes file here had a `switch`, which MWCC turns into a jump table (`lis/slwi/lwzx/mtctr/bctr`) retail does not have. Reverting to the `if` chain is the whole diff (20 bytes shorter object). |
| `Update` | 536 | 6.87% | **100%** | **unchanged**. The stub was replaced with Prime 1's body verbatim; only `mSaveCtx`-era names differ (`error` was already read at the top, `mInGame` already existed). First try, no edits. |
| `__ct__` | 768 | 89.76% | **100%** | **one edit, not in Prime 1's file**: `mSaveWorlds.push_back(token)` -> `push_back_unsafe(token)`. See "the vector loop" below. |
| `Draw` | 104 | 64.58% | **100%** | **one edit**: the per-call temporary `CGuiWidgetDrawParms(1.f, CVector3f::Zero())` hoisted to a file-scope `static const CGuiWidgetDrawParms sDrawParms`. Prime 1 writes `CGuiWidgetDrawParms::Default()`; Echoes has no `Default()` and no `CGuiWidgetDrawParms.cpp` unit, so the static lives in this file. Same value, retail's shape. |
| `SetUIColors` | 76 | 5.26% | **100%** | **unchanged** (4 lines), plus a new `include/GuiSys/CGuiTableGroup.hpp`. Retail's 76 bytes confirm both `CColor`s exactly: `stw -1,12(r1)` for `0xffffffff` and four `stb` of 160/160/160/200 at `8(r1)`. |
| `ConstructCardDriver` | 212 | 99.98% | 99.98% | **blocked, see below** (source is already Prime 1's). |
| `SetUIText` | 1440 | 1.15% | 1.15% | **not attempted - unknowable**, see below. |
| `DoAdvance` | 820 | 0.49% | 0.49% | **not attempted - blocked on APIs that do not exist here**, see below. |
| `PumpLoad` | 912 | 27.96% | 27.96% | **not attempted - blocked on `TFunctor`**, see below. |
| `fn_8017DEE8` / `fn_8017D378` / `fn_8017D124` / `fn_8017D188` | 212/168/100/84 | (none) | (none) | unnamed retail functions, see below. |

Every other function in the unit was already at 100% and is unchanged.

## The three non-obvious fixes

### 1. `SelectUIType` — a `switch` where retail has an `if` chain

Retail is a straight-line `cmpwi/bne` chain over `CMemoryCardDriver::EError` (1->5, 2->6, 4->8,
5->9, 6->10, 8->11, 3->7, default->0) and the object was 320 B against retail's 340 B. Six
`if`/`cmpwi`/`bne` pairs replace a seven-entry jump table; the difference is exactly the 20 bytes
measured. The enum values in `include/MetroidPrime/CMemoryCardDriver.hpp:64` already agree with
retail, so no header change was needed. Prime 1 spells it as `if`s, and that spelling is the
reason this one is a 100%.

### 2. `__ct__` — the vector loop has no growth path in retail

Retail's `push_back` inside the `mSaveWorlds` loop is 5 instructions with **no capacity check and
no call to `reserve`**: it computes `mItems + mCount * 8`, writes `mCount = mCount + 1`, and
branches on a null destination. Ours compiled 44 bytes longer because
`rstl::vector::push_back` (`include/rstl/vector.hpp:78`) opens with

    if (mCount >= mCapacity) { reserve(mCapacity != 0 ? mCapacity * 2 : 4); }

The fix at the call site is `push_back_unsafe`, which is `construct(mItems + mCount, in); ++mCount;`
- the same thing once capacity is already reserved, and it is: `mSaveWorlds.reserve(worlds.size())`
runs immediately before, and the loop runs once per world. **Do not "fix" this by editing
`rstl/vector.hpp`** - that header is shared by every unit in the tree and its `push_back` is what
the rest of the retail code was matched against.

### 3. `Draw` and `SetUIColors` need a shared object and a class that does not exist here

* `Draw`: retail does `lis/addi` on a fixed address (`lbl_80411024`, a `.bss` object) and passes it
  straight to `CGuiFrame::Draw`, with no stack temporary. The file-scope
  `static const CGuiWidgetDrawParms sDrawParms(1.f, CVector3f::Zero());` reproduces that and
  drops 6 instructions and 8 bytes of stack.
* `SetUIColors`: this repo has **no** `CGuiTableGroup` at all - only a forward declaration in
  `CSaveGameScreen.hpp` and `CQuitGameScreen.hpp`. The call target is retail's `fn_80279260`
  (`0x80279260`, a loop that calls `CGuiWidget::SetColor` on each row). I added
  `include/GuiSys/CGuiTableGroup.hpp` declaring **only** `SetColors`, with no data members, so no
  layout is invented and no call site can read a wrong offset. It is a declaration without a
  definition on purpose: the body is in a GuiSys unit that is still an `auto/*` split.

## What is blocked, and the evidence

**`ConstructCardDriver` - 99.98% is the ceiling, and it is one immediate.** Instruction-by-
instruction against `build/G2ME01/obj/...` and `build/G2ME01/src/...` (53 instructions, 212 B
both sides), every instruction and every relocation matches except one:

    retail  addi r4,r4,199     <- operator new's __FILE__ string, +199
    ours    addi r4,r4,79      <- mwccppc's unknown-file placeholder "??"(??)", +79

MWCC emits `__nw__FUlPCcPCc(size, file, line)` for `rs_new`; retail's real file string is 120 bytes
further into the string base than the `??(??)` placeholder this build produces. Nothing in the
source changes that, and `objdiff` will not score 99.98% as a match.

**`SetUIText` (1440 B) - the message indices are not knowable from this tree.** Prime 1's version
is a 15-arm `switch` on `mUiType` that loads `STRG_MemoryCard` strings by index (0..28) and
pushes them into five `CGuiTextPane`s. Two things block it: Echoes' string table is a different
resource, so every index would be a guess with no measurement to check it against, and
`CGuiTextPane::SetIsSelectable` does not exist in this repo's header. Writing it would be 40
guessed immediates. **Not attempted on purpose.**

**`DoAdvance` (820 B) - blocked on two APIs that are simply absent.** Prime 1's dispatch needs
`gpMain->SetManageCard(true)` and `gpGameState->GetFileIdx()`; neither `SetManageCard` nor
`GetFileIdx` appears anywhere in `include/` or `src/`. Echoes also has 16 `EUIType` values to
Prime 1's 14 (it adds `kUIT_BusyWritingInitial` and drops the two pre-GM8P `kUIT_Still*` cases),
and retail's 16-way jump table is at an address dtk does not resolve. **Not attempted.**

**`PumpLoad` (912 B) - blocked on the `TFunctor` templates.** Retail's `PumpLoad` installs two
callbacks with `TFunctor1FromMethod`/`TFunctor2FromMethod`; **this repo has no `TFunctor` code at
all** (`grep -rl TFunctor1FromMethod include/ src/` returns nothing). The two unnamed functions
next to it, `fn_8017D124` (100 B) and `fn_8017D188` (84 B), are those templates' invokers -
`memcpy` of a 12-byte pmf plus `__ptmf_scall` - and are emitted only by that machinery. Also
needs `CGuiTableGroup::SetMenuAdvanceCallback` / `SetMenuSelectionChangeCallback`. Adding a
`TFunctor` family is a shared-header change affecting every unit, not this item's work.

**The four unnamed functions.** `fn_8017DEE8` (212 B) is the out-of-line
`rstl::vector::reserve` for this element type and `fn_8017D378` (168 B) is its destructor; ours
emits both as weak COMDAT copies that mwldeppc discards, so objdiff has no counterpart and
reports no score. `unit_fit.sh` lists 12 such COMDATs totalling 1148 B in this unit, all harmless
for a `NonMatching` unit. They cannot be paired by name (`reserve__Q24rstl65vector<...>` vs
`fn_8017DEE8`).

## Verification (all run in `wt-mp2-goal-L8`)

    sha1sum build/G2ME01/main.dol   ->  6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
    ./tools/probe_sources.sh       ->  749 files, 0 failed, 0 errors
    python3 tools/check_symbol_names.py ->  checked 503 units; 0 declared names are missing
    python3 tools/check_files_cmake.py  ->  every configured DOL object is in files.cmake or excluded
    ./tools/decomp_build.sh        ->  All: 30.29% fuzzy, 22.17% matched, 11.74% linked (9829 / 28465)
    python3 tools/report_diff.py build/goal/judge/report.base.json build/report.json
                                   ->  matched 9824 -> 9829, linked 4895 -> 4895, no regression

`flip_test.sh` was **not** run: this is a `progress` item, the unit is meant to stay `NonMatching`,
and 8 of its 24 functions are still short (four unnamed, plus the blocked ones above).

## Files changed

* `src/MetroidPrime/CSaveGameScreen.cpp` - `SelectUIType` if-chain, `Update` body, the
  `push_back_unsafe` call, `sDrawParms` + its use in `Draw`, `SetUIColors` body, the
  `CGuiTableGroup.hpp` include.
* `include/GuiSys/CGuiTableGroup.hpp` - **new**, 18 lines: one class, one declared method, no
  members.

## Lesson for the next lane on this unit

Do not judge a data-reference `ARG_MISMATCH` in objdiff's instruction diff by eye - it is not
what the score uses. `__ct__` showed `lbl_8041C870` vs `skSaveBanner` on ten instructions and
still reached 100% once the vector loop was right. Only the retest counts.

And `rstl::vector::push_back`'s growth branch is real retail code everywhere else; when a unit
calls `reserve(n)` first and loops exactly `n` times, retail's `push_back` inlines down to
`push_back_unsafe`. That is worth checking before blaming a 44-byte constructor.
