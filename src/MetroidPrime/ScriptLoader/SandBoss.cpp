#include "MetroidPrime/ScriptLoader.hpp"

// SandBoss - retail 0x802189A4, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193E0 and calls member 0 of it.
//
// This unit claims .text 0x802189A4..0x802189D0 and .sbss 0x804193E0..0x804193E8. The 8-byte
// setter at 0x802189D0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SandBoss;

CEntity* LoadSandBoss(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_SandBoss.value)(mgr, input, info);
}
