#include "MetroidPrime/ScriptLoader.hpp"

// ChozoGhost - retail 0x80218CF8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419448 and calls member 0 of it.
//
// This unit claims .text 0x80218CF8..0x80218D24 and .sbss 0x80419448..0x80419450. The 8-byte
// setter at 0x80218D24 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_ChozoGhost;

CEntity* LoadChozoGhost(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_ChozoGhost.value)(mgr, input, info);
}
