#include "MetroidPrime/ScriptLoader.hpp"

// DarkTrooper - retail 0x80218DC8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419468 and calls member 0 of it.
//
// This unit claims .text 0x80218DC8..0x80218DF4 and .sbss 0x80419468..0x80419470. The 8-byte
// setter at 0x80218DF4 is its own unit, `Carve80218DF4.c`: REL modules import it by its
// retail name, so it stays `fn_80218DF4` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_DarkTrooper` would mangle. It references `gLoader_DarkTrooper` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_DarkTrooper;

CEntity* LoadDarkTrooper(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_DarkTrooper.value)(mgr, input, info);
}
