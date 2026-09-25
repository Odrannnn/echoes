#include "MetroidPrime/ScriptLoader.hpp"

// SpankWeed - retail 0x80218D94, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419460 and calls member 0 of it.
//
// This unit claims .text 0x80218D94..0x80218DC0 and .sbss 0x80419460..0x80419468. The 8-byte
// setter at 0x80218DC0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SpankWeed;

CEntity* LoadSpankWeed(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_SpankWeed.value)(mgr, input, info);
}
