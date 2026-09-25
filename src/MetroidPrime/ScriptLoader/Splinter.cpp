#include "MetroidPrime/ScriptLoader.hpp"

// Splinter - retail 0x80218C38, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419438 and calls member 0 of it.
//
// This unit claims .text 0x80218C38..0x80218C64 and .sbss 0x80419438..0x80419440. The 8-byte
// setter at 0x80218C64 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Splinter;

CEntity* LoadSplinter(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_Splinter.value)(mgr, input, info);
}
