#include "MetroidPrime/ScriptLoader.hpp"

// Sandworm - retail 0x80218850, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193C0 and calls member 0 of it.
//
// This unit claims .text 0x80218850..0x8021887C and .sbss 0x804193C0..0x804193C8. The 8-byte
// setter at 0x8021887C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Sandworm;

CEntity* LoadSandworm(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Sandworm.value)(mgr, input, info);
}
