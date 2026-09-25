// CScriptCoinTouchBounds.cpp - CScriptCoin's own GetTouchBounds override.
//
// Retail's vtable lbl_58_data_40 puts fn_58_1B24 in the CActor::GetTouchBounds slot (vtable
// index 16, right after the two GetDamageVulnerability overloads). The signature is
// `rstl::optional_object<CAABox> CActor::GetTouchBounds() const`, so the ABI passes a hidden
// return pointer in r3 and `this` in r4 - which is why retail can reuse r4 for the call to
// CPhysicsActor::GetBoundingBox. That call is also hidden-pointer, so r3 is the 0x18-byte scratch
// at sp+8 and r4 is still `this`.
//
// The gate is a bit at 0x2f9 of the object, past the end of CPhysicsActor (CHECK_SIZEOF 0x2d0), so
// it belongs to CScriptCoin itself and the field is not identified.

#include "types.h"

#include "Kyoto/Math/CAABox.hpp"
#include "rstl/optional_object.hpp"

// CPhysicsActor::GetBoundingBox() const, from the DOL.
extern "C" CAABox GetBoundingBox__13CPhysicsActorCFv(const void* self);

extern "C" rstl::optional_object< CAABox > fn_58_1B24(const void* self) {
  if (*reinterpret_cast< const uchar* >(static_cast< const char* >(self) + 0x2f9) & 1) {
    return GetBoundingBox__13CPhysicsActorCFv(self);
  }
  return rstl::optional_object_null();
}
