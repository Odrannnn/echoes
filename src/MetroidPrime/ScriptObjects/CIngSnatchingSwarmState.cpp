// CIngSnatchingSwarmState.cpp - IngSnatchingSwarm's (module 33) state run, .text
// 0x3068..0x31A4: `fn_33_3068` (`DiveToTarget`) and `fn_33_312C` (`FollowArcPath`), contiguous,
// sitting directly **below** the state entry point `CIngSnatchingSwarmUpdate.cpp` claims
// (0x348C..0x35F8).
//
// **The module's own `.data` names both.** `build/G2ME01/IngSnatchingSwarm/asm/auto_04_00000000_data.s`
// holds a run of 12-byte records `.4byte 0, 0xFFFFFFFF, <fn>` at `.data:0x100, 0x10C, 0x118, 0x124,
// 0x130, 0x13C`, naming `fn_33_35F8, fn_33_348C, fn_33_31A4, fn_33_312C, fn_33_3068, fn_33_2D24`, and a
// run of names at `.data:0x148` (`"Start"`, `"Dead"`, `"ExitPortal"`, `"FollowArcPath"`,
// `"DiveToTarget"`, `"SnatchTarget"`) in the same order - so these two are the module's
// `FollowArcPath` and `DiveToTarget` entries, dispatching on a small integer state, exactly like
// the sibling run `CIngSnatchingSwarmAi.cpp` documents.
//
// The claim is the whole run and nothing else: everything outside 0x3068..0x31A4 is left
// unclaimed, so dtk fills it from retail and the module's sha1 against `config/G2ME01/config.yml`
// still holds. Every offset below is read off `build/G2ME01/IngSnatchingSwarm/asm/
// auto_00_000000A8_text.s`.
//
// Like the other class-body units in this module, the object is a stand-in class carrying the
// measured offsets and the bodies are ordinary C++ over them - **only the offsets are derived.**

// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% while the module's hash breaks - only
// `tools/flip_test.sh` catches that. `fn_33_312C` (0x312C) therefore comes first in this file and
// `fn_33_3068` (0x3068) last.

#include "types.h"

#include "Kyoto/Math/CVector3f.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CStateManager.hpp"

// The module's `CGenericFSM2` member, as the **vtable** rather than as the class. Retail's call is
// `addi r3, r31, 0x380` / `lwz r12, 0x380(r31)` / `lwz r12, 0x30(r12)` - the member at 0x380 is a
// polymorphic object *by value* (the vptr is the word at 0x380 itself, not a pointer to one) and the
// slot at 0x30 is index 11 of the 13 the table has to be written out to.
//
// The table is written out from what the module's own `fn_33_1AF8` (0x1AF8, still retail) does with
// the same member: `SetupFSM__17CGenericFSM2StateFPC12CGenericFSM2`, then three calls that each take
// one of the module's `.data` tables and a count - 9 on `lbl_33_data_70`, 6 on `lbl_33_data_148`, 6 on
// `lbl_33_data_1F0` - at slots 0x18, 0x14 and 0x1c, then one at 0x20 that takes the state manager,
// the owner and a string, and finally the slot at 0x30 that this unit calls and which **returns a
// float**: retail never loads `f1` before the call, and `fn_33_17C8`'s own first floating argument
// arrives in `f1` untouched, so the value the slot returns is what is handed on.
//
// **Every entry is pure, and that is load-bearing**: an inline virtual with a body would make
// mwcceppc emit an out-of-line weak copy of it into this object, and the 0x3068..0x31A4 claim has no
// room for one - the same constraint `CIngSnatchingSwarmUpdate.cpp` records for
// `CParticleGen::SetGeneratorRate`.
class CIngSnatchingSwarmFsm {
public:
  virtual ~CIngSnatchingSwarmFsm() = 0;
  virtual void Destroy() = 0;
  virtual void Reset() = 0;
  // vtable slot 0x14, index 3 - `fn_33_1AF8` passes this module's `lbl_33_data_148` and 6.
  virtual void SetupStates(const void* states, int count) = 0;
  // vtable slot 0x18, index 4 - the same shape on `lbl_33_data_70` and 9.
  virtual void SetupMessages(const void* msgs, int count) = 0;
  // vtable slot 0x1c, index 5 - the same shape on `lbl_33_data_1F0` and 6.
  virtual void SetupFlags(const void* flags, int count) = 0;
  // vtable slot 0x20, index 6 - `fn_33_1AF8` passes the state manager, the owner and a string.
  virtual void Setup(CStateManager& mgr, void* owner, const char* name) = 0;
  virtual void Enter() = 0;
  virtual void Exit() = 0;
  virtual bool IsDone() = 0;
  // vtable slot 0x30, index 11 - the no-argument slot whose float return is `fn_33_17C8`'s
  // argument.
  virtual float Think() = 0;
};

