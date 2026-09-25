#include "MetroidPrime/ScriptLoader.hpp"

// SplitterMainChassis - retail registers 2 loaders through one loader-struct pointer at
// .sbss 0x80419440; each is a vtable thunk calling one member of it.
//
//   LoadSplitterMainChassis      0x80218CC4 -> member 0
//   LoadSplitterCommandModule    0x80218C98 -> member 1
//
// This unit claims .text 0x80218C98..0x80218CF0 and .sbss 0x80419440..0x80419448. The 8-byte
// setter at 0x80218CF0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SSplitterMainChassisLoaders {
  FScriptLoader slot0;
  FScriptLoader slot1;
};

struct SLoaderSlot {
  SSplitterMainChassisLoaders* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SplitterMainChassis;

CEntity* LoadSplitterMainChassis(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_SplitterMainChassis.value->slot0(mgr, input, info);
}

CEntity* LoadSplitterCommandModule(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return gLoader_SplitterMainChassis.value->slot1(mgr, input, info);
}
