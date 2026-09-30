// CSpacePirateRel.cpp - SpacePirate's (module 72) head and entry path, .text 0x0..0x140: the
// eleven accessors, `fn_72_64`'s vtable call, RELExit, RELMain and the loader registration
// RELMain calls. Same arrangement as `MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp` and
// `MetroidPrime/ScriptObjects/CSplinterRel.cpp`, and the ranges come from
// `config/G2ME01/rels/SpacePirate/symbols.txt`:
//
//   0x000 fn_72_0   0x08  li r3,1                     a predicate that is always true
//   0x008 fn_72_8   0x08  addi r3,r3,0x920            the address of the member at +0x920
//   0x010 fn_72_10  0x08  li r3,1                     a predicate that is always true
//   0x018 fn_72_18  0x0C  lhz r0,0xa94(r4) / sth r0,0x0(r3)   a copy of the TUniqueId at +0xa94
//   0x024 fn_72_24  0x0C  lbl_72_rodata_B00           this module's own .rodata:0xB00, .float 50
//   0x030 fn_72_30  0x10  lbl_8041AAB8 -> *((float*)(self + 0x448))
//   0x040 fn_72_40  0x08  li r3,0                     a predicate that is always false
//   0x048 fn_72_48  0x0C  the byte at +0x34c, bit 3
//   0x054 fn_72_54  0x08  addi r3,r3,0x754            the address of the member at +0x754
//   0x05C fn_72_5C  0x08  li r3,1                     a predicate that is always true
//   0x064 fn_72_64  0x2C  virtual dispatch, vtable slot 0x38
//   0x090 RELExit   0x24  li r3,0 / bl fn_80200E3C
//   0x0B4 RELMain   0x20  bl fn_72_D4
//   0x0D4 fn_72_D4  0x6C  lbl_72_bss_24 = {fn_72_140, lbl_72_data_8C0, lbl_72_data_8CC}
//
// **Unlike FlyingPirate, the claim starts at 0x0.** The family claims the whole head, 0x0 up to
// the module's first behavioural class function, and this module has none above the head: the
// first function after the registration is `fn_72_140` (0x140, 0xACC), so everything in the head
// is claimed and nothing of ours is left unclaimed at the bottom. Everything from `fn_72_140` up
// stays retail: it is the module's own entity loader and its class methods, which need the
// CActor/CPatterned hierarchy this tree does not model. The module's 262 text symbols therefore
// split 14 ours + 5 `REL_Setup` + 2 `global_destructor_chain` + 241 unclaimed.
//
// **Nine of the eleven accessors are the family** and are transferred verbatim from units already
// `Matching` here: the `+0x448` float store (`fn_22_4E0`), the bit at +0x34c (`fn_29_4C`), the
// `+0x754` member address (`fn_29_64`), the three predicates, and the vtable dispatch
// (`fn_29_90`). Which body belongs to which offset was read off
// `build/G2ME01/SpacePirate/asm/auto_00_00000000_text.s`, not off the `fn_<id>_<off>` names, which
// say nothing about which function is which. Unlike `CIngRel.cpp` and `CFlyingPirateRel.cpp` this
// head has **no `GetBoundingBox` wrapper, no three-float copy and no `*id = kInvalidUniqueId`
// reset**, which is why it is eleven accessors where Ing's is fourteen and why the claim ends at
// 0x140 rather than 0x130.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%, `tools/unit_fit.sh` still saying "fits" and the link
// still succeeding. Only `tools/flip_test.sh` catches that, and it cannot run on a REL module unit
// in this tree, so it is checked with `tools/check_decl_order.py` instead.

#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

class CEntity;

