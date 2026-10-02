#include "MetroidPrime/ScriptLoader.hpp"

// FogOverlay - retail 0x80232808, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419638 and calls member 0 of it.
//
// This unit claims .text 0x80232808..0x80232834 and .sbss 0x80419638..0x80419640. The 8-byte
// setter at 0x80232834 is its own unit, `Carve80232834.c`: REL modules import it by its retail
// name, so it stays `fn_80232834` verbatim, and a carve reproduces that name where a C++
// `SetLoader_FogOverlay` would mangle. It references `gLoader_FogOverlay` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_FogOverlay;

CEntity* LoadFogOverlay(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_FogOverlay.value)(mgr, input, info);
}
