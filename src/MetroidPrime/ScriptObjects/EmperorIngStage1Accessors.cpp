// EmperorIngStage1Accessors.cpp - a carve of EmperorIngStage1's .text 0x00B994..0x00BA30, 14 short accessors.
//
// The class is unnamed in retail - the only name the module's loader has is
// `LoadEmperorIngStage1__FR13CStateManagerR12CInputStreamRC11CEntityInfo` in the DOL, which is named after
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
// The dtk `fn_<id>_<off>` names are kept, so `config/G2ME01/rels/EmperorIngStage1/symbols.txt` needs no
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

// .text 0x00BA14, 0x1c bytes. `out` is the hidden return pointer and `self` arrives in r4, so this builds a 12-byte
// value out of self[0x54], self[0x58] and self[0x5c].
void fn_16_BA14(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x00BA0C, 0x08 bytes. a predicate that is always false.
bool fn_16_BA0C(void*) { return false; }

// .text 0x00BA04, 0x08 bytes. a predicate that is always false.
bool fn_16_BA04(void*) { return false; }

// .text 0x00B9FC, 0x08 bytes. a predicate that is always true.
bool fn_16_B9FC(void*) { return true; }

// .text 0x00B9F4, 0x08 bytes. the address of the member at +0x754.
void* fn_16_B9F4(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x00B9E8, 0x0c bytes. a constant float out of the DOL.
float fn_16_B9E8(void*) { return lbl_8041B758; }

// .text 0x00B9DC, 0x0c bytes. the flag at +0x34c - `rlwinm r3,r0,29,31,31`, the third `bool : 1` of its byte.
bool fn_16_B9DC(const void* self) { return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0; }

// .text 0x00B9CC, 0x10 bytes. resets the unique id to the invalid value.
void fn_16_B9CC(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x00B9C4, 0x08 bytes. a predicate that is always false.
bool fn_16_B9C4(void*) { return false; }

// .text 0x00B9BC, 0x08 bytes. a predicate that is always false.
bool fn_16_B9BC(void*) { return false; }

// .text 0x00B9B4, 0x08 bytes. a predicate that is always false.
bool fn_16_B9B4(void*) { return false; }

// .text 0x00B9AC, 0x08 bytes. a predicate that is always false.
bool fn_16_B9AC(void*) { return false; }

// .text 0x00B9A4, 0x08 bytes. the byte at +0x44f.
unsigned char fn_16_B9A4(const void* self) { return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F); }

// .text 0x00B994, 0x10 bytes. stores the default float at +0x448.
void fn_16_B994(void* self) { *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned; }

} // extern "C"
