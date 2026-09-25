#include "MetroidPrime/ScriptLoader.hpp"

// MinorIng - retail 0x80218A74, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419400 and calls member 0 of it.
//
// This unit claims .text 0x80218A74..0x80218AA0 and .sbss 0x80419400..0x80419408. The 8-byte
// setter at 0x80218AA0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MinorIng;

CEntity* LoadMinorIng(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_MinorIng.value)(mgr, input, info);
}
