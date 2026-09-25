#include "MetroidPrime/ScriptLoader.hpp"

// SpacePirate - retail 0x80200E10, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419358 and calls member 0 of it.
//
// This unit claims .text 0x80200E10..0x80200E3C and .sbss 0x80419358..0x80419360. The 8-byte
// setter at 0x80200E3C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SpacePirate;

CEntity* LoadSpacePirate(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_SpacePirate.value)(mgr, input, info);
}
