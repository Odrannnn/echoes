#include "MetroidPrime/ScriptLoader.hpp"

// MetroidAlpha - retail 0x80218B3C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419418 and calls member 0 of it.
//
// This unit claims .text 0x80218B3C..0x80218B68 and .sbss 0x80419418..0x80419420. The 8-byte
// setter at 0x80218B68 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MetroidAlpha;

CEntity* LoadMetroidAlpha(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_MetroidAlpha.value)(mgr, input, info);
}
