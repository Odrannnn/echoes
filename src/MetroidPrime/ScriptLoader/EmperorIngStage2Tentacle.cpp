#include "MetroidPrime/ScriptLoader.hpp"

// EmperorIngStage2Tentacle - retail 0x8022A544, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195C0 and calls member 0 of it.
//
// This unit claims .text 0x8022A544..0x8022A570 and .sbss 0x804195C0..0x804195C8. The 8-byte
// setter at 0x8022A570 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_EmperorIngStage2Tentacle;

CEntity* LoadEmperorIngStage2Tentacle(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_EmperorIngStage2Tentacle.value)(mgr, input, info);
}
