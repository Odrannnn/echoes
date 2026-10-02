// WispTentacleAccessors.cpp - a carve of WispTentacle's .text 0x00032C..0x0003C8, 14 short accessors.
//
// The class is unnamed in retail - the only name the module's loader has is
// `LoadWispTentacle__FR13CStateManagerR12CInputStreamR11CEntityInfo` in the DOL, which is named after
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
// The dtk `fn_<id>_<off>` names are kept, so `config/G2ME01/rels/WispTentacle/symbols.txt` needs no
// rename: the names the retail module defines and the names this object exports are the same
// strings.  
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim.

#include "types.h"

extern "C" const float skDamageHitTime__10CPatterned;
extern "C" const float lbl_8041B758;
extern "C" const unsigned short kInvalidUniqueId;

extern "C" {

// .text 0x0003AC, 0x1c bytes. `out` is the hidden return pointer and `self` arrives in r4, so this builds a 12-byte
// value out of self[0x54], self[0x58] and self[0x5c].
void fn_86_3AC(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x0003A4, 0x08 bytes. a predicate that is always false.
bool fn_86_3A4(void*) { return false; }

// .text 0x00039C, 0x08 bytes. a predicate that is always false.
bool fn_86_39C(void*) { return false; }

// .text 0x000394, 0x08 bytes. a predicate that is always true.
bool fn_86_394(void*) { return true; }

// .text 0x00038C, 0x08 bytes. the address of the member at +0x754.
void* fn_86_38C(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x000380, 0x0c bytes. a constant float out of the DOL.
float fn_86_380(void*) { return lbl_8041B758; }

// .text 0x000374, 0x0c bytes. the flag at +0x34c - `rlwinm r3,r0,29,31,31`, the third `bool : 1` of its byte.
bool fn_86_374(const void* self) { return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0; }

// .text 0x000364, 0x10 bytes. resets the unique id to the invalid value.
void fn_86_364(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x00035C, 0x08 bytes. a predicate that is always false.
bool fn_86_35C(void*) { return false; }

// .text 0x000354, 0x08 bytes. a predicate that is always false.
bool fn_86_354(void*) { return false; }

// .text 0x00034C, 0x08 bytes. a predicate that is always false.
bool fn_86_34C(void*) { return false; }

// .text 0x000344, 0x08 bytes. a predicate that is always false.
bool fn_86_344(void*) { return false; }

// .text 0x00033C, 0x08 bytes. the byte at +0x44f.
unsigned char fn_86_33C(const void* self) { return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F); }

// .text 0x00032C, 0x10 bytes. stores the default float at +0x448.
void fn_86_32C(void* self) { *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned; }

} // extern "C"
