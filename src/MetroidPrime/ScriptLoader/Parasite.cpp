#include "MetroidPrime/ScriptLoader.hpp"

// Parasite - retail registers 3 loaders through one loader-struct pointer at
// .sbss 0x80419368; each is a vtable thunk calling one member of it.
//
//   LoadParasite                 0x80200ED0 -> member 0
//   LoadBrizgee                  0x80200EA4 -> member 1
//   LoadCrystallite              0x80200E78 -> member 2
//
// This unit claims .text 0x80200E78..0x80200EFC and .sbss 0x80419368..0x80419370. The 8-byte
// setter at 0x80200EFC is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SParasiteLoaders {
  FScriptLoader slot0;
  FScriptLoader slot1;
  FScriptLoader slot2;
};

struct SLoaderSlot {
  SParasiteLoaders* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Parasite;

CEntity* LoadParasite(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_Parasite.value->slot0(mgr, input, info);
}

CEntity* LoadBrizgee(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_Parasite.value->slot1(mgr, input, info);
}

CEntity* LoadCrystallite(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_Parasite.value->slot2(mgr, input, info);
}
