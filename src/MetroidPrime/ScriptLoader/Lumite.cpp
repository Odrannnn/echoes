#include "MetroidPrime/ScriptLoader.hpp"

// Lumite - retail 0x80218BD0, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419428 and calls member 0 of it.
//
// This unit claims .text 0x80218BD0..0x80218BFC and .sbss 0x80419428..0x80419430. The 8-byte
// setter at 0x80218BFC is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Lumite;

CEntity* LoadLumite(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_Lumite.value)(mgr, input, info);
}
