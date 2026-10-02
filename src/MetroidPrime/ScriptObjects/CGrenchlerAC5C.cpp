// CGrenchlerAC5C.cpp - Grenchler's (module 27) .text 0xAC5C..0xACF0: the two virtuals
// `tools/rel_class_map.py Grenchler` reads out of the module's own vtable at slots 18 and 21.
//
//   0xAC5C fn_27_AC5C  0x40  vtable slot 18: forwards to the object's *own* vtable slot 0x54 -
//                            slot 19, `fn_27_AB94` - with the state manager and a `0.0f` from
//                            the module's `.rodata:0x180`
//   0xAC9C fn_27_AC9C  0x54  vtable slot 21: the `CVector3f` at +0xF0C when the state word at
//                            +0xA78 is 0xE, otherwise `CActor::GetScanObjectIndicatorPosition`
//
// The two are contiguous - `config/G2ME01/rels/Grenchler/symbols.txt` gives 0xAC5C size 0x40 and
// 0xAC9C size 0x54, so 0xAC5C + 0x40 = 0xAC9C and 0xAC9C + 0x54 = 0xACF0 is `fn_27_ACF0` - so the
// claim covers them and nothing else. The 0x1BC-byte `fn_27_ACF0` and the rest of dtk's
// `auto_00_00000168_text` stay unclaimed and dtk fills them from retail, which is what keeps the
// module's sha1 at `config/G2ME01/config.yml`'s `de128c91f467b5b61b4de4a2ed6b217044a975c5`.
//
// **The dispatch is a member call, and the class is a stand-in - both measured, not assumed.**
// `fn_27_AC5C` reaches its callee through the vtable at `lwz r12,0x0(r4); lwz r12,0x54(r12)`, and
// 0x54 is exactly where `tools/rel_class_map.py Grenchler` reports this module's own slot 19
// (`fn_27_AB94`, the module's `GetAimPosition`), so the call is `this->GetAimPosition(mgr, 0.f)`
// and *not* a hand-loaded vtable: `CIngPuddleRel.cpp` measured that spelling as `lwz r3,0(r3)`
// against retail's `lwz r12,0(r3)`, 99.09% on the function. mwcceppc lays a class's virtuals out
// as two leading words then one word each, so a stand-in with twenty virtuals puts the twentieth
// at 0x54. It is declared and never defined, so no vtable and no code is emitted for it - only
// the dispatch - and the class name never reaches the object, which is why the mangled name of
// the callee does not have to agree with anything.
//
// **`fn_27_AC5C`'s registers are the by-value-return convention and that is why the receiver is
// in r4.** `fn_27_AB94`'s own bytes write the returned `CVector3f` through its r3 and read the
// object at r4 (`lfs f1,0xc48(r4)`, `lbz r5,0x90c(r4)`), so a `CVector3f`-returning member
// receives the hidden return pointer in r3 and the receiver in r4; `fn_27_AC5C` keeps r3 (retail
// never reloads it) and dispatches through r4, which is that convention and not an assumption.
// The `0.0f` comes from `lbl_27_rodata_180` (`.float 0`, dtk's `auto_03_00000000_rodata`).
//
// **`fn_27_AC9C` is a member call to a DOL virtual, not a stand-in.** Its `else` branch is
// `bl GetScanObjectIndicatorPosition__6CActorCFRC13CStateManager` (`symbols.txt:1453`,
// 0x8004BC20, size 0x38) with r3 (the hidden return pointer), r4 (the receiver) and r5 (the state
// manager) exactly as they arrive - the same by-value-return convention - so it is declared under
// its retail spelling and called with (self, mgr).
//
// Both functions are in the module's `ldscript.lcf` FORCEACTIVE block (they are vtable slots with
// no call site in the module's bytes), so nothing here needs a `force_active:` entry - measured:
// the module links to retail's exact size and the `.rel` is byte-identical.
//
// The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, the arrangement
// `CLumiteRelTail.cpp` and `CPlantScarabSwarmTail.cpp` use: `check_files_cmake.py` requires every
// configure.py `Matching` object to be in `files.cmake`, and only a RELMain/RELExit unit is
// exempt - but a host body would make the port link the module's own `fn_27_AB94` slot and the
// DOL's `GetScanObjectIndicatorPosition` spelling through a host-incompatible call.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.

