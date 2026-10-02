// CScriptDarkSamusBattleStageRel.cpp - DarkSamusBattleStage's (module 11) head, .text
// 0x0..0x74: the three functions above the module's class code, and nothing else. Same
// arrangement as `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` and
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/DarkSamusBattleStage/symbols.txt`:
//
//   0x00  fn_11_0   0x24  li r3,0 ; bl fn_80235DCC          (RELExit)
//   0x24  fn_11_24  0x20  bl fn_11_44                        (RELMain)
//   0x44  fn_11_44  0x30  lbl_11_bss_0 = fn_11_74 ; fn_80235DCC(&lbl_11_bss_0)
//
// **This module has no accessor block**, which is why the head is three functions and
// 0x74 bytes where `CIngPuddleRel.cpp`'s is five and 0xA8. That is measured, not assumed:
// `build/G2ME01/DarkSamusBattleStage/asm/auto_00_00000000_text.s` has `fn_11_0` calling
// `fn_80235DCC` and nothing else at all, whereas the MysteryFlyer family's `fn_45_0` is
// `li r3,1`. The class is a `CScript` stage object rather than a `CActor`: the module's
// vtable is the 0x20-byte table at `.data:0x0`
// (`build/G2ME01/DarkSamusBattleStage/asm/auto_04_00000000_data.s`) and holds
// `fn_11_B78`, `TypesMatch__27CScriptDarkSamusBattleStageCFi`,
// `PreThink__7CEntityFfR13CStateManager`, `Think__7CEntityFfR13CStateManager`,
// `AcceptScriptMsg__7CEntityFR13CStateManagerRC10CScriptMsg` and `SetActive__7CEntityFb`
// after two leading words - six virtuals off a `CEntity` base, not a `CActor`'s fourteen.
// So there is no `GetBoundingBox` wrapper, no `+0x818` member accessor and no
// `skDamageHitTime__10CPatterned` store to reproduce, and nothing is missing from the block: `fn_11_74`
// (0x74, 0x150) is already the module's own entity loader.
//
// Everything between fn_11_74 and the module's `REL_Setup` tail (0x74..0xD94) is left
// unclaimed, so dtk fills it from retail and the module's sha1 against
// `config/G2ME01/config.yml` still holds. Those twelve are the module's class code - the
// generated `SLdrDarkSamusBattleStage` loader, the `SLdrEditorProperties` reader with its
// 30-way typedef switch, and the `CEntity` constructor/destructor - which needs the
// `CEntity` hierarchy this tree does not model.
//
// The tail at 0xD94..0xF38 is claimed separately, by the shared "REL" lib's
// `REL/REL_Setup.cpp` (`_unresolved`, `_epilog`, `_prolog`, `ModuleDestructors`,
// `ModuleConstructors`), which is the free 0 -> 5 every module gets once its `splits.txt`
// claims the range - see `docs/RUNNING_THE_DECOMP.md`. That claim needs
// `RELExit`/`RELMain` named `scope:global` above, which is why `symbols.txt` gained those
// two names and not any other, and it is why `_epilog`/`_prolog` resolve against this
// file's definitions rather than against dtk's retail ones. **20 text symbols split
// 3 ours + 5 `REL_Setup` + 12 unclaimed**, which is exactly the sum of this module's
// units' `total_functions` in `build/report.json`; `tools/audit_rel_claim.py
// DarkSamusBattleStage` reports 3/3 and 5/5 with 0 problems and 0 of 20 text symbols
// dropped by `-strip_partial`.
//
// The two callees are named by what they are, not invented:
//   - `fn_80235DCC` is the DOL's 0x80235DCC, 8 bytes, in
//     `build/G2ME01/asm/auto_03_80235DCC_text.s`:
//     `stw r3, gLoader_DarkSamusBattleStage@sda21(r0); blr`. So it stores the *address* of a
//     loader slot, not a loader. `LoadDarkSamusBattleStage` in the `Matching` unit
//     `src/MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp` reads it as
//     `(*gLoader_DarkSamusBattleStage.value)(mgr, input, info)`, which is why the store below
//     hands it `&lbl_11_bss_0` and why that slot is four bytes wide. That source also records
//     that the 8-byte setter is deliberately not claimed in the DOL, because REL modules
//     import it by its retail name - so it stays in dtk's auto unit and it is the plain DOL
//     symbol, so **no `symbols.txt` rename and no DOL change are needed**. It is `extern "C"`:
//     an alias would be a different symbol and the call would resolve to nothing.
//     `strings build/G2ME01/DarkSamusBattleStage/DarkSamusBattleStage.plf | grep 80235D` is
//     where the module's own import table names it `fn_80235DCC`.
//   - `lbl_11_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. **This is the only object in the module's `.bss`**
//     (`build/G2ME01/DarkSamusBattleStage/asm/auto_05_00000000_bss.s` holds exactly one,
//     `lbl_11_bss_0` at 0x0), so unlike `CSwampBossStage1Rel.cpp` (which had to read the
//     offset off the dump to find the loader was the *last* of three) there is nothing to
//     choose here. This unit's split claims .text only, so dtk's `.bss` object has to define
//     it, and a second definition under MWCC is what produced mwldeppc's internal linker error
//     on ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" {
// .text 0x74, 0x150 bytes, still retail. The module's own entity loader; its signature is
// fixed by what the DOL's `LoadDarkSamusBattleStage` thunk calls it with, and only its address
// is taken here, so the body is not needed to reproduce the three functions above.
CEntity* fn_11_74(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80235DCC(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_11_bss_0;
#else
FScriptLoader lbl_11_bss_0 = 0;
#endif

// .text 0x44, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_11_44() {
  lbl_11_bss_0 = fn_11_74;
  fn_80235DCC(&lbl_11_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp would call. The MWCC
// branch is the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CIngPuddleRel.cpp`,
// `CMetareeSwarmRel.cpp` and `CSwampBossStage1Rel.cpp`: listing this file in `files.cmake`
// would make the port link `fn_11_74` and `fn_80235DCC`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps
// reading `DarkSamusBattleStage.rel` off the disc through `platform/rel.cpp`, and the `#else`
// branch exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_11_44(); }

void RELExit() { fn_80235DCC(nullptr); }
#else
void mp_relmain_darksamusbattlestage() { fn_11_44(); }

void mp_relexit_darksamusbattlestage() { fn_80235DCC(nullptr); }
#endif
}
