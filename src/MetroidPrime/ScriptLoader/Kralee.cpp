#include "MetroidPrime/ScriptLoader.hpp"

// Kralee - retail 0x80200E44, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419360 and calls member 0 of it.
//
// This unit claims .text 0x80200E44..0x80200E70 and .sbss 0x80419360..0x80419368. The 8-byte
// setter at 0x80200E70 is its own unit, `Carve80200E70.c`: REL modules import it by its
// retail name, so it stays `fn_80200E70` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_Kralee` would mangle. It references `gLoader_Kralee` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Kralee;

CEntity* LoadKralee(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Kralee.value)(mgr, input, info);
}
