/**
 * `CMain::FillInAssetIDs` - retail **0x80006B38, 0x48 = 72 bytes**, and the whole reason this
 * file exists is that a `Matching` unit is one range of one section, and 72 bytes in the middle
 * of `MetroidPrime/main.cpp`'s 0x1AD4-byte claim cannot be one.
 *
 * The body is one line and it was already written and already at 100.00% inside `main.cpp`.
 * **The unit, not the function, was the thing that was `NonMatching`**: `FillInAssetIDs` shared a
 * claim with 49 other functions, so objdiff scored the 50-function claim and the 72-byte function
 * could not be promoted on its own. `tools/unit_fit.sh` on this unit reports
 * `.text claimed 72, ours 72, retail 72, fits`, with no extra functions, and `tools/flip_test.sh`
 * PASSes - the build still reproduces retail with **this** object in the link.
 *
 * ## Why the source split is part of this carve
 *
 * `dtk dol split` refuses an interior carve outright - `Split 3:0x80006B38..3:0x80006B80 overlaps
 * with previous split` - so `main.cpp`'s single claimed range had to be cut three ways:
 *
 *   `main.cpp`                    0x800053B8-0x80006B38  (keeps the retail globals)
 *   **`CMainFillInAssetIDs.cpp`**  0x80006B38-0x80006B80  (this file, 72 bytes, `Matching`)
 *   `MetroidPrime/mainMid.cpp`    0x80006B80-0x8000848C  (the ten functions above this one)
 *
 * **Narrowing `main.cpp`'s claim on its own is not a change, it is a regression**: the ten
 * functions in 0x80006B80-0x8000848C stop being claimed, become `main/auto_03_80006B80_text`, and
 * score 0% - measured, `matched` 3974 -> 3965. The carve and the source split are one change, and
 * they are four files: this one, `main.cpp`, `mainMid.cpp`, and the three manifests.
 *
 * ## What it costs, measured, and it is not free
 *
 * mwcceppc's `@stringBase0` pool is **per translation unit and ordered by first use in emission
 * order**, so cutting `main.cpp` in three re-orders the pool of whichever unit received
 * `CGameArchitectureSupport`'s constructor (0x80007EC4) and `CGameGlobalObjects::AddPaksAndFactories`
 * (0x80007168) - the two functions that read it. The constructor's four `operator new` sites
 * reference `@stringBase0 + 0` while the whole file is one unit and `@stringBase0 + 0x76` once it
 * is not. **That is a real fidelity loss on the function the boot probe executes at step 17**, and
 * it is why this carve is a decision rather than a free win. The numbers are in
 * `docs/HANDOFF.md`; this file's header is not the place they move.
 *
 * ## The retail globals stay in `main.cpp`
 *
 * `gpSimplePool` and `gpResourceFactory` are declared `extern` in their own headers
 * (`include/Kyoto/CSimplePool.hpp:62`, `include/Kyoto/CResFactory.hpp:183`) and **defined** in
 * `main.cpp`, which is the unit that kept the low range. This is the arrangement
 * `MetroidPrime/mainTail.cpp` already records, and it is the one the port wants: `files.cmake`
 * lists this file too, so the port link resolves both globals against `main.cpp`'s definitions.
 *
 * `lbl_803A56C0` is retail's own `.rodata` string pool, **declared and never defined** in both
 * files, which is one undefined symbol the port's link gap already counts.
 */
#include "MetroidPrime/CMain.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/SObjectTag.hpp"

// Retail `.rodata` 0x803A56C0 - the pool `main.cpp`'s header documents. The resource name this
// function looks up is `+0x07C` into it.
extern const char lbl_803A56C0[];

// Superseded 2026-09-28: upstream's configure.py has no CMainFillInAssetIDs unit, so this file
// is host-only now, and upstream's CSfxManager.cpp defines retail 0x8029C7E8 under its real
// name. The port's CSimplePool::fn_8029c7e8 stand-in (CSimplePoolPort.cpp) went with the merge.
void CMain::FillInAssetIDs() {
  CSfxManager::LoadTranslationTable(gpSimplePool,
                                    gpResourceFactory->GetResourceIdByName(lbl_803A56C0 + 0x07C));
}
