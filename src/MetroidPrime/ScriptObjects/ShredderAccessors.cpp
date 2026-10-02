// ShredderAccessors.cpp - a carve of Shredder's .text 0x000000..0x0000C8: `fn_68_0`, the
// module's GetBoundingBox wrapper, and the 12 short accessors above it.
//
// The class is unnamed in retail - the only name the module's loader has is
// `LoadShredder__FR13CStateManagerR12CInputStreamR11CEntityInfo` in the DOL, which is named after
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
// The dtk `fn_<id>_<off>` names are kept, so `config/G2ME01/rels/Shredder/symbols.txt` needs no
// rename: the names the retail module defines and the names this object exports are the same
// strings.  
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim.

#include "types.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

// `GetBoundingBox__13CPhysicsActorCFv` is the DOL's, and `fn_68_284C` at .text 0x284C is this
// module's own unclaimed out-of-line `optional_object<CAABox>` converting constructor, so both are
// called by their retail names and neither is defined here.
//
// The module *head* files - `CMysteryFlyerRel.cpp`, `CPillBugRel.cpp` and the rest - are
// deliberately absent from `files.cmake`, because a head body would make the host port link the
// module's own functions, which it cannot, and the port's link gap would grow. This file is
// listed, and so are the other 19 `*Accessors.cpp` units: they were safe because they read raw
// offsets and DOL globals and nothing else. `fn_68_0` breaks that - it makes the host link
// `fn_68_284C` and a host-mangled `CPhysicsActor::GetBoundingBox`. The port reads `Shredder.rel`
// off the disc through `platform/rel.cpp` and never calls into the module, so nothing is lost, and
// the guard is the same arrangement `CScriptWallCrawler.cpp` uses for its `RELMain`/`RELExit`.
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

// .text 0x0000AC, 0x1c bytes. `out` is the hidden return pointer and `self` arrives in r4, so this builds a 12-byte
// value out of self[0x54], self[0x58] and self[0x5c].
void fn_68_AC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x0000A4, 0x08 bytes. a predicate that is always true.
bool fn_68_A4(void*) { return true; }

// .text 0x00009C, 0x08 bytes. the address of the member at +0x754.
void* fn_68_9C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x000090, 0x0c bytes. a constant float out of the DOL.
float fn_68_90(void*) { return lbl_8041B758; }

// .text 0x000084, 0x0c bytes. the flag at +0x34c - `rlwinm r3,r0,29,31,31`, the third `bool : 1` of its byte.
bool fn_68_84(const void* self) { return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0; }

// .text 0x000074, 0x10 bytes. resets the unique id to the invalid value.
void fn_68_74(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x00006C, 0x08 bytes. a predicate that is always false.
bool fn_68_6C(void*) { return false; }

// .text 0x000064, 0x08 bytes. a predicate that is always false.
bool fn_68_64(void*) { return false; }

// .text 0x00005C, 0x08 bytes. a predicate that is always false.
bool fn_68_5C(void*) { return false; }

// .text 0x000054, 0x08 bytes. a predicate that is always false.
bool fn_68_54(void*) { return false; }

// .text 0x00004C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_68_4C(const void* self) { return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F); }

// .text 0x00003C, 0x10 bytes. stores the default float at +0x448.
void fn_68_3C(void* self) { *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned; }

#ifdef __MWERKS__
// .text 0x284C, unclaimed: `optional_object<CAABox>`'s converting constructor, out of line - six
// words copied through, then `stb 1,0x18(r3)` for the valid flag. Retail calls it rather than
// building the value in the mem-init, so this is a call and not a construction. `const CAABox&` is
// load-bearing: by value the frame below grows to 0x40 and the unit no longer matches.
void fn_68_284C(void* out, const CAABox& box);

// .text 0x000000, 0x3c bytes. `out` is the hidden return pointer and `self` arrives in r4, so
// this is `optional_object<CAABox>(out, self->GetBoundingBox())`. Retail takes the box by
// address in a 0x30 frame, which is what the reference parameter gives. `self` needs no move
// because `GetBoundingBox` takes `this` in r4 as well. Instruction for instruction this is
// `CMysteryFlyerRel.cpp`'s `fn_45_10`, the same wrapper in another module.
void fn_68_0(void* out, const CPhysicsActor* self) {
  fn_68_284C(out, self->GetBoundingBox());
}
#endif

} // extern "C"
