// CParasiteRel.cpp - Parasite's (module 47) head, .text 0x0..0x148: the seventeen
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp` and
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp`, and the ranges come from
// `config/G2ME01/rels/Parasite/symbols.txt`:
//
//   0x000 fn_47_0    0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x010 fn_47_10   0x08  lbz r3, 0x44f(r3)
//   0x018 fn_47_18   0x08  li r3,0
//   0x020 fn_47_20   0x08  li r3,0
//   0x028 fn_47_28   0x08  li r3,0
//   0x030 fn_47_30   0x08  li r3,0
//   0x038 fn_47_38   0x10  *self = kInvalidUniqueId
//   0x048 fn_47_48   0x0C  lbl_8041B758
//   0x054 fn_47_54   0x08  addi r3,r3,0x754
//   0x05C fn_47_5C   0x08  li r3,1
//   0x064 fn_47_64   0x08  li r3,0
//   0x06C fn_47_6C   0x08  li r3,0
//   0x074 fn_47_74   0x1C  three floats from self+0x54 -> *out
//   0x090 fn_47_90   0x2C  virtual dispatch, vtable slot 0x38
//   0x0BC RELExit    0x24  li r3,0 / bl fn_80200EFC
//   0x0E0 RELMain    0x20  bl fn_47_100
//   0x100 fn_47_100  0x48  lbl_47_bss_50 = {fn_47_1568, fn_47_CF8, fn_47_148}
//                             ; fn_80200EFC(&lbl_47_bss_50)
//
// The file name is not invented: the module's own `build/G2ME01/Parasite/Parasite.preplf`
// string table carries `CParasiteRel.cpp`, next to `REL_Setup.cpp` and the
// `auto_00_00000148_text` unit that starts exactly where this claim ends.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_47_148 (0x148, 0x618), the module's own entity loader, and the other 113 functions above
// it are its methods. `fn_47_148` opens a 0x840 frame whose first act is
// `bl __ct__20SLdrEditorPropertiesFv`, so it is behavioural class code and it needs the
// CActor/CPatterned/CAi hierarchy this tree does not model - the same wall as `Ripper`'s
// `fn_54_178` and `CIngSpaceJumpGuardian`'s `fn_34_170`.
//
// **The accessor block is a sibling of AtomicAlpha's, not a copy of it, and that is measured
// rather than assumed.** It is AtomicAlpha's *minus three* functions - its two leading
// `addi r3,r3,0x8c8` / `0x7d8` member-address accessors and the 0x0C-byte `+0x34c` bit-3 flag
// test - and *plus two* more `li r3,0` predicates: four in the run after the `+0x44f` byte read
// (0x18/0x20/0x28/0x30) where AtomicAlpha runs two (0x28/0x30), against its pair at 0x78 and
// 0x70 after the `+0x754` address. So fourteen accessors, and every body below is one
// `CAtomicAlphaRel.cpp` already reproduces at 100%, with the offsets this disassembly gives.
// No spelling had to be discovered. `fn_47_74` carries over with the caveat
// `CAtomicAlphaRel.cpp` records: retail interleaves its loads and its stores
// (`lfs f0,0x54 / stfs f0,0 / lfs f0,0x58 / stfs f0,4 / lfs f0,0x5c / stfs f0,8`), so it is
// spelled as three subscript stores and must not be "improved" into a `CVector3f` copy -
// `CMetareeSwarmRel.cpp` records that the built-in spelling reverses the loads and the score
// falls.
//
// **fn_47_90 is vtable entry 0x3C of the module's own vtable, not a free function.** dtk puts
// all fourteen accessors and fn_47_90 in the module's FORCEACTIVE list
// (`build/G2ME01/Parasite/ldscript.lcf`) *and* `.data:0x27C` - 0x150 bytes, two leading zero
// words (offset-to-top, then the RTTI pointer, both zero in this REL) and then one word per
// virtual - stores them, so nothing here is a dead-stripping hazard and no `force_active:` entry
// is needed (the trap `CGeomBlobV2` hit). In that table the entry above fn_47_90, at offset
// 0x38, is `HealthInfo__3CAiFv`, which is what the call below dispatches on.
//
// So the call is a member call, and that spelling is measured rather than guessed. Loading the
// vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09%
// on the function (measured in `CIngPuddleRel.cpp`, 2026-09-29, on the same call).
// `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a class's
// virtuals out the way retail's vtable is laid out, so thirteen virtuals put the thirteenth at
// 0x38 - which is the thirteen-virtual stand-in class the family already uses. The slots are
// named by position because no header here models a CPatterned virtual; none of them is
// defined or called from this file, because the only object that carries this vtable is the
// module's own retail bytes. Slot 12's return type is int because `.data:0x27C` names it
// `HealthInfo__3CAiFv`; the call discards it, so this does not affect the bytes, but naming it
// anything else would misdescribe the vtable.
//
// The two callees are named by what they are, not invented:
//   - `fn_80200EFC` is the DOL's 0x80200EFC, two instructions,
//     `stw r3, gLoader_Parasite@sda21(r0); blr` (dtk's listing for that address, the
//     `auto_03_80200EFC_text.s` object under `build/G2ME01/`), immediately after
//     `LoadParasite__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80200ED0, which is
//     0x2C bytes and so ends exactly at 0x80200EFC. So it stores the *address* of a loader
//     record, not a loader. **The import name is the plain `fn_80200EFC`** that
//     `config/G2ME01/symbols.txt:8383` already gives it - not the long MWCC-mangled
//     `SetLoader_...` form `CIngSnatchingSwarmRel.cpp` had to spell out - so no `symbols.txt`
//     rename is needed and the DOL is untouched. It is `extern "C"`: an alias would be a
//     different symbol and the call would resolve to nothing.
//   - `lbl_47_bss_50` is `.bss:0x50`, `size:0xC data:4byte` (dtk's `.bss` listing for the
//     module, the `auto_05_00000000_bss.s` object): the module's own copy of the loader
//     record. This unit's split claims `.text` only, so dtk's `.bss` object has to define it,
//     and a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// **The record is 0xC bytes - three FScriptLoaders - and that is the one thing here a copy of
// `CFishCloudRel.cpp` would have got wrong**: `fn_47_100` is 0x48 bytes against FishCloud's
// 0x30 because this module registers **three** entities. `src/MetroidPrime/ScriptLoader/Parasite.cpp`
// (a `Matching` unit) already models the record as `SParasiteLoaders { FScriptLoader slot0;
// slot1; slot2; }` behind `gLoader_Parasite` and reads all three, and the disassembly agrees -
// `stwu r6` is `fn_47_1568`, `stw r5, 0x4(r3)` is `fn_47_CF8` and `stw r0, 0x8(r3)` is
// `fn_47_148`. The `stwu` is what makes the source spelling below fall out: it stores the first
// field and walks r3 over the record, which is the shape `CFishCloudRel.cpp` and
// `CMetroidRel.cpp` rely on.
//
// **The slot names are `slot0..2`, deliberately.** Which of `fn_47_148` / `fn_47_CF8` /
// `fn_47_1568` is the `parasite` / `brizgee` / `crystallite` loader is not settled by this
// disassembly - retail's order is visible, retail's names are not - so the record is spelled
// locally rather than guessed from the three `Load*` thunks in the DOL. Settling it would need
// a string reference inside the module. The local spelling also avoids editing
// `include/MetroidPrime/ScriptLoaderRel.hpp`, which other units include.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// The record. `SParasiteLoaders` is the model `src/MetroidPrime/ScriptLoader/Parasite.cpp`
// already carries, reproduced locally because that is a `.cpp` and this file needs the type;
// it is not added to `include/MetroidPrime/ScriptLoaderRel.hpp` because the three field names
// this module's bytes establish are not the names retail's loader thunks use.
struct SParasiteLoaders {
  FScriptLoader slot0;
  FScriptLoader slot1;
  FScriptLoader slot2;
};

