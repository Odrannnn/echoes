# progress-unit-cscandisplay

**Kind:** `progress` — target `MetroidPrime/Player/CScanDisplay` stays `NonMatching`; one real
function matched and kept. `tools/goal_check.sh build/goal/item.json` printed
`goal_check: PASS progress-unit-cscandisplay`.

## Measured

`build/report.json`, the target unit (base = `build/goal/judge/report.base.json`):

| | matched_functions | fuzzy_match_percent | matched_code_percent |
|---|---|---|---|
| base | 36 / 54 | 40.54214 | 30.976503 |
| now | **37 / 54** | 40.87174 | 31.558525 |

Global `matched_functions` **11692 -> 11693**; `All:` line unchanged at
`33.32% fuzzy, 26.27% matched, 12.24% linked (11693 / 28465 functions)`.
Every other unit is byte-for-byte identical to the base report (`report.base.json` diffed over
all 2067 units: only this one changed). `sha1sum build/G2ME01/main.dol` =
`6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. No `asm` added; the diff is 16 lines in one `.cpp`.

## What changed

`src/MetroidPrime/Player/CScanDisplay.cpp` — `CScanDisplay::CScanTargetPredicate::IsValid`
only, plus a comment recording why each odd spelling is there.

| function | before | after |
|---|---|---|
| `IsValid__Q212CScanDisplay20CScanTargetPredicateCFRC13CStateManager9TUniqueId` (0x80116790, 108 B) | 43.37037% | **100%** (byte-exact) |

Nothing else in the unit was touched; the other 17 unmatched functions are exactly as the base
report has them.

### How the spellings were ranked

`objdiff`'s percentage is size-dominated, so it cannot rank candidate spellings. The unit was
compiled standalone with the flags `build.ninja` uses for it (copy `cflags` + the three
`-DMUSY_*` defines out of `build.ninja`, `-i extern/musyx/include`,
`-pragma "inline_max_size(125)"`) into
`build/tools/wibo build/tools/sjiswrap.exe .../GC/2.7/mwcceppc.exe -c`, and compared with
`python3 tools/bytescmp.py <obj.o> IsValid 0x80116790 0x6c`, which counts differing
instructions against the retail DOL and flags which fields are relocations. Numbers below are
`(ours bytes, differing instructions / ours instructions)`; retail is 108 B / 27 instructions.

| # | spelling | result |
|---|---|---|
| 1 | `if (id != mObject)` … `return TCastToConstPtr<..>(entity) != nullptr && entity->GetActive();` (the scaffold as it stood) | 128 B, 25/32 — objdiff **43.37%** |
| 2 | null-test `mgr.GetObjectById(id)` first, then the cast, then `&&` | 140 B, 25/35 — two `cmplwi`, retail has one |
| 3 | `poi = cast(..); if (poi == nullptr) return false; return cast(poi)->GetActive();` | 112 B, 19/28 — the `!!` idiom appears, but the null branch is mirrored |
| 4 | as 3 with `if (poi != nullptr) { return ..; } return false;` | 112 B, 19/28 — layout right, still one `lhz r0,0(r5)` extra |
| 5 | `const TUniqueId uid(id);` local + 4 | 112 B, 17/28 — r5 survives, but a second `sth r5,12(r1)` appears |
| 6 | local + compare against the local | 116 B, 22/29 |
| 7 | local + `== nullptr` first | compile error: implicit conversion from the incomplete `CScriptPointOfInterest*` |
| 8 | **`id.value != mObject.value`, one null test on the cast result, `GetActive()` returned bare** | **108 B, 2/27 — and both differences are `bl` relocation fields, so objdiff sees 100%** |

The three things that matter, all measured:

* **One null test, on `TCastToPtr`'s return.** Retail branches once (`cmplwi r3,0` then
  `lbz r0,32(r3)`), so the `CEntity*` must not be tested separately. `CScriptPointOfInterest` has
  no header in this repo, hence `reinterpret_cast<const CEntity*>` — free, and it keeps the
  single test.
* **`id.value != mObject.value` rather than `operator!=`.** Retail keeps the by-value `TUniqueId`
  in its incoming register (`lhz r5,0(r5)`) and stores that register straight into the outgoing
  argument slot (`sth r5,8(r1)`). Through `operator!=` MWCC copies it to r6 for the compare and
  re-loads it (`lhz r0,0(r5)`) — one instruction too many, and it cascades into every branch
  displacement. A local `uid` (5) fixes the register but costs a stack home (a second `sth`).
* **`GetActive()` on its own line, not as the operand of `&&`.** `m_active` is `uint:1`
  (`include/MetroidPrime/CEntity.hpp:71`); as a `&&` operand the value test folds away and the
  compiler emits `rlwinm.` / `beq` / `li r30,1`. Returning it bare emits the
  `neg` / `or` / `srwi` normalisation retail has.

Corollary worth keeping: `&&` on a `bool`-returning accessor is not free on this target.

## What is still open, with the evidence

### The 12 unnamed functions in this unit are unreachable without a rename

`fn_80114CB8` (192 B), `fn_80114DFC` (80), `fn_80114E4C` (64), `fn_80114E8C` (32),
`fn_80115178` (32), `fn_80115594` (56), `fn_801155CC` (96), `fn_8011605C` (56),
`fn_80116094` (96), `fn_801166E8` (28), `fn_80116704` (140), `fn_80116BA0` (212) all have
`fuzzy_match_percent: None` in `build/report.json` — `objdiff` has no name to pair them with.
Reading them (`tools/dis.sh`):

* `fn_80114CB8` / `fn_80116BA0` are `rstl::vector<SObjectTag>` (`mWorldModels`) member
  functions — `assign` and a grow-only `resize`. **Our object already emits byte-identical
  bodies** under their real mangled names (`__as__Q24rstl47vector<10SObjectTag...` and friends);
  they are simply invisible to objdiff.
* `fn_80114DFC` / `fn_80114E4C` / `fn_80114E8C` are
  `rstl::optional_object<CScannableObjectInfo>::operator=(const optional_object&)`,
  `::clear()` and the `destroy()` wrapper; `fn_80115178` is `rstl::construct<CScannableObjectInfo>`,
  which our object emits as the local symbol `construct<20CScannableObjectInfo>__4rstlFPvRC20CScannableObjectInfo`.
* `fn_80115594`/`fn_801155CC` and `fn_8011605C`/`fn_80116094` are `construct`/`destroy_impl` pairs
  for `TCachedToken<CStringTable>` and `rstl::basic_string<...>`.
* `fn_801166E8` is a 7-word store-args-and-`blr` thunk.

Naming them needs either a `config/G2ME01/symbols.txt` rename (which `tools/check_symbol_names.py`
exists to police, because a rename that does not match the object breaks all 86 REL links) or a
C-mangled definition, and a C++ template instantiation cannot carry a chosen symbol name. Not
attempted; not recorded as a `NEW:` item.

### `StartScan` (0x80115700, 1696 B, 60.0% before) — needs an `SScanHierarchyNode` layout fix first

Our object is 1180 B; the missing ~500 B is the two `TODO`s at the end of `StartScan`. I read
the whole 1696 B (`tools/dis.sh 0x80115700 0x6a0`) and the logic is recoverable:

```cpp
  mTextGroup->SetVisibility(showText, kTM_Children);
  mTextGroup->SetColor(CColor::White().WithAlphaOf(0.f));
  reinterpret_cast< char* >(gpGameState)[0xD9] = 1;
  mHintsSuppressed = true;
  // existing string-table / token block is already right
  if (!mgr.fn_80036F10()) {
    mHistoryRoot = historyRoot;  mHistoryRight = historyRight;
    mHistory = history;          mHistoryWidgets = widgets;
    mHistoryRoot->SetVisibility(!history.empty(), kTM_Children);
    mHistoryRoot->SetColor(CColor::White().WithAlphaOf(0.f));
    mHistoryRight->SetColor(gpTweakGuiColors->GetScanHudHierarchyInactiveFrameColor());
    for (int i = 0; i < mHistoryWidgets.size(); ++i) {                 // 0x801159ac
      mHistoryWidgets[i].mRoot->SetVisibility(i < mHistory.size(), kTM_Children);
      if (i < mHistory.size() - 1) {
        mHistoryWidgets[i].mHistory->TextSupport().SetFontColor(
            gpTweakGuiColors->GetScanHudHierarchyTextColor());
        mHistoryWidgets[i].mFlash->SetColor(
            gpTweakGuiColors->GetScanHudHierarchyTextFrameColor());
      } else if (i == mHistory.size() - 1) {
        mHistoryWidgets[i].mHistory->TextSupport().SetFontColor(
            gpTweakGuiColors->GetScanHudHierarchyFinalTextColor());
        mHistoryWidgets[i].mFlash->SetColor(
            gpTweakGuiColors->GetScanHudHierarchyFinalTextFrameColor());
      }
      const SScanHierarchyNode& node = mHistory[i];
      widgets[i].mPercent->SetColor(complete ? CompleteFlashIconColor : FlashIconColor);
    }
    mCategoryName = rstl::wstring_l(L"");
    mHistoryStrings.clear();                       // retail calls clear() at 0x80115b00; our source does not
    mHistoryStrings.reserve(history.size());
    for (...) push_back(TCachedToken<CStringTable>(gpSimplePool->GetObj(SObjectTag('STRG', node.mStringTable))));
    for (...) Lock();                              // a second pass, 0x80115bc8
  }
  if (mScannableInfo) {                            // tests mScannableInfo.m_valid at 456 = 0x1C8
    if (info.GetScanTextureId() != kInvalidAssetId) { mScanTexture = ...('TXTR'); mScanTexture->Lock(); }
    if (info.UsesScanModel() && info.GetStaticModelId(0) != kInvalidAssetId) { mScanModelToken = ...('CMDL'); Lock(); }
  }
