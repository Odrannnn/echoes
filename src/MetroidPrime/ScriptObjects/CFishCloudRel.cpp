// CFishCloudRel.cpp - FishCloud's (module 20) head, .text 0x0..0xAC: the four functions above the
// module's class code. Same arrangement as `MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp`
// and `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/FishCloud/symbols.txt`:
//
//   0x00  fn_20_0   0x2C   the CActor `GetHealthInfo` slot, calling vtable slot 0x38
//   0x2C  RELExit   0x24   li r3,0 / bl SetLoader_FishCloud
//   0x50  RELMain   0x20   bl fn_20_70
//   0x70  fn_20_70  0x3C   lbl_20_bss_0 = {fn_20_340, fn_20_AC}
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds (measured: 79ae4b2e..., unchanged). The first
// neighbour left retail is fn_20_AC (0xAC, 0x240), the module's own `LoadFishCloudModifier`
// entity loader, and the other half of the pair is fn_20_340 (0x340, 0x838), `LoadFishCloud`.
// Both are behavioural class code and both need the CActor/CPatterned hierarchy this tree does
// not model; neither is simple, and the measured call counts are in the "what is left" paragraph
// of "`CFishCloudRel` is a module head, and the header already had the record" in
// `docs/RUNNING_THE_DECOMP.md`.
//
// **The record is 8 bytes: two FScriptLoaders**, and that is the only difference from
// `CSnakeWeedSwarmRel.cpp`'s 0x1C-byte one. It needs no locally spelled struct, unlike
// SnakeWeedSwarm's, because `include/MetroidPrime/ScriptLoaderRel.hpp` already has it as
// `SFishCloud_FuncPtrs` {FScriptLoader fishCloud; FScriptLoader fishCloudModifier;}, and the field
// names are retail's own: the DOL's `LoadFishCloud` (0x8021BB3C) reads `value->fishCloud` and
// `LoadFishCloudModifier` (0x8021BB10) reads `value->fishCloudModifier`, which is what fixes the
// order below. The struct itself is not claimed by this unit's .text-only split.
//
// The two callees are named by what they are, not invented:
//   - `SetLoader_FishCloud` is the DOL's 0x8021BB68, two instructions, `stw r3,
//     gLoader_FishCloud; blr`. So it stores the *address* of the record, not a loader. Its body
//     already lives in `src/MetroidPrime/ScriptLoaderRel.cpp` at 100.00%, so it is declared here
//     in C++ and `mwcceppc` mangles it to `SetLoader_FishCloud__FP19SFishCloud_FuncPtrs`, the
//     name `config/G2ME01/symbols.txt` gives the DOL. Do not declare an `fn_80xxxxxx` alias for
//     it: an alias is a different symbol and the call would resolve to nothing. (The same warning
//     as in `CAtomicAlphaRel.cpp`; the header's `SetSFishCloud_FuncPtrs` is a third name for the
//     same idea and is likewise not what the call site mangles to.)
//   - `lbl_20_bss_0` is `.bss:0x0`, `size:0x8` (build/G2ME01/FishCloud/asm/
//     auto_05_00000000_bss.s): the module's own copy of the record. This unit's split claims .text
//     only, so dtk's `.bss` object has to define it, and a second definition under MWCC is what
//     produced mwldeppc's internal linker error on ScriptPlayerProxy. Hence extern under MWCC and
//     a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "REL/REL_Setup.h"

class CEntity;
class CHealthInfo;

// `SetLoader_FishCloud` is a C++ function, not a C one, and that is the whole point of the
// declaration: `mwcceppc` mangles it to `SetLoader_FishCloud__FP19SFishCloud_FuncPtrs`, the name
// `config/G2ME01/symbols.txt` gives the DOL. Its body already lives in
// `src/MetroidPrime/ScriptLoaderRel.cpp` at 100.00%, so this is a declaration only.
// (The header's own `SetSFishCloud_FuncPtrs` is a third name for the same idea and is not what
// the call site mangles to; do not use it, and do not invent an `fn_80xxxxxx` alias either - an
// alias is a different symbol and the call would resolve to nothing. The same warning as in
// `CAtomicAlphaRel.cpp`.)
void SetLoader_FishCloud(SFishCloud_FuncPtrs* loader);

