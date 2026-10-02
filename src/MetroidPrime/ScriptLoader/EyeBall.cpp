#include "MetroidPrime/ScriptLoader.hpp"

// EyeBall - retail 0x80234900, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419650 and calls member 0 of it.
//
// This unit claims .text 0x80234900..0x8023492C and .sbss 0x80419650..0x80419658. The 8-byte
// setter at 0x8023492C is its own unit, `Carve8023492C.c`: REL modules import it by its
// retail name, so it stays `fn_8023492C` verbatim, and a carve reproduces that name where a C++
// `SetLoader_EyeBall` would mangle. It references `gLoader_EyeBall` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_EyeBall;

CEntity* LoadEyeBall(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_EyeBall.value)(mgr, input, info);
}
