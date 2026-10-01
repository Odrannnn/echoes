# port-boot-stub-fn-8032f6ec-6268340 - `fn_8032F6EC` gets a real body

Done by hand on 2026-10-01 at 4d759af4 after lane 1's attempt died on a transport error with no change made.

## What it is

Retail `fn_8032F6EC` (0x88 bytes, in the unsplit `auto_03_8032EE68_text`) is the skinned-model workspace
set-up. `CGraphics::ConfigureVideo` calls it with the 0x40000-byte arena slice; `Shutdown` and
`ConfigureVideo` call it with `(nullptr, 0)`. From the asm: store `size` to `lbl_80418C78`; empty the
list at `lbl_803E0574` (`fn_8032F648`/`fn_8032F688`/`fn_8032F884`: unlink, `CMemory::Free`, count--);
store `buffer`/0 to `lbl_80419C58`/`lbl_80419C64`; if `buffer`, build a `CCircularBuffer(buffer, size,
kOS_NotOwned)` and assign it into the `optional_object<CCircularBuffer>` at `lbl_803E054C`
(`fn_8032F774`/`fn_8032F7A4`).

## What changed

- `src/Kyoto/Graphics/CGraphicsHostWorkspace.cpp` (new, port-only, in `files.cmake`): the body, with
  host-typed storage for the three globals, the list and the optional buffer. Nothing else in the port
  reads those retail names. `CGraphicsHostStartup.cpp` calls it as before (3 call sites).
- `PortReachStubs.cpp`: the `fn_8032F6EC` alias removed (it would be a duplicate under `MP_BOOT_STUBS`).
- Docs: PORT_NOTES, the CGraphicsHostStartup header, the gap list/table (59 -> 58, 319 -> 318), probe count 746 -> 747.

## Measured

- Baseline recorded at the unmodified head with `boot-progress.sh --record`: both runs exit 0 at `frame: 300`.
- `boot-progress.sh`: `BOOT_PROGRESS PASS: all 2 runs got further than all 2 head runs`; each run is
  "same markers, both exit with code 0, and stubs the head hit are no longer hit: ['fn_8032F6EC']".
- `link_check.sh`: unique undefined symbols 324 -> 323, duplicates 0.
- `probe_sources.sh`: 747 files, 0 failed; LINKED (323 undefined, 0 duplicates).
- `decomp_build.sh`: main.dol sha1 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010; `All:  33.77% fuzzy, 26.97% matched, 12.64% linked (11953 / 28465 functions)`.

## Caveats

- The list is walked from a zero-initialised header, so it is always empty in the port: retail builds the
  sentinel in a static constructor that the port does not run. The walk is kept so a future owner of the list
  is honoured.
- Not run: RELs `cmp`, `check_symbol_names.py`, `flip_test.sh` (no DOL unit touched).
