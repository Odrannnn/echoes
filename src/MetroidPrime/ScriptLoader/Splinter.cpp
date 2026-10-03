#include "MetroidPrime/ScriptLoader.hpp"

// Splinter - retail 0x80218C38, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419438 and calls member 0 of it.
//
// This unit claims .text 0x80218C38..0x80218C64 and .sbss 0x80419438..0x80419440. The 8-byte
// setter at 0x80218C64 is claimed by Carve80218C64.cpp, not here: the name constraint is why it
// is that file (see its header). docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Splinter;

CEntity* LoadSplinter(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Splinter.value)(mgr, input, info);
}
