#include "MetroidPrime/ScriptLoader.hpp"

// MetareeSwarm - retail 0x8022D57C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195E0 and calls member 0 of it.
//
// This unit claims .text 0x8022D57C..0x8022D5A8 and .sbss 0x804195E0..0x804195E8. The 8-byte
// setter at 0x8022D5A8 is its own unit, `Carve8022D5A8.c`: REL modules import it by its
// retail name, so it stays `fn_8022D5A8` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_MetareeSwarm` would mangle. It references `gLoader_MetareeSwarm` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MetareeSwarm;

CEntity* LoadMetareeSwarm(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_MetareeSwarm.value)(mgr, input, info);
}