// `fn_72_18`'s member, `+0xa94`, is a TUniqueId, and that is measured three ways rather than
// assumed. `fn_72_7970` loads it (`lhz r0, 0xa94(r3)`) and stores it through a pointer to call
// `GetObjectById__13CStateManagerCF9TUniqueId`; `fn_72_AF70` hands it the same way, and the same
// function compares it word for word against `kInvalidUniqueId`.
//
// **The getter is written with an explicit out-pointer, and that is the only spelling that can
// be right.** Retail does `lhz r0, 0xa94(r4)` / `sth r0, 0x0(r3)` / `blr` - `self` in r4 and a
// *store* through r3 - so the value comes back through memory. A two-byte struct returned in r3
// would be one instruction (`lhz r3, 0xa94(r3)`), not three. So:
//
//   void fn_72_18(TUniqueId* out, const void* self) {
//     *out = *reinterpret_cast< const TUniqueId* >(static_cast< const char* >(self) + 0xA94);
//   }
//
// which is how the family spells its other struct-returning accessors (`fn_22_4A4`, `fn_29_74`).
// The cast has to be `reinterpret_cast`: MWCC rejects an explicit conversion from `const char*` to
// `const TUniqueId*` with `illegal explicit conversion`.
//
// **The record is 0x1C bytes, not the four most of this family uses, and the `.bss` dump is the
// cheap check that says so**: `build/G2ME01/SpacePirate/asm/auto_05_00000000_bss.s` gives
// `lbl_72_bss_24` at 0x24 `size:0x1C`, where the four-byte family's slot is `size:0x4`. So it is
// an FScriptLoader and two CodeWarrior pointer-to-member-functions, which are 12 bytes each
// (`__ptmf_scall` at `build/G2ME01/asm/Runtime/ptmf.s:0x80345454` reads all three words - the
// `this` adjustment, a vtable offset and the address). The two 12-byte objects are
// `.data:0x8C0` = `0 / 0xFFFFFFFF / fn_72_E0D8` and `.data:0x8CC` =
// `0 / 0xFFFFFFFF / fn_72_E0AC` (`auto_04_00000000_data.s`), non-virtual member-function pointers
// with vtable offset -1 meaning "call it directly" - and the registration below copies them word
// for word rather than assigning a function to them, exactly as
// `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` does. `fn_72_D4` is instruction for instruction that
// `fn_71_70`: same register allocation (r9/r8/r7 then r5/r4/r0), same six `lwz` out of `.data`,
// same `stwu` of the loader, same six stores, same single call.
//
// The two member signatures are read off the two functions, both in this module. `fn_72_E0AC`
// takes no argument, writes `kInvalidUniqueId` to `+0xa84` and clears a bit in `+0xb30`'s byte at
// `+0x118`. `fn_72_E0D8` takes one 16-bit id **through a pointer** (`lhz r0, 0x0(r4)`, and r4 is
// never reassigned before it), does the same when the old id is `kInvalidUniqueId`, and returns
// whether it took it. Neither is called from this file, so only the 12-byte size matters to the
// codegen - but the signature is the honest reading and it costs nothing.
struct SSpacePirate_FuncPtrs {
  FScriptLoader spacePirate;
  bool (CEntity::*setTarget)(const TUniqueId*);
  void (CEntity::*clearTarget)();
};

// The module's own copies of the two member-function pointers, in `.data` and not claimed by this
// unit, so they are extern here: dtk's data object defines them. **MWCC does not encode a
// variable's type in its name**, so `extern bool (CEntity::*lbl_72_data_8C0)(const TUniqueId*);`
// really does reference `lbl_72_data_8C0` itself.
extern bool (CEntity::*lbl_72_data_8C0)(const TUniqueId*);
extern void (CEntity::*lbl_72_data_8CC)();

// The DOL's constant, `lbl_8041AAB8`; the same accessor is `fn_22_4E0` in FlyingPirate.
extern "C" const float lbl_8041AAB8;

