// CChozoGhostRel.cpp - ChozoGhost's (module 8) head, .text 0x350..0x488: the eleven accessors
// above the module's class code, plus RELExit, RELMain and the loader registration RELMain
// calls. Same arrangement as `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp` and
// `MetroidPrime/ScriptObjects/CSwampBossStage2Rel.cpp`, and the ranges come from
// `config/G2ME01/rels/ChozoGhost/symbols.txt`:
//
//   0x350 fn_8_350 0x0C  lbl_8_rodata_0, **this module's own** .rodata:0x0
//   0x35C fn_8_35C 0x3C  GetBoundingBox into a local, then fn_8_55E4(out, &box)
//   0x398 fn_8_398 0x10  skDamageHitTime__10CPatterned -> *((float*)(self + 0x448))
//   0x3A8 fn_8_3A8 0x08  li r3,0
//   0x3B0 fn_8_3B0 0x08  li r3,0
//   0x3B8 fn_8_3B8 0x08  li r3,0
//   0x3C0 fn_8_3C0 0x10  *self = kInvalidUniqueId
//   0x3D0 fn_8_3D0 0x08  addi r3,r3,0x754
//   0x3D8 fn_8_3D8 0x08  li r3,0
//   0x3E0 fn_8_3E0 0x08  li r3,0
//   0x3E8 fn_8_3E8 0x2C  virtual dispatch, vtable slot 0x38
//   0x414 RELExit   0x24  li r3,0 / bl fn_80218D24
//   0x438 RELMain   0x20  bl fn_8_458
//   0x458 fn_8_458 0x30  lbl_8_bss_C = fn_8_488 ; fn_80218D24(&lbl_8_bss_C)
//
// **The claim starts at 0x350, not at 0x0, and that is the one thing about this module the
// family does not predict.** The three functions below the accessor block - `fn_8_0` (0x0, 0xD8),
// `fn_8_D8` (0xD8, 0x1E4) and `fn_8_2BC` (0x2BC, 0x94) - are the destructors of the module's
// CPatterned / CBodyController / CKnockBackMgr chain: they store the vtable pointers, run
// `__dt__6CTokenFv` on three `optional_object` members, walk a `CBodyStateInfo` with
// `__dt__14CBodyStateInfoFv` and `__dt__16CBodyStateCmdMgrFv`, free it with
// `Free__7CMemoryFP`, and finish with `__dt__3CAiFv`. That is behavioural class code needing the
// CActor/CPatterned hierarchy this tree does not model, so it stays retail. So 0x0..0x350 is left
// unclaimed and dtk's `auto_00_00000000_text` splits in two - `0x0..0x350` and `0x488..0x5620` -
// with our unit owning the middle. `ScriptCoin` and `Metaree` already do this (their Matching
// units start above 0x0), so a claim that does not begin at the module's first function is an
// ordinary arrangement here, not a new one.
//
// **The accessor block is the family's, in a different order and with the constant-float
// accessor moved to the front**, which is measured by diffing
// `build/G2ME01/ChozoGhost/asm/auto_00_00000000_text.s` over 0x350..0x414 against
// `CMysteryFlyerRel.cpp`'s over 0x0..0xFC rather than read off the `fn_<id>_<off>` names, which
// say nothing about which function is which. Against MysteryFlyer's fourteen accessors this one
// has eleven, and the two multisets differ by exactly: `li r3,1` lost (there is **no** always-true
// predicate in this block), `addi r3,r3,0x818` lost, `lbz r3,0x44f(r3)` lost, the `extrwi` flag
// test at +0x34c lost, `lbl_8041B758` (the DOL constant) lost, and the interleaved three-float
// copy at +0x54 lost - against `lbl_8_rodata_0`, this module's **own** `.rodata:0x0`, gained at
// the front where MysteryFlyer's opening `li r3,1` is, and two more `li r3,0` predicates gained
// (`fn_8_3D8`, `fn_8_3E0`), which is five `li r3,0` in a row across `fn_8_3A8`..`fn_8_3E0` with
// nothing between them. So the block is 0x350..0x414 with eleven functions, the head is
// 0x350..0x488 with fourteen, and nothing is missing: all eleven accessors are vtable entries of
// the 0x148-byte table at `.data:0x29C` (`build/G2ME01/ChozoGhost/asm/auto_04_00000000_data.s`
// stores `fn_8_350`, `fn_8_35C`, `fn_8_398`, `fn_8_3A8`, `fn_8_3B0`, `fn_8_3B8`, `fn_8_3C0`,
// `fn_8_3D0`, `fn_8_3D8`, `fn_8_3E0` and `fn_8_3E8` there), so there is no `force_active:` hazard
// either - and `config/G2ME01/config.yml` carries no `force_active:` list for this module.
//
// `fn_8_350` reads `lbl_8_rodata_0`, `.rodata:0x0`, `size:0x4 data:float`, `.float 60`
// (`build/G2ME01/ChozoGhost/asm/auto_03_00000000_rodata.s`). That is the same position and shape
// as `CDarkCommandoRel.cpp`'s `fn_3_8` and `CIngSpaceJumpGuardianRel.cpp`'s `fn_34_4`, and the
// same "this module's own rodata, not a DOL constant" distinction
// `CMysteryFlyerRel.cpp`'s `fn_45_90` (`lbl_8041B758`) draws: this unit's split claims `.text`
// only, so `lbl_8_rodata_0` stays defined in dtk's `.rodata` object and the reference is an
// ordinary `extern "C"` declaration. `REL_Setup`'s claim is `.rodata 0x168..0x1EC`, so `0x0` is
// not in it.
//
// `fn_8_35C` is not an `optional_object` problem, though it returns one. Retail *calls* the
// converting constructor, out of line at 0x55E4 (`fn_8_55E4`, 0x3C bytes: `stb 1, 0x18(r3)` then
// six words copied), which this unit does not claim, so `fn_8_35C` is written as the free
// function it compiles to - hidden return pointer in r3, `self` in r4 - and calls `fn_8_55E4` by
// its dtk name. `self` needs no move because `GetBoundingBox` takes `this` in r4 too (its own
// return pointer is r3). This is instruction for instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`.
// `fn_8_55E4` lands in the *second* auto unit here (`0x488..0x5620`), where `fn_8_2BBC` landed
// in MysteryFlyer's; nothing about the reference differs.
//
// Everything from `fn_8_488` (0x488, 0x768) up is left unclaimed: that is the module's own
// entity loader and its members - behavioural class code - and dtk fills it from retail, so the
// module's sha1 against `config/G2ME01/config.yml` still holds. The module has 120 text symbols in
// all, which is what `tools/audit_rel_claim.py ChozoGhost` counts: 14 ours, 5 `REL_Setup`, 2
// `global_destructor_chain` and 99 unclaimed.
//
// The three callees are named by what they are, not invented:
//   - `fn_80218D24` is the DOL's 0x80218D24, two instructions,
//     `stw r3, gLoader_ChozoGhost@sda21(r0); blr` (see
//     `build/G2ME01/asm/auto_03_80218D24_text.s`), immediately after
//     `LoadChozoGhost__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218CF8, which is
//     44 bytes and so ends exactly there. So it stores the *address* of a loader slot, not a
//     loader. `LoadChozoGhost` in `src/MetroidPrime/ScriptLoader/ChozoGhost.cpp` (a `Matching`
//     unit) reads it as `lwz r6, gLoader_ChozoGhost; lwz r12, 0(r6); mtctr r12; bctrl`, which is
//     why the store below hands it `&lbl_8_bss_C` and why that slot is four bytes wide. That
//     source also records that the setter is deliberately not claimed in the DOL, because REL
//     modules import it by its retail name - so it stays in dtk's auto unit, and **it is the
//     plain DOL symbol, so no `symbols.txt` rename and no DOL change are needed**. It is
//     `extern "C"`: an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_8_bss_C` is `.bss:0xC`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. It is **not** `.bss:0x0` as in MysteryFlyer - this module's `.bss` holds three
//     objects (`auto_05_00000000_bss.s`: `lbl_8_bss_0` `size:0xC` at 0x0, this one at 0xC, and
//     `lbl_8_bss_10` `size:0x10` at 0x10), and only this one is `data:4byte`, which is what
//     marks it as the slot `fn_8_458` stores through (`lis r3, lbl_8_bss_C@ha; ... stwu r0,
//     lbl_8_bss_C@l(r3)`). This unit's split claims .text only, so dtk's `.bss` object has to
//     define it, and a second definition under MWCC is what produced mwldeppc's internal linker
//     error on ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//   - `fn_8_488` is the module's entity loader, referenced only by address in the registration.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "Kyoto/Math/CAABox.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "REL/REL_Setup.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8_rodata_0;

