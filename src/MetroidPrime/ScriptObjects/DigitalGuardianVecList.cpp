// DigitalGuardianVecList.cpp - a carve of DigitalGuardian's .text 0x0001AB24..0x0001AB50:
// `fn_14_1AB24` and `fn_14_1AB30`, a flag-clearing accessor and the module's copy of
// `CVector3f::Up()`.
//
// The class is unnamed in retail, exactly as in `DigitalGuardianAccessors.cpp`, so the object is
// reached as a `void*` and the offset is stated literally.  That keeps the unit layout-immune: it
// reads one member through an offset, so nothing in it is evidence about a class declaration that
// does not exist.  Both names are retail's own `fn_14_<off>` strings, so
// `config/G2ME01/rels/DigitalGuardian/symbols.txt` needs no rename.
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim.  Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/DigitalGuardianVecList.cpp`.

#include "types.h"

#ifdef __MWERKS__
// `CVector3f` only for the by-value return of `fn_14_1AB30`; the host build has the same header
// (and `src/Kyoto/Math/CVector3f.cpp` is already in `files.cmake`), so nothing here is MWCC-only,
// but the member-function shape the twin uses is spelled out under the guard to keep the two
// builds from disagreeing about it.
#include "Kyoto/Math/CVector3f.hpp"
#endif

extern "C" {

// .text 0x0001AB30, 0x20 bytes. Returns `CVector3f::Up()` by value through the hidden return
// pointer in r3, with `this` in r4 unused.
//
// The twin is `CPatterned::GetIngSnatchingNormal(float) const`
// (`src/MetroidPrime/Enemies/CPatterned.cpp:462`), `main/MetroidPrime/Enemies/CPatterned` at
// 100.00%.  `CVector3f::Up()` is `inline const CUnitVector3f&` off `CVector3f::sUpVector`
// (`include/Kyoto/Math/CUnitVector3f.hpp:39`), and copying a 12-byte aggregate out of a reference
// is the `lfsu` on the first word plus two `lfs`/`stfs` pairs - the `lfsu`, not `lfs`, is what
// says the three reads are consecutive, and it only appears because the whole value is copied.
#ifdef __MWERKS__
CVector3f fn_14_1AB30(const void* self) { return CVector3f::Up(); }
#endif

// .text 0x0001AB24, 0x0c bytes. Resets the one-byte flag at +0xC and returns `this`.
//
// The twin is `CUnknownVec3List::ClearFlag()`
// (`src/MetroidPrime/TypesMatch.cpp:975`), `main/MetroidPrime/TypesMatch` at 100.00%: the same
// store of 0 to the same one-byte member at +0xC, with `this` left in r3 for the return.  It is
// also what `rstl::optional_object_null()` compiles to, which is why
// `include/MetroidPrime/Weapons/CEnergyProjectile.hpp:44`'s `GetImpactParticle` - the twin
// `twin_scan` names for this function - is the same three instructions.
//
// No relocation at all: `powerpc-eabi-nm -u` on this object prints nothing but `fn_14_1AB30`'s
// `sUpVector__9CVector3f`, which the DOL exports and the port already links
// (`src/Kyoto/Math/CVector3f.cpp`), so listing the file in `files.cmake` moves the port's
// undefined count by nothing.
void* fn_14_1AB24(void* self) {
  *reinterpret_cast< unsigned char* >(static_cast< char* >(self) + 0xC) = 0;
  return self;
}

} // extern "C"