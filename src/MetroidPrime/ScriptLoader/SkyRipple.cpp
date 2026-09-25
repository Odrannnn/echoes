#include "MetroidPrime/ScriptLoader.hpp"

// SkyRipple - retail 0x80232308, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419630 and calls member 0 of it.
//
// This unit claims .text 0x80232308..0x80232334 and .sbss 0x80419630..0x80419638. The 8-byte
// setter at 0x80232334 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SkyRipple;

CEntity* LoadSkyRipple(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_SkyRipple.value)(mgr, input, info);
}
