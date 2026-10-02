#include "MetroidPrime/CStateManager.hpp"

#include "Collision/CRayCastResult.hpp"
#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CActorModelParticles.hpp"
#include "MetroidPrime/CObjectList.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "MetroidPrime/CSortedLists.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/CGameCollision.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CDamageInfo.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CGameCollision.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CPhysicsActor.hpp"
#include "MetroidPrime/CPortalTransition.hpp"
#include "MetroidPrime/CSaveGameScreen.hpp"
#include "MetroidPrime/CStateManagerContainer.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"
#include "MetroidPrime/Player/CGameMode.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/ScriptObjects/CScriptEffect.hpp"
#include "MetroidPrime/ScriptObjects/CScriptPlatform.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"
#include "MetroidPrime/Weapons/CWeaponMgr.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/RAssertDolphin.hpp"
#include "Kyoto/CARAMToken.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/CToken.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "rstl/vector.hpp"

const int gkPVSEnabled = 1;

extern "C" void fn_8030184C();
// Retail 0x800B89FC. Unwritten: it is a real function the port does not define, so naming it here
// adds one entry to the port's undefined list. Called only from CStateManager::AreaLoaded.
// The area id is a REFERENCE on purpose. mwcceppc passes a by-value 4-byte class type as a
// pointer to a caller-side temporary, so declaring the parameter by value makes AreaLoaded copy
// `area` onto its own frame (stwu -0x20, lwz/addi/stw) before forwarding the address. Retail
// forwards r4 untouched. A reference takes the same address without the copy.
extern "C" void fn_800B89FC(CMapWorldInfo* info, const TAreaId& area, CStateManager* mgr);
// Retail 0x80041CCC, called only from UpdateActorInSortedLists. Unwritten here, so it costs one
// undefined symbol - exactly what defining UpdateActorInSortedLists takes back out of the port's
// list, which is why this pair is the unit's only net-zero undefined-cost candidate. It writes a
// CAABox at +0 and a validity byte at +0x18.
// Retail 0x80041CCC writes a CAABox at +0 and a validity byte at +0x18, and the caller
// copy-constructs the whole 25 bytes. A raw buffer rather than a `CAABox` member because CAABox
// has no default constructor, and any constructor here emits a real
// `bl CAABox::CAABox(CVector3f, CVector3f)` that retail does not have.
struct SFoundBounds {
  CVector3f min;
  CVector3f max;
  uchar valid;
  CAABox& Box() { return *(CAABox*)this; }
};
extern "C" void fn_80041CCC(SFoundBounds* out, CStateManager* mgr, CActor* actor);
extern "C" int lbl_80419A10;
extern "C" int lbl_80419A18;
extern "C" int lbl_80418FB8;
extern "C" int lbl_80418FBC;
extern "C" uchar lbl_80419730;
extern "C" uchar lbl_80419745;
extern "C" uchar lbl_80419A98;

extern "C" void fn_80036F68(CStateManager* mgr, void* node) {
  // Port: the console stores these as 32-bit guest addresses; on a 64-bit host the
  // list link is a host pointer, so go through uintptr_t rather than truncating.
  *reinterpret_cast< uintptr_t* >(static_cast< char* >(node) + 0x9c) = mgr->x2458;
  mgr->x2458 = reinterpret_cast< uintptr_t >(node);
}

extern "C" void fn_8003A834(uint* out) { *out = lbl_80418FB8; }

extern "C" void fn_8003AD74(uchar value) {
  lbl_80419745 = value;
  lbl_80419A98 = value;
  lbl_80419730 = value;
}

extern "C" void fn_8003B648(uint* out) { *out = lbl_80418FBC; }

extern "C" bool fn_8003C59C() { return false; }

// The two vector initializers have distinct retail entry points.
extern "C" void fn_80038D40(int* vec) { vec[1] = 0; }
extern "C" void fn_80038D4C(int* vec) { vec[1] = 0; }

// Retail's CStateManager.o defines the out-of-line CLight copy constructor (0x80038C9C, 0xA4
// bytes): fn_80038C5C returns a {u16, CLight} aggregate and calls it to fill the second word, so
// it belongs to this translation unit rather than to src/Kyoto/Graphics/CLight.cpp. It copies all
// 0x4D bytes of the object member by member - lfs/stfs per float, lwz/stw for CColor's packed word
// and the two ids, and one lbz/stb for the dirty-flag byte, which is why those two flags are one
// struct member rather than two bitfields.
CLight::CLight(const CLight& other)
: mPos(other.mPos)
, mDir(other.mDir)
, mColor(other.mColor)
, mType(other.mType)
, mSpotCutoff(other.mSpotCutoff)
, mDistC(other.mDistC)
, mDistL(other.mDistL)
, mDistQ(other.mDistQ)
, mAngleC(other.mAngleC)
, mAngleL(other.mAngleL)
, mAngleQ(other.mAngleQ)
, mPriority(other.mPriority)
, mLightId(other.mLightId)
, mCachedRadius(other.mCachedRadius)
, mCachedIntensity(other.mCachedIntensity)
, mDirty(other.mDirty) {}

extern "C" void fn_80038624(CStateManager*);
extern "C" void fn_800388EC(CStateManager* mgr) { fn_80038624(mgr); }
extern "C" void fn_801EBBC8(void*);
extern "C" void fn_80039B1C(void* value) { fn_801EBBC8(value); }

// The three destructor families below (retail 0x80043180/0x800434CC/0x80043688) each have the
// same three-layer shape, and all nine land together because the port's undefined count is
// gated: a forwarder whose callee is only *declared* asks the host linker for a symbol nothing
// defines, and `tools/probe_sources.sh` compares that count against
// `docs/research/port_link_baseline.txt` and reports STRICT FAIL when it grows. So the layer
// under each forwarder is written too, down to the leaf.
//
// The leaf shape is MWCC's deleting destructor:
//
//   stwu r1,-16(r1) ; mflr r0 ; stw r0,20(r1) ; stw r31,12(r1) ; mr r31,r4 ; stw r30,8(r1)
//   mr. r30,r3 ; beq <epilogue>          <- the `if (this)` guard
//   ...destroy the members...
//   extsh. r0,r31 ; ble <epilogue>       <- `if (flag > 0)`, sign-extending the SHORT flag
//   mr r3,r30 ; bl Free__7CMemoryFPCv
//   <epilogue> ... mr r3,r30 ...        <- MWCC destructors return `this`
//
// Three details are load-bearing. The flag parameter is a **`short`**: `int` gives `cmpwi r31,0`
// where retail has `extsh. r0,r31`. The return type is a **pointer**: a `void` leaf loses the
// trailing `mr r3,r30`. And the caller's `li r4,-1` is MWCC's `kDestructorFlagNone` - the
// non-deleting value - which is why the forwarders pass -1 rather than 0 or 1.
//
// The element destructors are NOT inlined: `<rstl/vector.hpp>` spells `~vector()` in the
// header, and mwcceppc at `-inline deferred` still emits the out-of-line copy, so
// `self->x0.~vector()` is a real `bl __dt__Q24rstl36vector<f,Q24rstl17rmemory_allocator>Fv`
// under the retail symbol's own name. That name is what objdiff pairs, and the weak copy lands
// in this object - which is how `__dt__Q24rstl36vector<f,...>Fv`, a function OF this unit at
// 0x800432B8, gets written at all.

