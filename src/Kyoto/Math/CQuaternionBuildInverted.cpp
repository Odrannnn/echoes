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
// **The body moved to `src/MetroidPrime/CAnimData.cpp`** (just below its
// `GetLocatorTransform(CSegId, CCharAnimTime*)`), because upstream's `config/G2ME01/splits.txt`
// gives 0x80028E08..0x80028E38 to `MetroidPrime/CAnimData.cpp` and a range may only belong to
// one unit. The port build compiles both files, so keeping the definition here as well would be
// a duplicate; the object is left empty on purpose and this note is the whole translation unit.
// The member stays declared in `Kyoto/Math/CQuaternion.hpp`.
