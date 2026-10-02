// CDarkCommandoVecCopy.cpp - DarkCommando's (module 3) pair at .text 0x9970..0x99A8:
// `fn_3_9970` and `fn_3_998C`, the two byte-identical copies of the vector at +0x1B0 into the
// hidden return pointer in r3.
//
// **This is `fn_3_E0` again.** `MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp` already
// decompiles this module's head accessor at 0xE0, which is the same seven instructions against the
// vector at +0x54 - `lfs f0,<off>(r4) / stfs f0,0(r3)` three times over, reusing one f register and
// one base register rather than hoisting three loads - and it is 18/18 at 100.00%. Retail reuses
// f0 for all three loads, so the source is three element copies and **not** a `CVector3f` copy or
// a three-argument construction, either of which hoists into f0/f1/f2; the head file records that
// measurement and `MetareeSwarm`'s records the reverse case where the built-in spelling is the one
// that matches. So the three floats are written out here as three stores rather than handed to a
// container, which is what reproduces the interleaving.
//
//   0x9970 fn_3_9970  0x1C  lfs f0,0x1b0(r4) / stfs f0,0(r3) ; 0x1b4 -> 4 ; 0x1b8 -> 8
//   0x998C fn_3_998C  0x1C  the same seven instructions, byte for byte
//
// Retail has two of these because two vtable slots read the same member; they are separate
// functions, not one folded twice, so both are defined here and both are in the module's
// `ldscript.lcf` FORCEACTIVE block (`fn_3_9970`, `fn_3_998C`).
//
// The range is claimed exactly and nothing else. `fn_3_9934` (0x9934, 0x3C) below and `fn_3_99A8`
// (0x99A8, 0x74) above stay retail, so dtk fills them and the module's sha1 against
// `config/G2ME01/config.yml` still holds.
//
// Definitions are in descending retail text order - mwcceppc emits them in reverse source order
// and mwldeppc keeps the object's `.text` order verbatim. Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CDarkCommandoVecCopy.cpp`.

#include "types.h"

// The object the copy is written over, reduced to the three words it reads. The padding between
// offset 0 and the vector is the space the real CDarkCommando body occupies and is written as
// `char x_padN[...]`, which `tools/check_raw_offsets.py` does not count as a raw offset.
//
// The three floats are declared one by one rather than as a `CVector3f` for the codegen reason in
// the note at the top: this file's shape is retail's, not a tidier spelling of it.
class CDarkCommandoVecCopy {
private:
  char x_pad0[0x1B0];

public:
  float x1B0;
  float x1B4;
  float x1B8;
};

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/DarkCommando/symbols.txt` calls them.
extern "C" {
// .text 0x998C, 0x1C bytes. `out` is the hidden return pointer in r3 and `self` arrives in r4.
void fn_3_998C(float* out, const CDarkCommandoVecCopy* self) {
  out[0] = self->x1B0;
  out[1] = self->x1B4;
  out[2] = self->x1B8;
}

// .text 0x9970, 0x1C bytes. the same seven instructions as the function above, at the lower offset.
void fn_3_9970(float* out, const CDarkCommandoVecCopy* self) {
  out[0] = self->x1B0;
  out[1] = self->x1B4;
  out[2] = self->x1B8;
}
}