#include "MetroidPrime/ScriptLoader.hpp"

// IngPuddle - retail 0x80229EB4, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419598 and calls member 0 of it.
//
// This unit claims .text 0x80229EB4..0x80229EE0 and .sbss 0x80419598..0x804195A0. The 8-byte
// setter at 0x80229EE0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_IngPuddle;

CEntity* LoadIngPuddle(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_IngPuddle.value)(mgr, input, info);
}
