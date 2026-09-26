// Retail `GetVector__10CAxisAngleCFv` = `_ZNK10CAxisAngle9GetVectorEv`,
// .text 0x8001D2BC..0x8001D2C0, 0x4 = 4 bytes:
//
//     8001d2bc  blr
//
// One `blr` and no other instruction, which is what a function returning a *reference to a
// member* compiles to: the C++ reference and the `this` pointer are the same register, so
// `return mVector;` has nothing to do. That is only true for the reference form - returning
// `CVector3f` by value would need a 16-byte copy, which is a different function entirely.
//
// It sits between `CAxisAngle::GetAngle` (0x8001D2B4) and `CAxisAngle::Identity`
// (0x8001D2C0), inside dtk's `auto_03_8001D084_text`, so it gets a unit of its own: a unit
// may not claim two discontiguous ranges in one section.
//
// The header's `const CVector3f& GetVector() const` is what makes the body this short;
// `float GetAngle() const` immediately below returns a member by value and is 8 bytes.
#include "MetroidPrime/CAxisAngle.hpp"

const CVector3f& CAxisAngle::GetVector() const { return mVector; }