// ---- family 1: fn_800431C4 (0x800431C4, 112 B) / fn_800431A0 (36 B) / fn_80043180 (32 B) ----
// The object is three members, all destroyed in DESCENDING offset order (0x3C, 0x2C, 0x1C),
// and the middle one is the `rstl::vector<float>` whose own out-of-line destructor is the
// `__dt__Q24rstl36vector<f,...>Fv` of this unit. The `free` of the block is behind the flag,
// as everywhere.
//
// fn_80043234 (0x80043234, 132 B) is the third member's destructor: a vector of 6-byte
// elements - `mulli r0,r0,6` - whose per-element destructor is EMPTY, so the walk retail
// leaves in place (`addi r4,r4,6 ; cmplw r4,r0 ; bne`, and note `cmplw`, not `cmplwi`) is
// what a `for` over trivially destructible elements compiles to. It then frees the buffer and
// the block. Written over a real `rstl::vector` of a 6-byte struct so the 6 comes from the
// element size rather than a literal.
struct SFixed6 {
  short a, b, c;
};
CHECK_SIZEOF(SFixed6, 6) // `mulli r0,r0,6`

// The three members sit at 0x3C, 0x2C and 0x1C and are 16 bytes each, so the object is 0x4C
// bytes and everything below 0x1C is untouched by the destructor. All three have the
// `rstl::vector` shape (a count at +4 and the buffer at +12, which is what fn_80043234 reads),
// so they are spelled as vectors rather than as a raw block - the `addi` offsets in the leaf
// then come from the layout instead of from a literal.
struct SVectorOwner3 {
  uint x0[7];                             // 0x00..0x1B, not touched here
  rstl::vector< SFixed6 > x1c;
  rstl::vector< float > x2c;
  rstl::vector< int > x3c;
};
CHECK_SIZEOF(SVectorOwner3, 0x4C) // 0x1C + 3 * 16; `addi r3,r30,60` is the last member

