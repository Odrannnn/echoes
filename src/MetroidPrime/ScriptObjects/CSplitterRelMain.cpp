// CSplitterRelMain.cpp - Splitter's (module 75) module entry and loader registration,
// .text 0x81F8..0x82B0: three short accessors, RELExit, RELMain and the registration
// RELMain calls. Same arrangement as `MetroidPrime/ScriptObjects/CSandBossRel.cpp` and
// `MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Splitter/symbols.txt`:
//
//   0x81F8 fn_75_81F8 0x08  the address of the member at +0xb7c
//   0x8200 fn_75_8200 0x08  a predicate that is always true
//   0x8208 fn_75_8208 0x08  a predicate that is always false
//   0x8210 RELExit    0x24  li r3,0 / bl fn_80218CF0
//   0x8234 RELMain    0x20  bl fn_75_8254
//   0x8254 fn_75_8254 0x5C  fills lbl_75_bss_20 and calls fn_80218CF0(&lbl_75_bss_20)
//
// **This range is not next to the module's accessor block.** Every other landed REL head puts
// RELExit/RELMain immediately above its accessors - MysteryFlyer's at 0xFC, SandBoss's at 0x104 -
// so one unit claims 0x0..end. Here `fn_75_D0` ends at 0xFC and the next function is `fn_75_FC`,
// the module's 0x370-byte entity loader, so RELMain sits at 0x8234, 0x8138 bytes past the end of the head. One
// object cannot cover both, so this is a second unit: `CSplitterRel.cpp` claims 0x0..0xFC and
// this one claims 0x81F8..0x82B0, with dtk filling `fn_75_FC` and everything between from retail.
//
// The record is **0x14 bytes written into a 0x18-byte `.bss` object**: two loaders and one
// 12-byte CodeWarrior member-function pointer, aligned up by `.bss`'s `align:8`. That is the
// shape `src/MetroidPrime/ScriptLoader/SplitterMainChassis.cpp` (a `Matching` unit) already
// reads - `gLoader_SplitterMainChassis.value->slot0(...)` at `LoadSplitterMainChassis` 0x80218CC4
// and `->slot1(...)` at `LoadSplitterCommandModule` 0x80218C98 - so the first two members are
// named from that reader rather than guessed. The third is only ever written here: the module
// copies the three words of `lbl_75_data_CAC` (`auto_04_00000000_data.s`: 0 / 0xFFFFFFFF /
// fn_75_5A54) out of `.data` rather than building the pointer, so it is assigned from an extern
// of the same type, exactly as `CSnakeWeedSwarmRel.cpp` does. `fn_75_5A54` (0x5A54, 0x2C bytes)
// reads the byte at +0x420 bit 1, the byte at +0xf6a bit 3, sets that bit and stores the incoming
// `f1` at +0xef0 - one `this` and one float, so `void (CEntity::*)(float)` is the signature the
// body has.
//
// The setter is named by what it is, not invented. `fn_80218CF0` is the DOL's 0x80218CF0, two
// instructions: `stw r3, gLoader_SplitterMainChassis@sda21(r0); blr` (see
// `build/G2ME01/asm/auto_03_80218CF0_text.s`). So it stores the *address* of the record, not a
// loader, which is why the store below hands it `&lbl_75_bss_20`. It is **the plain DOL symbol -
// no `symbols.txt` rename and no DOL change are needed** - and `SplitterMainChassis.cpp`'s header
// comment records why it stays in dtk's auto unit: REL modules import it by its retail name. It
// is `extern "C"`: an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_75_bss_20` is `.bss:0x20`, `size:0x18`: the module's own copy of the record. This
//     unit's split claims .text only, so dtk's `.bss` object has to define it, and a second
//     definition under MWCC is what produced mwldeppc's internal linker error on ScriptPlayerProxy.
//     Hence extern under MWCC and a host definition.
//
// Everything from `fn_75_82B0` (0x82B0) up is left unclaimed: behavioural class code needing the
// CActor/CPatterned hierarchy this tree does not model.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "MetroidPrime/ScriptLoader.hpp"

class CEntity;

/** The record `fn_80218CF0` stores. The first two members are the two loaders
 *  `SplitterMainChassis.cpp` calls through `gLoader_SplitterMainChassis`; the third is this
 *  module's own, and nothing in the DOL reads it. */
struct SSplitter_FuncPtrs {
  FScriptLoader slot0;
  FScriptLoader slot1;
  void (CEntity::*method)(float);
};

extern "C" {
CEntity* fn_75_82B0(CStateManager&, CInputStream&, const CEntityInfo&);
// .text 0xFC, unclaimed: the module's other loader, `LoadSplitterCommandModule`.
CEntity* fn_75_FC(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218CF0(SSplitter_FuncPtrs* record);

// The module's own copy of the member-function pointer, in `.data` and not claimed by this unit,
// so it is extern here: dtk's data object defines it.
extern void (CEntity::*lbl_75_data_CAC)(float);

#ifdef __MWERKS__
extern SSplitter_FuncPtrs lbl_75_bss_20;
#else
SSplitter_FuncPtrs lbl_75_bss_20 = {0, 0, 0};
#endif

// .text 0x8254, 0x5C bytes: the module's loader registration. Retail loads the two loaders out
// of `.text` and the third member out of `.data`, and stores them with `stwu` so r3 walks the
// record - which is why the two `lwz` reads sit between the first store and the rest.
void fn_75_8254() {
  lbl_75_bss_20.slot0 = fn_75_82B0;
  lbl_75_bss_20.slot1 = fn_75_FC;
  lbl_75_bss_20.method = lbl_75_data_CAC;
  fn_80218CF0(&lbl_75_bss_20);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in `CSandBossRel.cpp` and
// `CSnakeWeedSwarmRel.cpp`: listing this file in `files.cmake` would make the port link
// `fn_75_82B0`, `fn_75_FC` and `fn_80218CF0`, which it cannot, and `tools/link_check.sh --strict`
// fails on a growing undefined count. So the port keeps reading `Splitter.rel` off the disc
// through `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid
// translation unit for `tools/probe_sources.sh`.
#ifdef __MWERKS__
void RELMain() { fn_75_8254(); }

// `0`, not `nullptr`: mwcceppc 1.3.2 has no `nullptr` in this translation unit, and a null record
// is what retail passes (`li r3,0`). Same spelling as CSnakeWeedSwarmRel.cpp's RELExit.
void RELExit() { fn_80218CF0(0); }
#else
void mp_relmain_splitter() { fn_75_8254(); }

void mp_relexit_splitter() { fn_80218CF0(nullptr); }
#endif

// .text 0x8208, 0x08 bytes. a predicate that is always false.
bool fn_75_8208(void*) { return false; }

// .text 0x8200, 0x08 bytes. a predicate that is always true.
bool fn_75_8200(void*) { return true; }

// .text 0x81F8, 0x08 bytes. the address of the member at +0xb7c.
void* fn_75_81F8(void* self) { return static_cast< char* >(self) + 0xb7c; }
}
