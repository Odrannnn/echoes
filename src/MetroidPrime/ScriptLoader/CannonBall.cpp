#include "MetroidPrime/ScriptLoader.hpp"

// CannonBall - retail 0x8021FA88, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419538 and calls member 0 of it.
//
// This unit claims .text 0x8021FA88..0x8021FAB4 and .sbss 0x80419538..0x80419540. The 8-byte
// setter at 0x8021FAB4 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_CannonBall;

CEntity* LoadCannonBall(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_CannonBall.value)(mgr, input, info);
}
