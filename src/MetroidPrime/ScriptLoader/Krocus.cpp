#include "MetroidPrime/ScriptLoader.hpp"

// Krocus - retail 0x802274C8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419550 and calls member 0 of it.
//
// This unit claims .text 0x802274C8..0x802274F4 and .sbss 0x80419550..0x80419558. The 8-byte
// setter at 0x802274F4 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Krocus;

CEntity* LoadKrocus(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_Krocus.value)(mgr, input, info);
}