#include "types.h"

#include "Kyoto/Math/CVector3f.hpp"

class CStateManager;

/** The stand-in `fn_27_AC5C` dispatches through: twenty virtuals, so the twentieth sits at
 *  +0x54, which is the slot `tools/rel_class_map.py Grenchler` reads as this module's own
 *  `fn_27_AB94`. Only the last one is ever called and none is defined, so the object gets one
 *  indirect call and no vtable. The parameter types are the callee's, as its own bytes read
 *  them: the returned `CVector3f` is by value (hidden pointer in r3) and the float arrives in f1,
 *  which is where the `lfs f1, lbl_27_rodata_180@l(r6)` before the dispatch puts it. */
class CGrenchlerAimDispatch {
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
  virtual void Slot13();
  virtual void Slot14();
  virtual void Slot15();
  virtual void Slot16();
  virtual void Slot17();
  virtual void Slot18();
  virtual CVector3f GetAimPosition(const CStateManager& mgr, float dt) const; // +0x54
};

/** 0xAC9C's range: the state word at +0xA78 and the vector at +0xF0C. The class is CGrenchler,
 *  whose full layout this tree does not model, so the gaps are opaque padding of the measured
 *  size rather than invented members. */
struct SGrenchlerScanPos {
  uchar mUnknown00[0xA78];
  int mState0A78; // +0xA78
  uchar mUnknownA7C[0xF0C - 0xA7C];
  CVector3f mScanPos; // +0xF0C
};

/** 0x8004BC20, `symbols.txt:1453`, size 0x38: `CActor::GetScanObjectIndicatorPosition`'s own
 *  implementation, reached through the mangled name the module's own `bl` uses. */
extern "C" CVector3f GetScanObjectIndicatorPosition__6CActorCFRC13CStateManager(
    const void* self, const CStateManager& mgr);

/** The module's own `.rodata:0x180`, `.float 0`, stored by dtk's `auto_03_00000000_rodata`.
 *  **Referenced rather than spelled `0.f`**: a literal would make mwcceppc emit a private
 *  4-byte `.rodata` constant into this object, and the module's `.rodata` grew 0x944 -> 0x94C
 *  with the `.text` and every function already exact (measured). Loading the module's own symbol
 *  is also what retail's `lis r6,lbl_27_rodata_180@ha / lfs f1,lbl_27_rodata_180@l(r6)` does. */
extern "C" const float lbl_27_rodata_180;

#ifdef __MWERKS__

// 0xAC9C, 0x54 bytes. The `0xE` is the word retail compares with `cmpwi`, and the `CVector3f`
// copy in the taken branch is three load/store pairs because the destination is the caller's
// hidden return pointer, which is also what the `bl` in the other branch forwards.
extern "C" CVector3f fn_27_AC9C(const SGrenchlerScanPos* self, const CStateManager& mgr) {
  if (self->mState0A78 == 0xE) {
    return self->mScanPos;
  }
  return GetScanObjectIndicatorPosition__6CActorCFRC13CStateManager(self, mgr);
}

// 0xAC5C, 0x40 bytes. One virtual dispatch on the receiver, with the state manager and the
// module's own `0.0f`; the result is the callee's, passed straight back because both returns use
// the same hidden pointer.
extern "C" CVector3f fn_27_AC5C(const CGrenchlerAimDispatch* self, const CStateManager& mgr) {
  return self->GetAimPosition(mgr, lbl_27_rodata_180);
}

#endif // __MWERKS__
