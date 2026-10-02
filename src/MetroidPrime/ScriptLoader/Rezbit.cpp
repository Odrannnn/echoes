#include "MetroidPrime/ScriptLoader.hpp"

// Rezbit - retail 0x80227ACC, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419580 and calls member 0 of it.
//
// This unit claims .text 0x80227ACC..0x80227AF8 and .sbss 0x80419580..0x80419588. The 8-byte
// setter at 0x80227AF8 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Rezbit;

CEntity* LoadRezbit(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Rezbit.value)(mgr, input, info);
}
