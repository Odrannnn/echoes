#include "MetroidPrime/ScriptLoader.hpp"

// OctopedeSegment - retail 0x80227574, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419578 and calls member 0 of it.
//
// This unit claims .text 0x80227574..0x802275A0 and .sbss 0x80419578..0x80419580. The 8-byte
// setter at 0x802275A0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_OctopedeSegment;

CEntity* LoadOctopedeSegment(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_OctopedeSegment.value)(mgr, input, info);
}
