#include "MetroidPrime/ScriptLoader.hpp"

// RubiksPuzzle - retail 0x802399C8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804196C0 and calls member 0 of it.
//
// This unit claims .text 0x802399C8..0x802399F4 and .sbss 0x804196C0..0x804196C8. The 8-byte
// setter at 0x802399F4 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_RubiksPuzzle;

CEntity* LoadRubiksPuzzle(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_RubiksPuzzle.value)(mgr, input, info);
}
