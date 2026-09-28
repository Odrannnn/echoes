// `CScriptRelay` + `LoadRelay` - the entity class FourCC SRLY constructs, and its
// loader. 0x800B8EFC, 340 bytes, two property cases.
//
// The class's constructor is **declared and not defined** in the matching build: the
// loader only needs the constructor's *name* in `config/G2ME01/symbols.txt`, and dtk's
// object supplies its bytes, so this unit claims no vtable and no ctor range. See
// `docs/research/missing_classes.md`.
#include "MetroidPrime/ScriptObjects/CScriptRelay.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader/SLdrRelay.hpp"

// 0x800B8EFC, FourCC SRLY, 340 bytes. The shape is LoadTimeKeyframe's: an SLdrRelay on
// the stack at r1+16, a two-case property loop, `operator new`(0x28), one four-argument
// constructor call, and the `SLdrEditorProperties` destructor. `sldrThis.oneShot` is the
// byte at r1+76 = 0x4c, which is `SLdrEditorProperties` (0x3c) into the aggregate.
CEntity* LoadRelay(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrRelay sldrThis;
  sldrThis.oneShot = false;

  const u16 propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    // `Get< uint >()`, not `(uint)ReadInt32()`: the cast spelling materialises the id in a
    // sixth register and costs 9 differing bytes. `docs/research/missing_classes.md`.
    const uint propertyId = input.Get< uint >();
    const u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefSLdrEditorProperties(sldrThis.editorProperties, input);
      break;
    case 0xead7b7bb:
      sldrThis.oneShot = input.ReadBool();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  return new CScriptRelay(mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
                          LdrToEntityInfo(info, sldrThis.editorProperties),
                          sldrThis.oneShot);
}

#ifndef __MWERKS__
// Retail's constructor is 0x800B919C (120 bytes) and is linked from dtk's object for the
// matching build; the port needs a body. The layout is CEntity (CHECK_SIZEOF 0x24) plus
// a short and a byte, measured from that constructor's stores - see
// include/MetroidPrime/ScriptObjects/CScriptRelay.hpp.
CScriptRelay::CScriptRelay(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                           bool oneShot)
: CEntity(uid, info, name, 0), m_x24(0), m_flags(oneShot ? 1 : 0) {}
#endif
