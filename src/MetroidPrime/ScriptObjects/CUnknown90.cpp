// `CUnknown90` + `LoadTimeKeyframe` - the entity class FourCC TKEY constructs, and its
// loader. 0x801F9050, 320 bytes, two property cases.
//
// STATUS: **`Matching`, 100%** (320/320 `.text`, 8/8 `.rodata`), verified by
// `tools/flip_test.sh MetroidPrime/ScriptObjects/CUnknown90.cpp`: the DOL still hashes to
// 6ef9b491d0cc08bc81a124fdedb8bfaec34d0010 and all 86 RELs are cmp-equal to
// `orig/G2ME01/files/RelProd/`.
//
// It took two things, and the second is the one to remember:
//
//  1. The `static_cast< u16 >` on the property count below. mwcceppc otherwise hands the
//     `operator new` result the register the hoisted `0x44335aff` case constant held (r29);
//     retail's is r28. The cast is redundant and is there to move the allocation; what does
//     *not* work is measured in `docs/research/missing_classes.md`.
//  2. `SLdrTimeKeyframe` declaring no constructor or destructor in the matching build, which
//     is what `include/MetroidPrime/ScriptLoader/SLdrTimeKeyframe.hpp` now does under
//     `#ifdef TARGET_PC`. **At 100% bytes the link still failed**, with
//     `undefined: 'SLdrTimeKeyframe::SLdrTimeKeyframe()'`: our object called
//     `__ct__16SLdrTimeKeyframeFv` where retail calls `__ct__20SLdrEditorPropertiesFv`, and
//     until the link resolves them both `bl` sites are the same word `48 00 00 01`. objdiff
//     could not see it and neither could `tools/unit_fit.sh`. Only the link could.
//
// `tools/unit_fit.sh` still reports 84 bytes of `__dt__16SLdrTimeKeyframeFv` over the claim.
// That is the weak COMDAT destructor MW now emits for the aggregate's scope exit; mwldeppc
// drops it and it is not in the DOL, which is what the flip confirms.
//
// The class's constructor is **declared and not defined** in the matching build. That is
// deliberate and it is the whole point: the loader only needs the constructor's *name* in
// `config/G2ME01/symbols.txt`, and dtk's object supplies its bytes, so this unit claims no
// vtable and no ctor range. See `docs/research/missing_classes.md`.
#include "MetroidPrime/ScriptObjects/CUnknown90.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTimeKeyframe.hpp"


// Retail's 1.0f, at .sdata2 0x8041D648. Named rather than written as a literal on
// purpose: `lbl_8041D648` is an 8-byte symbol in symbols.txt that no code but this
// loader's `lfs f0,-19832(r2)` reaches, so a literal would put 4 bytes of .sdata2
// in an object that has to own 8 and shift every section after it. Referencing
// retail's own symbol emits the same instruction with no .sdata2 of our own.
extern "C" const float lbl_8041D648;

#ifndef __MWERKS__
// The port has no DOL .sdata2 behind that name. The definition is port-only: the
// matching build resolves the reference from the dtk object that owns
// .sdata2 0x8041D648, and a definition here would be a second one.
extern "C" const float lbl_8041D648 = 1.0f;
#endif

// 0x801F9050, FourCC TKEY, 320 bytes.
//
// The whole loader is `operator new(0x28)`, a four-argument constructor call and
// the `SLdrEditorProperties` destructor; the loop has exactly two properties. The
// frame is 112 bytes with r26-r31 saved, which is what mwcceppc emits for this
// source shape - the same shape as LoadAreaProperties and LoadStreamedAudio.
CEntity* LoadTimeKeyframe(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SLdrTimeKeyframe sldrThis;
  // The default has to live in the struct rather than in a bare local: a bare local
  // is register-cached by mwcceppc (it kept ours in f31 and grew the frame to 128),
  // and retail's frame is 112 with sldrThis.time at r1+76 = 0x4c.
  sldrThis.time = lbl_8041D648;

  // The explicit `u16` conversion is redundant - `ReadUint16()` already returns `u16` - and
  // it is the one thing in this function that decides where the `operator new` result
  // lives. mwcceppc hands that result the register the loop-invariant `0x44335aff` case
  // constant occupied (r29), which is dead by the `new`; retail's is r28, the one the
  // property count held. One extra `u16`-typed node on this read moves the choice onto
  // r28. Measured, not guessed: `static_cast< u16 >`, `+x`, `x & 0xffff` and `u16(u16(x))`
  // all reproduce retail byte for byte, and so do 192 combinations of the other spellings. What
  // does not: widening the declared type to `uint`/`u32` (one byte), a `long` conversion (no
  // change), the same node on `propertySize` (132 bytes) or `propertyId` (6), a cast on the
  // loop bound (6-7), and all 26 flag sets tried. See `docs/research/missing_classes.md`.
  const u16 propertyCount = static_cast< u16 >(input.ReadUint16());
  for (int i = 0; i < propertyCount; ++i) {
    // `Get< uint >()`, not `(uint)ReadInt32()`. They are the same function, but the cast
    // spelling makes mwcceppc materialise the id in a sixth register and reload the stream
    // pointer into r4: 9 extra differing bytes. `CScriptStreamedAudio`'s spelling is the
    // one retail used; `CScriptAreaProperties`'s is the one that costs 9 bytes.
    const uint propertyId = input.Get< uint >();
    const u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(sldrThis.editorProperties, input);
      break;
    case 0x44335aff:
      sldrThis.time = input.ReadFloat();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  return new CUnknown90(mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
                        LdrToEntityInfo(info, sldrThis.editorProperties),
                        sldrThis.time);
}

#ifndef __MWERKS__
// Retail's constructor is 0x801F9474 (104 bytes) and is linked from dtk's object
// for the matching build; the port needs a body. The layout is
// CEntity (CHECK_SIZEOF 0x24) plus the one float, measured from that constructor's
// stores - see include/MetroidPrime/ScriptObjects/CUnknown90.hpp.
CUnknown90::CUnknown90(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, float time)
: CEntity(uid, info, name, 0), m_time(time) {}
#endif
