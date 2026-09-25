#include "MetroidPrime/ScriptLoader.hpp"

// BacteriaSwarm - retail 0x8022A580, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195D0 and calls member 0 of it.
//
// This unit claims .text 0x8022A580..0x8022A5AC and .sbss 0x804195D0..0x804195D8. The 8-byte
// setter at 0x8022A5AC is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_BacteriaSwarm;

CEntity* LoadBacteriaSwarm(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_BacteriaSwarm.value)(mgr, input, info);
}
