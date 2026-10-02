// EyeBallAccessors.cpp - a carve of EyeBall's .text 0x0..0xD8: `fn_19_0`, the module's
// GetBoundingBox wrapper, and the fourteen short accessors above it.
//
// The class is unnamed in retail (the only name the module's loader has is
// `LoadEyeBall__FR13CStateManagerR12CInputStreamR11CEntityInfo` in the DOL, which is named after
// the module, not the type), so - as with `CScriptWallCrawler.cpp`, `CScriptPuffer.cpp` and
// `CScriptCoinTouchBounds.cpp` - the object is reached as a `void*` and the offsets are stated
// literally. That keeps this unit layout-immune: it reads members through offsets, so nothing
// here is evidence about a class declaration that does not exist.
//
// Why this range: EyeBall's 0x3C..0xD8 is the same fourteen functions, at the same sizes and with
// the same bytes, as WallCrawler's 0x00..0x9C, which
// `MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp` already reproduces at 100% as a `Matching`
// unit. Both classes descend from the same scripted-actor base, and the accessor block the base
// puts at the head of the module is identical in the two. The 0x3C bytes below that block hold
// the GetBoundingBox wrapper every head in this family opens with, and it is the same fifteen
// instructions as `CMysteryFlyerRel.cpp`'s `fn_45_10`, `ShredderAccessors.cpp`'s `fn_68_0` and
// `KrocussAccessors.cpp`'s `fn_38_0`. That is measured, not assumed; the byte sequences are
//
//   0x00  94 21 ff d0 / 7c 08 02 a6 / 90 01 00 34 / 93 e1 00 2c / 7c 7f 1b 78 / 38 61 00 08 /
//        48 00 26 b9 / 7f e3 fb 78 / 38 81 00 08 / 48 00 25 6d / 80 01 00 34 / 83 e1 00 2c /
//        7c 08 03 a6 / 38 21 00 30 / 4e 80 00 20   (fn_19_0, GetBoundingBox -> fn_19_2590)
//   0x3c  3c 80 00 00 / c0 04 00 00 / d0 03 04 48 / 4e 80 00 20   (*(f32*)(self+0x448) = 2.0f)
//   0x4c  88 63 04 4f / 4e 80 00 20                              (return *(u8*)(self+0x44f))
//   0x54  38 60 00 00 / 4e 80 00 20                              (return false)  x4
//   0x74  3c 80 00 00 / a0 04 00 00 / b0 03 00 00 / 4e 80 00 20 (*(u16*)self = kInvalidUniqueId)
//   0x84  88 03 03 4c / 54 03 ef fe / 4e 80 00 20              ((self[0x34c] & 8) != 0)
//   0x90  3c 60 00 00 / c0 23 00 00 / 4e 80 00 20              (return lbl_8041B758)
//   0x9c  38 63 07 54 / 4e 80 00 20                              (return self + 0x754)
//   0xa4  38 60 00 01 / 4e 80 00 20                              (return true)
//   0xac  38 60 00 00 / 4e 80 00 20                              (return false)  x2
//   0xbc  c0 04 00 54 / d0 03 00 00 / ... / 4e 80 00 20          (copy self[0x54..0x5f] to *out)
//
// The relocations name the two globals outright: 0x3e/0x42 are `skDamageHitTime__10CPatterned` (.sdata2, 2.0f) and
// 0x76/0x7a are `kInvalidUniqueId` (.sbss, size 0x2), and 0x92/0x96 are `lbl_8041B758`.
//
// `rlwinm r3,r0,29,31,31` at 0x88 is the third `bool : 1` of its byte's group, which is what
// `CScriptWallCrawler.cpp` spells `(self[0x34C] & 8) != 0` and gets byte-identically - see
// `include/MetroidPrime/CSimpleShadow.hpp` and `src/MetroidPrime/CSimpleShadowValid.cpp` for the
// same encoding measured against a named field.
//
// The dtk `fn_19_*` names are kept, so `config/G2ME01/rels/EyeBall/symbols.txt` needs no rename:
// the names the retail module defines and the names this object exports are the same strings.
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim.

#include "types.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// `GetBoundingBox__13CPhysicsActorCFv` is the DOL's, and `fn_19_2590` at .text 0x2590 is this
// module's own unclaimed out-of-line `optional_object<CAABox>` converting constructor, so both are
// called by their retail names and neither is defined here.
//
// The module *head* files - `CMysteryFlyerRel.cpp`, `CPillBugRel.cpp` and the rest - are
// deliberately absent from `files.cmake`, because a head body would make the host port link the
// module's own functions, which it cannot, and the port's link gap would grow. This file is
// listed, and so are the other `*Accessors.cpp` units: they were safe because they read raw
// offsets and DOL globals and nothing else. `fn_19_0` breaks that - it makes the host link
// `fn_19_2590` and a host-mangled `CPhysicsActor::GetBoundingBox`. The port reads `EyeBall.rel` off
// the disc through `platform/rel.cpp` and never calls into the module, so nothing is lost, and the
// guard is the same arrangement `CScriptWallCrawler.cpp` uses for its `RELMain`/`RELExit`.
// The MWCC branch is the retail source token for token, so the matching build cannot see it.
#ifdef __MWERKS__
#include "Kyoto/Math/CAABox.hpp"

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

// 0xBC, 0x1C bytes. `out` is the hidden return pointer and `self` arrives in r4, so this returns
// a 12-byte value built out of self[0x54], self[0x58] and self[0x5c] - the shape
// CWallCrawler_CopyVectorFromArgument has, and the reason `this` is not in r3 here.
void fn_19_BC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}
bool fn_19_B4(void*) { return false; }
bool fn_19_AC(void*) { return false; }
bool fn_19_A4(void*) { return true; }
void* fn_19_9C(void* self) { return static_cast< char* >(self) + 0x754; }
float fn_19_90(void*) { return lbl_8041B758; }
bool fn_19_84(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0;
}
void fn_19_74(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }
bool fn_19_6C(void*) { return false; }
bool fn_19_64(void*) { return false; }
bool fn_19_5C(void*) { return false; }
bool fn_19_54(void*) { return false; }
unsigned char fn_19_4C(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F);
}
void fn_19_3C(void* self) {
  *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned;
}

#ifdef __MWERKS__
// .text 0x2590, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line - six
// words copied through, then `stb 1,0x18(r3)` for the valid flag. Retail calls it rather than
// building the value in the mem-init, so this is a call and not a construction. `const CAABox&` is
// load-bearing: by value the frame below grows to 0x40 and the unit no longer matches.
void fn_19_2590(void* out, const CAABox& box);

// .text 0x000000, 0x3c bytes. `out` is the hidden return pointer and `self` arrives in r4, so
// this is `optional_object<CAABox>(out, self->GetBoundingBox())`. Retail takes the box by
// address in a 0x30 frame, which is what the reference parameter gives. `self` needs no move
// because `GetBoundingBox` takes `this` in r4 as well. Instruction for instruction this is
// `CMysteryFlyerRel.cpp`'s `fn_45_10`, the same wrapper in another module.
void fn_19_0(void* out, const CPhysicsActor* self) {
  fn_19_2590(out, self->GetBoundingBox());
}
#endif

} // extern "C"
