# progress-prime1-cmemorycarddriver

`kind: progress`, `target: MetroidPrime/CMemoryCardDriver`, worktree `wt-mp2-goal-L1` (lane 1),
branch `goal/lane-1`, base commit `c28f8f1`. Not committed (the driver commits).

## Result

`main/MetroidPrime/CMemoryCardDriver` in `build/report.json`, measured after the change:

| | before | after |
|---|---|---|
| `matched_functions` | 15 / 53 | **37 / 53** |
| `fuzzy_match_percent` | 33.19032 | **51.2139** |
| `matched_code_percent` | 13.405047 | **42.407944** |
| project `All:` | 29.89% fuzzy, 21.69% matched, 9668 / 28465 functions | 29.92% fuzzy, 21.73% matched, **9690** / 28465 functions |

The unit stays `NonMatching` in `configure.py` (unchanged) and `flip_test.sh` was not run.
**+22 functions reached 100%.** No function anywhere got worse (verified: see Gates).

## Per function — before% -> after%, and whether Prime 1's source was used

Prime 1 (`../prime-ref/src/MetroidPrime/CMemoryCardDriver.cpp`) was the starting point for the
state-machine methods, but Echoes' engine is a fork: the member set differs (one save file, not
two; `mFileSlots`/`mGameOptionsData`/`mGlobalGameOptionsData`; no `CMain::SetCardBusy`), so
several functions needed Echoes' own shape. Every spelling below was checked against retail's
disassembly (`build/G2ME01/main.elf`, read with `tools/dis.sh` offsets from `report.json`), not
taken on faith.

### Now 100% (was sub-100)

