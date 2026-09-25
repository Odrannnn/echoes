#include "MetroidPrime/ScriptLoader.hpp"

// MysteryFlyer - retail 0x8023283C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419640 and calls member 0 of it.
//
// This unit claims .text 0x8023283C..0x80232868 and .sbss 0x80419640..0x80419648. The 8-byte
// setter at 0x80232868 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MysteryFlyer;

CEntity* LoadMysteryFlyer(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_MysteryFlyer.value)(mgr, input, info);
}
