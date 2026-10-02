#include "MetroidPrime/ScriptLoader.hpp"

// PlantScarabSwarm - retail 0x8022FFCC, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419618 and calls member 0 of it.
//
// This unit claims .text 0x8022FFCC..0x8022FFF8 and .sbss 0x80419618..0x80419620. The 8-byte
// setter at 0x8022FFF8 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_PlantScarabSwarm;

CEntity* LoadPlantScarabSwarm(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_PlantScarabSwarm.value)(mgr, input, info);
}
