#include "MetroidPrime/ScriptLoader.hpp"

// CommandPirate - retail 0x80218884, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193C8 and calls member 0 of it.
//
// This unit claims .text 0x80218884..0x802188B0 and .sbss 0x804193C8..0x804193D0. The 8-byte
// setter at 0x802188B0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_CommandPirate;

CEntity* LoadCommandPirate(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_CommandPirate.value)(mgr, input, info);
}
