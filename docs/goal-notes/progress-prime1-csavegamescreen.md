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

---

# Lane 8, 2026-09-30, second attempt on the same item

Re-measured on a clean `wt-mp2-goal-L8` at `goal/lane-8` @ `9070b2af`. **The previous run's work
is already in the branch head**: `build/goal/judge/report.base.json` and `build/report.json` were
byte-identical for this unit (16/24, fuzzy 48.90136), so nothing was `STALE:` - the unit still had
8 unmatched functions. Judge: `PASS` (`tools/goal_check.sh build/goal/item.json`).

## Two of last run's walls were not walls

Last run concluded, in `ConstructCardDriver`:

> MWCC emits `__nw__FUlPCcPCc(size, file, line)` for `rs_new`; retail's real file string is 120
> bytes further into the string base than the `??(??)` placeholder this build produces. **Nothing
> in the source changes that.**

That is wrong, and it cost the function. **The immediate is an offset into this translation unit's
own `.rodata`, so adding string literals moves it.** Measured:

    retail  addi r4,r4,199       ours  addi r4,r4,79      (only differing instruction)

`199` is where retail's `??(??)` sits in this unit's `.rodata`; `79` is where ours sits. Retail's
`.rodata` has 120 bytes of literals ours did not, and they are exactly the seven widget names
`PumpLoad` binds. Adding them put ours at 199 and the function went to 100%.

Second: last run wrote of `SetUIText` that the indices are "not knowable from this tree". **Echoes'
`SetUIText` does not use string-table indices at all.** It calls
`GetString__12CStringTableCFPCc` (a `char const*` name overload, `include/Kyoto/Text/CStringTable.hpp:34`),
and every name is a string literal in this unit's `.rodata` at a measured offset. Whole map
(offsets from the start of the unit's `.rodata`, which is `lbl_803A9D90` in the DOL; every offset
below was resolved by reading the string at it):

    0 TXTR_SaveBanner      16 TXTR_SaveIcon0      31 TXTR_SaveIcon1
    46 STRG_MemoryCard    62 FRME_GenericMenu    79 textpane_message
    96 tablegroup_choices 115 textpane_choice0  132 textpane_choice1
    149 textpane_choice2  166 textpane_choice3  183 model_messagebg
    199 ??(??)                       <- operator new's __FILE__, offset in ConstructCardDriver
    206 StatusWriting     220 StatusWritingInitial  241 NoMemoryCard
    254 ChoiceRetry       266 ChoiceContinueWithoutSave  292 CorruptedCard
    306 ChoiceFormatCard  323 EncodingMismatch  340 DamagedCard
    352 WrongDevice       364 InsufficientSpaceMain  386 ChoiceManageMemoryCard
    409 BadSectorSize     423 CorruptedFile    437 ChoiceDeleteCorruptedFile
    463 TitleWarning      476 IPLWarning        487 ChoiceCancel
    500 ChoiceContinueWithWarning  526 ConfirmOverwrite  543 ConfirmFormat
    557 SaveFile          566 ChoiceYes         576 ChoiceNo

Every one of the 24 offsets `SetUIText` uses (`206 220 241 254 266 292 306 323 340 352 364 386 409
423 437 463 476 487 500 526 543 557 566 576`) lands on one of these. Reproduce the dump with a
`struct.unpack('>III', ...)` read of the DOL section table (the header offsets are
`d[0x20 + i*12 : 0x20 + i*12 + 12]`, **not** `tools/dol_read.py`'s `find()`, whose label is
0x100 low here - read the raw section table and index from `0x803A9D90` directly).

## What changed

* `src/MetroidPrime/CSaveGameScreen.cpp`
  * seven `static const char* const sk...` widget names, declared after `skGenericMenu` and in
    retail's `.rodata` order.
  * `PumpLoad`: replaced the `// TODO ... return false` tail with retail's real body - the six
    `FindWidget` bindings plus `model_messagebg` visibility, then `ConstructCardDriver`,
    `StartCardProbe`, `SelectUIType`, `SetUIText`, `return true`. The `TFunctor2FromMethod`
    install between the `SetVisibility` and the `ConstructCardDriver` is still missing (see
    below) and is now the only TODO left in that function.
  * includes for `CGuiTextPane.hpp` and `CGuiWidget.hpp`.
