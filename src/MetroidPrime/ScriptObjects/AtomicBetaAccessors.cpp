// AtomicBetaAccessors.cpp - a carve of AtomicBeta's .text 0x000000..0x00009C, 14 short accessors.
//
// The class is unnamed in retail - the only name the module's loader has is
// `LoadAtomicBeta__FR13CStateManagerR12CInputStreamR11CEntityInfo` in the DOL, which is named after
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
// The dtk `fn_<id>_<off>` names are kept, so `config/G2ME01/rels/AtomicBeta/symbols.txt` needs no
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

// .text 0x000080, 0x1c bytes. `out` is the hidden return pointer and `self` arrives in r4, so this builds a 12-byte
// value out of self[0x54], self[0x58] and self[0x5c].
void fn_5_80(void* out, const void* self) {
  const float* values = reinterpret_cast< const float* >(static_cast< const char* >(self) + 0x54);
  float* result = static_cast< float* >(out);
  result[0] = values[0];
  result[1] = values[1];
  result[2] = values[2];
}

// .text 0x000078, 0x08 bytes. a predicate that is always false.
bool fn_5_78(void*) { return false; }

// .text 0x000070, 0x08 bytes. a predicate that is always false.
bool fn_5_70(void*) { return false; }

// .text 0x000068, 0x08 bytes. a predicate that is always true.
bool fn_5_68(void*) { return true; }

// .text 0x000060, 0x08 bytes. the address of the member at +0x754.
void* fn_5_60(void* self) { return static_cast< char* >(self) + 0x754; }

// .text 0x000054, 0x0c bytes. a constant float out of the DOL.
float fn_5_54(void*) { return lbl_8041B758; }

// .text 0x000048, 0x0c bytes. the flag at +0x34c - `rlwinm r3,r0,29,31,31`, the third `bool : 1` of its byte.
bool fn_5_48(const void* self) { return (static_cast< const unsigned char* >(self)[0x34C] & 8) != 0; }

// .text 0x000038, 0x10 bytes. resets the unique id to the invalid value.
void fn_5_38(void* self) { *static_cast< unsigned short* >(self) = kInvalidUniqueId; }

// .text 0x000030, 0x08 bytes. a predicate that is always false.
bool fn_5_30(void*) { return false; }

// .text 0x000028, 0x08 bytes. a predicate that is always false.
bool fn_5_28(void*) { return false; }

// .text 0x000020, 0x08 bytes. a predicate that is always false.
bool fn_5_20(void*) { return false; }

// .text 0x000018, 0x08 bytes. a predicate that is always false.
bool fn_5_18(void*) { return false; }

// .text 0x000010, 0x08 bytes. the byte at +0x44f.
unsigned char fn_5_10(const void* self) { return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x44F); }

// .text 0x000000, 0x10 bytes. stores the default float at +0x448.
void fn_5_0(void* self) { *reinterpret_cast< float* >(static_cast< char* >(self) + 0x448) = skDamageHitTime__10CPatterned; }

} // extern "C"
