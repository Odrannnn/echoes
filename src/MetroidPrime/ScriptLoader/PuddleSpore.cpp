#include "MetroidPrime/ScriptLoader.hpp"

// PuddleSpore - retail 0x8022A02C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195B8 and calls member 0 of it.
//
// This unit claims .text 0x8022A02C..0x8022A058 and .sbss 0x804195B8..0x804195C0. The 8-byte
// setter at 0x8022A058 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_PuddleSpore;

CEntity* LoadPuddleSpore(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_PuddleSpore.value)(mgr, input, info);
}
