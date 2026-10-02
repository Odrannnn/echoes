#include "MetroidPrime/ScriptLoader.hpp"

// IngSpiderBallGuardian - retail 0x80229FF8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195B0 and calls member 0 of it.
//
// This unit claims .text 0x80229FF8..0x8022A024 and .sbss 0x804195B0..0x804195B8. The 8-byte
// setter at 0x8022A024 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_IngSpiderBallGuardian;

CEntity* LoadIngSpiderBallGuardian(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_IngSpiderBallGuardian.value)(mgr, input, info);
}
