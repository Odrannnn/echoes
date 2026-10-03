#include "MetroidPrime/ScriptLoader.hpp"

// SandBoss - retail 0x802189A4, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193E0 and calls member 0 of it.
//
// This unit claims .text 0x802189A4..0x802189D0 and .sbss 0x804193E0..0x804193E8. The 8-byte
// setter at 0x802189D0 is its own unit, `Carve802189D0.c`: REL modules import it by its
// retail name, so it stays `fn_802189D0` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_SandBoss` would mangle. It references `gLoader_SandBoss` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SandBoss;

CEntity* LoadSandBoss(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_SandBoss.value)(mgr, input, info);
}
