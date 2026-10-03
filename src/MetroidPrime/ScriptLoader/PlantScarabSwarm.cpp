#include "MetroidPrime/ScriptLoader.hpp"

// PlantScarabSwarm - retail 0x8022FFCC, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419618 and calls member 0 of it.
//
// This unit claims .text 0x8022FFCC..0x8022FFF8 and .sbss 0x80419618..0x80419620. The 8-byte
// setter at 0x8022FFF8 is its own unit, `Carve8022FFF8.c`: REL modules import it by its
// retail name, so it stays `fn_8022FFF8` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_PlantScarabSwarm` would mangle. It references `gLoader_PlantScarabSwarm` as
// `extern` rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_PlantScarabSwarm;

CEntity* LoadPlantScarabSwarm(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_PlantScarabSwarm.value)(mgr, input, info);
}
