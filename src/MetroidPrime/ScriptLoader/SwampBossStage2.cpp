#include "MetroidPrime/ScriptLoader.hpp"

// SwampBossStage2 - retail 0x8022EC04, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419600 and calls member 0 of it.
//
// This unit claims .text 0x8022EC04..0x8022EC30 and .sbss 0x80419600..0x80419608. The 8-byte
// setter at 0x8022EC30 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SwampBossStage2;

CEntity* LoadSwampBossStage2(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_SwampBossStage2.value)(mgr, input, info);
}
