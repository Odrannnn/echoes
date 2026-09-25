#include "MetroidPrime/Enemies/CPatterned.hpp"

#include "MetroidPrime/TGameTypes.hpp"

#include "Kyoto/Math/CVector3f.hpp"

// Two unnamed float constants in retail's shared .sdata2 pool, read by CPatterned::TakeDamage and
// CPatterned::VSlot69. Both are declared extern for the same reason CAi declares kCAiSplashDenom: a
// local definition adds bytes to this object's .sdata2, which shifts every address above it and
// breaks the modules. Their values were read out of the disc with objdump rather than guessed -
// 0x8041AAB8 holds 0.33f and 0x8041B758 holds 24.525f - but writing the literals here re-creates
// the pool entry, so the names are what the code must reference.
extern const float lbl_8041AAB8;
extern const float lbl_8041B758;

// The sub-object at 0x754, which vtable slot 73 hands back by reference. Its real type is not
// identified and nothing in this range calls it, so an opaque class is enough to get the address.
class CPatternedAnimEvent;

// CAi's last four vtable entries point at copies the linker kept in whichever translation units
// needed a vtable using them, and this range holds the IsListening copy (0x80073CAC). It is defined
// under CAi's name, which is why vtable slot 43 lands here, and it must be written as a CAi::
// member rather than as a CPatterned override - a CPatterned:: version mangles differently and the
// vtable entry would stop resolving.
bool CAi::IsListening() const { return false; }

CPatternedAnimEvent& CPatterned::VSlot73() {
  return *reinterpret_cast< CPatternedAnimEvent* >(reinterpret_cast< uchar* >(this) + 0x754);
}

float CPatterned::VSlot69() { return lbl_8041B758; }

// Vtable slot 68. Retail emits `rlwinm r3,r0,29,31,31`, which moves bit 2 of the byte at 0x34c
// into the top bit. Counting the constructor's own writes in the same file's doc header: the byte
// at 0x34c is the first of the eleven one-bit fields, and bit 2 is its fourth, x34c_28_. That is
// the field Metaree reads (fn_42_36C) and the one the constructor fills from kInvalidUniqueId, not
// from moveType - the "notFlyer" reading of it was a guess that the constructor disproves.
bool CPatterned::VSlot68() { return x34c_28_; }

TUniqueId CPatterned::VSlot67() { return kInvalidUniqueId; }

int CPatterned::VSlot57() { return 0; }
int CPatterned::VSlot56() { return 0; }
int CPatterned::VSlot55() { return 0; }

uchar CPatterned::VSlot50() { return x44c_color.GetAlphau8(); }

void CPatterned::TakeDamage(const CVector3f&, float) { x448_ = lbl_8041AAB8; }