// The four spills retail keeps before the walk (`stw r3,20(r1) ; stw r3,8(r1) ; stw r0,16(r1) ;
// stw r0,12(r1)`, and a 32-byte frame against this function's 16) are the two `pointer_iterator`s
// `begin()` and `end()` each holding: the vector pointer twice, and the end pointer twice. The
// walk itself is MWCC's, three instructions with no remainder loop. So the loop is written over
// the vector's own iterators, not over raw pointers, and the element destructor is the trivial
// one - which is what leaves the body empty and keeps the walk.
extern "C" rstl::vector< SFixed6 >* fn_80043234(rstl::vector< SFixed6 >* self, short flag) {
  if (self != nullptr) {
    rstl::destroy(self->begin(), self->end());
    CMemory::Free(self->mItems);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// ---- family 3: fn_8004371C (0x8004371C, 160 B) / fn_800436CC (80 B) / 6A8 / 688 ----
// The object is a counted array of 44-byte records at +4, with the count at +0, walked by
// index rather than by pointer: `li r29,0` is the loop counter, `addi r30,r30,44` steps and
// `cmpw r29,r0` against `lwz r0,0(r28)` is the test. Each record holds a pointer at +36 whose
// first byte is a flag, and a `CToken*` at +40; the token is destroyed and freed when the
// pointer is non-null and its flag byte is non-zero.
struct SRecord44 {
  uchar x0[36];  // 0x00..0x23
  uchar* x24;    // 0x24 - the flag pointer: `addic. r3,r30,36` is 0x24 into the record
  CToken* x28;   // 0x28 - `lwz r31,40(r30)`
};
CHECK_SIZEOF(SRecord44, 0x2C) // 44, the `addi r30,r30,44`

struct SRecordArray {
  int m_count;
  SRecord44 m_items[1];
};

// The redundant `beq 43780` between the `cmplwi r31,0` test and the `~CToken` call is
// MWCC's duplicate of the branch it just emitted: it re-tests the token pointer it has already
// proved non-null, and this time the target is the `CMemory::Free` rather than the loop end.
extern "C" void fn_8004371C(SRecordArray* self) {
  for (int i = 0; i < self->m_count; ++i) {
    SRecord44& rec = self->m_items[i];
    if (rec.x24 == nullptr || rec.x24[0] == 0 || rec.x28 == nullptr) {
      continue;
    }
    rec.x28->~CToken();
    CMemory::Free(rec.x28);
  }
}

extern "C" SRecordArray* fn_800436CC(SRecordArray* self, short flag) {
  if (self != nullptr) {
    fn_8004371C(self);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void fn_800436A8(SRecordArray* self) { fn_800436CC(self, -1); }

extern "C" void fn_80043688(SRecordArray* self) { fn_800436A8(self); }

// ---- family 2: fn_80043510 (0x80043510, 84 B) / fn_800434EC (36 B) / fn_800434CC (32 B) ----
// The object is a single `rstl::vector<CToken>` and nothing else, so the leaf calls the element
// destructor with r3 untouched - no `addi r3,r30,off` for a member at a non-zero offset.
struct STokenVectorOwner {
  rstl::vector< CToken > x0;
};
CHECK_SIZEOF(STokenVectorOwner, 0x10) // one vector, at offset 0

extern "C" STokenVectorOwner* fn_80043510(STokenVectorOwner* self, short flag) {
  if (self != nullptr) {
    self->x0.~vector();
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void fn_800434EC(STokenVectorOwner* self) { fn_80043510(self, -1); }

extern "C" void fn_800434CC(STokenVectorOwner* self) { fn_800434EC(self); }

extern "C" SVectorOwner3* fn_800431C4(SVectorOwner3* self, short flag) {
  if (self != nullptr) {
    self->x3c.~vector();
    self->x2c.~vector();
    fn_80043234(&self->x1c, -1);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

extern "C" void fn_800431A0(SVectorOwner3* self) { fn_800431C4(self, -1); }

extern "C" void fn_80043180(SVectorOwner3* self) { fn_800431A0(self); }

// ---- the array layer above the three families: fn_800430D0 / 43120, 4341C / 4346C,
// 435D8 / 43628 --------------------------------------------------------------------------------
// Each family that landed above is reached in retail through a *second* pair: a counted array
// whose elements are that family's object, and a deleting destructor over the array. The two
// halves of each pair are written together for the same reason as the families themselves - the
// forwarder `fn_800430D0` calls `fn_80043120`, and a call to a merely-declared function is one
// more undefined symbol in the host link.
//
// The walks are MWCC's counted-array form: the counter lives in r30 (`li r30,0` before the
// loop), the element pointer steps by the element size (`addi r31,r31,<size>`), and the test is
// `lwz r0,0(r29) ; cmpw r30,r0 ; blt` against the count at offset 0. Note `cmpw` on a register
// pair and that the test block sits *below* the body, so the loop is written as an indexed
// `for`, not as a pointer-bounded `while`.
struct SVectorOwner3Array {
  int m_count;  // 0x00
  SVectorOwner3 m_items[1]; // 0x04, stride 0x4C
};
CHECK_SIZEOF(SVectorOwner3Array, 0x50) // 4 + 0x4C; the first element is at +4

extern "C" void fn_80043120(SVectorOwner3Array* self) {
  SVectorOwner3* p = self->m_items;
  for (int i = 0; i < self->m_count; ++i) {
    fn_80043180(p);
    p = reinterpret_cast< SVectorOwner3* >(
        reinterpret_cast< uchar* >(p) + sizeof(SVectorOwner3));
  }
}

extern "C" SVectorOwner3Array* fn_800430D0(SVectorOwner3Array* self, short flag) {
  if (self != nullptr) {
    fn_80043120(self);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// The stride here is 24, not the 16 that family 2's own object is: the array's element is a
// wider struct that begins with the `rstl::vector<CToken>` the leaf destroys, so only its first
// 16 bytes are touched here. Spelled as its own type rather than as a bare stride literal.
struct STokenVectorOwner24 {
  rstl::vector< CToken > x0;
  uint x10[2];
};
CHECK_SIZEOF(STokenVectorOwner24, 0x18) // `addi r31,r31,24`

struct STokenVectorOwner24Array {
  int m_count; // 0x00
  STokenVectorOwner24 m_items[1]; // 0x04, stride 24
};

extern "C" void fn_8004346C(STokenVectorOwner24Array* self) {
  STokenVectorOwner24* p = self->m_items;
  for (int i = 0; i < self->m_count; ++i) {
    fn_800434CC(reinterpret_cast< STokenVectorOwner* >(p));
    p = reinterpret_cast< STokenVectorOwner24* >(
        reinterpret_cast< uchar* >(p) + sizeof(STokenVectorOwner24));
  }
}

extern "C" STokenVectorOwner24Array* fn_8004341C(STokenVectorOwner24Array* self, short flag) {
  if (self != nullptr) {
    fn_8004346C(self);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// 488 = 0x1E8. The element opens with family 3's `SRecordArray`, whose destructor is the leaf;
// the remaining bytes are not reached from here.
struct SRecordArray488 {
  SRecordArray x0; // 0x00, 0x30 bytes
  uchar x30[488 - 0x30];
};
CHECK_SIZEOF(SRecordArray488, 488) // `addi r31,r31,488`

struct SRecordArray488List {
  int m_count; // 0x00
  SRecordArray488 m_items[1]; // 0x04, stride 488
};

extern "C" void fn_80043628(SRecordArray488List* self) {
  SRecordArray488* p = self->m_items;
  for (int i = 0; i < self->m_count; ++i) {
    fn_80043688(&p->x0); // x0 is the SRecordArray at the element's offset 0
    p = reinterpret_cast< SRecordArray488* >(
        reinterpret_cast< uchar* >(p) + sizeof(SRecordArray488));
  }
}

extern "C" SRecordArray488List* fn_800435D8(SRecordArray488List* self, short flag) {
  if (self != nullptr) {
    fn_80043628(self);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// ---- fn_8004380C (0x8004380C, 124 B) and the forwarder fn_800437BC (0x800437BC, 80 B) -------
// A fourth family, of the same three-layer shape as the three above, whose walk is not a
// destructor walk at all. Its outer loop is the counted form again (count at +0, elements at +4,
// stride 356) but the inner loop zeroes a byte run: `lwz r7,0(r5)` is the element's length, and
// the body is MWCC's memset expansion - 8 bytes per trip with a 7-bit `srwi` trip count, then a
// one-byte-per-trip remainder counted by `subf r0,r3,r7`. The `cmplwi r5,0` guard tests the
// *element pointer* for null, and `li r3,0` seeds the written offset.
//
// So the element is a counted byte buffer reached through a pointer that may be null: a length at
// +0 and a payload at +4.
struct SByteBuf356 {
  int m_length; // 0x00, `lwz r7,0(r5)`
  uchar m_data[356 - 4];
};
CHECK_SIZEOF(SByteBuf356, 356) // `addi r5,r5,356`

struct SByteBuf356List {
  int m_count;        // 0x00, `lwz r6,0(r3)`
  SByteBuf356 m_items[1]; // 0x04 - the elements are INLINE, not behind a pointer: retail seeds
                        // the element pointer once with `addi r5,r3,4` and then only ever steps
                        // it by 356, where a pointer member makes the compiler reload it.
};

extern "C" void fn_8004380C(SByteBuf356List* self) {
  uchar* p = reinterpret_cast< uchar* >(self) + 4;
  for (int i = 0; i < self->m_count; ++i) {
    SByteBuf356* buf = reinterpret_cast< SByteBuf356* >(p);
    if (buf != nullptr) {
      // The inner loop is MWCC's expansion of a byte clear: `srwi r0,r4,3` for the
      // 8-bytes-per-trip count, then a one-byte remainder counted by `subf r0,r3,r7`, and
      // `li r3,0` seeds the byte offset. It stores nothing, so what is written here is a loop
      // whose body is a dead counter - a hand-written `*p++ = 0` compiles to a plain
      // 1-byte-per-trip loop (measured 40.23%), `memset` emits real stores (4.58%), and a
      // *descending* dead counter (49.10%) does not unroll. This ascending one is the only
      // spelling measured that unrolls by 8.
      int written = 0;
      for (int j = 0; j < buf->m_length; ++j) {
        written += 8;
      }
    }
    p += sizeof(SByteBuf356);
  }
}

extern "C" SByteBuf356List* fn_800437BC(SByteBuf356List* self, short flag) {
  if (self != nullptr) {
    fn_8004380C(self);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// fn_800391B4 (0x2FB4, 48 bytes) is the fourth such forwarder and it now lands with its callee:
// 100.00%, and its `mr r3,r31` before the `blr` is the return-value copy, so it returns `this`.
// fn_800391E4 (0x2FE4, 96 bytes) is a copy-assign over a counted array of **16-byte** elements -
// not rstl::vector<float>, whose element is 4 bytes: retail computes `count << 4` for the end
// pointer and copies four `lfs`/`stfs` pairs per trip, with no remainder loop and no `bdnz`, so
// it is a plain pointer-bounded loop over 16-byte elements. The self-assignment guard
// (`cmplw r3,r4 ; beqlr`) is the whole of the early out, and the count is re-read from the source
// for the trailing store rather than reused, which is what fixes the order of the two loads.
//
// fn_800391E4 is at 94.38%: the loop body is instruction-for-instruction retail's, and the whole
// residue is the prologue. Retail builds the end pointer from the *raw* source pointer
// (`add r7,r4,r0 ; addi r7,r7,4`, r7 = `(char*)other + count*16 + 4`) and allocates the two data
// pointers as r6 then r5; mwcceppc here strength-reduces `other->m_items` into r5 first and puts
// the end pointer in r0, so the loop's `cmplw` operand differs. Fourteen spellings were measured
// (see the goal notes) and none moved it: the pointer-bounded form is what removes the 16-float
// unroll an indexed `for (i = 0; i < n; ++i)` produces, and every pointer-bounded spelling then
// lands on the same two prologue bytes.
//
// The array is declared with one element because retail's extent is the count; nothing here reads
// past it.
struct SF16 {
  float x, y, z, w;
};
struct SF16List {
  int m_count;
  SF16 m_items[1];
};

extern "C" void fn_800391E4(SF16List* self, const SF16List* other) {
  if (self == other) {
    return;
  }
  int count = other->m_count;
  const SF16* src = other->m_items;
  const SF16* end = src + count;
  SF16* dst = self->m_items;
  while (src != end) {
    *dst = *src;
    ++dst;
    ++src;
  }
  self->m_count = other->m_count;
}

extern "C" SF16List* fn_800391B4(SF16List* self, const SF16List* other) {
  fn_800391E4(self, other);
  return self;
}

void TouchPlayerActor(CEntity& ent, CStateManager& mgr);

struct queryOutput {
  int* unk0;
  int unk4;
};

void fn_80041518(queryOutput&, MapWorldInfoAreas& mAllocatedObjectIndices, ushort ourIndex);

// mGraveyard (0x1608) is an rstl::list< rstl::reserved_vector<CEntity*, 32> >, and the
// two template members retail left out of line are the two functions below:
//
//   fn_8003C0C4 == list::create_node(node* prev, node* next, const T&)   (4 args:
//                  r3 = the list itself, which create_node never reads - that is the
//                  signature of a non-static member, and it is how the call in
//                  fn_8003C054 is register-for-register the same)
//   fn_8003C054 == list::do_insert_before(node* n, const T&)
//
// <rstl/list.hpp> spells both of them out inline, so writing them through the template
// inlines them into fn_8003BF84 and emits nothing for objdiff to pair. They are written
// here over the same layout instead. The list object and its node are modelled locally
// for the same reason: a name objdiff can pair is what earns the match.
typedef rstl::reserved_vector< CEntity*, 32 > GraveyardBucket;
// The same 132 bytes as GraveyardBucket, named so the node's storage is legible and so
// the layout is asserted rather than assumed. The element itself is the real
// reserved_vector: its copy constructor is what emits MWCC's unrolled block move, and
// a hand-written loop or a memcpy call here does not reproduce it.
struct GraveyardBucketView {
  uint x0_count;
  CEntity* x4_data[32];
};
struct GraveyardNode {
  GraveyardNode* x0_prev;
  GraveyardNode* x4_next;
  GraveyardBucketView x8_item;
  GraveyardNode* get_prev() const { return x0_prev; }
  GraveyardNode* get_next() const { return x4_next; }
  void set_prev(GraveyardNode* p) { x0_prev = p; }
  void set_next(GraveyardNode* n) { x4_next = n; }
  GraveyardBucket* get_value() { return reinterpret_cast< GraveyardBucket* >(&x8_item); }
};
struct GraveyardList {
  uint x0_allocator; //!< rstl::rmemory_allocator, empty
  GraveyardNode* x4_start;
  GraveyardNode* x8_end;
  GraveyardNode* xc_empty_prev;
  GraveyardNode* x10_empty_next;
  uint x14_count;
};
CHECK_SIZEOF(GraveyardBucketView, 0x84) // == sizeof(rstl::reserved_vector<CEntity*, 32>)
CHECK_SIZEOF(GraveyardNode, 0x8C)       // 140 - the `li r3,140` in fn_8003C0C4
CHECK_SIZEOF(GraveyardList, 0x18)

extern "C" GraveyardNode* fn_8003C0C4(GraveyardList*, GraveyardNode* prev, GraveyardNode* next,
                                    GraveyardBucket* val) {
  // `allocate(0x8C)`, not rmemory_allocator::allocate(n, 1): the templated out-parameter
  // form gives `n` two definitions and MWCC spills r3 across the copy loop, which costs
  // 9 instructions out of 59. The single-definition form is byte-exact.
  GraveyardNode* n = reinterpret_cast< GraveyardNode* >(rstl::rmemory_allocator::allocate(0x8C));
  n->x0_prev = prev;
  n->x4_next = next;
  rstl::construct(n->get_value(), *val);
  return n;
}

extern "C" GraveyardNode* fn_8003C054(GraveyardList* l, GraveyardNode* n, GraveyardBucket* val) {
  GraveyardNode* const nn = fn_8003C0C4(l, n->get_prev(), n, val);
  if (n == l->x4_start) {
    l->x4_start = nn;
  }
  nn->get_prev()->set_next(nn);
  nn->get_next()->set_prev(nn);
  ++l->x14_count;
  return nn;
}

extern "C" void fn_8003C02C(rstl::list< rstl::reserved_vector< CEntity*, 32 > >& v, void* value) {
  fn_8003C054(reinterpret_cast< GraveyardList* >(&v),
              reinterpret_cast< GraveyardNode* >(v.end().get_node()),
              reinterpret_cast< GraveyardBucket* >(value));
}

// The script-ID map is an rstl red-black tree. Keep the tree view local until
// its lower/upper-bound template instantiations can be named in rstl itself.
struct ScriptIdNode {
  ScriptIdNode* left;
  ScriptIdNode* right;
  ScriptIdNode* parent;
  int color;
  CStateManager::TIdList::value_type value;
};
struct ScriptIdMapView {
  char header[8];
  ScriptIdNode* leftmost;
  ScriptIdNode* rightmost;
  ScriptIdNode* root;
};
CHECK_SIZEOF(ScriptIdNode, 0x18)
CHECK_SIZEOF(ScriptIdMapView, 0x14)

extern "C" ScriptIdNode* fn_8003C2D8(const CStateManager::TIdList& ids,
                                      const TEditorId& eid) {
  ScriptIdNode* cur = reinterpret_cast< const ScriptIdMapView& >(ids).root;
  ScriptIdNode* found = nullptr;
  while (cur) {
    if (!(cur->value.first < eid)) {
      found = cur;
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  bool noResult = false;
  if (!found || eid < found->value.first) {
    noResult = true;
  }
  if (noResult) {
    return nullptr;
  }
  return found;
}

template < typename T >
inline T* ScriptIdNodeCast(ScriptIdNode* node, T*) {
  return reinterpret_cast< T* >(node);
}

extern "C" CStateManager::TIdList::const_iterator fn_8003C420(
    const CStateManager::TIdList& ids, const TEditorId& eid) {
  ScriptIdNode* found = nullptr;
  ScriptIdNode* cur = reinterpret_cast< const ScriptIdMapView& >(ids).root;
  while (cur) {
    if (!(cur->value.first < eid)) {
      found = cur;
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  CStateManager::TIdList::const_iterator it = ids.end();
  it.mNode = ScriptIdNodeCast(found, it.mNode);
  return it;
}

extern "C" CStateManager::TIdList::const_iterator fn_8003C46C(
    const CStateManager::TIdList& ids, const TEditorId& eid) {
  ScriptIdNode* found = nullptr;
  ScriptIdNode* cur = reinterpret_cast< const ScriptIdMapView& >(ids).root;
  while (cur) {
    if (eid < cur->value.first) {
      found = cur;
      cur = cur->left;
    } else {
      cur = cur->right;
    }
  }
  CStateManager::TIdList::const_iterator it = ids.end();
  it.mNode = ScriptIdNodeCast(found, it.mNode);
  return it;
}

extern "C" CStateManager::TIdList::const_iterator fn_8003C28C(
    const CStateManager::TIdList& ids, const TEditorId& eid) {
  CStateManager::TIdList::const_iterator it = ids.end();
  it.mNode = ScriptIdNodeCast(fn_8003C2D8(ids, eid), it.mNode);
  return CStateManager::TIdList::const_iterator(it);
}

TUniqueId CStateManager::GetIdForScript(TEditorId eid) const {
  TIdList::const_iterator it = fn_8003C28C(mScriptIdMap, eid);
  if (it != mScriptIdMap.end()) {
    return it->second;
  }
  return kInvalidUniqueId;
}

extern "C" CStateManager::TIdListResult fn_8003C3A8(const CStateManager::TIdList& ids,
                                                      const TEditorId& eid) {
  // Direct return, not a named local that is then copied out of. Retail interleaves each
  // store with its own load (`lwz r0,0x10(r1) ; stw r0,0(r29) ; ...`); a copy of a copy makes
  // MWCC hoist all four words into r3/r4/r5/r0 before storing any. 78.70% -> 100.00%.
  return CStateManager::TIdListResult(fn_8003C420(ids, eid), fn_8003C46C(ids, eid));
}

CStateManager::TIdListResult CStateManager::GetIdListForScript(TEditorId eid) const {
  const TIdListResult result = fn_8003C3A8(mScriptIdMap, eid);
  return result;
}

CStateManager::CStateManager(const rstl::ncrc_ptr< CScriptMailbox >&,
                             const rstl::ncrc_ptr< CMapWorldInfo >&,
                             const rstl::ncrc_ptr< CPlayerState >&,
                             const rstl::ncrc_ptr< CWorldTransManager >&)
: mNextFreeIndex(0)
, mBossId(kInvalidUniqueId)
, mSpecialFunctionId(kInvalidUniqueId)
, mPlayerActorHead(kInvalidUniqueId)
, mPlanes()
, mPendingDockArea(kInvalidAreaId)
, mPendingDock(0)
, mShowSoftTransition(true) {}

CStateManager::~CStateManager() {}

TUniqueId CStateManager::AllocateUniqueId() {

  const ushort lastIndex = mNextFreeIndex;
  ushort ourIndex;
  queryOutput query;
  do {
    ourIndex = mNextFreeIndex;
    mNextFreeIndex = (ourIndex + 1) % 1024;
    if (mNextFreeIndex == lastIndex) {
      rs_debugger_printf("Object list full!");
    }
    fn_80041518(query, mAllocatedObjectIndices, ourIndex);
  } while ((query.unk4 & *query.unk0) != 0);

  mObjectIndexArray[ourIndex] = (mObjectIndexArray[ourIndex] + 1) & 0x3f;
  if (TUniqueId(mObjectIndexArray[ourIndex], ourIndex) == kInvalidUniqueId) {
    mObjectIndexArray[ourIndex] = 0;
  }

  fn_80041518(query, mAllocatedObjectIndices, ourIndex);
  *query.unk0 = *query.unk0 | query.unk4;

  return TUniqueId(mObjectIndexArray[ourIndex], ourIndex);
}

const CEntity* CStateManager::GetObjectById(TUniqueId uid) const {
  return GetObjectListById(kOL_All).GetObjectById(uid);
}

void CStateManager::SetIsDarkWorld(bool b) {
  mIsDarkWorld = b;
  gpGameState->SetIsDarkWorld(mIsDarkWorld);
}

bool CStateManager::ApplyLocalDamage(const CVector3f& pos, const CVector3f& dir, CActor& damagee,
                                     float damage, const TUniqueId& uid1, const TUniqueId& uid2,
                                     const CDamageInfo& damageInfo, int unkParam) {
  CHealthInfo* healthInfo = damagee.HealthInfo();
  if (!healthInfo || damage < 0.0f) {
    return false;
  }

  float hp = healthInfo->GetHP();
  if (hp <= 0.0f) {
    fn_8003dd88(damagee, uid1, damageInfo, false, unkParam);
    return true;
  }

  CPlayer* player = TCastToPtr< CPlayer >(damagee);

  if (player && player->Get_x12f8() != 0) {
    if (player->Get_x12f8() != 3) {
      return false;
    }
    player->fn_8000d3ac(pos, *this);
    if (!player->fn_8000d40c(dir, *this)) {
      return false;
    }
  }
  TUniqueId playerId = player ? player->GetUniqueId() : kInvalidUniqueId;
  if (player) {
    int playerIndex = MaskUIdNumPlayers(playerId);
    CPlayerState& playerState = *PlayerState(playerIndex);

    if (GetCameraManager(playerIndex)->IsInCinematicCamera()) {
      return false;
    }

    if (gpGameState->GetHardModeEnabled()) {
      switch ((EWeaponType)damageInfo.GetWeaponMode1()) {
      case kWT_Power:
      case kWT_Dark:
      case kWT_Light:
      case kWT_Annihilator:
      case kWT_Bomb:
      case kWT_PowerBomb:
      case kWT_Missile:
      case kWT_BoostBall:
      case kWT_CannonBall:
      case kWT_ScrewAttack:
      case kWT_AI:
      case kWT_PoisonWater1:
      case kWT_PoisonWater2:
      case kWT_Lava:
      case kWT_Heat:
      case kWT_Unused1:
      case kWT_AreaDark:
        damage *= gpGameState->GetHardModeDamageMultiplier();
        break;
      }
    }

    float damageReduction = 0.0f;

    if (playerState.HasPowerUp(CPlayerState::kIT_VariaSuit)) {
      damageReduction = player->GetTweakPlayer()->GetVariaSuitDamageReduction();
    }
    if (playerState.HasPowerUp(CPlayerState::kIT_DarkSuit)) {
      float reduction = player->GetTweakPlayer()->GetDarkSuitDamageReduction();
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.HasPowerUp(CPlayerState::kIT_LightSuit)) {
      float reduction = player->GetTweakPlayer()->GetLightSuitDamageReduction();
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.GetItemAmount(CPlayerState::kIT_AbsorbAttack, true) != 0) {
      float reduction = 1.5f;
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.GetItemAmount(CPlayerState::kIT_LightShield, true) != 0) {
      // TODO: flag
      float reduction = 0.75f;
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    if (playerState.GetItemAmount(CPlayerState::kIT_DarkShield, true) != 0) {
      // TODO: flag
      float reduction = 0.75f;
      if (reduction > damageReduction) {
        damageReduction = reduction;
      }
    }
    hp = playerState.CalculateHealth();
    damage = -(damageReduction * damage - damage);
  }
}

// Retail: `lis r4,31 ; li r0,0 ; addi r4,r4,-31616 ; stw r4,0x24dc(r3) ; stw r0,0x15f8(r3) ;
// stw r0,0x15fc(r3) ; stw r0,0x1600(r3)`. The constant is 0x1E8480 = 2000000, and the three
// cleared slots are the cached mCurrentRenderPlayer / mPlayerState / mCameraManager pointers.
// Retail's symbol is unmangled (`nm` prints `fn_8003B21C`, not `fn_8003B21C__13CStateManagerFv`),
// so it is a free function taking the manager, like `fn_8003AD74` above - declaring it as a
// member emits the bytes correctly but under the wrong symbol and objdiff never pairs the two.
extern "C" void fn_8003B21C(CStateManager* mgr) {
  mgr->mCurrentRenderPlayerIndex = 2000000;
  mgr->mCurrentRenderPlayer = nullptr;
  mgr->mPlayerState = nullptr;
  mgr->mCameraManager = nullptr;
}

void CStateManager::fn_8003BF84(CEntity* ent) {
  // Clear Graveyard? Retail hands fn_8003C02C the address of a 4-byte stack slot holding a
  // zero, and it passes a DIFFERENT slot in each branch - `stw r0,0x8c(r1)` in the empty()
  // branch, `stw r0,0x8(r1)` in the size() == 32 branch. Declared at function scope we got
  // one slot reused by both and a `stwu r1,-160(r1)` frame, 85.50%; declared inside each `if`
  // body we get both slots and `stwu r1,-288(r1)`, 100.00%. The two objects' 44 instructions
  // already agreed one-for-one; the frame was the whole percentage.
  if (mGraveyard.empty()) {
    GraveyardBucket fresh;
    fn_8003C02C(mGraveyard, &fresh);
  } else if ((--mGraveyard.end())->size() == 32) {
    GraveyardBucket fresh;
    fn_8003C02C(mGraveyard, &fresh);
  }
  (--mGraveyard.end())->push_back(ent);
}

void CStateManager::fn_8003BE54() {
  while (!mScriptMsgs.empty()) {
    CScriptMsg msg = mScriptMsgs.fn_8019E6BC();
    CEntity* ent = GetObjectByIdFromListAll(msg.GetId());
    if (ent) {
      bool flag = ent->GetActive();
      ent->AcceptScriptMsg(*this, msg);
      if (flag != ent->GetActive()) {
        if (CActor* actor = TCastToPtr< CActor >(ent)) {
          UpdateActorInSortedLists(actor);
        }
      }
      if (msg.GetMessage() == kSM_XDelete) {
        fn_8003BF84(ent);
        fn_800412EC(ent->GetUniqueId());
      }
    }
  }
}

void CStateManager::DeferStateTransition(EStateManagerTransition t) {
  if (!IsMultiplayer()) {
    if (t == kSMT_InGame) {
      if (mDeferredTransition != kSMT_InGame) {
        mWorld->SetLoadPauseState(false);
        mDeferredTransition = kSMT_InGame;
      }
    } else if (mDeferredTransition == kSMT_InGame) {
      mWorld->SetLoadPauseState(true);
      mDeferredTransition = t;
      if (mDeferredTransition == kSMT_SaveGame) {
        mSaveGameScreen =
            new CSaveGameScreen(kSC_InGame, gpGameState->GetCardSerial());
      }
    }
  }
}

void CStateManager::ShowPausedHUDMemo(CAssetId strg, float time) {
  mHudMessageTime = time;
  mPauseHudMessage = strg;
  DeferStateTransition(kSMT_MessageScreen);
}

void CStateManager::SendScriptMsg(const CScriptMsg& msg) {
  mScriptMsgs.Append(msg);
  int v = mScriptMsgs.fn_8019E69C();
  if (0x80 < v && !mDispatchingScriptMessages) {
    mDispatchingScriptMessages = true;
    fn_8003BE54();
    mDispatchingScriptMessages = false;
  }
}

bool CStateManager::IsMultiplayer() const {
  // The live CGameMode's type (vtable word 17), not CGameState's deserialised field of the same
  // name. The `result` spelling is what gives retail's shared epilogue for both compares.
  int v = gpGameState->GetGameMode().GetGameModeType();
  bool result = false;
  if (v != 'SNGL' && v != 'FRND') {
    result = true;
  }
  return result;
}

uint CStateManager::MaskUIdNumPlayers(TUniqueId id) const {
  uint index = id.Value();
  return index < mNumPlayers ? index : 0;
}

CEntity* CStateManager::ObjectById(TUniqueId uid) {
  return ObjectListById(kOL_All).GetObjectById(uid);
}

void CStateManager::AddObject(CEntity* entity) {
  if (entity) {
    AddObject(*entity);
  }
}

void CStateManager::DeleteObjectRequest(TUniqueId id) {
  SendScriptMsg(id, kInvalidUniqueId, kSM_XDelete, kInvalidUniqueId);
}

void CStateManager::SendScriptMsg(TUniqueId dest, TUniqueId src, EScriptObjectMessage msg,
                                  TUniqueId other) {
  // CScriptMsg's `m_id` is the destination (DeliverScriptMsg resolves ObjectById(GetId())).
  SendScriptMsg(CScriptMsg(src, dest, other, msg, kSS_InvalidState));
}

void CStateManager::SendScriptMsg(CEntity* dest, TUniqueId src, EScriptObjectMessage msg,
                                  TUniqueId other) {
  if (dest) {
    SendScriptMsg(CScriptMsg(src, dest->GetUniqueId(), other, msg, kSS_InvalidState));
  }
}

void CStateManager::SetupParticleHook(const CActor& actor) const {
  mActorModelParticles->SetupHook(actor.GetUniqueId());
}

void CStateManager::BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CVector3f& pos, const CVector3f& dir,
                                  float mag, const CMaterialFilter& filter,
                                  const CActor* actor) const {
  mSortedListManager->BuildNearList(out, pos, dir, mag, filter, actor);
}

void CStateManager::BuildColliderList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CActor& actor,
                                      const CAABox& aabb) const {
  mSortedListManager->BuildNearList(out, actor, aabb);
}

void CStateManager::BuildNearList(rstl::reserved_vector< TUniqueId, 1024 >& out, const CAABox& aabb,
                                  const CMaterialFilter& filter, const CActor* actor) const {
  mSortedListManager->BuildNearList(out, aabb, filter, actor);
}

// Retail 0x800422D4. The `*=` is load-bearing: retail scales the three components of the delta in
// the same 8/12/16(r1) slots it read them from, so there is one CVector3f on the frame. `operator*`
// returns a second temporary at 0x14, pushes the frame to -2112 against retail's -2096, and scores
// 99.16%. The `1.f / len` is the CVector3f::AsNormalized idiom (`lfs f0,-31632(r2)` then `fdivs`);
// `len` itself stays live in f1 because it is BuildNearList's `mag` argument.
bool CStateManager::RayCollideWorld(const CVector3f& start, const CVector3f& end,
                                    const CMaterialFilter& filter, const CActor* damagee) {
  rstl::reserved_vector< TUniqueId, 1024 > nearList;
  CVector3f dir = end - start;
  float len = dir.Magnitude();
  BuildNearList(nearList, start, dir *= (1.f / len), len, filter, damagee);
  return RayCollideWorldInternal(start, end, filter, nearList, damagee);
}

bool CStateManager::RayCollideWorld(const CVector3f& start, const CVector3f& end,
                                    const rstl::reserved_vector< TUniqueId, 1024 >& nearList, const CMaterialFilter& filter,
                                    const CActor* damagee) const {
  return RayCollideWorldInternal(start, end, filter, nearList, damagee);
}

CRayCastResult CStateManager::RayStaticIntersection(const CVector3f& pos, const CVector3f& dir,
                                                    float length,
                                                    const CMaterialFilter& filter) const {
  return CGameCollision::RayStaticIntersection(*this, pos, dir, length, filter);
}

CRayCastResult CStateManager::RayWorldIntersection(TUniqueId& idOut, const CVector3f& pos,
                                                   const CVector3f& dir, float length,
                                                   const CMaterialFilter& filter,
                                                   const rstl::reserved_vector< TUniqueId, 1024 >& list) const {
  return CGameCollision::RayWorldIntersection(*this, idOut, pos, dir, length, filter, list);
}

void CStateManager::QueueMessage(int frameCount, CAssetId msg, float f1) {
  mPausedHudMemoFrameCount = frameCount;
  mPausedHudMemoAssetId = msg;
  x2468 = f1;
}

void CStateManager::SetBossParams(TUniqueId bossId, float maxEnergy, uint stringIdx) {
  mBossId = bossId;
  mBossHealth = maxEnergy;
  mBossLanguageTableIndex = stringIdx;
}

void CStateManager::fn_80039244() {}

void CStateManager::fn_8003FF1C() {}

void CStateManager::fn_8003FF20() {}

bool CStateManager::fn_8003FF24() {
  CFrameDelayedKiller::StallAndFlushAllAllocations();
  fn_8030184C();
  CARAMToken::UpdateAllDMAs();
  return true;
}

void CStateManager::fn_8003FF50() { fn_8003FF24(); }

void CStateManager::fn_8003FF70(int, int) {}

void CStateManager::fn_8003FF74(int value) {
  mRenderFrameIndex = value;
  lbl_80419A18 = mRenderFrameIndex;
  lbl_80419A10 = mRenderFrameIndex;
  fn_8003FF70(2, 0x180000);
}

// Retail 0x80041B08. The bit tests read the object byte exactly as retail does:
// `lbz r0,336(r4)` + `rlwinm. r0,r0,28,31,31` is bit 3 of CActor+0x150 = mNotInSortedLists;
// `lbz r0,32(r31)` + `rlwinm. r0,r0,25,31,31` is bit 6 of CEntity+0x20 = m_scriptingBlocked.
// fn_80041CCC writes a CAABox plus a validity byte at +0x18; the byte is copied to 60(r1)
// (36 + 0x18) unconditionally and the 24-byte box only when it is set.
void CStateManager::UpdateActorInSortedLists(CActor* actor) {
  if (!actor->GetTransformDirty()) {
    return;
  }
  actor->SetTransformDirty(false);
  if (!actor->GetUseInSortedLists()) {
    return;
  }

  SFoundBounds bounds;
  SFoundBounds found;
  fn_80041CCC(&found, this, actor);
  bounds.valid = found.valid;
  if (found.valid) {
    bounds.Box() = found.Box();
  }

  // The shape here is not stylistic. Retail loads the flag into r4 right after the ActorInLists
  // call and keeps it live for the whole tail (`cmplwi r4,0` twice, no second `lbz`), which only
  // happens if the flag is a plain local rather than `bounds.valid` read back off the frame.
  // The `inLists || valid` guard is likewise load-bearing: retail's `clrlwi. / lbz / bne /
  // cmplwi / beq` is a short-circuit `||`, and a bare `if (inLists)` collapses it.
  const bool inLists = mSortedListManager->ActorInLists(actor);
  const uint valid = bounds.valid;
  if (inLists || valid) {
    if (inLists) {
      if (!actor->GetActive() || !valid) {
        mSortedListManager->Remove(actor);
      } else {
        mSortedListManager->Move(actor, bounds.Box());
      }
    } else if (actor->GetActive() && valid) {
      mSortedListManager->Insert(actor, bounds.Box());
    }
  }
}

// Retail 0x800419C8 is a bare `blr`; symbols.txt names it AreaUnloaded(TAreaId). It used to be
// defined here as `fn_800419C8()`, which objdiff cannot pair with the retail symbol, so the unit
// showed an unwritten `AreaUnloaded` and an unpaired `fn_800419C8` instead of one match.
void CStateManager::AreaUnloaded(TAreaId area) {}

// Retail 0x80041A60. `fn_800B89FC` walks CMapWorldInfo's four parallel words, matching the
// high half of each against the area id and dispatching SendScriptMsgs on the hits; it is a real
// retail function (0x800B89FC, 0x15C bytes) that nothing in the port writes, so declaring it here
// costs one undefined symbol. It reads the 0x167C rc_ptr, which this header puts at mMapWorldInfo.
void CStateManager::AreaLoaded(TAreaId area) {
  fn_800B89FC(mMapWorldInfo.GetPtr(), area, this);
  mEnvFxManager->AreaLoaded();
}

bool CStateManager::fn_800421B4() const { return mWorld != nullptr; }

int CStateManager::fn_80036B6C() const {
  if (mNumPlayers == 1u) {
    return 0;
  }
  int ret = 2;
  if (mNumPlayers == 2u) {
    ret = 1;
  }
  return ret;
}

int CStateManager::GetWeaponIdCount(TUniqueId id, EWeaponType type) {
  return mWeaponMgr->GetNumActive(id, type);
}

void CStateManager::RemoveWeaponId(TUniqueId id, EWeaponType type) {
  mWeaponMgr->fn_800B321C(id, type);
}

void CStateManager::AddWeaponId(TUniqueId id, EWeaponType type) {
  mWeaponMgr->fn_800B32E0(id, type);
}

void CStateManager::fn_8003A3C0(int& a, int& b, int type) const {
  int shift = 1;
  if (type == 3) {
    shift = 2;
  }
  a = 1 << shift;
  b = 0;
}

CScriptObjectLoaderHelper& CStateManager::fn_80036200() {
  return mStateManagerContainer->ScriptObjectLoaderHelper();
}

CScriptObjectLoaderHelper& CStateManager::ScriptObjectLoaderHelper() {
  return mStateManagerContainer->ScriptObjectLoaderHelper();
}

rstl::single_ptr< CPortalTransition >& CStateManager::fn_80036220() { return mPortalTransition; }

void CStateManager::SetPortalTransition(rstl::single_ptr< CPortalTransition >& ptr) { mPortalTransition = ptr; }

bool CStateManager::fn_80036284() {
  for (CGameArea::CConstChainIterator it = mWorld->GetChainHead(CWorld::kC_Alive);
       it != CWorld::GetAliveAreasEnd(); ++it) {
    if (it->HasPendingLayerLoads()) {
      return true;
    }
  }
  return false;
}

// Render flags derived from the active visor; defined outside this unit's split.
extern "C" uint lbl_80419A9C;
extern "C" uint lbl_80419AA0;

void CStateManager::fn_80036650() {
  CPlayerState::EPlayerVisor visor = mPlayerState->GetActiveVisor(*this);
  uint flagsA = 0;
  uint flagsB = 8;
  switch (visor) {
  case CPlayerState::kPV_Echo:
    flagsA |= 8;
    break;
  case CPlayerState::kPV_Combat:
  case CPlayerState::kPV_Scan:
    flagsB |= 1;
    break;
  case CPlayerState::kPV_Dark:
    flagsB |= 2;
    break;
  }
  uint flagsC = mIsDarkWorld ? flagsB | 4 : flagsB | 16;
  lbl_80419A9C = flagsA;
  lbl_80419AA0 = flagsC;
}

void CStateManager::fn_800362E0() {
  for (CGameArea::CChainIterator it = mWorld->ChainHead(CWorld::kC_Alive); it != CWorld::AliveAreasEnd(); ++it) {
    it->UpdateDynamicLayers(*this);
  }
}

bool CStateManager::fn_80037904(TUniqueId id) {
  CStateManagerContainer::TIdList& list = mStateManagerContainer->IdList13ED8();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_80037944(TUniqueId id) {
  CStateManagerContainer::TIdList& list = mStateManagerContainer->IdList13F88();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_80037984(TUniqueId id) {
  CStateManagerContainer::TIdList& list = mStateManagerContainer->IdList13F5C();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_800379C4(TUniqueId id) {
  CStateManagerContainer::TIdList& list = mStateManagerContainer->IdList13F30();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

bool CStateManager::fn_80037A04(TUniqueId id) {
  CStateManagerContainer::TIdList& list = mStateManagerContainer->IdList13F04();
  if (list.size() == 20) {
    return false;
  }
  list.push_back(id);
  return true;
}

void CStateManager::fn_8003EC0C() {
  // Retail's `FrameDone__6CModelFv` (0x803111A4) reached through its own
  // `CModel::FrameDone`, which is `src/Kyoto/Graphics/CModelPortStub.cpp` in the port - see
  // that file for why the host body is empty. This used to be called through the `fn_`
  // placeholder name, which no translation unit defines.
  CModel::FrameDone();
  gpSimplePool->Flush();
}

void CStateManager::MoveActors(float dt) {
  CObjectList* physicsList = mObjectLists[kOL_PhysicsActor].get();
  for (int i = physicsList->GetFirstObjectIndex(); i != -1;
       i = physicsList->GetNextObjectIndex(i)) {
    CPhysicsActor* actor = static_cast< CPhysicsActor* >((*physicsList)[i]);
    if (actor == nullptr || !actor->GetActive() || actor->GetMass() == 0.f ||
        (!actor->GetUpdateDuringCinematicSkip() && gpMain->GetMaxSpeed())) {
      continue;
    }

    if (!actor->GetUpdateWhileOccluded() && actor->GetCurrentAreaId() != kInvalidAreaId) {
      const CGameArea& area = mWorld->GetAreaAlways(actor->GetCurrentAreaId());
      const float occludedTime = area.IsLoaded() ? area.GetPostConstructed()->mOccludedTime : 0.f;
      if (occludedTime > 5.f) {
        continue;
      }
    }

    CPatterned* patterned = TCastToPtr< CPatterned >(actor);
    if (patterned != nullptr && !ShouldUpdatePatterned(*patterned)) {
      SendScriptMsg(patterned->GetUniqueId(), kInvalidUniqueId, kSM_SuspendedMove,
                    kInvalidUniqueId);
      continue;
    }

    if (TCastToPtr< CPlayer >(actor) == nullptr &&
        TCastToPtr< CScriptPlatform >(actor) == nullptr) {
      CGameCollision::Move(*this, *actor, dt, nullptr);
    }
  }
}

void CStateManager::ThinkEntity(float dt, CEntity& entity) { entity.Think(dt, *this); }

bool CStateManager::ShouldUpdatePatterned(const CPatterned& actor) {
  bool update = !mCinematicPause;
  if (update && actor.GetCurrentAreaId() != kInvalidAreaId) {
    const CGameArea& area = mWorld->GetAreaAlways(actor.GetCurrentAreaId());
    const float occludedTime = area.IsLoaded() ? area.GetPostConstructed()->mOccludedTime : 0.f;
    if (occludedTime > 5.f) {
      update = false;
    }
  }
  return update;
}

void CStateManager::Think(float dt) {
  if (!IsMultiplayer() && mPlayers[0]->GetDeathTime() > 0.f) {
    mPlayers[0]->DoThink(dt, *this);
    return;
  }

  CObjectList* allList = mObjectLists[kOL_All].get();
  if (mGameState == kGS_SoftPaused) {
    for (int i = allList->GetFirstObjectIndex(); i != -1; i = allList->GetNextObjectIndex(i)) {
      CScriptEffect* effect = TCastToPtr< CScriptEffect >((*allList)[i]);
      if (effect != nullptr) {
        effect->Think(dt, *this);
      }
    }
  } else {
    for (long i = allList->GetFirstObjectIndex(); i != -1; i = allList->GetNextObjectIndex(i)) {
      CEntity* entity = (*allList)[i];
      if (entity == nullptr || (!entity->GetUpdateDuringCinematicSkip() && gpMain->GetMaxSpeed())) {
        continue;
      }

      if (!entity->GetUpdateWhileOccluded() && entity->GetCurrentAreaId() != kInvalidAreaId) {
        const CGameArea& area = mWorld->GetAreaAlways(entity->GetCurrentAreaId());
        const float occludedTime = area.IsLoaded() ? area.GetPostConstructed()->mOccludedTime : 0.f;
        if (occludedTime > 5.f) {
          continue;
        }
      }

      CPatterned* patterned = TCastToPtr< CPatterned >((*allList)[i]);
      if (patterned != nullptr && !ShouldUpdatePatterned(*patterned)) {
        continue;
      }
      if (TCastToPtr< CGameCamera >(entity) == nullptr) {
        ThinkEntity(dt, *entity);
      }
    }
  }
}

uint CStateManager::ReturnFirstIfSingleElseSecond(uint single, uint multi) const {
  return IsMultiplayer() ? multi : single;
}

void CStateManager::AddDrawableActorPlane(const CActor& actor, const CPlane& plane,
                                          const CAABox& bounds) const {
  const_cast< CActor& >(actor).SetAddedToken(mObjectDrawToken + 1);
  gpRender->AddPlaneObject(&actor, bounds, plane, 0);
}

void CStateManager::AddDrawableActor(const CActor& actor, const CVector3f& pos,
                                     const CAABox& bounds) const {
  const_cast< CActor& >(actor).SetAddedToken(mObjectDrawToken + 1);
  gpRender->AddDrawable(&actor, pos, bounds, 0,
                        IRenderer::EDrawableSorting(actor.GetAlphaSorted()));
}

bool CStateManager::CanCreateProjectile(TUniqueId id, EWeaponType type, int max) const {
  return mWeaponMgr->GetNumActive(id, type) < max;
}

void CStateManager::DeliverScriptMsg(const CScriptMsg& msg) {
  CEntity* entity = ObjectById(msg.GetId());
  if (entity) {
    entity->AcceptScriptMsg(*this, msg);
  }
}

void CStateManager::fn_80037784() {
  mSaveGameScreen = rs_new CSaveGameScreen(kSC_FrontEnd, gpGameState->GetCardSerial());
}

void CStateManager::KillSaveGameInterface() {
  mUnkFlagA5 = mSaveGameScreen->GetIowRet() == 1;
  mSaveGameScreen = nullptr;
}

float CStateManager::fn_80038364() { return gpGameState->GetEscapeTime(); }

void CStateManager::fn_80038370(float value) {
  gpGameState->SetEscapeTime(value);
  mEscapeTotalTime = value;
}

// Retail's `TouchSky__6CWorldCFv` (0x8004F770) reached through its own `CWorld::TouchSky`,
// which is `src/MetroidPrime/CWorldTouchSky.cpp` in the port. It used to be declared here as
// the placeholder `extern "C" void fn_8004F770(CWorld*)` and called through it, which asked the
// linker for a `fn_` symbol that no translation unit defines; the named method is the same
// function and is defined.
void CStateManager::TouchSky() { mWorld->TouchSky(); }

float CStateManager::fn_80036F78(float value) {
  CPlayerState* playerState = mPlayerState;
  if (playerState->GetActiveVisor(*this) == CPlayerState::kPV_Scan) {
    return value * (1.f - playerState->GetVisorTransitionFactor());
  }
  return value;
}

void CStateManager::TouchPlayerActor() {
  // By reference, so the compare and the argument are ONE load. Retail is
  // `lhz r4,0x2452(r3) ; cmplw r4,r0 ; beq ; sth r4,0x8(r1)`. Re-reading the member after
  // the branch emitted a second `lhz r0,0x2452(r31)` and measured 85.48%; this measures
  // 100.00%.
  const TUniqueId& head = mPlayerActorHead;
  if (head != kInvalidUniqueId) {
    if (const CEntity* entity = GetObjectById(head)) {
      ::TouchPlayerActor(const_cast< CEntity& >(*entity), *this);
    }
  }
}

void CStateManager::fn_80039CCC(int pass) {
  if (x2944 > 0.f) {
    switch (pass) {
    case 0:
    case 2:
      gpRender->DrawDarkWorldCloud(x2944, x2938, x2948);
      break;
    case 1:
      break;
    }
  }
}

void CStateManager::fn_80039DDC(const TAreaId& area, int type, int mask, int targetMask) {
  switch (type) {
  case 1:
    break;
  case 2:
    gpRender->DrawSpecialGeometryAlpha(area.Value(), mask, targetMask);
    break;
  default:
    gpRender->DrawSpecialGeometry(area.Value(), mask, targetMask);
    break;
  }
}

TEditorId CStateManager::GetEditorIdForUniqueId(TUniqueId id) const {
  const CEntity* entity = GetObjectById(id);
  if (entity) {
    return entity->GetEditorId();
  }
  return kInvalidEditorId;
}