// This module's own constant: `.rodata:0xB00` is `.float 50`
// (`build/G2ME01/SpacePirate/asm/auto_03_00000000_rodata.s`). It is `.obj ..., global`, and this
// unit's split claims `.text` only, so it stays defined in dtk's `.rodata` object and the
// reference is an ordinary cross-object relocation, exactly as `CDarkCommandoRel.cpp`'s
// `lbl_3_rodata_0` and `CIngRel.cpp`'s `lbl_29_rodata_64` are.
extern "C" const float lbl_72_rodata_B00;

// **fn_72_64 dispatches to vtable slot 0x38, and that is measured rather than assumed.**
// `.data:0x8D8` (0x148 bytes, 82 words) is CSpacePirate's vtable: it starts
// `[0][0][fn_72_F10C][TypesMatch__12CSpacePirateCFi]` and stores `fn_72_64` at **offset 0x3C**
// (word 15). Counting the two leading words - offset-to-top and the RTTI pointer, both zero in
// this REL - as not being virtuals, word n is virtual n-1, so `fn_72_64` is the **fourteenth**
// virtual and the slot it dispatches to, 0x38 = word 13, is the **twelfth**.
//
// **Correcting an off-by-one in the note I started from**: it said 0x38 was the thirteenth virtual
// and that the slot held `HealthInfo__6CActorFv`. It does not. This module's own table puts
// `fn_72_3A0C` at 0x38 (word 13) and `HealthInfo__3CAiFv` at 0x40 (word 14), so 0x38 is the
// module's own override and `HealthInfo` is the thirteenth. The neighbourhood is still CActor's
// `HealthInfo` / `GetHealthInfo` pair, which is what `CSnakeWeedSwarmRel.cpp` records for module
// 71 - with one extra CPatterned virtual below it, which is why `HealthInfo` sits at 0x40 here
// and at 0x38 there. Nothing about the code changes: only the *offset* of the callee slot is
// load-bearing, and thirteen virtuals on the stand-in class put `Slot12` at exactly 0x38.
//
// It has to be a member call: a by-hand vtable load compiles to `lwz r3,0(r3)` where retail has
// `lwz r12,0(r3)`. The slots are named by position because no header here models a CActor
// virtual, and none of them is defined or called from this file, because the only object that
// carries this vtable is the module's own retail bytes.
class CSpacePirateDispatch {
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
  virtual void Slot12();
};

