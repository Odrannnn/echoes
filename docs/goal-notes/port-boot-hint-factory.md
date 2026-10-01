# port-boot-hint-factory (2026-10-01)

Goal: wire `'HINT'` (`fn_8017F988`) to `FHintFactory`, then follow the boot chain.

## Measured
- Baseline HEAD: 300 frames, exit 0, parked in `CPreFrontEnd` (`mHints.IsLoaded()` false).
- HINT forward (below): 44 hints parsed from the pak, plausible values. `CMemoryCard::InitializePump`
  then proceeds, `~CPreFrontEnd` runs, `SetGameState` is called a second time (`kCFS_FrontEnd`).
- First crash after that: `SetGameState` SIGSEGV on the uninitialised `CGMSinglePlayer` (reach stub).

## Committed: real `CGMSinglePlayer`
`src/MetroidPrime/PortCGMSinglePlayer.cpp`: ctor 0x80193E08, dtor 0x80193BD4, vtable 0x803B5CB0,
virtuals 0x80193C30-0x80193E04. `EndGame` picks the restart mode from the ending tier
(<75% 1, <100% 2, else 3). `files.cmake` lists it; the `CGMSinglePlayer` stub is gone from
`PortReachStubs.cpp`.
Gates: `boot-progress.sh` PASS (2 vs 2, exit 0, frame 300); link_check 322 undefined (323 before),
0 duplicates; probe 749 files 0 failed; `main.dol` sha1 6ef9b491...; `All:` 33.78% / 11956 functions
unchanged; check_symbol_names 0 missing; check_docs_claims agrees.

## Not committed: the HINT forward
In `src/Kyoto/CFactoryFunctionsPort.cpp`, replace `PORT_FACTORY(fn_8017F988) // HINT` with:

    const CFactoryFnReturn FHintFactory(const SObjectTag&, CInputStream&, const CVParamTransfer&);
    extern "C" CFactoryFnReturn fn_8017F988(const SObjectTag& tag, CInputStream& in,
                                            const CVParamTransfer& params) {
      return FHintFactory(tag, in, params);
    }

With it plus `CGMSinglePlayer`, the boot crashes in `CIOWinManager::Draw` (`CIOWinManager.cpp:200`)
at frame 5-7 on a null IOWin; `boot-progress.sh` judges it BEHIND (a crash before a clean 300-frame
head run). No null check was added; the null IOWin is the stubbed `fn_801F47F4`.

## Why it crashes
`CMainFlow::AdvanceGameState` runs on each `kAM_TimerTick`, so PreFrontEnd, FrontEnd and Game
advance on consecutive ticks. `kCFS_Game` builds `fn_801F47F4` (retail CMFGame ctor, 0x2D4 bytes,
returns a 44-byte IOWin). Also stubbed on the path: `fn_80143E88` (takes the FrontEnd branch since
`InitialWorld` is not found), the `CGMFrontEnd` ctor and virtuals, `EnsureWorldPaksReady`,
`fn_800068F4`, `CRelayTracker`/`CMapWorldInfo` ctors and `PutTo`, `~CWorldState`. The log also says
"Nothing read out of a pak will work".

## Next blocker
A real CMFGame (the game flow subsystem). That is a large new subsystem; stopped here.
