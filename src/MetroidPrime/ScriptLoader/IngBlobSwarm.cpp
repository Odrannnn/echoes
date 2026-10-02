#include "MetroidPrime/ScriptLoader.hpp"

// IngBlobSwarm - retail 0x8022E108, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195E8 and calls member 0 of it.
//
// This unit claims .text 0x8022E108..0x8022E134 and .sbss 0x804195E8..0x804195F0. The 8-byte
// setter at 0x8022E134 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_IngBlobSwarm;

CEntity* LoadIngBlobSwarm(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_IngBlobSwarm.value)(mgr, input, info);
}
