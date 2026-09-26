// Retail `BuildInverted__11CQuaternionCFv` = `_ZNK11CQuaternion13BuildInvertedEv`,
// .text 0x80028E08..0x80028E38, 48 bytes:
//
//     lfs  f1,4(r4) / lfs f0,0(r4) / lfs f2,8(r4)      ; w, x, y   (r4 = this)
//     fneg f1,f1
//     stfs f0,0(r3)                                    ; out->w = w
//     lfs  f0,12(r4)                                    ; z
//     fneg f2,f2
//     stfs f1,4(r3)                                    ; out->x = -x
//     fneg f0,f0
//     stfs f2,8(r3)                                    ; out->y = -y
//     stfs f0,12(r3)                                   ; out->z = -z
//     blr
//
// `CQuaternion` is `{ float w; CVector3f imaginary; }` and the imaginary part is laid out at
// +0x04/+0x08/+0x0C, so this is `CQuaternion(w, -x, -y, -z)` - **not** `BuildEquivalent()`,
// which is `(-w, -x, -y, -z)` and negates all four. The scalar is copied unchanged, which is the
// whole difference between the two and the reason this is not the header's inline.
//
// All three negations happen before any store and the stores then run in member order, so the
// source is the four-argument constructor and mwcceppc's scheduler does the rest. Declared in
// `Kyoto/Math/CQuaternion.hpp` with no body until now; `CQuaternion.cpp` uses it (in `LookAt`)
// and could not, because nothing defined it - which is why
// `_ZNK11CQuaternion13BuildInvertedEv` is in the port's link gap list.
#include "Kyoto/Math/CQuaternion.hpp"

CQuaternion CQuaternion::BuildInverted() const {
  return CQuaternion(w, -imaginary.GetX(), -imaginary.GetY(), -imaginary.GetZ());
}
