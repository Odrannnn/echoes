#include "MetroidPrime/ScriptLoader.hpp"

// EyeBall - retail 0x80234900, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419650 and calls member 0 of it.
//
// This unit claims .text 0x80234900..0x8023492C and .sbss 0x80419650..0x80419658. The 8-byte
// setter at 0x8023492C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_EyeBall;

CEntity* LoadEyeBall(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_EyeBall.value)(mgr, input, info);
}
