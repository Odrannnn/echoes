// CDarkCommandoVecList.cpp - DarkCommando's (module 3) pair at .text 0x9864..0x9890:
// `fn_3_9864`, the flag-clearing accessor, and `fn_3_9870`, this module's copy of the
// module-wide "up vector" getter.
//
// **These two are byte-identical to `fn_14_1AB24` and `fn_14_1AB30`**, which
// `MetroidPrime/ScriptObjects/DigitalGuardianVecList.cpp` (module 14) already decompiles at 2/2 -
// same three instructions `38 00 00 00 / 98 03 00 0C / 4E 80 00 20`, then the same seven
// `3C 80 00 00 / C4 04 00 00 / D0 03 00 00 / C0 04 00 04 / ...` pair. That is measured, not
// assumed: the DigitalGuardian object's disassembly in
// `build/G2ME01/DigitalGuardian/asm/MetroidPrime/ScriptObjects/DigitalGuardianVecList.s` and
// `build/G2ME01/DarkCommando/asm/auto_00_0000019C_text.s` agree word for word, so this file
// reuses a spelling that is already proved rather than inventing one.
//
//   0x9864 fn_3_9864  0x0C  li r0,0 / stb r0,0xc(r3) / blr
//   0x9870 fn_3_9870  0x20  lis r4, sUpVector__9CVector3f@ha ; lfsu f0,@l(r4) ; three
//                       lfs/stfs pairs into the hidden return pointer in r3
//
// The range is claimed exactly and nothing else. `fn_3_9890` (0x9890, 0x8C) above and
// `fn_3_9934` (0x9934, 0x3C) below stay retail, so dtk fills them and the module's sha1 against
// `config/G2ME01/config.yml` still holds. Both this file's functions are in the module's
// `ldscript.lcf` FORCEACTIVE block (`fn_3_9864`, `fn_3_9870`), so nothing dead-strips.
//
// Definitions are in descending retail text order - mwcceppc emits them in reverse source order
// and mwldeppc keeps the object's `.text` order verbatim. Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CDarkCommandoVecList.cpp`.

#include "types.h"

#ifdef __MWERKS__
// `CVector3f` only for the by-value return of `fn_3_9870`; the host build has the same header
// (and `src/Kyoto/Math/CVector3f.cpp` is already in `files.cmake`), so nothing here is
// MWCC-only, but the member-function shape the twin uses is spelled out under the guard to keep
// the two builds from disagreeing about it.
#include "Kyoto/Math/CVector3f.hpp"
#endif

// The object `fn_3_9864` is written over, reduced to the one member it touches: the one-byte flag
// at +0xC that the module's own head clears too. Retail destroys a `CUnknownVec3List`-shaped
// object here, but nothing in these two functions reads anything but that byte, so nothing here is
// evidence about a class declaration this tree does not have.
class CDarkCommandoVecList {
private:
  char x_pad0[0xC];

public:
  bool x0C;
};

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/DarkCommando/symbols.txt` calls them.
extern "C" {
// .text 0x9870, 0x20 bytes. Returns `CVector3f::Up()` by value through the hidden return pointer
// in r3, with `this` in r4 unused. `CVector3f::Up()` is `inline const CUnitVector3f&` off
// `CVector3f::sUpVector` (`include/Kyoto/Math/CUnitVector3f.hpp:39`), and copying a 12-byte
// aggregate out of a reference is the `lfsu` on the first word plus two `lfs`/`stfs` pairs - the
// `lfsu`, not `lfs`, is what says the three reads are consecutive, and it only appears because the
// whole value is copied. The DOL exports `sUpVector__9CVector3f` and the port already links it
// (`src/Kyoto/Math/CVector3f.cpp`), so listing this file in `files.cmake` moves the port's
// undefined count by nothing.
#ifdef __MWERKS__
CVector3f fn_3_9870(const void* self) { return CVector3f::Up(); }
#endif

// .text 0x9864, 0x0C bytes. Resets the one-byte flag at +0xC and returns `this`. No relocation at
// all, and `r3` is already the returned pointer so nothing is moved.
void* fn_3_9864(CDarkCommandoVecList* self) {
  self->x0C = 0;
  return self;
}
}