#include "MetroidPrime/ScriptLoader.hpp"

// FlyerSwarm - retail 0x80229F90, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195A0 and calls member 0 of it.
//
// This unit claims .text 0x80229F90..0x80229FBC and .sbss 0x804195A0..0x804195A8. The 8-byte
// setter at 0x80229FBC is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_FlyerSwarm;

CEntity* LoadFlyerSwarm(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_FlyerSwarm.value)(mgr, input, info);
}