// `fn_80200EFC` is the plain DOL symbol, `extern "C"` for the reason given above.
extern "C" void fn_80200EFC(SParasiteLoaders*);

// The vtable slot fn_47_90 dispatches to; see the note above on the layout.
class CParasiteDispatch {
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
  virtual int Slot12();
};

extern "C" {
CEntity* fn_47_1568(CStateManager&, CInputStream&, const CEntityInfo&);
CEntity* fn_47_CF8(CStateManager&, CInputStream&, const CEntityInfo&);
CEntity* fn_47_148(CStateManager&, CInputStream&, const CEntityInfo&);

#ifdef __MWERKS__
extern SParasiteLoaders lbl_47_bss_50;
#else
SParasiteLoaders lbl_47_bss_50 = {0, 0, 0};
#endif

// .text 0x100, 0x48 bytes: the module's loader registration. Retail loads the three loaders
// with `lis` in reverse field order and `addi` in field order, then stores the first through
// `stwu` so r3 walks the record and the other two at 0x4(r3) and 0x8(r3) - so the register
// numbers below fall out of the field order rather than being chosen here.
void fn_47_100() {
  lbl_47_bss_50.slot0 = fn_47_1568;
  lbl_47_bss_50.slot1 = fn_47_CF8;
  lbl_47_bss_50.slot2 = fn_47_148;
  fn_80200EFC(&lbl_47_bss_50);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp`, `CIngPuddleRel.cpp` and `CFishCloudRel.cpp`: listing this file in
// `files.cmake` would make the port link `fn_47_148`, `fn_47_CF8`, `fn_47_1568` and
// `fn_80200EFC`, which it cannot, and `tools/link_check.sh --strict` fails on a growing
// undefined count. So the port keeps reading `Parasite.rel` off the disc through
// `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid
// translation unit. `tools/check_files_cmake.py` counts the other heads' files as a known gap.
#ifdef __MWERKS__
void RELMain() { fn_47_100(); }

void RELExit() { fn_80200EFC(nullptr); }
#else
void mp_relmain_parasite() { fn_47_100(); }

void mp_relexit_parasite() { fn_80200EFC(nullptr); }
#endif

// .text 0x90, 0x2C bytes. Vtable entry 0x3C of the module's vtable at `.data:0x27C`; the call
// it makes targets vtable slot 0x38, which that table names `HealthInfo__3CAiFv`. The class
// has no header here, so the object is reached as a `CParasiteDispatch*`.
void fn_47_90(CParasiteDispatch* self) { self->Slot12(); }

// .text 0x74, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this
// builds a 12-byte value out of self[0x54], self[0x58] and self[0x5c]. Retail interleaves the
// loads and the stores, so this is spelled as subscript stores rather than as a vector copy.
void fn_47_74(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x6C, 0x08 bytes. a predicate that is always false.
bool fn_47_6C(void*) { return false; }

// .text 0x64, 0x08 bytes. a predicate that is always false.
bool fn_47_64(void*) { return false; }

// .text 0x5C, 0x08 bytes. a predicate that is always true.
bool fn_47_5C(void*) { return true; }

// .text 0x54, 0x08 bytes. the address of the member at +0x754.
void* fn_47_54(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x48, 0x0c bytes. a constant float out of the DOL.
float fn_47_48(void*) { return lbl_8041B758; }

// .text 0x38, 0x10 bytes. resets the unique id to the invalid value.
void fn_47_38(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x30, 0x08 bytes. a predicate that is always false.
bool fn_47_30(void*) { return false; }

// .text 0x28, 0x08 bytes. a predicate that is always false.
bool fn_47_28(void*) { return false; }

// .text 0x20, 0x08 bytes. a predicate that is always false.
bool fn_47_20(void*) { return false; }

// .text 0x18, 0x08 bytes. a predicate that is always false.
bool fn_47_18(void*) { return false; }

// .text 0x10, 0x08 bytes. the byte at +0x44f.
unsigned char fn_47_10(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}

// .text 0x00, 0x10 bytes. stores the default float at +0x448.
void fn_47_0(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}
}
