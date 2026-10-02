#include "MetroidPrime/ScriptLoader.hpp"

// DigitalGuardian - retail registers 2 loaders through one loader-struct pointer at
// .sbss 0x80419510; each is a vtable thunk calling one member of it.
//
//   LoadDigitalGuardian          0x8021F984 -> member 0
//   LoadDigitalGuardianHead      0x8021F958 -> member 1
//
// This unit claims .text 0x8021F958..0x8021F9B0 and .sbss 0x80419510..0x80419518. The 8-byte
// setter at 0x8021F9B0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SDigitalGuardianLoaders {
  FScriptLoader slot0;
  FScriptLoader slot1;
};

struct SLoaderSlot {
  SDigitalGuardianLoaders* value;
  unsigned int padding;
};

SLoaderSlot gLoader_DigitalGuardian;

CEntity* LoadDigitalGuardian(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_DigitalGuardian.value->slot0(mgr, input, info);
}

CEntity* LoadDigitalGuardianHead(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return gLoader_DigitalGuardian.value->slot1(mgr, input, info);
}
