# port-boot-savw-factory - a real SAVW factory (`CWorldSaveGameInfo`)

Done by hand on 2026-10-01 at 8510078d.

## Format (Echoes, read off `fn_80182EC8`, all big-endian `u32`)

magic (skipped), version, areaCount; cinematics, relays (`count` + `TEditorId`s); layers (`count` +
(area, layer) pairs); doors; scans (`count` + (asset id, u32) pairs); if version > 3 the system and
game environment variables (`count` + string, min, max, default); if version > 4 the unmappable
objects. Prime 1 gates the first five on lower versions; Echoes reads them unconditionally.
Checked against `FrontEnd.pak`'s SAVW: 64 bytes, `c001d00d 5 1 1 9 0 0 0 0 0 0 0` then `ffffffff`
pak padding = version 5, 1 area, 1 cinematic (`0x9`), all other lists empty.

## What changed

- `src/MetroidPrime/CWorldSaveGameInfo.cpp` (new, port-only, in `files.cmake`): the constructor,
  `TEditorId(CInputStream&)` (undefined anywhere before; `!__MWERKS__`) and `fn_80182830`.
  Retail's code is in the unsplit `auto_03_80182830_text`, so it cannot be a matching unit yet.
- `CFactoryFunctionsPort.cpp`: the `PORT_FACTORY(fn_80182830)` line removed.

## Measured

- gdb: the ctor builds `areaCount=1 cin=1 relays=0 layers=0 doors=0 unmap=0 scans=0 sys=0 game=0`.
  `CMemoryCard::mWorldInter` is null by frame 126, so every `CSaveWorldIntermediate` finished.
- Next wait (temporary stderr print in `CMemoryCard::InitializePump`, reverted): `mWorldName`/
  `mDarkWorldName` invalid (name ids -1, nothing to wait for) and `mHints.IsLoaded()` = 0.
- `boot-progress.sh`: undecidable against the recorded head (same markers, both exit 0 at frame
  300, every head stub still hit) - this change moves state the judge cannot see.
- `link_check`/`probe_sources`: 323 undefined before and after, 0 duplicates; 748 files, 0 failed.
- `decomp_build.sh`: sha1 6ef9b491..., `All: 33.77% fuzzy, 26.97% matched, 12.64% linked
  (11953 / 28465 functions)` before and after.

## Not run

RELs `cmp`, `flip_test.sh`: no matching-build source touched (`configure.py` unchanged).

## Next blocker

`'HINT'` returns an empty object, so `mHints.IsLoaded()` never turns true.

## Negative result: forwarding 'HINT' (`fn_8017F988`) to `FHintFactory`

Tried after the SAVW commit: replace `PORT_FACTORY(fn_8017F988)` with an `extern "C"` forwarder to
`FHintFactory` (`CGameHintInfo.cpp:261`, not declared in a header). `link_check` stayed at 323
undefined / 0 duplicates, but `boot-progress.sh` against a baseline re-recorded at the SAVW commit
reported both runs `crash: SIGSEGV; last marker: frame: 6`, in `CMainFlow::SetGameState`
(`CMainFlowDtor.cpp:351`) via `CMainFlow::OnMessage`. The real hint factory is not usable as it
stands, so the change was reverted and is not committed. Next: find what in `SetGameState`
dereferences at frame 6 once the hint object is non-empty.
