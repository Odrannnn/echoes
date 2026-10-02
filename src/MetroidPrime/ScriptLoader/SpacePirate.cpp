#include "MetroidPrime/ScriptLoader.hpp"

// SpacePirate - retail 0x80200E10, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419358 and calls member 0 of it.
//
// This unit claims .text 0x80200E10..0x80200E3C and .sbss 0x80419358..0x80419360. The 8-byte
// setter at 0x80200E3C is its own unit, `Carve80200E3C.c`: REL modules import it by its
// retail name, so it stays `fn_80200E3C` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_SpacePirate` would mangle. It references `gLoader_SpacePirate` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SpacePirate;

CEntity* LoadSpacePirate(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_SpacePirate.value)(mgr, input, info);
}
