#include "MetroidPrime/CModelData.hpp"

// Retail 0x800E6AD0, 0x98 bytes: `CModelData`'s default constructor, and the only code in the DOL
// that touches four of its members. The port calls it as `fn_800E6AD0` from its own one-line
// `CModelData::CModelData()` in CScriptCannonBall.cpp, CScriptSkyRipple.cpp and
// CScriptScriptStreamedMovie.cpp, so the name stays the one the link needs.
//
// `NonMatching` at 97.11%, and the residue is **one instruction**: MWCC puts the colour it loads
// out of `CColor::White()` in r3, retail puts it in r4 and spends a dead `mr r3,r31` first. Both
// stores are 4 bytes, which is the whole 148-vs-152 gap.
//
// **That instruction is MWCC's constructor convention, and writing it as a constructor is
// byte-exact.** Measured 2026-09-25: the same stores as a real `CModelData::CModelData()` compile
// to 152 bytes that match retail exactly, including the `mr r3,r31`. Two things have to change
// together for that, and the second is why this unit is `NonMatching`:
//
//   1. The symbol becomes `__ct__10CModelDataFv`, so `config/G2ME01/symbols.txt` has to be renamed
//      and the three port-side wrappers deleted.
//   2. **The four bit-fields have to move into the mem-init list.** C++ forbids that; MWCC 2.7
//      accepts it, and it is the only way to get retail's store order, because retail writes the
//      four `rlwimi`/`stb` pairs at 0x14 *between* `xc_animData` at 0xC and `x18_ambientColor` at
//      0x18, which is member-declaration order, while a constructor body runs after every mem-init.
//      With them in the body the order comes out as scale, animData, colour, optionals, bit-fields.
//
// Step 1 is blocked, and the blocker is a Matching REL unit: `MetroidPrime/ScriptObjects/
// CScriptScriptStreamedMovie.cpp` claims `.text 0x1054..0x1074` of the `ScriptStreamedMovie`
// module and its object *defines* `__ct__10CModelDataFv` while *importing* `fn_800E6AD0`
// (`nm` on `build/G2ME01/src/MetroidPrime/ScriptObjects/CScriptScriptStreamedMovie.o` shows both).
// Renaming the DOL symbol would make that wrapper resolve to itself. Renaming the wrapper as well
// should keep the module's 32 bytes identical - a `bl`'s displacement is a relocation - but that is
// a `Matching` REL's hash to re-verify, and is left to whoever wants the function.
//
// The body is flat and in store order. The scale's 1.0f is a pooled `.sdata2` word; this unit
// claims 0x8041B72C..0x8041B730, which is where retail has it (`lbl_8041B72C` in
// `config/G2ME01/symbols.txt`) and the only reference to it in the whole DOL - `grep -c
// '27796(r2)'` over the disassembly is 1, this instruction - so the pool word replaces it outright.
extern "C"
void fn_800E6AD0(CModelData* self) {
  self->x0_scale.SetX(1.0f);
  self->x0_scale.SetY(1.0f);
  self->x0_scale.SetZ(1.0f);
  // One byte and one word of `xc_animData`, and one byte of each of the three
  // `rstl::optional_object`s - their valid flags, which is all a default-constructed one has set.
  // They are written directly rather than assigned or placement-newed because both classes are
  // non-trivial in this tree: a placement-new emits a null test (`addic. r3,r31,12; beq`) and an
  // assignment from a default-constructed temporary emits
  // `__dt__Q24rstl20auto_ptr<9CAnimData>Fv`, `__dt__15TToken<6CModel>Fv` and a `__dt__6CTokenFv`
  // call per member, none of which retail has.
  self->xc_animData.x0_has = false;
  self->xc_animData.x4_item = nullptr;
  self->x14_24_renderSorted = false;
  self->x14_25_sortThermal = false;
  self->x14_26_ = true;
  self->x14_27_ = false;
  self->x18_ambientColor = CColor::White();
  self->x1c_normalModel.m_valid = false;
  self->x2c_xrayModel.m_valid = false;
  self->x3c_infraModel.m_valid = false;
}
