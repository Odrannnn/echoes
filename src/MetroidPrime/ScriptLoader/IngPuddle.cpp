#include "MetroidPrime/ScriptLoader.hpp"

// IngPuddle - retail 0x80229EB4, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419598 and calls member 0 of it.
//
// This unit claims .text 0x80229EB4..0x80229EE0 and .sbss 0x80419598..0x804195A0. The 8-byte
// setter at 0x80229EE0 is its own unit, `Carve80229EE0.c`: REL modules import it by its
// retail name, so it stays `fn_80229EE0` verbatim, and a carve reproduces that name where a C++
// `SetLoader_IngPuddle` would mangle. It references `gLoader_IngPuddle` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_IngPuddle;

CEntity* LoadIngPuddle(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_IngPuddle.value)(mgr, input, info);
}