| function | before | after | Prime 1 source? |
|---|---|---|---|
| `StartCardCheck` | 99.80 | 100 | **small edit**: Prime 1 has `mError` before `mState`; ours had them swapped. The 0.20% was exactly the two stores' order. |
| `StartCardFormat` | 99.80 | 100 | same swap |
| `StartFileWrite` | 99.82 | 100 | same swap |
| `StartFileWriteTransactional` | 99.83 | 100 | same swap |
| `StartFileRead` | 99.80 | 100 | same swap |
| `StartFileDeleteBad` | 99.83 | 100 | same swap |
| `GetCardFreeBytes` | 82.33 | 100 | **unchanged** from Prime 1. Ours hoisted the result into a local and compared twice, which made mwcc keep a `cntlzw`/`srwi` boolean. Prime 1's `if (call != kCR_READY) { NoCardFound(); return false; } return true;` is byte-identical. |
| `SGameFileSlot::PutTo` | 90.00 | 100 | `.size()` -> `.capacity()`. Retail emits `li r5, 2616`, a compile-time constant. |
| `SGameFileSlot::SGameFileSlot(CInputStream&)` | 95.59 | 100 | same `.size()` -> `.capacity()` |
| `IsRepairingHeader` | 23.33 | 100 | retail is `lwz r3,0x1a8(r3); lwz r0,0x10(r3); subfic r0,r0,2; cntlzw; srwi r3,r0,5`. `0x1a8` is `mFileInfo` (pinned by the 100%-matched `__dt__`, which calls `~CCardFileInfo` on it) and `+0x10` of `CCardFileInfo` is `mStatus`, so the body is `mFileInfo->IsRepairingHeader()`. Needed one new inline accessor on `CCardFileInfo`. Naming it `EStatus GetFileStatus()` and comparing in the caller does **not** compile: mwcceppc rejects `CMemoryCardSys::CCardFileInfo::kS_RepairHeader` from outside with "illegal access to protected/private member". Expose the bool on `CCardFileInfo` instead. |
| `UpdateMountCard` | 64.30 | 100 | **unchanged** from Prime 1. Ours had merged the READY and BROKEN arms into one `if (a \|\| b)`. |
| `UpdateCardProbe` | 80.68 | 100 | **unchanged** from Prime 1 (needs `const`-less `ProbeResults` and the `kCR_BUSY` early `return`). Ours had `StartMountCard()` inside the sector-size branch instead of after the chain. |
| `UpdateFileRead` | 80.57 | 100 | **mostly Prime 1**, plus Echoes' `kCR_BUSY` check that Prime 1 lacks. Two things were needed: the error arm must be the *last* `else` (retail `bne`s over it), and `readRes == kCR_BUSY` must be an explicit arm (`cmpwi r3,-1; beq end; cmpwi r3,-1003; bne end`). |
| `HandleCardError` | 45.62 | 100 | **unchanged cases**, but reordered. Retail's jumptable (read at `0x803B56E8`, 13 entries for `-13..-1`) needs `case kCR_BUSY: break;` to be present so the range check is `addi r0,r4,13; cmplwi r0,12` (12, not 11), and the case bodies come out in ascending-value order: BUSY, WRONGDEVICE, NOCARD, IOERROR, ENCODING. That is Prime 1's order. |
| `ImportPersistentOptions` | 3.03 | 100 | new body from retail: `CMemoryInStream` -> `CBitStreamReader` -> `CPersistentOptions` -> `gpGameState->SetSystemOptions` |
| `ExportPersistentOptions` | 1.72 | 100 | new body from retail: same three temporaries, then `gpGameState->ExportPersistentOptions(state)`, `mSaveIdx = state.GetSaveIdx()`, then a `CMemoryStreamOut`/`CBitStreamWriter` block and `state.PutTo(writer)`. |
| `ImportGameOptions` | 3.70 | 100 | new body from retail: loop `CopyCompressedGameOptions(i, mGameOptionsData[i].data())` for `i < capacity()`, then `CopyCompressedMultiplayerOptions(mGlobalGameOptionsData.data())`. |
| `WriteBackupBuf` | 4.00 | 100 | Echoes-only. Retail reads the index from `gpGameState+0x7c`, which is `mSystemOptions + 0x28` = `CPersistentOptions::mSaveIdx`, indexes `mFileSlots` at `8*idx`, and calls `CopyCompressedGameState(idx, mFileSlots[idx]->mSaveBuffer.data())` guarded by `!mFileSlots[idx].null()`; then `SetCardSerial(mCardSerial)`. |
| `IndexFiles` | 1.92 | 100 | Echoes-only, one file. `mError = kE_OK`, `mFileInfo->Open()`; `kCR_NOFILE` -> `kE_FileMissing`/`kS_FileBad`; `kCR_READY` -> `CardStat stat;` (its ctor already memsets 0x6C, which is retail's `memset`) then `GetStatus(mCardPort, mFileInfo->GetFileNo(), stat)`, `GetCommentAddr() == -1` -> `kE_FileCorrupted`/`kS_FileBad`, else `StartFileRead()`. |
| `UpdateFileWrite` | 2.56 | 100 | Echoes-only (it takes `successState`/`errorState`). `PumpCardTransfer()`; READY -> `mState = successState` and `WriteBackupBuf()` only when `successState == kS_DriverClosed`; BUSY -> return; IOERROR -> `kS_FileWriteFailed`/`kE_CardIOError`; else `NoCardFound()`. No `CloseFile()` — Prime 1 has one, retail does not. |
| `SGameFileSlot::InitializeFromGameState` | 2.04 | 100 | Prime 1's shape, but the `CMemoryStreamOut`/`CBitStreamWriter` pair must be in an **explicit inner scope** `{ ... }` so the two destructors are emitted *before* `LoadGameFileState`, not at function end. |
| `SGameFileSlot::SGameFileSlot()` | 42.93 | 100 | `CMemoryStreamOut` + `CBitStreamWriter` + `CGameState::SerializeNewForCleanSlot(writer, gpGameState->GetHardModeEnabled())`, and **no** `LoadGameFileState` (retail goes straight to the epilogue). |

### Improved but not 100% (kept: the bodies are verified correct against retail)

* `ExportGameOptions` 1.89 -> **97.98**. Correct and complete; the only difference is register
  allocation: we materialise `gpGameState + 0x144` in a register and use displacements `+0x8` /
  `+0x10`, retail keeps `gpGameState` and folds `0x14c` / `0x154` into the loads. Same
  instruction count, same semantics. Needs `CGameState::CompressedGameOptions()` /
  `CompressedMultiplayerOptions()` accessors (the members are private and, in the matching build,
  live in the non-`TARGET_PC` branch - **mwcceppc does not define `TARGET_PC`**, so the matching
  build compiles the `#else` layout; putting the accessors in the `#ifdef TARGET_PC` block fails
  with "undefined identifier").
* `StartFileCreate` 3.33 -> **86.30**. Correct and complete. Retail tests `-9` (kCR_INSSPACE)
  first but emits that call block *after* the `-8` (kCR_NOENT) body, sinking it to the function
  tail. Spellings tried: `-9` first without a `kCR_READY` early-out (76.0), `-9` first with an
  empty `if (result == kCR_READY) {}` arm (79.5), `-8` first with the empty arm (86.3), `-8`
  first nested in `if (result != kCR_READY)` (86.3, kept - same bytes, no empty branch). Not a
  wall: 3 instructions of 33 differ and they are all block ordering.

## Still sub-100, and what blocks them

Unchanged, with the evidence for each:

* `__ct__17CMemoryCardDriver` 61.88% (812 B). Retail's initialiser list is fully readable, but
  one member is initialised through an **out-of-line copy ctor** our tree does not have: the
  `rstl::reserved_vector<reserved_vector<uchar,32>,3>` at `0x114` is built from a stack temp via
  the unnamed 56-byte function at `0x8017C27C` calling a 184-byte `uninitialized_copy_n` at
  `0x8017C2B4`, which takes its element count from `src->mCount` (retail copies `src.mCount = 32`
  into a 3-element destination and then copies 3 elements - it reads past the temp, so the
  spelling is not recoverable from the instruction stream alone). Our
  `reserved_vector<...,3>::reserved_vector(const&)` instead inlines an `uninitialized_fill_n`.
  Fixing this means changing `rstl/construct.hpp` or `reserved_vector.hpp`, which every unit
  shares - not something to do inside this item.
* `EraseFileSlot` 1.25% (320 B) and `CopyFileSlot` 0.79% (504 B). Both bodies are readable and
  both end in `fn_80003D00(gpGameState + 0x80, &opts)` followed by `fn_80004D84(&opts)`
  (`~CGameOptions`), i.e. `CGameState::SetGameOptions(const CGameOptions&)`. `fn_80003D00` is
  *defined* in `src/MetroidPrime/CMainResetGameState.cpp:281` but not declared in any header, so
  calling it needs a new declaration (and `check_symbol_names.py` would have to accept the
  symbol). `CopyFileSlot` additionally needs `operator new(size, name, name)` and the
  byte-wise `uninitialized_copy` of one 32-byte option buffer. Both were left as honest TODOs
  rather than guessed at.
* `BuildSaveBuffer` 1.10% (364 B). Almost fully readable, but it calls an unnamed helper at
  `0x80142BA4` with `(&mFileInfo->SaveBuffer(), 8184, <string at -30797(r13)>)` and no declaration
  for it exists. Not written.
* `ReadFinished` 0.66% (608 B), `BuildExistingFileSlot` 0.61% (660 B), `BuildNewFileSlot` 0.99%
  (404 B), `InitializeFileInfo` 0.90% (444 B), `Update` 1.03% (388 B), `GetSaveSignature` 1.94%
  (288 B). Not attempted; each needs either unnamed helpers or the save-header/comment string
  layout, and each is one function.
* `fn_8017BE84` (80 B), `fn_8017BED4` (124 B), `fn_8017C27C` (56 B), `fn_8017C2B4` (184 B) - four
  retail functions with no symbol in our object. They are `rstl` template instantiations
  (`reserved_vector::erase` + its `destroy_elements`, and the vector-of-vectors copy ctor plus
  its out-of-line `uninitialized_copy_n`). Matching them means changing shared `rstl` headers so
  those instantiations come out at those exact addresses. **Not a target for a per-unit item.**

## Codegen rules worth keeping (measured, not recalled)

* When retail stores two adjacent members, the **source order is the store order**. Six
  `Start*` functions sat at 99.8% purely because `mError` and `mState` were assigned in the
  wrong order. `StartCardProbe` and `StartMountCard`, which retail also stores `mState` first,
  were already 100% and were left alone - the rule is per function, not "always mError first".
* A `switch` over a sparse enum becomes a range check plus a jumptable. The range check's width
  comes from the **highest and lowest case present**, so a `case` that does nothing still has to
  be written to get the right `cmplwi` immediate.
* `reserved_vector::capacity()` is a compile-time constant and `size()` is not. Retail's
  `li r5, 2616` and `cmpwi r29, 0x3` mean the source spelled `capacity()`, and a `size()` there
  costs an extra load.
* A local that retail's codegen destroys early needs an explicit `{ }` scope; mwcc otherwise runs
  the destructor at end of function.
* `CMemoryStreamOut`'s defaults already are `kOS_NotOwned` (=1) and `blockLen = 4096` - the enum
  is `{ kOS_Owned, kOS_NotOwned }`, so the default is 1, which is what retail passes. Do not
  "fix" the call sites to pass `kOS_Owned`.

## Gates (all run in this worktree, all clean)

```
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      -> probe: 744 files, 0 failed, 0 errors; LINKED
python3 tools/check_symbol_names.py           -> checked 502 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit main/MetroidPrime/CMemoryCardDriver
                                               -> none emits its functions out of retail order
./tools/decomp_build.sh                       -> All: 29.92% fuzzy, 21.73% matched (9690 / 28465)
86 RELs                                       -> cmp-equal to orig/G2ME01/files/RelProd/, 0 differ
```

"No function anywhere got worse" was checked the expensive way: `git stash` of the three changed
files, full rebuild, per-unit `(matched_functions, fuzzy_match_percent)` for all 1531 code units
snapshotted, `git stash pop`, full rebuild, then diffed. **0 units worse.** The only shared-header
edits are two additive inline accessors, neither used outside this unit.

`config/G2ME01/config.yml` and `splits.txt` were not touched; `total_functions` is still 28465.
`configure.py` is untouched, so the unit is still `NonMatching` and no `flip_test.sh` was run.

## Files changed

* `src/MetroidPrime/CMemoryCardDriver.cpp` - the 20 bodies above (156 insertions / 57 deletions
  in the unit's own file; the diff also rewrites `UpdateFileRead`, `UpdateFileWrite`,
  `WriteBackupBuf`, `UpdateCardProbe`, `IndexFiles`, `StartFileCreate`, `HandleCardError`,
  `UpdateMountCard`, the two `SGameFileSlot` ctors, `InitializeFromGameState`, the four
  Export/Import functions, `GetCardFreeBytes`, `SGameFileSlot::PutTo`, `IsRepairingHeader`, and
  swaps `mError`/`mState` in six `Start*` functions).
* `include/Kyoto/CMemoryCardSys.hpp` - one line: `CCardFileInfo::IsRepairingHeader()`.
* `include/MetroidPrime/Player/CGameState.hpp` - nine lines: `CompressedGameOptions()` and
  `CompressedMultiplayerOptions()` in the non-`TARGET_PC` public section.

No file under `tools/`, `docs/`, or `build/goal/` other than these notes was edited. No
`asm` was added; no initialisation was removed.

---

# Run 2 — `wt-mp2-goal-L7` (lane 7), branch `goal/lane-7`, base commit `fb644df0`

Re-measured the clean tree first, as the previous run's conclusions are hypotheses from a run that
did not reach these functions. The 37/53 the last run recorded was still the starting point, so
everything below is new measurement on this tree. Not committed (the driver commits).

## Result

| | before (measured on this tree) | after |
|---|---|---|
| `matched_functions` | 37 / 53 | **40 / 53** |
| `matched_code_percent` | 42.407944 | **53.90981** |
| `fuzzy_match_percent` | 51.2139 | **83.1175** |
| project `All:` | 31.32% fuzzy, 23.71% matched, 10316 / 28465 | 31.37% fuzzy, 23.72% matched, **10319** / 28465 |

**+3 functions reached 100%: `Update`, `EraseFileSlot`, `BuildNewFileSlot`.** Four more went from
~1% to 90-99% (`ReadFinished` 89.77, `BuildExistingFileSlot` 90.25, `BuildSaveBuffer` 98.68,
`CopyFileSlot` 99.16). `configure.py` is untouched, so the unit stays `NonMatching` and no
`flip_test.sh` was run. `tools/goal_check.sh build/goal/item.json` -> **PASS**.

## The two things that unlocked the rest

Neither is in the previous run's notes, and both are one-line fixes worth more than the bodies.

### 1. `operator new`'s placement string: name retail's, do not spell your own

Every `new SGameFileSlot(...)` in this unit goes through `__nw__FUlPCcPCc`, and retail passes
`lbl_803A9A94 + 459` as the file operand (`lis`+`addi` at 0x8017A9D8-0x8017A9E8,
0x8017A754-0x8017A768, 0x8017AEF8, 0x8017BC5C). `lbl_803A9A94` is `symbols.txt:17212`, a 0x1EC-byte
`.rodata` pool; offset 459 is the six bytes `??(??)`, which is exactly what `rs_new` spells. So:

```cpp
extern "C" const char lbl_803A9A94[];
#define CMEMORY_NEW_FILE (lbl_803A9A94 + 459)   // BEFORE any include
```

declared and never defined, for the reason `src/MetroidPrime/Factories/CStateMachineFactory.cpp`
already gives: a literal of our own is routed through mwcceppc's per-TU `@stringBase0` and makes
this object emit a `.rodata` section that shifts every later global-pool entry. Retail's copy is
in `auto_06_803A9A58_rodata.o`, which precedes this object, so naming it resolves as-is.
`CMEMORY_NEW_FILE` must be set before *any* include - `rs_new` is expanded inside
`Kyoto/IObj.hpp` further down the include chain.

**Without this, `BuildNewFileSlot` sits at 99.01% and no amount of body-tweaking reaches 100%** -
the object is 4 bytes short and the missing 4 are the third `addi` of the address pair. Measured:
`lis r3,0 / addi r4,r4,0 / li r3,2664 / li r5,0 / bl` (ours, 5) vs retail's 6 with
`addi r4,r4,459`.

### 2. A `.sdata` byte used by address must be declared **non-const**

`BuildSaveBuffer` calls `fn_80142BA4(&saveBuffer, 8184, &lbl_80418533)`; `lbl_80418533` is
`.sdata:0x80418533`, `size:0x1`, and retail's operand is `addi r5,r13,-30797` (0x8017B0FC) - the
single `R_PPC_EMB_SDA21` form. `extern "C" const uchar lbl_80418533[];` gives
`lis r3,0 / addi r3,r3,0` with `R_PPC_ADDR16_HA`/`_LO` instead: **three extra instructions**, and
the function drops from 98.68% to 96.70%. `extern "C" unsigned char lbl_80418533;` (matching what
`src/MetroidPrime/Player/CGameStateSlotDefaults.cpp:62` does for the same object) is right, and
the operand is `&lbl_80418533`, not the array.

## Per function — measured before% -> after%, and the spelling that got there

Prime 1 (`../prime-ref/src/MetroidPrime/CMemoryCardDriver.cpp`) is the starting point for the
state machine, but Echoes' engine is a fork and Prime 1's `Update` dispatches eleven states
against Echoes' eight. Retail's disassembly, not Prime 1, decided every body.

### Now 100%

| function | before | after | what decided it |
|---|---|---|---|
| `Update` | 1.03 | 100 | Prime 1's shape with Echoes' `switch`. The `switch` spans `kS_CardProbe`..`kS_CardFormat` (`addi r0,r3,-19 ; cmplwi r0,8`, 0x8017BB5C/60) and **`case kS_CardProbe: break;` must be written** - it is what makes the range 9 wide rather than 8. It costs one `case` that does nothing and buys the right `cmplwi` immediate. `mIsCardBusy` is `CMemoryCardSys::mIsCardBusy`, not Prime 1's `gpMain->SetCardBusy`. |
| `EraseFileSlot` | 1.25 | 100 | `mFileSlots[idx] = nullptr`; a `CGameOptions` local built with `__ct__12CGameOptionsFv`, serialised into `mGameOptionsData[idx]` through `CMemoryStreamOut`+`CBitStreamWriter`, handed to `CopyCompressedGameOptions(idx, ...)`, then `if (gpGameState->SystemOptions().GetSaveIdx() == idx) fn_80003D00(&gpGameState->GameOptions(), &opts)`, then `fn_80004D84(&opts, -1)`. **The local is a POD mirror, not a `CGameOptions`**: `CGameOptions.hpp:21` *declares* a destructor, so a real local makes mwcceppc call `__dt__12CGameOptionsFv`, a name retail's symbol table does not carry. `CMainResetGameState.cpp:300-310` sets out the same arrangement for its `SGameOptionsCopy`; the two `extern "C"` declarations are already in `src/MetroidPrime/main.cpp:1793-1794`, so this unit only needs its own copies (they are TU-local declarations of global symbols, not new definitions). |
| `BuildNewFileSlot` | 0.99 | 100 | `mFileSlots[idx] = rs_new SGameFileSlot()` when null; then a 3-iteration loop copying **every** slot into `gpGameState` (`CopyCompressedGameState(i, mFileSlots[i]->mSaveBuffer.data())` / `ClearCompressedGameState(i)`); then a scoped `CMemoryInStream` over `mSystemData` for `ReadSystemOptions`; then `SystemOptions().SetSaveIdx(idx)`, `ImportPersistentOptions()`, `ImportGameOptions()`, `SetCardSerial(mCardSerial)`. **No `slot->LoadGameState(idx)`** - Echoes has no such method, and retail's `SGameFileSlot` is 0xA68 with only the two constructors. Needs the new `CompressedGameStates()` accessor (see below). The 100% is entirely due to finding #1. |

### Improved but not 100% (kept: every one is verified against retail, and the bodies are complete)

* `ReadFinished` 0.66 -> **89.77** (608 B). The body is right and instruction-for-instruction
  modulo register allocation: retail keeps `this` in r28 and the slot pointer in r31, ours in r27
  and r30. Nothing tried moved it - `int i` shared across the two loops, `3` vs `.capacity()`,
  `= nullptr` vs `= rstl::auto_ptr<SGameFileSlot>()`, a named `rstl::auto_ptr&` per iteration
  (90.14, the best of those). The residue is one extra callee-saved register, which is what the
  `lbl_803A9A94 + 459` pair needs to stay live across the slot loop.
* `BuildExistingFileSlot` 0.61 -> **90.25** (660 B). Mirror of `BuildNewFileSlot`'s loop: read each
  of `gpGameState`'s three compressed **game-state** buffers
  (`rstl::vector<uchar>` is 16 bytes with `mItems` at `+0xC`, so element `i`'s count is
  `this+0x118+16i` and its data `this+0x120+16i` - measured off retail's
  `lwz r0,280(r3) / lwz r4,288(r3)` at 0x8017A738/0x8017A744), rebuild the slot from it, else null.
  Then `ExportGameOptions()`, `SetSaveIdx(idx)`, allocate-or-`InitializeFromGameState()` on slot
  `idx`, and a scoped `CMemoryStreamOut` over `mSystemData` for `WriteSystemOptions`, then
  `mSaveIdx = gpGameState->SystemOptions().GetSaveIdx()`. The `CMemoryInStream`'s length is
  `li r5,2616` (0x8017A74C) = `SGameFileSlot::mSaveBuffer`'s capacity, which is not a constant
  expression here, so it is named as a local `enum { sSaveSlotSize = 0xa38 }`.
* `BuildSaveBuffer` 1.10 -> **98.68** (364 B). Two instructions short, and they are the same two
  bytes of the `bool` normalisation in all three `mSavePresent` stores: retail
  `cntlzw ; rlwinm r0,r0,27,24,31` twice, ours `cntlzw ; rlwinm ; cntlzw ; srwi r0,r0,5`. Both
  compute `!(x == 0)`, and the difference is which `!` mwcc picks. **`mFileSlots[i].null() == false`
  scores 98.68 and `!mFileSlots[i].null()` scores 96.70** - the two spellings are not the same code.
  Spellings measured, all worse: `!!get()` 89.62, `get() != nullptr` 89.62, `owner()` 74.40,
  `!(get() == nullptr)` 89.62, `? true : false` around either 88.41/90.05, `!(null() == true)`
  92.86, unrolled three statements 91.59. The tail loop is
  `for (it = mFileSlots.data(); it != mFileSlots.data() + mFileSlots.size(); ++it)` -
  96.70 with a countdown, 91.87 with an index, 80.37 with a hoisted `end`, **96.70->98.68 (with
  finding #2) with the two-expression condition**.
* `CopyFileSlot` 0.79 -> **99.16** (504 B). Stream the source slot's `mSaveBuffer` into a new
  `SGameFileSlot` on the destination index, then `CopyCompressedGameOptions(to,
  CompressedGameOptions()[from].data())`, then `mGameOptionsData[to] = mGameOptionsData[from]`.
  The length is `sSaveSlotSize` again (`li r5,2616`, 0x8017AB48), and the `CMemoryInStream` must be
  in an explicit `{ }` so `__dt__12CInputStreamFv` is emitted before the option copy, not at
  function end. One instruction left: retail indexes `this+0x154+16*from` off `gpGameState`
  directly, we materialise `gpGameState+0x144` in a register first. Everything tried for it
  (`const&` to the vector, `(void)to`, an iterator pair) is byte-identical or worse.

### Not attempted, and why

* `InitializeFileInfo` 0.90% (444 B) and `GetSaveSignature` 1.94% (288 B). Both are fully
  readable, and both are blocked on the same missing thing: retail's `InitializeFileInfo` builds a
  `CCardFileInfo` from a name string it loads out of a **runtime global**
  (`lwz r4,-23384(r2)` then `string_l__4rstlFPCc`, 0x8017BC6C/0x8017BC74 - a `.sdata2` pointer
  table, not a literal), then `sprintf`s a timestamp from `OSCalendarTime`, and
  `GetSaveSignature` walks a 112-byte-stride array off `gpMemoryCard` calling a **virtual** at
  `vtable+0x0C` through a `CToken` round-trip (0x8017C498-0x8017C4C0) to hash each world's `SAVW`
  resource. Neither the global's name nor that vtable slot is nameable here, and guessing either
  would be a fabricated body, which the reviewer rejects.
* `__ct__17CMemoryCardDriver` 61.88% (812 B). **Unchanged, and the previous run's wall holds**: the
  `rstl::reserved_vector<reserved_vector<uchar,32>,3>` member is built from a stack temp through
  retail's out-of-line `uninitialized_copy_n` at 0x8017C2B4, which reads past the temp. Fixing it
  means changing `rstl/construct.hpp`, which every unit shares. Not a per-unit item.
* `fn_8017BE84` (80 B), `fn_8017BED4` (124 B), `fn_8017C27C` (56 B), `fn_8017C2B4` (184 B) - the
  four `rstl` template instantiations with no symbol in our object. Same conclusion as last run.

## Header change

`include/MetroidPrime/Player/CGameState.hpp`, **+8 lines, one accessor**, in the non-`TARGET_PC`
public section (mwcceppc does not define `TARGET_PC`, so the matching build compiles the `#else`
layout - putting it in the `#ifdef TARGET_PC` block fails with "undefined identifier"):

```cpp
rstl::reserved_vector< rstl::vector< uchar >, 3 >& CompressedGameStates() { return mCompressedGameStates; }
```

Purely additive, used only by this unit. Its offset is pinned by the two disassembly citations in
its comment. **No class layout changed** - `CHECK_SIZEOF(CGameState, 0x2f0)` still holds and
`tools/probe_gs_offsets.py` still reports 44/44 ok.

## Codegen rules worth keeping (measured here, not recalled)

* `mwcceppc` is 4-byte-aligning in this configuration, so `rstl::vector` is
  `{Alloc(4), int mCount, int mCapacity, T* mItems}` - 16 bytes with **`mItems` at `+0xC`, not
  `+0x8`**. Getting this wrong silently costs the `+0x118`/`+0x120` reads in
  `BuildExistingFileSlot` and `ReadFinished`. Measured with a `#define private public` +
  `offsetof` probe compiled with this unit's own flags (the values are in `.sdata`; read them with
  `powerpc-eabi-nm -n`, the globals are common-symbols and need `const` to be emitted at all).
* A `bool` member store from `x == false` and one from `!x` are **different code**
  (`98.68` vs `96.70` on the same three stores). Measure, do not assume.
* `operator new`'s third argument is the *file-name* string; mwcceppc's own is `@stringBase0`,
  a per-TU pool entry, and emitting one costs a `.rodata` section. Name a retail symbol instead
  (there is precedent and a mechanism for exactly this: `CMEMORY_NEW_FILE` in
  `Kyoto/Alloc/CMemory.hpp`).
* A `switch` over a sparse enum: the `cmplwi` immediate comes from the highest and lowest case
  **written**, including cases with no body.
* Register allocation, not the body, is what the last few percent of a long function is. Three
  separate functions here are 90-99% with byte-identical semantics and differ only in which
  callee-saved register holds `this`.

## Gates (all run in this worktree, all clean)

```
./tools/decomp_build.sh                       -> All: 31.37% fuzzy, 23.72% matched, 11.83% linked (10319 / 28465)
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      -> probe: 752 files, 0 failed, 0 errors; LINKED (250 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           -> checked 505 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit main/MetroidPrime/CMemoryCardDriver
                                               -> none emits its functions out of retail order
86 RELs                                       -> all sha1s match config/G2ME01/config.yml, 0 differ
./tools/goal_check.sh build/goal/item.json     -> PASS
```

"No function anywhere got worse" was measured the expensive way: `git stash`, full rebuild,
per-function `fuzzy_match_percent` for all 2062 units snapshotted, `git stash pop`, full rebuild,
diffed. **0 functions worse, 0 units worse, 3 newly at 100%, 4 improved.** Snapshots kept at
`.tmp/opencode/report.clean.json` and `.tmp/opencode/report.mine.json`.

`docs/HANDOFF.md`'s state block moved 10316 -> 10319 - that is `tools/goal_check.sh` running
`gate.sh` with `MP_GATE_DOCS_WRITE=1` (goal_check.sh:67 -> gate.sh:115), i.e. the judge's own
rewrite, not an agent edit.

`config/G2ME01/config.yml` and `splits.txt` were not touched; `total_functions` is still 28465.
`configure.py` is untouched, so the unit is still `NonMatching` and no `flip_test.sh` was run.

## Files changed

* `src/MetroidPrime/CMemoryCardDriver.cpp` - `Update`, `ReadFinished`, `BuildNewFileSlot`,
  `BuildExistingFileSlot`, `EraseFileSlot`, `CopyFileSlot`, `BuildSaveBuffer` bodies; the
  `lbl_803A9A94`/`CMEMORY_NEW_FILE` and `lbl_80418533` declarations; the `SGameOptionsMirror`
  POD struct and the `__ct__12CGameOptionsFv` / `fn_80003D00` / `fn_80004D84` / `fn_80142BA4`
  `extern "C"` declarations; the local `enum { sSaveSlotSize = 0xa38 }`; two new includes.
  (233 insertions / 9 deletions.)
* `include/MetroidPrime/Player/CGameState.hpp` - eight lines: `CompressedGameStates()`.

No file under `tools/`, `docs/` (beyond the judge's own state-block rewrite) or `build/goal/`
other than these notes was edited. No `asm` was added; no initialisation was removed.

---

# Run 3 — `wt-mp2-goal-L2` (lane 2), branch `goal/lane-2`, base commit `d2c46996`

Re-measured the clean tree first. Runs 1 and 2 had landed, so the starting point was **40 / 53**
measured here, not the 37 the first run recorded. Everything below is new measurement on this tree.
Not committed (the driver commits).

## Result

| | before (measured on this tree) | after |
|---|---|---|
| `matched_functions` | 40 / 53 | **44 / 53** |
| `fuzzy_match_percent` | 83.1175 | **83.46711** |
| `matched_code_percent` | 53.90981 | **66.321884** |
| project `matched_functions` | 11253 / 28465 | **11257** / 28465 |
| project `fuzzy_match_percent` | 32.405075 | 32.405594 |
| project `matched_code_percent` | 24.975672 | 24.994034 |

**+4 functions reached 100%: `StartFileCreate`, `ExportGameOptions`, `CopyFileSlot`,
`BuildSaveBuffer`.** A fifth, `BuildExistingFileSlot`, went 90.25 -> 90.87. `configure.py` is
untouched, so the unit stays `NonMatching` and no `flip_test.sh` was run.
`tools/goal_check.sh build/goal/item.json` -> **PASS**.

Neither run's wall was re-tried: `__ct__17CMemoryCardDriver` is still 61.88% and the four unnamed
`rstl` instantiations are still absent, for the reasons they give.

## The two things that unlocked it, and how they were found

Both are codegen rules, not body work, and both are worth more than the bodies they bought. I found
them by compiling isolated probe TUs with this unit's exact flags and reading the emitted
instructions, then confirming on the real unit with `tools/try_batch.py` (which counts *differing
instructions*, so a register-allocation difference is visible).

The probe command (the flags are `cflags_runtime` from `configure.py` plus the musyx include that
`tools/probe_cc.sh` is missing):

```sh
build/tools/wibo build/tools/sjiswrap.exe "$MP_TOOLCHAIN_DIR/build/compilers/GC/2.7/mwcceppc.exe" \
  -nodefaults -proc gekko -align powerpc -enum int -fp hardware -Cpp_exceptions off -O4,p -inline auto \
  -pragma "cats off" -pragma "warn_notinlined off" -maxerrors 1 -nosyspath -RTTI off -fp_contract on \
  -str reuse -i include -i libc -i build/G2ME01/include -i extern/musyx/include \
  -DBUILD_VERSION=0 -DVERSION=0 -multibyte -DNDEBUG=1 -use_lmw_stmw on -str reuse,pool,readonly \
  -gccinc -inline deferred,noauto -common on -lang=c++ -c probe.cpp -o probe.o
```

(`tools/probe_cc.sh` alone fails on any TU that reaches `Kyoto/CAudioSys.hpp`: it is missing
`-i extern/musyx/include` and the three `MUSY_*` defines.)

### 1. An accessor that returns the **element** keeps the object as the address base

Retail reads `gpGameState`'s three compressed-option buffers as
`lwz r0,gpGameState ; add r5,r0,r30 ; lwz r4,340(r5) ; lwz r5,332(r5)` (0x8017A4A0..0x8017A4B0) and
`+0x118` / `+0x120` in `BuildExistingFileSlot` (0x8017A730..0x8017A744): **`gpGameState` is the base,
the member offset rides in the load's displacement, and `r30` is the index.**

`gpGameState->CompressedGameOptions()[i]` produces one extra `addi` in every function that does it:

```
addi    r0,r4,324          ; ours materialises gpGameState + 0x144
add     r5,r0,r30
lwz     r4,16(r5)          ; +0x10 instead of +0x154
lwz     r5,8(r5)           ; +0x8  instead of +0x14C
```

A four-way probe on the identical loop shape settles what matters — **not** `rstl::reserved_vector`'s
layout (all three container shapes compile to retail's form when the member is indexed directly),
and **not** `const`:

| spelling | emitted |
|---|---|
| `self->mStates[i].data()` (direct member index) | retail's form |
| a local `const&` to the whole vector, then `[i]` | `addi r30, gpGameState, 276` materialised |
| `gpGameState->States()[i]` — **accessor returning `reserved_vector&`** | `addi r0, r4, 272` materialised |
| accessor returning **the element** (`return mStates[i];`) | retail's form |
| accessor returning **an element pointer** (`return &mStates[i];`) | retail's form |
| two accessors returning `.data()` / `.size()` | retail's form, but needs one more register |

So the trigger is *a reference to the whole vector*, and `gpGameState->CompressedGameOptions()[i]`
is exactly that. Two additive accessors fixed two functions outright:

| function | before | after |
|---|---|---|
| `ExportGameOptions` | 97.98 | **100** |
| `CopyFileSlot` | 99.16 | **100** |
| `BuildExistingFileSlot` | 90.25 | 90.87 |

Spellings tried and measured worse, for the record (differing instructions, `try_batch.py`):
whole-vector `const&` local, element `const&` local, non-const element local, `.at(i)`, a
pointer-walk `++o` over the elements (7), `size()` hoisted into a local before the `Put` (28),
`local-ref` on the outer vector (build fails). Nothing about `reserved_vector` itself needed to
change, which is the important part: `rstl/reserved_vector.hpp` is shared by every unit and was
not touched.

### 2. A `uchar` cast in front of the store, and nowhere else

`BuildSaveBuffer`'s two remaining instructions were the *same* normalisation written two ways:
retail closes it with `rlwinm r0,r0,27,24,31` (0x8017B154), we closed it with `srwi r0,r0,5`
(0x8017B13B8). Both are `>> 5` of a `cntlzw` result and compute the same value - the difference is
that retail's is **masked to a byte** and ours is not. Writing the assignment through a `uchar`
gives retail the masked form:

```cpp
header.mSavePresent[i] = static_cast< uchar >(mFileSlots[i].null() == false);   // 98.68 -> 100
```

Measured alternatives, all worse or unchanged: `null() == false` (2 differing instrs), `!null()`
(2), `get() != nullptr` (11), `!(get() == nullptr)` (11), `!!get()` (11), `get() ? 1 : 0` (31),
`!owner()` (41), and the three stores written out by hand (`null() == false` 11, `!null()` 11).
Only the `uchar` cast reaches 0.

This is a codegen nudge, not a change of value: `bool` -> `uchar` -> `bool` of 0/1 is identity.
It also matches a rule run 2 measured from the other side (`a bool member store from x == false and
one from !x are different code`) - the mask is a third form of the same choice.

### 3. `StartFileCreate`: an `||`, not an `else if` chain

Retail 0x8017B368..0x8017B398, after the `kCR_READY` test:

```
mr. r4,r3 ; beq end          ; kCR_READY -> leave
cmpwi r4,-9 ; beq body        ; kCR_INSSPACE
cmpwi r4,-8 ; bne other       ; kCR_NOENT      <-- same body as INSSPACE
body: li r3,15 ; li r0,5 ; stw r3,16(r31) ; stw r0,20(r31) ; b end
other: mr r3,r31 ; bl UpdateFileCreate
```

Both codes branch to **one** arm, and `-9` is tested first. That is what `a || b` compiles to and
what an `else if` chain does not: the `||` first arm is `kCR_INSSPACE`, the second `kCR_NOENT`, both
taking the same body, and everything else falls through to `UpdateFileCreate`.

```cpp
if (result != kCR_READY) {
  if (result == kCR_INSSPACE || result == kCR_NOENT) { mState = kS_FileCreateFailed; mError = kE_CardFull; }
  else { UpdateFileCreate(result); }
}
```

86.30 -> **100**. This is also a **behaviour correction**, not only a spelling: previously
`kCR_INSSPACE` fell into `UpdateFileCreate` (which retries the create on `kCR_READY`); retail
treats it exactly like `kCR_NOENT` and reports the card full. Run 2 read the same instructions and
described them as block ordering only; the `||` is what the tests actually say.

## Measured this run and still blocked

* `ReadFinished` **89.77%** (608 B) and `BuildExistingFileSlot` **90.87%** (660 B). Both differ
  from retail **only** by one extra callee-saved register. `mwcceppc` precomputes the
  `operator new` file-name address `lbl_803A9A94 + 459` into the **loop preheader** and keeps it
  live across the loop, so `this` lands in `r27` instead of `r28` and the whole allocation shifts
  (`stmw r25` / `stmw r27` against retail's `stmw r26`); retail emits
  `lis ; addi ; addi 459` at each `new` instead. `BuildExistingFileSlot` also has **two** `new`
  sites and retail recomputes both.
  The spelling search is over: compiled probes say the hoist happens for `sym + 459`, for a bare
  `sym` with no offset, for a non-`const` or `uchar` declaration of the symbol, for
  `sizeof("??(??)") - 1` and for `200 + 259` as the offset, for `&sym[459]`, through a
  `const char*` local declared before *or inside* the loop, through a `reinterpret_cast`, and with
  a folded `+ (i - i)` addend. A string *literal* avoids the `+459` but not the hoist, and it emits
  a `.rodata` section (run 2's reason for not using one). There is no symbol at 0x803A9C5F in
  `config/G2ME01/symbols.txt` to name instead, and defining one ourselves would move it.
* `InitializeFileInfo` 0.90% (444 B). Fully readable, but three things are unnameable from here:
  the object `new`ed at the top is an unnamed 364-byte type, the save-file name comes out of a
  runtime pointer (`lwz r4,-23384(r2)` at 0x8017BC6C, resolved by `tools/sda.py` against the
  `.sdata2` base, not a literal), and 33 bytes are copied from an unnamed `.rodata` object before
  `OSGetTime` / `OSTicksToCalendarTime` / a `sprintf` whose format string is `lbl_803A9A94 + 466`.
  Guessing any of the three is a fabricated body.
* `GetSaveSignature` 1.94% (288 B). Readable and short - lazy init of a 4-byte signature and a
  1-byte flag, the `"USA>"` seed `0x5553413E`, the `"SAVW"` asset id `0x53415657`, then a walk of
  a 112-byte-stride array off `gpMemoryCard` loading each world's resource through
  `gpSimplePool`'s vtable slot 3 into a `CToken`. **The blocker is placement, not naming**: the two
  statics are `.sbss` `0x80419230` / `0x80419234` (`tools/sda.py` on `-27472` / `-27468`), so the
  `lwz rX,<disp>(r13)` displacements only match if our own common symbols land on exactly those
  addresses, which is a linker-placement problem, not a source one.
* `__ct__17CMemoryCardDriver` 61.88% and `fn_8017BE84` / `fn_8017BED4` / `fn_8017C27C` /
  `fn_8017C2B4`: unchanged, and both prior runs' walls still hold (a shared `rstl` header change).

WALL: ReadFinished 89.77% - mwcceppc precomputes `lbl_803A9A94 + 459` into the loop preheader and the one extra callee-saved register shifts the whole allocation; no spelling of the address expression (12 probed forms) avoids the hoist.

## Codegen rules worth keeping (measured here, not recalled)

* **An accessor that hands back a reference to a whole container costs you the object as the
  address base.** Return the element (or a pointer to it) instead. Measured here and it is worth
  one instruction per function, and two whole functions.
* **`static_cast<uchar>` on a value about to be stored as `bool` picks `rlwinm r0,r0,27,24,31`
  where a plain `bool` picks `srwi r0,r0,5`.** Same value, different word; retail picked the
  masked one.
* **`a || b` is not `if (a) X else if (b) Y`.** mwcc compiles the `||` to two tests that both
  branch to the same body (first operand tested first); the `else if` chain gives each its own
  arm in the opposite order. Read the branch targets, not the arithmetic.
* **mwcceppc precomputes a constant address (`symbol + n`) out of an enclosing loop.** Retail's
  `operator new` file-name string does not get that treatment, so any loop containing `rs_new`
  costs one extra callee-saved register against retail. This is a general hazard for every
  `rstl`-flavoured loop with a `new` in it, not just this unit.
* `rstl::reserved_vector`'s own shape is **not** what decides the addressing: a direct member
  index compiles to retail's form for `uchar mData[N*sizeof(T)]`, for a typed `T mData[N]`, and
  for a plain `T[N]` C array. Do not go changing `rstl/reserved_vector.hpp` to chase this.

## Gates (all run in this worktree, all clean)

```
./tools/decomp_build.sh                       -> All: 32.41% fuzzy, 24.99% matched, 11.94% linked (11257 / 28465)
sha1sum build/G2ME01/main.dol                 -> 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
./tools/probe_sources.sh                      -> probe: 751 files, 0 failed, 0 errors; LINKED (244 undefined, 0 duplicates)
python3 tools/check_symbol_names.py           -> checked 514 units; 0 declared names are missing
python3 tools/check_decl_order.py --unit main/MetroidPrime/CMemoryCardDriver
                                               -> none emits its functions out of retail order
86 RELs                                       -> gate.sh's dtk shasum check passes (part of goal_check)
./tools/goal_check.sh build/goal/item.json     -> PASS
```

"No function anywhere got worse" was measured by diffing every function's `fuzzy_match_percent`
between `build/report.json` before and after: **0 functions worse, 0 units worse, 4 newly at 100%,
1 improved** (snapshots at `.tmp/opencode/report.before.json` and `build/report.json`).

`tools/unit_fit.sh MetroidPrime/CMemoryCardDriver.cpp` still reports 16 extra functions, all of them
COMDAT template instantiations and destructors (`uninitialized_fill_n<reserved_vector<...>>`,
`destroy_elements<reserved_vector<auto_ptr<SGameFileSlot>,3>>`, `~map<...>`,
`~CPersistentOptions`, ...). That list is unchanged by this run - nothing here adds or removes an
emitted function.

`docs/HANDOFF.md`'s state block moved 11253 -> 11257 and 9705 -> 9709: that is
`tools/goal_check.sh` running `gate.sh` with `MP_GATE_DOCS_WRITE=1` (goal_check.sh:67 -> gate.sh:115),
i.e. the judge's own rewrite, not an agent edit.

`config/G2ME01/config.yml` and `splits.txt` were not touched; `total_functions` is still 28465.
`configure.py` is untouched, so the unit is still `NonMatching` and no `flip_test.sh` was run.
No `NEW:` items are filed: everything found here is inside this item's own target, so filing it
would be a restatement of the current item.

## Files changed

* `src/MetroidPrime/CMemoryCardDriver.cpp` - `StartFileCreate`'s `||` (behaviour correction, 100%),
  `BuildSaveBuffer`'s `uchar` cast (100%), `CopyFileSlot` / `BuildExistingFileSlot` /
  `ExportGameOptions` switched to the per-element accessors, and a comment on each of the three
  changes citing the retail instruction that decides it. (23 insertions / 4 deletions.)
* `include/MetroidPrime/Player/CGameState.hpp` - two additive per-element accessors,
  `CompressedGameStatesAt(int)` and `CompressedGameOptionsAt(int)`, in the non-`TARGET_PC` public
  section, with the disassembly that pins the addressing. No member moved, no overload added, no
  existing declaration removed.

No file under `tools/`, `docs/` (beyond the judge's own state-block rewrite) or `build/goal/`
other than these notes was edited. No `asm` was added; no initialisation was removed.
