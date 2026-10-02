#include "MetroidPrime/ScriptLoader.hpp"

// Blogg - retail 0x80218ADC, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419410 and calls member 0 of it.
//
// This unit claims .text 0x80218ADC..0x80218B08 and .sbss 0x80419410..0x80419418. The 8-byte
// setter at 0x80218B08 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Blogg;

CEntity* LoadBlogg(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Blogg.value)(mgr, input, info);
}