// The object `mgr.GetObjectById` hands back in `fn_33_3068`, reached through the **vtable** at
// slot 0x54: retail's call is `lwz r12,0(r4)` / `lwz r12,0x54(r12)` with `r3` already holding the
// address of a 12-byte return slot, so the slot at 0x54 (index 19 of the table written out below)
// **returns a `CVector3f` by value** and takes `(float, CStateManager&, const float&)`. Nothing
// else in this module calls it, so the other eighteen slots are named by their position only.
class CIngSnatchingSwarmTarget {
public:
  virtual ~CIngSnatchingSwarmTarget() = 0;
  virtual void A01() = 0;
  virtual void A02() = 0;
  virtual void A03() = 0;
  virtual void A04() = 0;
  virtual void A05() = 0;
  virtual void A06() = 0;
  virtual void A07() = 0;
  virtual void A08() = 0;
  virtual void A09() = 0;
  virtual void A10() = 0;
  virtual void A11() = 0;
  virtual void A12() = 0;
  virtual void A13() = 0;
  virtual void A14() = 0;
  virtual void A15() = 0;
  virtual void A16() = 0;
  virtual void A17() = 0;
  virtual void A18() = 0;
  // vtable slot 0x54, index 19.
  virtual CVector3f GetOffsetPoint(float scale, CStateManager& mgr, const float& rate) const = 0;
};

// The object, reduced to the members these two functions touch: the float at 0x170, the
// `CGenericFSM2` member at 0x380, the state counter at 0x3D4 that `fn_33_37E0` (`x3D4 == 2`)
// reads, and the target id at 0x412 that `fn_33_3678`/`fn_33_3708` also read. Every offset is
// measured; the padding between members is the space the class's real body occupies and is
// deliberately written as `char x_padN[...]`, which `tools/check_raw_offsets.py` does not count
// as a raw offset.
class CIngSnatchingSwarmState {
private:
  char x_pad0[0x170];

public:
  float x170;

private:
  char x_pad1[0x380 - 0x174];

public:
  // The member itself is abstract, so it cannot be a member by value; it is carried as the four
  // bytes of its vptr and reached through the accessor below, which is exactly the address
  // `addi r3, r31, 0x380` in retail.
  char x380[4];

private:
  char x_pad2[0x3D4 - 0x384];

public:
  u32 x3D4;

private:
  char x_pad3[0x412 - 0x3D8];

public:
  TUniqueId x412;

  CIngSnatchingSwarmFsm* GetFsm() { return reinterpret_cast< CIngSnatchingSwarmFsm* >(x380); }
};

