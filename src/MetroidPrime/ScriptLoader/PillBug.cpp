#include "MetroidPrime/ScriptLoader.hpp"

// PillBug - retail 0x80200F04, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419370 and calls member 0 of it.
//
// This unit claims .text 0x80200F04..0x80200F30 and .sbss 0x80419370..0x80419378. The 8-byte
// setter at 0x80200F30 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_PillBug;

CEntity* LoadPillBug(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_PillBug.value)(mgr, input, info);
}
