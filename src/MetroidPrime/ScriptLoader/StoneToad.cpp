#include "MetroidPrime/ScriptLoader.hpp"

// StoneToad - retail 0x8021FA20, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419528 and calls member 0 of it.
//
// This unit claims .text 0x8021FA20..0x8021FA4C and .sbss 0x80419528..0x80419530. The 8-byte
// setter at 0x8021FA4C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_StoneToad;

CEntity* LoadStoneToad(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_StoneToad.value)(mgr, input, info);
}
