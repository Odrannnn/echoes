// CBloggRel.cpp - Blogg's (module 7) head, .text 0x94..0x108: the three functions that register
// the module's loader. The ranges come from `config/G2ME01/rels/Blogg/symbols.txt`:
//
//   0x94  RELExit   0x24   li r3,0 / bl fn_80218B08
//   0xB8  RELMain   0x20   bl fn_7_D8
//   0xD8  fn_7_D8   0x30   lbl_7_bss_10 = fn_7_108 ; fn_80218B08(&lbl_7_bss_10)
//
// Same arrangement as `MetroidPrime/ScriptObjects/CTryclopsRel.cpp` and
// `MetroidPrime/ScriptObjects/CPillBugRel.cpp`, and `fn_7_D8` is `CTryclopsRel.cpp`'s `fn_81_148`
// instruction for instruction, down to the `stwu` that walks r3 onto the loader slot. Only the
// names and the slot's address differ, so no spelling had to be discovered.
//
// **The claim starts at 0x94, not at 0x0.** Many landed heads open with the REL loader
// generator's thirteen-accessor block, which is why the recipe's first step is "copy
// `CAtomicAlphaRel.cpp`"; Blogg does not. The only function in front of `RELExit` is `fn_7_0`
// (0x0, 0x94) and it is a **destructor**. It installs `lbl_7_data_540` as `this`'s vtable, calls
// `__dt__20CDamageVulnerabilityFv` on `this + 0x34` and on `this + 4`, installs `lbl_7_data_5A0`,
// and then takes the `Free__7CMemoryFPCv` tail behind its `extsh. r0, r31` / `ble` pair. That is
// the `CDamageVulnerability` hierarchy, i.e. the behavioural class code the item's brief leaves
// retail, so 0x0..0x94 stays unclaimed and dtk fills it. Read the first function of a module's
// `.text` before planning its head: it decides 16 functions or 3.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// `fn_7_108` (0x108, 0x96C), the module's own entity loader, and the 207 functions from there to
// `fn_7_BEFC` are Blogg's methods: behavioural class code needing the CActor/CPatterned
// hierarchy this tree does not model.
//
// The two callees are named by what they are, not invented:
//   - `fn_80218B08` is the DOL's 0x80218B08, two instructions,
//     `stw r3, gLoader_Blogg@sda21(r0); blr` in
//     `build/G2ME01/asm/auto_03_80218B08_text.s`, and a plain named DOL symbol in
//     `config/G2ME01/symbols.txt`. So it stores the *address* of a loader slot, not a loader:
//     the store below hands it `&lbl_7_bss_10` and that slot is four bytes wide. Like
//     Tryclops' it needs no mangled `SetLoader_*` name, no `symbols.txt` edit and no DOL change;
//     `strings build/G2ME01/Blogg/Blogg.preplf | grep 80218B` gives `fn_80218B08` directly.
//   - `lbl_7_bss_10` is `.bss:0x10`, `size:0x4 data:4byte` in
//     `build/G2ME01/Blogg/asm/auto_05_00000000_bss.s`. This unit's split claims .text only, so
//     dtk's `.bss` object has to define it, and a second definition under MWCC is what produced
//     mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC and a host
//     definition. (Tryclops' is `.bss:0x30`; neither is `+0x0`, so read the offset out of the
//     module's own `.bss` asm.)
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%. Only `flip_test.sh` catches that.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" {
// .text 0x108, 0x96C bytes: the module's own entity loader, left retail. Its signature is fixed
// by what the DOL's LoadBlogg thunk calls it with - (CStateManager&, CInputStream&,
// const CEntityInfo&) - and only its address is taken here, so the body is not needed to
// reproduce these three functions.
CEntity* fn_7_108(CStateManager&, CInputStream&, CEntityInfo&);

// The DOL's loader setter, 0x80218B08; see the note at the top.
void fn_80218B08(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_7_bss_10;
#else
FScriptLoader lbl_7_bss_10 = 0;
#endif

// .text 0xD8, 0x30 bytes. Publishes the module's loader and hands the slot to the DOL.
void fn_7_D8() {
  lbl_7_bss_10 = fn_7_108;
  fn_80218B08(&lbl_7_bss_10);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch is
// the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CTryclopsRel.cpp` and
// `CPillBugRel.cpp`: listing this file in `files.cmake` would make the port link `fn_7_108` and
// `fn_80218B08`, which it cannot, and `tools/link_check.sh --strict` fails on a growing undefined
// count. So the port keeps reading `Blogg.rel` off the disc through `platform/rel.cpp`, and the
// `#else` branch exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_7_D8(); }

void RELExit() { fn_80218B08(nullptr); }
#else
void mp_relmain_blogg() { fn_7_D8(); }

void mp_relexit_blogg() { fn_80218B08(nullptr); }
#endif
}