extern "C" {
// fn_72_140, the module's own entity loader, 0x140, 0xACC: left retail, named here only so the
// registration below can store its address.
CEntity* fn_72_140(CStateManager&, CInputStream&, const CEntityInfo&);

// **The import is the plain DOL symbol `fn_80200E3C`, and it has to stay that name.** It is the
// DOL's 0x80200E3C - `stw r3,-0x6a28(r13); blr` (`tools/dis.sh 0x80200E3C 0x8`), and
// `LoadSpacePirate` at 0x80200E10 reads the same displacement
// (`lwz r6,-0x6a28(r13); lwz r12,0(r6); mtctr r12; bctrl`) - so it stores the **address** of a
// loader record, which is why the registration hands it `&lbl_72_bss_24`.
// `src/MetroidPrime/ScriptLoader/SpacePirate.cpp` (a `Matching` unit) already records that this
// setter is deliberately not claimed in the DOL because REL modules import it by its retail name,
// so it stays in dtk's auto unit. Declared `extern "C"` under its retail name, as
// `CFlyingPirateRel.cpp` does for `fn_80218A04`: an alias would be a different symbol and the
// call would resolve to nothing. (`CSnakeWeedSwarmRel.cpp` can use a C++ name for the same role
// only because `config/G2ME01/symbols.txt:9532` renames the DOL's 0x8021BB08 to
// `SetLoader_SnakeWeedSwarm__FP24SSnakeWeedSwarm_FuncPtrs`; nothing renames this one.)
void fn_80200E3C(SSpacePirate_FuncPtrs* loader);

// `lbl_72_bss_24` is `.bss:0x24`, `size:0x1C`: the module's own copy of the record, and **not**
// `.bss:0x0`, which is a different 0xC-byte object (`lbl_72_bss_0`) read and written far above
// the head. This unit's split claims `.text` only, so dtk's `.bss` object has to define it, and
// a second definition under MWCC is what produced mwldeppc's internal linker error on
// ScriptPlayerProxy. Hence extern under MWCC and a host definition.
#ifdef __MWERKS__
extern SSpacePirate_FuncPtrs lbl_72_bss_24;
#else
SSpacePirate_FuncPtrs lbl_72_bss_24 = {0, 0, 0};
#endif

// .text 0xD4, 0x6C bytes: the module's loader registration. Retail loads all six words out of
// `.data` into r9/r8/r7 and r5/r4/r0, stores the loader through `stwu` so r3 walks the record,
// stores the six words back, and only then calls the setter - so the two member-function pointers
// are copied out of `.data` rather than being built here.
void fn_72_D4() {
  lbl_72_bss_24.spacePirate = fn_72_140;
  lbl_72_bss_24.setTarget = lbl_72_data_8C0;
  lbl_72_bss_24.clearTarget = lbl_72_data_8CC;
  fn_80200E3C(&lbl_72_bss_24);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CSnakeWeedSwarmRel.cpp` and `CSplinterRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_72_140` and `fn_80200E3C`, which it cannot, and `tools/link_check.sh --strict`
// fails on a growing undefined count. So the port keeps reading `SpacePirate.rel` off the disc
// through `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid
// translation unit for `tools/probe_sources.sh`.
#ifdef __MWERKS__
void RELMain() { fn_72_D4(); }

void RELExit() { fn_80200E3C(nullptr); }
#else
void mp_relmain_spacepirate() { fn_72_D4(); }

void mp_relexit_spacepirate() { fn_80200E3C(nullptr); }
#endif

// .text 0x64, 0x2C bytes. a member call into vtable slot 0x38.
void fn_72_64(CSpacePirateDispatch* self) { self->Slot12(); }

// .text 0x5C, 0x08 bytes. a predicate that is always true.
bool fn_72_5C(void*) { return true; }

// .text 0x54, 0x08 bytes. the address of the member at +0x754.
void* fn_72_54(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x48, 0x0C bytes. the flag at +0x34c, bit 3 - `extrwi r3,r0,1,28`, the `bool : 1` of its
// byte that `CIngRel.cpp`'s `fn_29_4C` and nine other units already reproduce byte for byte.
bool fn_72_48(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x40, 0x08 bytes. a predicate that is always false.
bool fn_72_40(void*) { return false; }

// .text 0x30, 0x10 bytes. stores the default float at +0x448.
void fn_72_30(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = lbl_8041AAB8;
}

// .text 0x24, 0x0C bytes. this module's own `lbl_72_rodata_B00`, `.float 50`; the family spelling
// is `CDarkCommandoRel.cpp`'s `fn_3_8` and `CIngRel.cpp`'s `fn_29_18`.
float fn_72_24(void*) { return lbl_72_rodata_B00; }

// .text 0x18, 0x0C bytes. the TUniqueId at +0xa94, out through a hidden return pointer; see the
// note at the top of the file.
void fn_72_18(TUniqueId* out, const void* self) {
  *out = *reinterpret_cast< const TUniqueId* >(static_cast< const char* >(self) + 0xA94);
}

// .text 0x10, 0x08 bytes. a predicate that is always true.
bool fn_72_10(void*) { return true; }

// .text 0x08, 0x08 bytes. the address of the member at +0x920 - a fourth offset for an accessor
// the family already spells three ways (`fn_29_0` at +0x9dc, `fn_22_470` at +0x970, `fn_45_8` at
// +0x818 and `fn_27_0` at +0x7c0), with nothing to work out.
void* fn_72_8(void* self) { return static_cast< char* >(self) + 0x920; }

// .text 0x00, 0x08 bytes. a predicate that is always true.
bool fn_72_0(void*) { return true; }
}
