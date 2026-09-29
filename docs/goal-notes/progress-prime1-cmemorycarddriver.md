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
