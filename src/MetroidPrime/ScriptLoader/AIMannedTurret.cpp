#include "MetroidPrime/ScriptLoader.hpp"

// AIMannedTurret - retail 0x80227504, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419560 and calls member 0 of it.
//
// This unit claims .text 0x80227504..0x80227530 and .sbss 0x80419560..0x80419568. The 8-byte
// setter at 0x80227530 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_AIMannedTurret;

CEntity* LoadAIMannedTurret(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_AIMannedTurret.value)(mgr, input, info);
}
