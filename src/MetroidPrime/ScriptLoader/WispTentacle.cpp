#include "MetroidPrime/ScriptLoader.hpp"

// WispTentacle - retail 0x80218D60, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419458 and calls member 0 of it.
//
// This unit claims .text 0x80218D60..0x80218D8C and .sbss 0x80419458..0x80419460. The 8-byte
// setter at 0x80218D8C is its own unit, `Carve80218D8C.c`: REL modules import it by its
// retail name, so it stays `fn_80218D8C` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_WispTentacle` would mangle. It references `gLoader_WispTentacle` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_WispTentacle;

CEntity* LoadWispTentacle(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_WispTentacle.value)(mgr, input, info);
}
