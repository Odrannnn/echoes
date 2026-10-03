#include "MetroidPrime/ScriptLoader.hpp"

// IngBoostBallGuardian - retail 0x8022FF98, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419610 and calls member 0 of it.
//
// This unit claims .text 0x8022FF98..0x8022FFC4 and .sbss 0x80419610..0x80419618. The 8-byte
// setter at 0x8022FFC4 was deliberately NOT claimed: REL modules import it by its retail name,
// so it cannot be renamed. That name is now preserved by Carve8022FFC4.c, which claims the
// range as its own Matching unit; the slot itself stays here and that unit takes it as extern.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_IngBoostBallGuardian;

CEntity* LoadIngBoostBallGuardian(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_IngBoostBallGuardian.value)(mgr, input, info);
}