// `GetBoundingBox__13CPhysicsActorCFv`, the DOL's 0x800EA054, declared through a stand-in rather
// than `MetroidPrime/CPhysicsActor.hpp`: that header reaches `Collision/CMaterialList.hpp`, whose
// file-scope `static EMaterialTypes SolidMaterial` (and the constants beside it) put 0x28 bytes of
// `.data` in this object. Retail's head has none, and the module's sha1 broke on exactly that
// with every function at 100%. Only the mangled name has to agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};

// The vtable slot fn_8_3E8 dispatches to; see `CAtomicAlphaRel.cpp` for the layout. Thirteen
// virtuals put the thirteenth at 0x38, which is the offset retail loads.
class CChozoGhostDispatch {
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
CEntity* fn_8_488(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80218D24(FScriptLoader* loader);
// .text 0x55E4, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line.
void fn_8_55E4(void* out, const CAABox& box);

#ifdef __MWERKS__
extern FScriptLoader lbl_8_bss_C;
#else
FScriptLoader lbl_8_bss_C = 0;
#endif

// .text 0x458, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_8_458() {
  lbl_8_bss_C = fn_8_488;
  fn_80218D24(&lbl_8_bss_C);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMysteryFlyerRel.cpp`, `CSwampBossStage2Rel.cpp` and `CSwampBossStage1Rel.cpp`: listing this
// file in `files.cmake` would make the port link `fn_8_488` and `fn_80218D24`, which it cannot,
// and `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps
// reading `ChozoGhost.rel` off the disc through `platform/rel.cpp`, and the `#else` branch exists
// only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_8_458(); }

void RELExit() { fn_80218D24(nullptr); }
#else
void mp_relmain_chozoghost() { fn_8_458(); }

void mp_relexit_chozoghost() { fn_80218D24(nullptr); }
#endif

// .text 0x3E8, 0x2C bytes. The call targets vtable slot 0x38.
void fn_8_3E8(CChozoGhostDispatch* self) { self->Slot12(); }

// .text 0x3E0, 0x08 bytes. a predicate that is always false.
bool fn_8_3E0(void*) { return false; }

// .text 0x3D8, 0x08 bytes. a predicate that is always false.
bool fn_8_3D8(void*) { return false; }

// .text 0x3D0, 0x08 bytes. the address of the member at +0x754.
void* fn_8_3D0(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x3C0, 0x10 bytes. resets the unique id to the invalid value.
void fn_8_3C0(TUniqueId* id) { *id = kInvalidUniqueId; }

// .text 0x3B8, 0x08 bytes. a predicate that is always false.
bool fn_8_3B8(void*) { return false; }

// .text 0x3B0, 0x08 bytes. a predicate that is always false.
bool fn_8_3B0(void*) { return false; }

// .text 0x3A8, 0x08 bytes. a predicate that is always false.
bool fn_8_3A8(void*) { return false; }

// .text 0x398, 0x10 bytes. stores a DOL float at +0x448.
void fn_8_398(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

// .text 0x35C, 0x3C bytes. Returns `optional_object<CAABox>(GetBoundingBox())` through the hidden
// pointer `out`; see the note at the top of the file.
void fn_8_35C(void* out, const CPhysicsActor* self) {
  fn_8_55E4(out, self->GetBoundingBox());
}

// .text 0x350, 0x0C bytes. this module's own `lbl_8_rodata_0`, `.float 60`.
float fn_8_350(void*) { return lbl_8_rodata_0; }
}
