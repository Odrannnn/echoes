// CFlyingPirateRel.cpp - FlyingPirate's (module 22) accessor block and entry path,
// .text 0x470..0x5D4: the thirteen accessors, RELExit, RELMain and the loader registration.
// Same arrangement as `MetroidPrime/ScriptObjects/CGrenchlerRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/FlyingPirate/symbols.txt`:
//
//   0x470 fn_22_470  0x08  addi r3,r3,0x970
//   0x478 fn_22_478  0x08  li r3,1
//   0x480 fn_22_480  0x24  byte at +0xbb8, bit 2 -> .rodata:0x3cc (.float 5) / :0x3c8 (.float 50)
//   0x4A4 fn_22_4A4  0x3C  GetBoundingBox into a local, then fn_22_AA48(out, &box)
//   0x4E0 fn_22_4E0  0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x4F0 fn_22_4F0  0x08  lbz r3, 0x44f(r3)
//   0x4F8 fn_22_4F8  0x08  li r3,0
//   0x500 fn_22_500  0x08  li r3,0
//   0x508 fn_22_508  0x10  *id = kInvalidUniqueId
//   0x518 fn_22_518  0x0C  byte at +0x34c, bit 3
//   0x524 fn_22_524  0x08  addi r3,r3,0x754
//   0x52C fn_22_52C  0x08  li r3,1
//   0x534 fn_22_534  0x2C  virtual dispatch, vtable slot 0x38
//   0x560 RELExit    0x24  li r3,0 / bl fn_80218A04
//   0x584 RELMain    0x20  bl fn_22_5A4
//   0x5A4 fn_22_5A4  0x30  lbl_22_bss_58 = fn_22_5D4 ; fn_80218A04(&lbl_22_bss_58)
//
// **Why this range and not 0x0.** The family claims the whole head, 0x0 up to the module's first
// class function, and this module's first four functions are the exception: `fn_22_0` (0x164),
// `fn_22_164` (0x1E4), `fn_22_348` (0x94) and `fn_22_3DC` (0x94) are 0x470 bytes of behavioural
// class code, not accessors, so the claim starts above them at 0x470. Everything below 0x470 and
// everything above 0x5D4 is left unclaimed, so dtk fills it from retail and the module's sha1
// against `config/G2ME01/config.yml` still holds. `fn_22_5D4` (0x5D4, 0x90C) is the module's own
// entity loader and the ~145 functions above it are its class methods; they stay retail because
// they need the CActor/CPatterned hierarchy this tree does not model.
//
// **Eleven of the thirteen accessors are the family, and the two that are not are the two this
// module is worth a lane for.** Which body belongs to which offset was read off
// `build/G2ME01/FlyingPirate/asm/auto_00_00000000_text.s`, not off the `fn_<id>_<off>` names, which
// say nothing about which function is which. Every other one is a body
// `CGrenchlerRel.cpp` or `CMysteryFlyerRel.cpp` already reproduces at 100% as a `Matching` unit -
// `+0x448` float store, the byte at +0x44f, the `kInvalidUniqueId` reset, the bit at +0x34c, the
// member address at +0x754, the predicates, the vtable dispatch - so no spelling had to be found
// for them. The two that are not the family's:
//   - `fn_22_470` opens at `addi r3,r3,0x970` where `fn_45_8` opens at `+0x818` and `fn_27_0` at
//     `+0x7c0`: the same "address of a member" accessor at a third offset.
//   - `fn_22_480` has no counterpart in the family. It returns `lbl_22_rodata_3CC` (`.float 5`)
//     or `lbl_22_rodata_3C8` (`.float 50`) according to **bit 5** of the byte at +0xbb8 - the
//     family's own bit accessor reads bit 3 of a different byte - and it is the only function
//     here that returns a float by branch. The spelling is the obvious one (the bit test written
//     as `(self[0xBB8] & 32) != 0`, the same shape as the +0x34c accessor below, and a `?:` over
//     two `extern "C" const float` module constants, the shape `CDarkCommandoRel.cpp` uses for
//     its own `lbl_3_rodata_0`). The mask is measured, not guessed: dtk renders the instruction
//     as `extrwi. r0, r0, 1, 26`, which reads as bit 2, and `powerpc-eabi-objdump` on the retail
//     `.rel` reads the same word as `rlwinm. r0, r0, 27, 31, 31` - a rotate left by 32-5 masked to
//     one bit, i.e. bit 5. `& 4` compiles to `rlwinm. r0, r0, 0, 29, 29` (a test of bit 29 of a
//     zero-extended byte, which is always zero) and puts 15 of 16 functions at 100.00% with the
//     module's sha1 broken on exactly two bytes.
//
// `fn_22_4A4` is instruction for instruction `CGrenchlerRel.cpp`'s `fn_27_8`
// (`build/G2ME01/Grenchler/asm/MetroidPrime/ScriptObjects/CGrenchlerRel.s`, 0x8, 0x3C) -
// `stwu r1,-0x30` / `mflr` / save LR and r31 / `mr r31,r3` / `addi r3,r1,8` / GetBoundingBox /
// `mr r3,r31` / `addi r4,r1,8` / `bl <the module's out-of-line ctor>` / restore - so it is written
// the way that file writes it: a hidden return pointer in r3, `self` in r4, and the converting
// constructor called by its dtk name because it is 0xAA48 bytes into the module and stays
// unclaimed. `const CAABox&` on the parameter is load-bearing: by value the frame grows to 0x40.
//
// `fn_22_534` is a vtable entry, not a free function: it dispatches to vtable slot 0x38, and the
// stand-in class carries thirteen virtuals so that the last one lands there. Loading the vtable
// by hand compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured 99.09% on
// `CIngPuddleRel.cpp`), so this is a member call. The slots are named by position because no
// header here models a CActor virtual, and none of them is defined or called from this file,
// because the only object that carries this vtable is the module's own retail bytes.
//
// **No dead-strip hazard, and that is measured, not assumed.**
// `build/G2ME01/FlyingPirate/ldscript.lcf` lists all thirteen accessors (`fn_22_470` ..
// `fn_22_534`) in its FORCEACTIVE block, and `RELMain`/`RELExit` are this module's entry points,
// referenced by `_epilog`/`_prolog`, which the shared `REL/REL_Setup.cpp` unit defines. So all
// sixteen survive for the reasons they do everywhere in this family, and nothing here needs a
// `force_active:` entry in `config/G2ME01/config.yml`. The two unclaimed callees are held by the
// same argument the rest of the family uses: `fn_22_5D4` is referenced by `fn_22_5A4` (which
// `RELMain` calls) and `fn_22_AA48` by `fn_22_4A4` (which is itself force-active).
//
// The two callees are named by what they are, not invented:
//   - `fn_80218A04` is the DOL's 0x80218A04, two instructions, `stw r3, gLoader_FlyingPirate; blr`
//     (`build/G2ME01/asm/` disassembly via tools/dis.sh), immediately after
//     `LoadGrenchler__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218A0C, which is
//     0x2C bytes and so ends exactly at 0x80218A38 - the next function. So it stores the
//     *address* of a loader slot, not a loader, which is why the store below hands it
//     `&lbl_22_bss_58` and why that slot is four bytes wide. `src/MetroidPrime/ScriptLoader/
//     FlyingPirate.cpp` (a `Matching` unit) records that the setter is deliberately not claimed in
//     the DOL, because REL modules import it by its retail name - so it stays in dtk's auto unit
//     and this declaration is the same one `CGrenchlerRel.cpp` makes for `fn_80218A38`. It is
//     `extern "C"`: an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_22_bss_58` is `.bss:0x58`, `size:0x4 data:4byte` (`.obj lbl_22_bss_58, global` in
//     `build/G2ME01/FlyingPirate/asm/auto_05_00000000_bss.s`): the module's own copy of the loader
//     pointer. It is **`lbl_22_bss_58` and not `lbl_22_bss_0`** - this module's `.bss:0x0` is a
//     different object, read and written far above the head - which is why the name below is the
//     one it is. This unit's split claims `.text` only, so dtk's `.bss` object has to define it,
//     and a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only `tools/flip_test.sh` catching it. Check
// first with `python3 tools/check_decl_order.py --unit CFlyingPirateRel`.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;

