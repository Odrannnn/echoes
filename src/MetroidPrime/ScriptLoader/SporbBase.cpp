#include "MetroidPrime/ScriptLoader.hpp"

// SporbBase - retail registers 4 loaders through one loader-struct pointer at
// .sbss 0x804193A8; each is a vtable thunk calling one member of it.
//
//   LoadSporbNeedle              0x80213C8C -> member 2
//   LoadSporbBase                0x80213C60 -> member 0
//   LoadSporbTop                 0x80213C34 -> member 3
//   LoadSporbProjectile          0x80213C08 -> member 1
//
// This unit claims .text 0x80213C08..0x80213CB8 and .sbss 0x804193A8..0x804193B0. The 8-byte
// setter at 0x80213CB8 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SSporbBaseLoaders {
  FScriptLoader slot0;
  FScriptLoader slot1;
  FScriptLoader slot2;
  FScriptLoader slot3;
};

struct SLoaderSlot {
  SSporbBaseLoaders* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SporbBase;

CEntity* LoadSporbNeedle(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_SporbBase.value->slot2(mgr, input, info);
}

CEntity* LoadSporbBase(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_SporbBase.value->slot0(mgr, input, info);
}

CEntity* LoadSporbTop(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_SporbBase.value->slot3(mgr, input, info);
}

CEntity* LoadSporbProjectile(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_SporbBase.value->slot1(mgr, input, info);
}
