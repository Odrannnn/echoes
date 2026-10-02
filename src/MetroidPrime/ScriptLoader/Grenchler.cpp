#include "MetroidPrime/ScriptLoader.hpp"

// Grenchler - retail 0x80218A0C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193F0 and calls member 0 of it.
//
// This unit claims .text 0x80218A0C..0x80218A38 and .sbss 0x804193F0..0x804193F8. The 8-byte
// setter at 0x80218A38 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Grenchler;

CEntity* LoadGrenchler(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Grenchler.value)(mgr, input, info);
}
