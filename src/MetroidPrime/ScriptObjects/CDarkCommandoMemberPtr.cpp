// CDarkCommandoMemberPtr.cpp - DarkCommando's (module 3) accessor at .text 0x6250..0x6258:
// `fn_3_6250`, the address of the sub-object at +0xC04.
//
// One function, one contiguous range, and the module's own single-instruction accessor shape:
// `fn_3_91F0` (0x91F0) and `fn_3_C8` (0xC8, in `CDarkCommandoRel.cpp`) are the same
// `addi r3,r3,<off>` over a member of theirs, and `MetroidPrime/ScriptObjects/
// CIngSnatchingSwarmBounds.cpp`'s `fn_33_39B8` over +0x1C0 is the same shape in another module.
// All three are Matching at 100.00%, so this is a spelling that transfers rather than a guess.
//
// The range is claimed exactly and nothing else. `fn_3_61C4` (0x61C4, 0x8C) below and
// `fn_3_6258` (0x6258, 0x2C) above stay retail, so dtk fills them and the module's sha1 against
// `config/G2ME01/config.yml` still holds. `fn_3_6250` is in the module's `ldscript.lcf`
// FORCEACTIVE block, so nothing dead-strips it.

#include "types.h"

// The object this accessor is written over, reduced to the one member it touches. The padding is
// the space the real CDarkCommando body occupies and is written as `char x_padN[...]`, which
// `tools/check_raw_offsets.py` does not count as a raw offset. Nothing here is evidence about a
// class declaration this tree does not have: retail names the class only through its vtable.
class CDarkCommandoMember {
private:
  char x_pad0[0xC04];

public:
  // +0xC04. The function hands this member's address back and never reads through it, so the
  // class carries the range and `char` is enough. The extent is the next member this module's own
  // code reaches - `fn_3_6284` (0x6284) returns `this + 0xC2C` and tests the byte at +0xCB4 - so
  // 0xC2C is where the object demonstrably continues.
  char xC04[0xC2C - 0xC04];
};

// `tools/check_symbol_names.py` reads this name out of the object, so it has to be exactly what
// `config/G2ME01/rels/DarkCommando/symbols.txt` calls it.
extern "C" {
// .text 0x6250, 0x08 bytes. the address of the sub-object at +0xC04, so the whole body is the one
// `addi r3,r3,0xc04` and the array member decays to its own address.
char* fn_3_6250(CDarkCommandoMember* self) { return self->xC04; }
}