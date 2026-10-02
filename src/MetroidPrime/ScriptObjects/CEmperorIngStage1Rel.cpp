// CEmperorIngStage1Rel.cpp - EmperorIngStage1's (module 16) entry-point block, .text
// 0xA22C..0xA2A0: three functions, and nothing else. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` and
// `MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp`, and the range comes from
// `config/G2ME01/rels/EmperorIngStage1/symbols.txt`:
//
//   0xA22C  fn_16_A22C  0x24  li r3,0 ; bl fn_8022756C          (RELExit)
//   0xA250  fn_16_A250  0x20  bl fn_16_A270                     (RELMain)
//   0xA270  fn_16_A270  0x30  lbl_16_bss_0 = fn_16_A2A0 ; fn_8022756C(&lbl_16_bss_0)
//
// **This block is in the middle of the module, not at its head**, and that is measured, not
// chosen. The fourteen-accessor block the REL loader generator emits at a module head is
// already claimed here by `EmperorIngStage1Accessors.cpp` at 0xB994..0xBA30, and this
// module's own entity loader `fn_16_A2A0` (0xA2A0, 0x31C) is the very next function above -
// so 0xA22C..0xA2A0 is the three functions between that loader and the code below it, and
// nothing more. Everything else in 0x0..0xC2A4 is the module's own class code and loader,
// left unclaimed so dtk fills it from retail and the module's sha1 still holds.
//
// The tail at 0xC2A4..0xC448 (plus `.rodata` 0xAC0..0xB44) is claimed separately, by the
// shared "REL" lib's `REL/REL_Setup.cpp` (`_unresolved`, `_epilog`, `_prolog`,
// `ModuleDestructors`, `ModuleConstructors`), which is the free 0 -> 5 every module gets once
// its `splits.txt` claims the range - see `docs/RUNNING_THE_DECOMP.md`. That claim needs
// `RELExit`/`RELMain` named `scope:global`, which is why `symbols.txt` renamed `fn_16_A22C`
// and `fn_16_A250` to those two names and no others, and it is why `_epilog`/`_prolog` resolve
// against this file's definitions rather than against dtk's retail ones. **The module's text
// symbols therefore split 3 ours + 14 accessors + 5 `REL_Setup` + the rest unclaimed**.
//
// The two callees are named by what they are, not invented:
//   - `fn_8022756C` is the DOL's 0x8022756C, 8 bytes, in
//     `build/G2ME01/asm/auto_03_8022756C_text.s`:
//     `stw r3, gLoader_EmperorIngStage1@sda21(r0); blr`. So it stores the *address* of a
//     loader slot, not a loader. `LoadEmperorIngStage1` in the `Matching` unit
//     `src/MetroidPrime/ScriptLoader/EmperorIngStage1.cpp` reads it as
//     `(*gLoader_EmperorIngStage1.value)(mgr, input, info)`, which is why the store below
//     hands it `&lbl_16_bss_0` and why that slot is four bytes wide. That source also records
//     that the 8-byte setter is deliberately not claimed in the DOL, because REL modules
//     import it by its retail name - so it stays in dtk's auto unit and it is the plain DOL
//     symbol, so **no `symbols.txt` rename and no DOL change are needed**. It is `extern "C"`:
//     an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_16_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it,
//     and a second definition under MWCC is what produced mwldeppc's internal linker error
//     on ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//   - `fn_16_A2A0` is the module's own entity loader, 0x31C bytes, still retail and
//     unclaimed. Its signature is fixed by what the DOL's `LoadEmperorIngStage1` thunk calls
//     it with, and only its address is taken here, so its body is not needed to reproduce the
//     three functions above.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" {
// .text 0xA2A0, 0x31C bytes, still retail: the module's own entity loader.
CEntity* fn_16_A2A0(CStateManager&, CInputStream&, CEntityInfo&);
void fn_8022756C(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_16_bss_0;
#else
FScriptLoader lbl_16_bss_0 = 0;
#endif

// .text 0xA270, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_16_A270() {
  lbl_16_bss_0 = fn_16_A2A0;
  fn_8022756C(&lbl_16_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp would call. The MWCC
// branch is the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CIngPuddleRel.cpp` and `CScriptDarkSamusBattleStageRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_16_A2A0` and `fn_8022756C`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `EmperorIngStage1.rel` off the disc through `platform/rel.cpp`, and the `#else` branch
// exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain(void) { fn_16_A270(); }

void RELExit(void) { fn_8022756C(nullptr); }
#else
void mp_relmain_emperoringstage1() { fn_16_A270(); }

void mp_relexit_emperoringstage1() { fn_8022756C(nullptr); }
#endif
}