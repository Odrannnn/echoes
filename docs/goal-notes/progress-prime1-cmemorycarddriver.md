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
