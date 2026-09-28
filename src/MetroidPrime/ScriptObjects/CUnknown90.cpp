// `CUnknown90` + `LoadTimeKeyframe` - the entity class FourCC TKEY constructs, and its
// loader. 0x801F9050, 320 bytes, two property cases.
//
// STATUS: **99.75%, unit is `NonMatching`.** 316 of 320 bytes match. The 4 that do not are
// one decision repeated - the register that receives the `operator new` result: retail uses
// r28, the register that held the property count, and mwcceppc gives us r29, the one that
// held the 0x44335aff FourCC. Both are dead by then, the prologue and the whole property
// loop are byte-identical, and twenty source spellings plus nine flag sets all produce the
// same 4 bytes; `docs/research/missing_classes.md` lists them. Fix those 4 bytes and
// `tools/flip_test.sh MetroidPrime/ScriptObjects/CUnknown90.cpp` should pass -
// `tools/unit_fit.sh` reports the only other objection, 84 bytes of
// `__dt__16SLdrTimeKeyframeFv`, and that is a weak definition mwldeppc discards (the retail
// linker discarded it too: it is not in the DOL).
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
CEntity* LoadTimeKeyframe(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrTimeKeyframe sldrThis;
  // The default has to live in the struct rather than in a bare local: a bare local
  // is register-cached by mwcceppc (it kept ours in f31 and grew the frame to 128),
  // and retail's frame is 112 with sldrThis.time at r1+76 = 0x4c.
  sldrThis.time = lbl_8041D648;

  const u16 propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    // `Get< uint >()`, not `(uint)ReadInt32()`. They are the same function, but the cast
    // spelling makes mwcceppc materialise the id in a sixth register and reload the stream
    // pointer into r4: 9 extra differing bytes. `CScriptStreamedAudio`'s spelling is the
    // one retail used; `CScriptAreaProperties`'s is the one that costs 9 bytes.
    const uint propertyId = input.Get< uint >();
    const u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefSLdrEditorProperties(sldrThis.editorProperties, input);
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
