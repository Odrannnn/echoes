#include "MetroidPrime/ScriptLoader.hpp"

// MetareeSwarm - retail 0x8022D57C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195E0 and calls member 0 of it.
//
// This unit claims .text 0x8022D57C..0x8022D5A8 and .sbss 0x804195E0..0x804195E8. The 8-byte
// setter at 0x8022D5A8 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MetareeSwarm;

CEntity* LoadMetareeSwarm(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_MetareeSwarm.value)(mgr, input, info);
}