* `include/GuiSys/CGuiTableGroup.hpp` - `CGuiTableGroup` now derives from `CGuiWidget`. Needed
  because `FindWidget` returns a `CGuiWidget*` and `static_cast<CGuiTableGroup*>` is illegal
  without a base; retail passes the same pointer to `SetIsActive__10CGuiWidgetFb`, so the base is
  evidence. No members added, so no layout is invented and `SetUIColors` stayed at 100%.

## Result, measured

    main/MetroidPrime/CSaveGameScreen
      matched_functions  16 -> 17 / 24
      fuzzy              48.90136 -> 51.816303
      matched_code       41.701122 -> 44.83166

    +100%  ConstructCardDriver   (99.98113% before; the `__FILE__` immediate)
     49.61%  PumpLoad           (27.964912% before; was 1.15% before last run's port)
    project: All: 31.31% fuzzy, 23.69% matched, 11.83% linked
             matched 10313 -> 10314, linked 5048 -> 5048, no regression (report_diff.py)

`tools/goal_check.sh build/goal/item.json` -> **PASS progress-prime1-csavegamescreen**, every line
`ok` (gate.sh incl. DOL sha1 + 86 RELs, counts, check_symbol_names, All:, target rose, no asm).

## Still blocked, with what is now measured

**`SetUIText` (1440 B) - three named APIs, and each one's shape is known.** The switch is fully
decoded (16 cases on `mUiType`, six `char const*` locals, `mHasMessage` at bit 1 of byte 144).
What is missing is not the body, it is three declarations this tree does not have:

* `CGuiTextPane::SetIsSelectable(bool)` - retail writes bit 2 of the byte at `+186`
  (`lbz; rlwimi r,val,5,26,26; stb` on all four text panes). `+186` is inside `CGuiWidget`
  (`CHECK_SIZEOF(CGuiWidget, 0xbc)`), next to the existing `bool mIsSelectable : 1`, so it is very
  likely an existing bitfield reached through an accessor rather than a new one - **not tried**.
* `CGuiTableGroup::SetUserSelection(int)` - `lwz r0,200(r3); stw r0,204(r3); stw 0,200(r3)`, two
  int words at 200/204 that no `CGuiTableGroup` header here describes.
* `CGuiWidget::SetIsActive(bool)` (retail `fn_8027D84C`) - takes `mTablegroupChoices` directly, so
  it is inherited and needs no layout, only a declaration.

**`SetUIText`'s four default `opt0..3` values are not plain `nullptr` in source.** Retail loads
them with `lwzu r6,-25472(r3)` / `lwz r5,4(r3)` / `lwz r4,8(r3)` / `lwz r3,12(r3)` off the
`.rodata` base, i.e. from 16 bytes *below* `.rodata`. In the DOL those 16 bytes
(`0x803A9D80..0x803A9D8F`) are all zero, so the values are null - but `const char* opt0 = nullptr`
in C++ would compile to `li r,0`, not to four loads. Whatever source produces those four loads is
not guessed at here.

**`PumpLoad`'s remaining ~50%** is the `TFunctor2FromMethod` install of `DoAdvance` /
`DoSelectionChange` (the 12-byte pmf `memcpy` from `lbl_803B5740`/`lbl_803B574C`, then
`fn_802794D4` / `fn_802794A0`, retail's `SetMenuAdvanceCallback` /
`SetMenuSelectionChangeCallback`). `grep -rl TFunctor include/ src/` is still empty.

**`PumpLoad`'s token prologue is still wrong, and this run did NOT try a spelling for it.**
Retail tests, per token, `mItem != nullptr || CToken::IsLoaded()` - that is the *const*
`TCachedToken<T>::IsLoaded()` (`include/Kyoto/TToken.hpp:53`), merged into one bool and then
`clrlwi. r0,r4,24; beq`. Ours calls `mTxtrSaveBanner.GetToken().IsLoaded()`, which is
`CToken::IsLoaded()` and drops the `mItem` term, so it is 4 instructions short per token
(5 tokens). Spelling candidates not tried: `tok.IsLoaded()` (picks the non-const overload, which
also *caches* `mItem` - a behaviour change), or spelling the `||` out by hand. **Untried, no
score, do not read the notes above as a measurement.**

**`DoAdvance` (820 B)** is now decoded too: a 16-way switch on `mUiType` each re-dispatching on
`caller->+200` and `mSaveCtx`, with the common tail
`if (sel==1) { if (mSaveCtx==kSC_InGame) this->+128 = 2; else ContinueWithoutSaving(); sfx = this->+140; }`
and `if (sel==0) { ResetCardDriver(); sfx = this->+132; }`. Three things it needs that do not
exist here: `CMain`'s `mManageCard` bit (**bit 28 of byte 144** on `gpMain`, retail `r13-28364`),
`CGameState`'s file index (**word at 124** on `gpGameState`, retail `r13-28360`), and
`CMemoryCardDriver::BuildExistingFileSlot(int)` (this header has `BuildNewFileSlot`). `DoAdvance`
also reads the driver's serial words at `+40/+44` into `mSerial`. **Not attempted** - four
independent unknowns is one item, not a slice of one.

**The four unnamed functions are unchanged and still unpairable.** `fn_8017D124`/`fn_8017D188`
are the `TFunctor` invokers (a 12-byte pmf `memcpy` + `__ptmf_scall`); a template instantiation
would carry a mangled name, so objdiff could not pair it with `fn_8017D124` even if the bytes
matched. `fn_8017DEE8`/`fn_8017D378` are this unit's out-of-line `rstl::vector::reserve` and its
element destructor, emitted here as weak COMDATs.

## Lesson worth carrying to other units

MWCC's `__FILE__` argument is the string `"??(??)"`, and the `addi` that materialises it is that
string's **offset inside the translation unit's own `.rodata`**. So a function that differs from
retail by one immediate on an `__nw__`/`__dl` call is usually not a compiler-wall at all: read
retail's `.rodata` for the unit, diff the string tables, and add back the literals that source
would have had. It moved a function from 99.98% to 100% here, and the same trick applies to any
`rs_new`-carrying unit.

NEW: progress-prime1-csavegamescreen | progress | MetroidPrime/CSaveGameScreen | SetUIText and DoAdvance are fully decoded (see notes: the string-name offsets and every branch) and are now blocked only on declarations this tree lacks - CGuiTextPane::SetIsSelectable (bit 2 of byte 186), CGuiTableGroup::SetUserSelection (ints at 200/204), CMain::mManageCard (bit 28 of byte 144), CGameState file index (word at 124), CMemoryCardDriver::BuildExistingFileSlot - not on spelling guesses.

## Review rejected run 13 (2026-09-30 11:21:18Z, reviewer worker)

The judge passed this attempt; the reviewer rejected it:

PumpLoad (src/MetroidPrime/CSaveGameScreen.cpp:162-168) deletes retail's frame-token guard and then dereferences a pointer this tree never initialises — `mFrmeGenericMenu.GetObject()` returns `TCachedToken::mItem`, which is nullptr because the ctor's `TCachedToken(const CToken&)` leaves it null and `CToken::Lock()` never caches it, so the first successful load calls `CGuiFrame::FindWidget` on a null pointer (crash as soon as the save screen is opened). Retail's corresponding block (asm 0x8017CED8-0x8017CF28) is the *caching* `TCachedToken::IsLoaded()` on `mFrmeGenericMenu`, which returns false when unloaded and, on success, is what populates the `mItem` that line 162 reads. An acceptable change keeps that check — `if (!mFrmeGenericMenu.IsLoaded()) { return false; }` spelled with the non-const caching overload, ahead of `mLoadedFrame = mFrmeGenericMenu.GetObject();` — before the six `FindWidget` bindings, and corrects the note's claim that all five prologue tokens use the const `mItem != nullptr || CToken::IsLoaded()` form (tokens 4 and 5 test `mLockHeld` and call `GetObj__6CTokenFv`). The rest of the diff (the seven real widget-name literals taking ConstructCardDriver to 100%, and `CGuiTableGroup : public CGuiWidget` with no members on a pointer-only class) is sound and can stay.

Rejected diff: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/review/progress-prime1-csavegamescreen-L8-13.patch
Review transcript: /run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrime2Port/../wt-mp2-goal/build/goal/agent/progress-prime1-csavegamescreen-L8-13-review1-20260930T111723.jsonl

## Fix round 1

Two changes only, both the ones the review asked for; the rest of the rejected diff is untouched.

1. `src/MetroidPrime/CSaveGameScreen.cpp` - `PumpLoad` regains retail's fifth token test, put
   after the `mSaveWorlds` loop and before `mLoadedFrame = mFrmeGenericMenu.GetObject()`:

       if (!mFrmeGenericMenu.IsLoaded()) {   // non-const: caches mItem
         return false;
       }

   `mFrmeGenericMenu` is a `TCachedToken< CGuiFrame >` and `PumpLoad` is non-const, so this picks
   the caching overload (`include/Kyoto/TToken.hpp:44-54`), which is what fills `mItem` and so is
   what makes the following `GetObject()` non-null. This is retail's 0x8017CED8-0x8017CF28 block
   (verified in the disassembly, not assumed: `lbz r0,72(r31)` = `mLockHeld`,
   `bl GetObj__6CTokenFv`, `stw r0,76(r31)` = the cache, `clrlwi./beq` back out). Without it the
   first successful load called `FindWidget` on nullptr.
2. The note's `PumpLoad` token-prologue paragraph was wrong about all five tokens using the const
   form. Rewritten: tokens 1-3 (0x8017CDC0-0x8017CE44) are the const
   `mItem != nullptr || CToken::IsLoaded()`, tokens 4 and 5 are the non-const caching overload
   (`mLockHeld` test + `GetObj__6CTokenFv`). Token 4 was already spelled right in the source
   (`!mStrgMemoryCard.IsLoaded()`), so the 4-instructions-short claim applies to 3 tokens, not 5,
   and the untried-spelling warning is kept.

Measured after the fix (`tools/decomp_build.sh main/MetroidPrime/CSaveGameScreen`):

    PumpLoad      49.61% -> 55.82%   (912 bytes)
    unit          51.816303% -> 52.65% fuzzy, matched_functions still 17/24
    project All:  31.31% fuzzy, 23.69% matched, 11.83% linked - unchanged, no regression
    ConstructCardDriver still 100%

`python3 tools/check_raw_offsets.py` -> ok, 160 sites in 67 files.

---

# Lane 7, 2026-09-30, third attempt on the same item

Re-measured on a clean `wt-mp2-goal-L7` at `goal/lane-7` @ `1c237fa2`. Lane 8's work was already
in the branch head (`build/goal/judge/report.base.json` and `build/report.json` were
byte-identical: 17/24, fuzzy 52.65387), so nothing was `STALE:` - seven functions were still
short. **All three named blockers in the notes above turned out to be reachable, and all three
named unmatched functions reached 100%.**

## Result, measured

    main/MetroidPrime/CSaveGameScreen
      matched_functions  17 / 24  ->  20 / 24
      fuzzy              52.65387  ->  91.671585
      matched_code       44.83166  ->  91.671585
    project: All: 32.48% fuzzy, 25.14% matched, 11.94% linked
             matched 11285 -> 11288, linked 5507 -> 5507, no regression (report_diff.py)

    +100%  PumpLoad__15CSaveGameScreenFv        (55.82% before)
    +100%  SetUIText__15CSaveGameScreenFv       ( 1.15% before)
    +100%  DoAdvance__15CSaveGameScreenFP14CGuiTableGroup  (0.49% before)

`tools/goal_check.sh build/goal/item.json` -> **PASS progress-prime1-csavegamescreen**, every line
`ok` (gate.sh incl. DOL sha1 + 86 RELs, counts, check_symbol_names, All:, target rose 17 -> 20,
no asm).

## Two of the previous run's "blocked on" conclusions were wrong

**`TFunctor` is not a shared-header change.** The notes said: *"Adding a `TFunctor` family is a
shared-header change affecting every unit, not this item's work."* A *new* header that exactly
one translation unit includes changes exactly one unit. `include/Kyoto/TFunctor.hpp` (only the
1- and 2-argument forms, 128 lines) is included by `CSaveGameScreen.cpp` and nothing else, so it
cannot move any other unit. That one header took `PumpLoad` from 55.82% to 100%.

**`SetUIText`'s names were not unknowable.** Lane 8's `.rodata` map is correct and complete; the
blocker was reading it as needing new *tables*, not literals. `CStringTable::GetString(const
char*)` already exists here.

## `PumpLoad` - 55.82% -> 100%, five steps

| step | score | what changed |
|---|---|---|
| 1 | 77.69% | `include/Kyoto/TFunctor.hpp` + the two `TFunctorNFromMethod::Make` installs |
| 2 | 87.04% | tokens 1-3 tested through a `const TCachedToken<T>&` binding |
| 3 | 97.48% | the 7 widget names as **inline literals**, not `sk...` variables |
| 4 | 97.48% | the frame-token check as `if (IsLoaded()) {...} else { return false; }` |
| 5 | **100%** | - |

Four things the notes had recorded wrongly, all now measured:

1. **`IsLoaded()` overloads.** Tokens 1-3 (0x8017CDC0) are the *const*
   `mItem != nullptr || CToken::IsLoaded()`; tokens 4 and 5 (0x8017CE58, 0x8017CEEC) are the
   caching overload (`mLockHeld` test + `GetObj__6CTokenFv` + the store back into `mItem`). The
   source said `!mTxtrSaveBanner.GetToken().IsLoaded()`, which is neither - it calls
   `CToken::IsLoaded()` and drops the `mItem` term. Fix: bind the three to
   `const TCachedToken<CTexture>&` and call `.IsLoaded()`; leave the two `mStrgMemoryCard` /
   `mFrmeGenericMenu` calls on the mutable members so the caching overload is picked. The
   previous run's fix round 1 got the *fifth* token right and left the first three wrong.
2. **The `TFunctor` record is 24 bytes**, measured from the two GuiSys setters: `fn_802794D4`
   copies argument words 0..20 to `212(r3)` and `fn_802794A0` to `260(r3)` - six words each.
   So `CMethodPtrStore` is `(sizeof(void(*)()) + 15) & ~15` = 16 and the record is
   `{Functor, object, method[16]}`. The 12-byte `memcpy` in `PumpLoad` is the *method pointer*,
   not the record.
3. **The 7 widget names are inline literals; the 5 asset names are file-scope variables.** This
   is forced by retail's `.rodata` order (TXTR_SaveBanner at 0, ..., FRME_GenericMenu at 62,
   textpane_message at 79, ...), which is the *declaration* order of the five
   `static const char* const` at the top of the file, followed by the literals in
   `PumpLoad`'s point-of-use order. A named variable puts the pointer in `.sdata2` and makes
   the call site `lwz r4,<SDA21>`; a literal makes it `lis r3; addi r4,r3,-25456; addi r4,r4,79`.
   Keeping the seven `sk...` variables (as the previous run did) cost 10.4 points.