// **fn_20_0 is a vtable entry, not a free function, and its bytes are identical to
// `CSnakeWeedSwarmRel.cpp`'s `fn_71_0`** - both `.text:0x0`, `size:0x2C`, eleven instructions, word
// for word. That is CActor's `GetHealthInfo`, returning the result of the `HealthInfo` slot above
// it. It is stored in *two* of this module's vtables, so nothing here is a dead-stripping hazard:
// `build/G2ME01/FishCloud/asm/auto_04_00000000_data.s` shows `lbl_20_data_8` (.data:0x8, 0x7C
// bytes) and `lbl_20_data_84` (.data:0x84, 0x7C bytes) each holding 31 words - two leading zero
// words (offset-to-top, then the RTTI pointer, both zero in this REL) and then one word per
// virtual, so 29 virtuals. In `lbl_20_data_8` word 14 - vtable offset 0x38, the 13th virtual, the
// one the call below dispatches on - is `HealthInfo__6CActorFv` and word 15, offset 0x3C, is
// `fn_20_0`; the table is CActor's with that one slot replaced. `lbl_20_data_84` has the same pair
// at the same two offsets.
//
// So the call is a member call, and that spelling is measured rather than guessed: loading the
// vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09%
// on the function (measured 2026-09-29 on CIngPuddleRel.cpp, the same call, where these bytes are
// `fn_32_8`). `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a
// class's virtuals out the way retail's vtable is laid out, so 29 virtuals put the 29th at the
// last word of the 0x7C-byte table. The slots are named by position because no header here models
// a CActor virtual; none of them is defined or called from this file, because the only objects
// that carry this vtable are the module's own retail bytes.
class CFishCloudVTable {
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
  virtual CHealthInfo* Slot12();
  virtual void Slot13();
  virtual void Slot14();
  virtual void Slot15();
  virtual void Slot16();
  virtual void Slot17();
  virtual void Slot18();
  virtual void Slot19();
  virtual void Slot20();
  virtual void Slot21();
  virtual void Slot22();
  virtual void Slot23();
  virtual void Slot24();
  virtual void Slot25();
  virtual void Slot26();
  virtual void Slot27();
  virtual void Slot28();
};

extern "C" {
CEntity* fn_20_340(CStateManager&, CInputStream&, const CEntityInfo&);
CEntity* fn_20_AC(CStateManager&, CInputStream&, const CEntityInfo&);

#ifdef __MWERKS__
extern SFishCloud_FuncPtrs lbl_20_bss_0;
#else
SFishCloud_FuncPtrs lbl_20_bss_0 = {0, 0};
#endif

// .text 0x70, 0x3C bytes: the module's loader registration. Retail loads `fn_20_340` into r5 and
// `fn_20_AC` into r4, stores the first through `stwu` so r3 walks the record, and stores the
// second at `0x4(r3)` - so the register numbers fall out of the field order above rather than
// being chosen here.
void fn_20_70() {
  lbl_20_bss_0.fishCloud = fn_20_340;
  lbl_20_bss_0.fishCloudModifier = fn_20_AC;
  SetLoader_FishCloud(&lbl_20_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch is the
// retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp`, `CIngPuddleRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_20_340` and `SetLoader_FishCloud`, which it cannot,
// and `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps
// reading `FishCloud.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists
// only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_20_70(); }

void RELExit() { SetLoader_FishCloud(nullptr); }
#else
void mp_relmain_fishcloud() { fn_20_70(); }

void mp_relexit_fishcloud() { SetLoader_FishCloud(nullptr); }
#endif

// .text 0x0, 0x2C bytes. Vtable entry 0x3C of both of the module's vtables: CActor's
// `GetHealthInfo` slot, returning the result of the `HealthInfo` slot above it at 0x38. Retail's
// CActor spelling of the same function is `return const_cast<CActor*>(this)->HealthInfo();`; the
// const_cast is invisible in the bytes - r3 is already the address that gets passed on - so the
// record's pointer is passed straight through. These eleven instructions are `fn_71_0`'s byte for
// byte, so this line is the same one `CSnakeWeedSwarmRel.cpp` already measures.
CHealthInfo* fn_20_0(CFishCloudVTable* self) { return self->Slot12(); }
}