// This module's own constants: `.rodata:0x3CC` is `.float 5` and `.rodata:0x3C8` is `.float 50`
// (build/G2ME01/FlyingPirate/asm/auto_03_00000000_rodata.s). Both are `.obj ..., global`, and
// this unit's split claims `.text` only, so they stay defined in dtk's `.rodata` object and the
// references are ordinary cross-object relocations, exactly as `CDarkCommandoRel.cpp`'s
// `lbl_3_rodata_0` is.
extern "C" const float lbl_22_rodata_3C8;
extern "C" const float lbl_22_rodata_3CC;

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100% (`CMysteryFlyerRel.cpp` measured it). Only the mangled name has to
// agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_22_534 dispatches to; see the note on the layout above.
class CFlyingPirateDispatch {
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
// fn_22_5D4, the module's own entity loader, 0x5D4, 0x90C: left retail, named here only so the
// registration below can store its address.
CEntity* fn_22_5D4(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218A04(FScriptLoader* loader);
// .text 0xAA48, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_22_AA48(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_22_bss_58;
#else
FScriptLoader lbl_22_bss_58 = 0;
#endif

// .text 0x5A4, 0x30 bytes. lbl_22_bss_58 = fn_22_5D4 ; fn_80218A04(&lbl_22_bss_58). Retail loads the
// loader out of `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so
// the store writes the slot itself and leaves r3 holding its address for the setter call.
void fn_22_5A4() {
  lbl_22_bss_58 = fn_22_5D4;
  fn_80218A04(&lbl_22_bss_58);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp` and `CGrenchlerRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_22_5D4` and `fn_80218A04`, which it cannot, and `tools/link_check.sh --strict`
// fails on a growing undefined count. So the port keeps reading `FlyingPirate.rel` off the disc
// through `platform/rel.cpp`, and the `#else` branch exists only so the file is still a valid
// translation unit.
#ifdef __MWERKS__
void RELMain() { fn_22_5A4(); }

void RELExit() { fn_80218A04(nullptr); }
#else
void mp_relmain_flyingpirate() { fn_22_5A4(); }

void mp_relexit_flyingpirate() { fn_80218A04(nullptr); }
#endif

// .text 0x534, 0x2C bytes. a member call into vtable slot 0x38.
void fn_22_534(CFlyingPirateDispatch* self) { self->Slot12(); }

// .text 0x52C, 0x08 bytes. a predicate that is always true.
bool fn_22_52C(void*) { return true; }

// .text 0x524, 0x08 bytes. the address of the member at +0x754.
void* fn_22_524(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x518, 0x0C bytes. the flag at +0x34c, bit 3 - `extrwi r3,r0,1,28`, the `bool : 1` of its
// byte that `CGrenchlerRel.cpp`'s `fn_27_74` and nine other units already reproduce byte for byte.
bool fn_22_518(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}

// .text 0x508, 0x10 bytes. resets the unique id to the invalid value.
void fn_22_508(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x500, 0x08 bytes. a predicate that is always false.
bool fn_22_500(void*) { return false; }

// .text 0x4F8, 0x08 bytes. a predicate that is always false.
bool fn_22_4F8(void*) { return false; }

// .text 0x4F0, 0x08 bytes. the byte at +0x44f.
unsigned char fn_22_4F0(const void* self) {
  return static_cast< const unsigned char* >(self)[0x44F];
}

// .text 0x4E0, 0x10 bytes. stores the default float at +0x448.
void fn_22_4E0(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x4A4, 0x3C bytes. Returns `optional_object<CAABox>(self->GetBoundingBox())` through the
// hidden pointer `out`; see the note at the top of the file.
void fn_22_4A4(void* out, const CPhysicsActor* self) {
  fn_22_AA48(out, self->GetBoundingBox());
}

// .text 0x480, 0x24 bytes. the one accessor in this head that is not the family's: **bit 5** of
// the byte at +0xbb8 chooses between this module's two float constants. The spelling is a shift,
// not a mask, and that is measured: `rlwinm. r0,r0,27,31,31` at 0x484 is `SH = 32 - 5` with
// `MB = ME = 31`, which is what mwcceppc emits for `(byte >> 5) & 1`
// (`CMetareeSwarmRel.cpp`'s `fn_43_0` measured that shape on a different module, and
// `CScriptMetaree.cpp`'s `fn_42_36C` is the `Matching` unit that proves the rule). dtk's asm
// spells the same word `extrwi. r0, r0, 1, 26`, which reads as bit 2 and is wrong; reading the
// mnemonic rather than the encoding costs two bytes of the module's sha1 and holds the unit at
// 15/16. Both `& 4` and `& 32` compile to `rlwinm. r0, r0, 0, 26, 26` / `..., 0, 29, 29` - a test
// of a bit above the byte - which also drops the value the function is supposed to test.
float fn_22_480(const void* self) {
  return (((static_cast< const unsigned char* >(self)[0xBB8] >> 5) & 1) != 0) ? lbl_22_rodata_3CC
                                                                           : lbl_22_rodata_3C8;
}

// .text 0x478, 0x08 bytes. a predicate that is always true.
bool fn_22_478(void*) { return true; }

// .text 0x470, 0x08 bytes. the address of the member at +0x970.
void* fn_22_470(void* self) { return static_cast< char* >(self) + 0x970; }
}