4. **The `return false` for the frame token is an `else` arm.** Retail branches *forward* to a
   shared `li r3,0; b end` block placed after the callback installs (0x8017D0B4); the guard-clause
   spelling makes MWCC inline the block and branch over it. `if (mFrmeGenericMenu.IsLoaded()) {
   ... } else { return false; }` is worth the last 2.5 points - and it is the reviewer's fix
   round 1's guard, just with the body moved into the `then`.

## `SetUIText` - 1.15% -> 100%, three steps

| step | score | what changed |
|---|---|---|
| 1 | 75.57% | the 16-arm switch, transcribed from the jump table at `0x803B5798` |
| 2 | 99.99% | the four option names as an **array**, not four separate locals |
| 3 | **100%** | `CGuiTableGroup`'s member order fixed |

1. **The six locals are `char const*` names, and four of them are an array.** Four separate
   `const char* opt0..opt3` locals are held in registers across the switch, so every arm's
   `addi`/`stw` interleaving is wrong (75.57%). `const char* opt[4] = {nullptr x4}` puts them in
   memory, which is what retail does - it stores each name at `184..196(r1)` inside the arm that
   sets it and reads them all back at the end - and the array's aggregate initialiser is what
   produces retail's `lwzu r6,-25472(r3)` + three `lwz` + four `stw` prologue. Worth 24 points.
   (Confirmed by reading the DOL: those four words are `0x803A9C80..0x803A9C8F`, sixteen zero
   bytes immediately below this unit's `.rodata` at `0x803A9C90`.)
2. **`CGuiTableGroup`'s members are in this order**: `mUserSelection` at **200**, then
   `mPrevUserSelection` at 204, then `mDoMenuAdvance` at 212, then `mDoMenuSelChange` at 260.
   The previous notes' 200/204 were right. **I "corrected" them to 388/392 mid-run and that was
   wrong** - the 0x8017DD14 disassembly reads `lwz r0,200(r3); stw r0,204(r3); stw 0,200(r3)`,
   and the error showed up as exactly the three `lwz`/`stw` objdiff flagged. Do not re-derive it.
3. Two more declarations were needed, both **inline in their header** because retail inlines
   them at the call site rather than calling:
   * `CGuiWidget::SetIsSelectable(bool)` - 0x8017DCBC is `lbz r4,186(r6); rlwimi r4,r7,5,26,26;
     stb r4,186(r6)`. `mIsSelectable` is already the third bit of the byte at 186 in this tree
     (`build/G2ME01/src/GuiSys/CGuiWidget.o` emits `rlwimi r0,r10,5,26,26` after `lbz r0,186`),
     so no layout change was needed. `CGuiWidget::SetIsActive` stays out-of-line: it is a real
     `bl` to `fn_8027D84C`.
   * `CGuiTableGroup::SetUserSelection(int)` - the three instructions above, inlined.
   `CGuiTextSupport::SetText(wstring const&, bool)`, `rstl::wstring`, `wstring_l` and both
   `CStringTable::GetString` overloads already existed.

The 16-arm switch maps 1:1 to the jump table at `0x803B5798`: cases 0-3 (Empty, BusyReading,
BusyWriting, BusyWritingInitial) all point at the end-of-switch address, and Echoes has no
`StillInsufficientSpace`/`StillFull` arms.

## `DoAdvance` - 0.49% -> 100%, six steps

| step | score | what changed |
|---|---|---|
| 1 | 80.82% | the whole body, transcribed from the jump table at `0x803B5758` |
| 2 | 81.32% | the four no-op cases written out |
| 3 | 91.60% | the `if`/`else if` chains reordered to retail's test order |
| 4 | 99.41% | - (same build as 3, read from objdiff) |
| 5 | 99.95% | the selection read from `mTablegroupChoices`, not from `caller` |
| 6 | **100%** | `mNavMoveSfx` -> `mNavConfirmSfx` |

1. **The four no-op cases must be written out.** Retail's dispatch is a 16-entry table indexed
   by `mUiType` (`cmplwi r0,15` then `slwi`/`lwzx`, 0x8017C634). Leave cases 0-3 out and MWCC
   range-checks instead and emits a 12-entry table based at case 4 - a different prologue.
2. **MWCC emits the `if`/`else if` chain in source order, and retail's order is not Prime 1's.**
   Measured per arm: 1-then-0 for NoCardFound/CardDamaged/WrongDevice/IncompatibleCard,
   ProgressWillBeLost, NotOriginalCard and AllDataWillBeLost; 1-0-2 for
   NeedsFormatBroken/NeedsFormatEncoding and InsufficientSpaceOKCheck; **2-1-0** for SaveCorrupt;
   0-1 for SaveReady. Prime 1 writes 0 first everywhere. Worth 10 points on its own.
3. **The selection comes from the member, not the parameter.** Retail's prologue is
   `lwz r0,16(r3)` (mUiType), `lwz r4,88(r3)` (mTablegroupChoices), `cmplwi r0,15`,
   `lwz r5,200(r4)` - so `caller` is never read. `mTablegroupChoices->GetUserSelection()`
   reproduces it; `caller->GetUserSelection()` reads the incoming `r4` and skips the member load.
4. **The action sfx is `mNavConfirmSfx` (132), not `mNavMoveSfx` (136).** Retail's acting arms
   end `...; lwz r6,132(r31)` and its backing-out arms `...; lwz r6,140(r31)`. Prime 1's source
   uses `mNavConfirmSfx` here too. `mNavMoveSfx` at 136 is only `DoSelectionChange`'s. This one
   wrong member name is the entire difference between 99.95% and 100%.
5. The other two unknowns resolved without a new field: `gpMain->mManageCard` already exists in
   `CMain.hpp` (byte 0x90 bit 4, which is what 0x8017C7E4 writes) and only needed an inline
   `SetManageCard`; and `gpGameState`'s word at 124 is `CGameState::mSystemOptions` (+0x54)
   `+ 0x28`, which is `CPersistentOptions::GetSaveIdx()` - the base is `CGameStateEnvVarManager`
   at 0x18 and the vector is 0x10, so `mSaveIdx` is already the last word of the 0x2c object.
   **`CPersistentOptions.hpp` needs no change**; I added a field there mid-run and reverted it.

## What is still not matched, and why it is not a wall

The four remaining functions are the unnamed ones, and the reasons are structural rather than
codegen:

* `fn_8017D124` (100 B) and `fn_8017D188` (84 B) are `TNonStaticCallback2`/`1::Function` - a
  12-byte pmf `memcpy` plus `__ptmf_scall`. This unit now emits them, as
  `Function__60TNonStaticCallback2<15CSaveGameScreen,CP14CGuiTableGroup,Ci>FPCvPCvP14CGuiTableGroupi`
  and `Function__57TNonStaticCallback1<...>`. objdiff pairs by symbol name and retail's two are
  `fn_*`, so they score 0.00% with no counterpart. Source cannot change the mangled name.
* `fn_8017DEE8` (212 B) and `fn_8017D378` (168 B) are the out-of-line `rstl::vector::reserve`
  and its element destructor for this element type, emitted here as weak COMDATs that mwldeppc
  discards. Same naming problem.

So 20/24 is the ceiling for this unit without objdiff-side or linker-side work, and the unit
stays `NonMatching` as a `progress` item requires. `flip_test.sh` was not run.

## Lessons worth carrying to other units

* **A `switch`'s jump-table shape is decided by which cases you write, not by which ones do
  something.** A run of no-op cases keeps a full-width table; omitting them turns it into a
  range check. Prime 1's `case kUIT_Empty: case kUIT_BusyReading: case kUIT_BusyWriting: break;`
  is load-bearing, not decoration.
* **MWCC preserves the order of an `if`/`else if` chain, and it matters.** When a Prime 1 port
  does not match, transcribe the *comparison order* from the disassembly before touching
  anything else - Prime 1's order was wrong in five of eight arms here.
* **Four locals or one array is a register-allocation decision, not a style choice.** Retail
  storing a value to the stack inside the arm that sets it means the source had an array.
* **A member's *identity* is worth a full point of score on its own.** `mNavMoveSfx` and
  `mNavConfirmSfx` are adjacent ints with different values; objdiff's per-instruction diff names
  the offset (0x88 vs 0x84) and the fix is one identifier.
* Reading a *linker-resolved* address out of `main.elf` (`addi r4,r13,-30792`) and comparing it
  with an *unlinked* object (`li r4,0` + `R_PPC_EMB_SDA21`) is a false diff. Compare
  `build/G2ME01/obj/...` with `build/G2ME01/src/...`, not the linked ELF.

## Files changed

* `src/MetroidPrime/CSaveGameScreen.cpp` - `SetUIText`, `DoAdvance`, the `PumpLoad` callback
  installs and token prologue, the inline widget-name literals; includes for `CStringTable.hpp`,
  `rstl/string.hpp`, `CMain.hpp`.
* `include/Kyoto/TFunctor.hpp` - **new**, 128 lines, `CMethodPtrStore` + `TFunctor1`/`TFunctor2`
  and their `FromMethod` makers. Included only by `CSaveGameScreen.cpp`.
* `include/GuiSys/CGuiTableGroup.hpp` - `SetMenuAdvanceCallback`, `SetMenuSelectionChangeCallback`,
  `SetUserSelection`, `GetUserSelection`, and the measured member layout.
* `include/GuiSys/CGuiWidget.hpp` - inline `SetIsSelectable`.
* `include/MetroidPrime/CMain.hpp` - inline `SetManageCard`.

No `tools/`, no `config/`, no `splits.txt`, no judge-owned path, no `.s` file, no commit.
