#include "MetroidPrime/ScriptLoader.hpp"

// GlowBug - retail 0x80218DFC, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419470 and calls member 0 of it.
//
// This unit claims .text 0x80218DFC..0x80218E28 and .sbss 0x80419470..0x80419478. The 8-byte
// setter at 0x80218E28 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_GlowBug;

CEntity* LoadGlowBug(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_GlowBug.value)(mgr, input, info);
}