// **The host branch only declares what retail owns.** `.rodata:0x34` is `.float 0`
// (`build/G2ME01/IngSnatchingSwarm/asm/auto_03_00000000_rodata.s`) and is the argument of both the
// `CGenericFSM2` slot's float parameter and the `GetOffsetPoint` call below. This unit claims
// `.text` only, so the reference has to be to the symbol the unclaimed `.rodata` object defines and
// not to a literal of our own - a literal would put a `.rodata` section in this object and move the
// module. The host branch needs a definition because the flat port link has no module to import
// from, and `CIngSnatchingSwarmAi.cpp` and `CIngSnatchingSwarmUpdate.cpp` already own theirs.
#ifdef __MWERKS__
extern "C" {
extern const float lbl_33_rodata_34;

// `fn_33_17C8` (0x17C8, 0x140) is this module's own function and is **not** claimed here - it is
// inside the unclaimed 0xA8..0x348C run, so dtk links it from retail and the call below becomes an
// import. Its body is a bezier evaluation that returns a plain `bool`, which the module's callers
// test with `clrlwi. r0, r3, 24` - mwcceppc's zero test for a `bool` in r3.
bool fn_33_17C8(CIngSnatchingSwarmState* self, float dt);

// `fn_33_1670` (0x1670, 0x158) is the same story: this module's own function, unclaimed, so dtk
// links it from retail. Its arguments are read off its two call sites in the module - `fn_33_3068`
// below passes `(self, &v, self->x170, arg, v.x)` and retail's own `fn_33_2D24` at 0x2F70 passes
// `(self, &v, self->x170, dt)` leaving the fifth one alone, so the third floating argument has a
// default that only `fn_33_3068` spells out.
void fn_33_1670(CIngSnatchingSwarmState* self, CVector3f* v, float a, float b, float c = 0.0f);
}
#else
extern "C" {
const float lbl_33_rodata_34 = 0.0f;
bool fn_33_17C8(CIngSnatchingSwarmState*, float) { return false; }
void fn_33_1670(CIngSnatchingSwarmState*, CVector3f*, float, float, float) {}
}
#endif

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt` calls them.
extern "C" {
// .text 0x312C, 0x78 bytes - the module's `FollowArcPath` state entry. `0` marks the path phase
// begun, `1` runs the `CGenericFSM2` at 0x380 for a frame and retires the object once the module's
// own bezier step reports the path finished, and anything else does nothing.
//
// The comparison tree is a `switch`: `cmpwi r5,1` / `beq` / `bge` then `cmpwi r5,0` / `bge` / `b`
// is the two-case lowering, and the `case 0` block is laid out before the `case 1` block.
void fn_33_312C(CIngSnatchingSwarmState* self, CStateManager& mgr, int state) {
  switch (state) {
  case 0:
    self->x3D4 = 1;
    break;
  case 1:
    if (!fn_33_17C8(self, self->GetFsm()->Think())) {
      self->x3D4 = 2;
    }
    break;
  }
}

// .text 0x3068, 0xC4 bytes - the module's `DiveToTarget` state entry. Only `state == 1` does
// anything, so the comparison tree is `cmpwi r5,1` / `beq` / `b` - the **`switch` with a single
// case** lowering, which lays the case body out after both branches and leaves the return as the
// fall-through. A plain `if (state == 1)` emits `bne` straight to the epilogue instead, which is
// the same tree but three bytes different. It looks the id at 0x412 up, and if that names a live
// object it asks it for a point (`vtable slot 0x54`) and hands that point, the float at 0x170 and
// the incoming argument on to `fn_33_1670`.
//
// `fmr f2, f31` copies the incoming `f1` straight into the second floating argument, and the
// returned point is stored through a **local** at 0x18 rather than through the virtual's own return
// slot at 0xc - the copy is three `stfs` and the address of the copy is what `r4` carries, so the
// parameter is `CVector3f*` and the source hands over a separate local.
void fn_33_3068(CIngSnatchingSwarmState* self, CStateManager& mgr, float dt, int state) {
  switch (state) {
  case 1: {
    const CEntity* obj = mgr.GetObjectById(self->x412);
    if (obj != nullptr) {
      CVector3f v = reinterpret_cast< const CIngSnatchingSwarmTarget* >(obj)->GetOffsetPoint(
          lbl_33_rodata_34, mgr, lbl_33_rodata_34);
      fn_33_1670(self, &v, self->x170, dt, v.GetX());
    }
    break;
  }
  }
}
}
