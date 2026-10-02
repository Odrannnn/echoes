// DigitalGuardianMemberPtr.cpp - a carve of DigitalGuardian's .text 0x0001ABD8..0x0001ABE0:
// `fn_14_1ABD8`, the address of the member at +0x764.
//
// Two instructions, and the shape is already in this module's own `Matching` unit:
// `DigitalGuardianAccessors.cpp`'s `fn_14_D8` at .text 0xD8 is the same accessor for the member at
// +0x754 and reproduces at 100.00%, so this is that spelling with the offset read off this module's
// own disassembly rather than copied. The class is unnamed in retail, so the object is reached as a
// `void*` and the offset is stated literally, which keeps the unit layout-immune.
//
// The name is retail's own `fn_14_<off>` string, so
// `config/G2ME01/rels/DigitalGuardian/symbols.txt` needs no rename. There is no relocation at all -
// `powerpc-eabi-nm -u` on this object prints nothing - which is why it is in `files.cmake` alongside
// the other accessor units and why the port's undefined count does not move.
//
// Definitions are in descending retail text order, because mwcceppc emits them in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim.

#include "types.h"

extern "C" {

// .text 0x0001ABD8, 0x08 bytes.
void* fn_14_1ABD8(void* self) { return static_cast< char* >(self) + 0x764; }

} // extern "C"