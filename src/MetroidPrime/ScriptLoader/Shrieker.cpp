#include "MetroidPrime/ScriptLoader.hpp"

// Shrieker - retail 0x80218C04, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419430 and calls member 0 of it.
//
// This unit claims .text 0x80218C04..0x80218C30 and .sbss 0x80419430..0x80419438. The 8-byte
// setter at 0x80218C30 is its own unit, `Carve80218C30.c`: REL modules import it by its
// retail name, so it stays `fn_80218C30` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_Shrieker` would mangle. It references `gLoader_Shrieker` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Shrieker;

CEntity* LoadShrieker(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Shrieker.value)(mgr, input, info);
}
