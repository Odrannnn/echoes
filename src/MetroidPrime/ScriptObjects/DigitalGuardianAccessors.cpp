// DigitalGuardianAccessors.cpp - a carve of DigitalGuardian's .text 0x000000..0x00010C: `fn_14_0`,
// `fn_14_8` and `fn_14_10`, the module's GetBoundingBox wrapper and the two predicates above it,
// plus the 13 short accessors below it.
//
// The class is unnamed in retail - the only name the module's loader has is
// `LoadDigitalGuardian__FR13CStateManagerR12CInputStreamR11CEntityInfo` in the DOL, which is named after
// the module and not the type - so, as in `CScriptWallCrawler.cpp`, `CScriptMetaree.cpp`,
// `CScriptPuffer.cpp` and `CScriptCoinTouchBounds.cpp`, the object is reached as a `void*` and
// the offsets are stated literally.  That keeps the unit layout-immune: it reads members
// through offsets, so nothing in it is evidence about a class declaration that does not exist.
//
// This block is the accessor set the REL loader generator emits at the head of a
// scripted-actor module, and it is byte-identical across modules.  `WallCrawler` .text
// 0x00..0x9C is the reference, and `MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp` already
// reproduces it at 100% as a `Matching` unit.  The three globals the relocations name are the
// same in every module and all three live in the DOL, which is why one body serves all of
// them; the relocations for this range are `skDamageHitTime__10CPatterned`, `kInvalidUniqueId` and `lbl_8041B758`.
//
// The dtk `fn_<id>_<off>` names are kept, so `config/G2ME01/rels/DigitalGuardian/symbols.txt` needs no
// rename: the names the retail module defines and the names this object exports are the same
// strings.  
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim.

#include "types.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// `GetBoundingBox__13CPhysicsActorCFv` is the DOL's, called by its retail name and not defined
// here, so nothing in this module is left to dtk below 0x10C.
//
// The module *head* files - `CMysteryFlyerRel.cpp`, `CPillBugRel.cpp` and the rest - are
// deliberately absent from `files.cmake`, because a head body would make the host port link the
// module's own functions, which it cannot, and the port's link gap would grow. This file is
// listed, and so are the other 17 `ScriptObjects/*Accessors.cpp` units (18 in all, `files.cmake`
// lines 896-913): they were safe because they read raw offsets and DOL globals and nothing else.
// `fn_14_10` breaks that - it makes the host link a host-mangled `CPhysicsActor::GetBoundingBox`.
// The port reads `DigitalGuardian.rel` off the disc through `platform/rel.cpp` and never calls
// into the module, so nothing is lost, and the guard is the same arrangement
// `CScriptWallCrawler.cpp` uses for its `RELMain`/`RELExit`, and the same one
// `RipperAccessors.cpp` uses for the same wrapper. The MWCC branch is the retail source token for
// token, so the matching build cannot see it. Measured with the guard in place: the port's link
// gap is 259 undefined, 0 duplicates, unchanged.
#ifdef __MWERKS__
#include "Kyoto/Math/CAABox.hpp"
#include "rstl/optional_object.hpp"

// A stand-in for `MetroidPrime/CPhysicsActor.hpp`, which reaches `Collision/CMaterialList.hpp`
// and its file-scope statics: those put 0x28 bytes of `.data` in this object, and retail's head
// has none, so the module's sha1 broke on them with every function at 100%. Only the mangled name
// has to agree, and one const member gives it.
class CPhysicsActor {
public:
  CAABox GetBoundingBox() const;
};
#endif

extern "C" {

// .text 0x0000F0, 0x1c bytes. `out` is the hidden return pointer and `self` arrives in r4, so this builds a 12-byte
// value out of self[0x54], self[0x58] and self[0x5c].
void fn_14_F0(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x0000E8, 0x08 bytes. a predicate that is always false.
bool fn_14_E8(void*) { return false; }

// .text 0x0000E0, 0x08 bytes. a predicate that is always true.
bool fn_14_E0(void*) { return true; }

// .text 0x0000D8, 0x08 bytes. the address of the member at +0x754.
void* fn_14_D8(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x0000CC, 0x0c bytes. a constant float out of the DOL.
float fn_14_CC(void*) { return lbl_8041B758; }

// .text 0x0000C0, 0x0c bytes. the flag at +0x34c - `rlwinm r3,r0,29,31,31`, the third `bool : 1` of its byte.
bool fn_14_C0(const void* self) { return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0; }

// .text 0x0000B0, 0x10 bytes. resets the unique id to the invalid value.
void fn_14_B0(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x0000A8, 0x08 bytes. a predicate that is always false.
bool fn_14_A8(void*) { return false; }

// .text 0x0000A0, 0x08 bytes. a predicate that is always false.
bool fn_14_A0(void*) { return false; }

// .text 0x000098, 0x08 bytes. a predicate that is always false.
bool fn_14_98(void*) { return false; }

// .text 0x000090, 0x08 bytes. a predicate that is always false.
bool fn_14_90(void*) { return false; }

// .text 0x000088, 0x08 bytes. the byte at +0x44f.
unsigned char fn_14_88(const void* self) { return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F); }

// .text 0x000078, 0x10 bytes. stores the default float at +0x448.
void fn_14_78(void* self) { *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned; }

#ifdef __MWERKS__
// .text 0x10, 0x68 bytes. Returns `rstl::optional_object<CAABox>(self->GetBoundingBox())` through
// the hidden pointer in r3, with `self` in r4.
//
// **This module inlines the conversion; it does not call an out-of-line constructor.** That is
// the one place it parts company with the same wrapper in the neighbouring modules:
// `fn_45_10` (`CMysteryFlyerRel.cpp`), `fn_81_10` (`CTryclopsRel.cpp`), `fn_38_0`
// (`KrocussAccessors.cpp`), `fn_54_0` (`RipperAccessors.cpp`) and `fn_68_0`
// (`ShredderAccessors.cpp`) are each 0x3C bytes and end in `bl <module>_ctor`, calling the
// module's own `optional_object<CAABox>` converting constructor, which lives elsewhere in the
// module's `.text` and stays unclaimed. `fn_14_10` is 0x68 bytes and has no such call: the
// constructor is inlined, so the flag store and the six-word `CAABox` copy are in the body.
//
// So the spelling is the real `rstl::optional_object<CAABox>` return type, not the
// `fn_XX_ctor(out, box)` free-function form the other five need, and MWCC reproduces the bytes
// from it without any hand-written copy: the `optional_object` converting constructor sets
// `m_valid` in its mem-init list and then placement-constructs the box, which is exactly
// `li r0,1; stb r0,0x18(r31)` followed by six `lwz`/`stw` pairs. `CAABox` carries
// `RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE` in `include/Kyoto/Math/CAABox.hpp`, so that construction
// is a word-wise copy rather than a call.
//
// `CAABox GetBoundingBox() const` on the stand-in is load-bearing for the frame shape: it takes
// `this` in r4, the same register `self` arrives in, so no move is needed, and it returns the box
// through a pointer at r1+0x8, which is the 0x30 frame and the 0x34 saved-LR slot retail has.
rstl::optional_object<CAABox> fn_14_10(const CPhysicsActor* self) { return self->GetBoundingBox(); }
#endif

// .text 0x000008, 0x08 bytes. a predicate that is always true.
bool fn_14_8(void*) { return true; }

// .text 0x000000, 0x08 bytes. a predicate that is always true.
bool fn_14_0(void*) { return true; }

} // extern "C"
