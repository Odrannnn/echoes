#include "MetroidPrime/ScriptLoader.hpp"

// EmperorIngStage1 - retail 0x80227540, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419570 and calls member 0 of it.
//
// This unit claims .text 0x80227540..0x8022756C and .sbss 0x80419570..0x80419578. The 8-byte
// setter at 0x8022756C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_EmperorIngStage1;

CEntity* LoadEmperorIngStage1(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_EmperorIngStage1.value)(mgr, input, info);
}
