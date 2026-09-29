// CRipperRelMain.cpp - Ripper's (module 54) module entry, .text 0xD8..0x178: the vtable entry
// above it, RELExit, RELMain and the loader registration RELMain calls. Same arrangement as
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` (0xD0, 0xFC, 0x120, 0x140) and
// `MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Ripper/symbols.txt`:
//
//   0x0D8  fn_54_D8   0x2C  lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
//   0x104  RELExit    0x24  li r3,0 / bl SetLoader_Ripper__...
//   0x128  RELMain    0x20  bl fn_54_148
//   0x148  fn_54_148  0x30  lbl_54_bss_0 = fn_54_178 ; SetLoader_Ripper__...(&lbl_54_bss_0)
//
// **It is a second unit, not an extension of the accessor block.** `RipperAccessors.cpp` claims
// 0x0..0xD8, and 0xD8 sits *inside* dtk's `auto_00_000000D8_text` - so claiming it is a sub-range
// carve of one existing auto unit rather than a new head, which is the arrangement
// `CSplitterRelMain.cpp` needed for the same reason in `Splitter`. The carve is the four files it
// is always four files: this source, `config/G2ME01/rels/Ripper/splits.txt`, the `Rel("Ripper", ...)`
// object list in `configure.py`, and `files.cmake` (which this file is *not* added to - see below).
//
// **fn_54_D8 is a vtable entry, not a free function**, and that is what fixes the spelling. dtk puts
// it in the module's FORCEACTIVE list, and `.data:0x48` - CRipper's own vtable, 0x150 bytes, two
// leading words (offset-to-top and the RTTI pointer, both zero in this REL) and then one word per
// virtual - stores it at offset 0x3C, immediately after `HealthInfo__3CAiFv` at 0x38
// (`build/G2ME01/Ripper/asm/auto_04_00000000_data.s`). So the call is a member call on slot 0x38, and
// thirteen virtuals put the last one there, which is the count `CMysteryFlyerRel.cpp` and
// `CIngSnatchingSwarmRel.cpp` both measured rather than guessed: loading the vtable by hand
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, because mwcceppc only reaches for r12
// on its own virtual-dispatch path. The slots are named by position - no header here models a
// CRipper virtual - and none of them is defined or called from this file, because the only object
// carrying this vtable is the module's own retail bytes.
//
// **The import is the long MWCC-mangled name, not the short one.** `SetLoader_Ripper` is the DOL's
// 0x8021BBD0 (`config/G2ME01/symbols.txt:9539`, `size:0x8`), two instructions, `stw r3,
// gLoader_Ripper; blr` - so it stores the *address* of a loader slot, not a loader.
// `src/MetroidPrime/ScriptLoader/Ripper.cpp` reads that slot as `lwz r6, gLoader_Ripper; lwz r12,
// 0(r6); mtctr r12; bctrl`, which is why the store below hands it `&lbl_54_bss_0` and why that slot
// is four bytes wide. But `FScriptLoader*` is a function-pointer typedef, so MWCC mangles the C++
// name into the long form, and this file is `extern "C"` - so MWCC emits the identifier verbatim and
// it has to be written out in full. Measured, not assumed: the name below is in the module's own
// `build/G2ME01/Ripper/Ripper.preplf` import table, and the short spelling compiles, links every
// object and then fails the REL step with `Failed to find symbol SetLoader_Ripper in any module`.
// This is the arrangement `CIngSnatchingSwarmRel.cpp` measured; the modules that import the plain
// `fn_802…` name instead (`CBacteriaSwarmRel.cpp`, `CTryclopsRel.cpp`) need no rename, and the DOL
// is untouched either way because the setter is deliberately not claimed there - REL modules import
// it by its retail name.
//
// `lbl_54_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader pointer.
// This unit's split claims .text only, so dtk's `.bss` object has to define it, and a second
// definition under MWCC is what produced mwldeppc's internal linker error on ScriptPlayerProxy.
// Hence extern under MWCC and a host definition.
//
// Everything from fn_54_178 (0x178, 0x35C) on is left unclaimed: that is Ripper's own entity
// loader, it opens a 0x790 frame and constructs an `SLdrEditorProperties`, and it is behavioural
// CActor/CPatterned class code this tree does not model. It is *called* here by name and defined
// nowhere, so dtk fills it from retail and the module's sha1 against `config/G2ME01/config.yml`
// still holds.
//
// **This file is deliberately absent from `files.cmake`**, for the reason `CMysteryFlyerRel.cpp`
// and `CIngSnatchingSwarmRel.cpp` give: it defines RELMain and RELExit, and a flat host link cannot
// hold fourteen translation units that all define one RELMain. The port keeps reading `Ripper.rel`
// off the disc through `platform/rel.cpp` and never calls into the module, so nothing is lost, and
// the `#else` branch exists only so the file is still a valid translation unit for anyone who
// compiles it standalone. `check_files_cmake.py` counts these under "further units are out because
// they define a module entry point (RELMain/RELExit), which collides in a flat link".
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module sha1 would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

// The vtable slot fn_54_D8 dispatches to; see the note above for the layout.
class CRipperDispatch {
public:
  virtual void Slot0();
  virtual void Slot1();
  virtual void Slot2();
  virtual void Slot3();
  virtual void Slot4();
  virtual void Slot5();
  virtual void Slot6();
  virtual void Slot7();
  virtual void Slot8();
  virtual void Slot9();
  virtual void Slot10();
  virtual void Slot11();
  virtual float Slot12();
};

extern "C" {
// .text 0x178, unclaimed: CRipper's own entity loader.
CEntity* fn_54_178(CStateManager&, CInputStream&, const CEntityInfo&);

// The import is named the way retail's symbol table names it, which is the long MWCC-mangled form -
// see the note at the top of the file.
void SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(
    FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_54_bss_0;
#else
FScriptLoader lbl_54_bss_0 = 0;
#endif

// .text 0x148, 0x30 bytes: the module's loader registration. Retail loads the loader out of `.text`
// and the slot address out of `.bss`, and stores the loader with `stwu` so the store writes the slot
// itself and leaves r3 holding its address for the setter call. Instruction for instruction this is
// `CMysteryFlyerRel.cpp`'s `fn_45_140`.
void fn_54_148() {
  lbl_54_bss_0 = fn_54_178;
  SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(&lbl_54_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
#ifdef __MWERKS__
void RELMain() { fn_54_148(); }

void RELExit() {
  SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(nullptr);
}
#else
void mp_relmain_ripper() { fn_54_148(); }

void mp_relexit_ripper() {
  SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(nullptr);
}
#endif

// .text 0xD8, 0x2C bytes. The call targets vtable slot 0x38.
void fn_54_D8(CRipperDispatch* self) { self->Slot12(); }
}
