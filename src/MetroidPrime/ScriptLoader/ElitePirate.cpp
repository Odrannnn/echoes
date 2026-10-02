#include "MetroidPrime/ScriptLoader.hpp"

// ElitePirate - retail 0x80218AA8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419408 and calls member 0 of it.
//
// This unit claims .text 0x80218AA8..0x80218AD4 and .sbss 0x80419408..0x80419410. The 8-byte
// setter at 0x80218AD4 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_ElitePirate;

CEntity* LoadElitePirate(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_ElitePirate.value)(mgr, input, info);
}
