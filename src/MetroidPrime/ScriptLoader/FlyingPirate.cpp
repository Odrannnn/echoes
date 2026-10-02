#include "MetroidPrime/ScriptLoader.hpp"

// FlyingPirate - retail 0x802189D8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193E8 and calls member 0 of it.
//
// This unit claims .text 0x802189D8..0x80218A04 and .sbss 0x804193E8..0x804193F0. The 8-byte
// setter at 0x80218A04 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_FlyingPirate;

CEntity* LoadFlyingPirate(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_FlyingPirate.value)(mgr, input, info);
}