```

Every offset retail uses is consistent with the existing `CScanDisplay` layout (`0x1CC` mSelHud,
`0x1F8` mHistory, `0x208` mHistoryWidgets, `0x218` mScanString, `0x228` mScanModelToken,
`0x240` mHistoryStrings, `0x250` mCategoryName; `sizeof(CScannableObjectInfo) == 0x1a4` checks
out against the `mScannableInfo.m_valid` at 456 and the string/texture/model asset ids at
`0x28`/`0x2C`/`0x34`). **The blocker is `SScanHierarchyNode`'s layout.**

**`NEW`-worthy defect, measured:** retail reads the two counts at `SScanHierarchyNode+0x1C` and
`+0x20` (`lwz r3,32(r5)` / `lwz r0,28(r5)` at 0x80115a7c / 0x80115a80, where `r5 = &mHistory[i]`),
but `include/MetroidPrime/HUD/CScanHistory.hpp` puts `mTotalScans` at `0x14` and
`mCompletedScans` at `0x18`. `CHECK_SIZEOF(SScanHierarchyNode, 0x24)` is right — 36 bytes matches
retail's `addi r17,r17,36` stride — so two members are missing or misplaced, and reading the
counts by name cannot byte-match until that is fixed. `SScanHierarchyNode` is in no unit and in
no `splits.txt`, so nothing currently measures it and both `StartScan` here and the `Update`
history-text block at 0x80114C48 are wrong on the same point.

Retail's completion test, read literally:
`r4 = (node[0x20] == node[0x1C]) || node[0x1C] == 0` — `cmpw r3,r0; beq` keeps `r4 = 1`, then
`cmpwi r0,0; beq` also keeps `r4 = 1`, otherwise `li r4,0`.

Also measured on this function, so nobody re-derives it: retail's frame is `stwu r1,-0x110`
(272 B) saving `r15`-`r28` with `this` in r31 and `mgr` in r30, `scanTime` in f29 and the two
vector references read straight off the caller's stack at 280(r1)/288(r1)/304(r1). Every call
into `gpSimplePool->GetObj` goes through a vtable slot (`lwz r12,0(r4); lwz r12,12(r12);
mtctr r12; bctrl`) — the existing `GetObj(SObjectTag)` spelling already produces exactly that,
so only the stack slot numbers will move.

### `Draw`, `ProcessInput`, `Update`, `PrepareScanDisplay` — untouched, 22.6 / 29.5 / 19.3 / 0.13%

Not attempted: between 1772 and 3620 bytes each, all four are the interactive/presentation half
of the class and none of them is close. `Prime 1`'s
`/run/media/odran/Leo/projects/Restored-projects/Chatgpt/prime-ref/src/MetroidPrime/Player/CScanDisplay.cpp`
(505 lines, read) is a donor only for the *shape* of the logic — it has no `CScanTargetPredicate`,
no `history`/`SScanHierarchyWidgets` at all, `Update` takes `(dt, scanningTime)` rather than
`CStateManager&`, and its scan model is four `CAuiImagePane` buckets rather than Echoes' history
tree. Nothing in it maps onto the four unmatched functions without reading retail.

## Verification

```
sha1sum build/G2ME01/main.dol       # 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh            # 743 files, 0 failed, 0 errors; LINKED, 0 duplicates
python3 tools/check_symbol_names.py # 515 units; 0 missing names
./tools/decomp_build.sh             # All: 33.32% fuzzy, 26.27% matched, 12.24% linked (11693 / 28465)
./tools/goal_check.sh build/goal/item.json
#   ok  no judge-owned path touched
#   ok  gate.sh (DOL sha1, 86 RELs, report diff, wiring, docs claims, port probe)
#   ok  counts: matched 11692 -> 11693   linked 5625 -> 5625
#   ok  target rose: main/MetroidPrime/Player/CScanDisplay: 36 -> 37 / 54 functions
#   ok  no asm added
#   goal_check: PASS
```

`tools/unit_fit.sh src/MetroidPrime/Player/CScanDisplay.cpp` reports
`not declared in any splits.txt` — the unit is declared in `configure.py` only (line 636,
`Object(NonMatching, ...)`), so that script has nothing to check here; `goal_check.sh`'s gate
covered the link and the REL hashes instead.

Not committed, per the brief. The only file changed is
`src/MetroidPrime/Player/CScanDisplay.cpp`.

NEW: progress-startscan-cscandisplay | progress | MetroidPrime/Player/CScanDisplay | StartScan (0x80115700, 1696 B, 60.0%) is ~500 B of recoverable GUI work (body transcribed in this item's notes) but cannot byte-match until SScanHierarchyNode's layout is fixed: retail reads the two counts at +0x1C/+0x20, CScanHistory.hpp puts them at +0x14/+0x18, and that struct is in no unit so nothing else measures it.