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
//
// **The member names here are upstream's**, and each is the member the pre-merge name stood for at
// the same offset and the same type - `x0_scale` -> `mScale`, `xc_animData.x0_has`/`x4_item` ->
// `mAnimData`'s `auto_ptr::mHas`/`mItem`, `x18_ambientColor` -> `mAmbientColor`, and the three
// `optional_object` validity flags in order. Two are pure renames with no offset change:
// `x2c_xrayModel` -> `mEchoModel` and `x3c_infraModel` -> `mDarkModel`, which upstream reads as
// Echoes' alternate resources; the three `EWhichModel` cases still map onto the three members by
// position (0x1C/0x2C/0x3C). `x14_flags` was a four-bit struct at 0x14 and upstream declares four
// loose bit-fields in the same byte, bit 0 first in both: `x24_renderSorted` -> `mRenderSorted`,
// `x25_sortThermal` -> `mTexturesLocked`, `x26_` -> `mRenderUnsortedParts`, `x27_` ->
// `mRenderFullEchoModel`.
//
// **The four flags are stored one bit at a time here, through `bool&`, on purpose.** They share
// retail's single byte at 0x14, so how they are reached decides the shape: retail's default
// constructor writes four separate `lbz`/`rlwimi`/`stb` triples, which four assignments in a body
// reproduce, while `CModelData`'s copy constructor (`CModelDataCopyCtor.cpp`) copies the whole byte
// as **one `lbz`/`stb` pair** - and MWCC 2.7 emits exactly that pair, and nothing else, for a
// struct of four one-bit bit-fields copied as a unit, where four loose bit-fields in a mem-init
// list give four read-modify-write chains instead, 28 instructions against retail's 2. Upstream
// declares them loose, so only the first of those two shapes is still expressible; a struct form
// belongs with the `__ct__10CModelDataFv` rename above, which is the blocked one. Measured as
// `tools/bfprobe` shapes V4 (struct) and V1 (loose).
//
// The three validity flags are written through `rstl::optional_object::Invalidate`, and
// deliberately **not** through `clear()`. `clear()` null-tests first and, when the flag is set,
// destroys the held `TLockedToken<CModel>` - which unlocks a token - and retail has no store for
// that. `fn_800E6AD0` is called on a `CModelData` that `CScriptSkyRipple` and
// `CScriptScriptStreamedMovie` reuse, so "the flag is false" is not a safe assumption and
// `clear()` is a behaviour change, not a codegen difference.
extern "C"
void fn_800E6AD0(CModelData* self) {
  CVector3f& scale = self->Scale();
  scale.SetX(1.0f);
  scale.SetY(1.0f);
  scale.SetZ(1.0f);
  // One byte and one word of `mAnimData`, and one byte of each of the three
  // `rstl::optional_object`s - their valid flags, which is all a default-constructed one has set.
  // They are written directly rather than assigned or placement-newed because both classes are
  // non-trivial in this tree: a placement-new emits a null test (`addic. r3,r31,12; beq`) and an
  // assignment from a default-constructed temporary emits
  // `__dt__Q24rstl20auto_ptr<9CAnimData>Fv`, `__dt__15TToken<6CModel>Fv` and a `__dt__6CTokenFv`
  // call per member, none of which retail has. `auto_ptr::reset` is the one exception and is
  // exactly the two stores - `mHas = false; mItem = nullptr;` - with no destructor and no test.
  self->AnimData().reset();
  self->SetRenderSorted(false);
  self->SetTexturesLocked(false);
  self->SetRenderUnsortedParts(true);
  self->SetRenderFullEchoModel(false);
  self->SetAmbientColor(CColor::White());
  self->NormalModel().Invalidate();
  self->EchoModel().Invalidate();
  self->DarkModel().Invalidate();
}
