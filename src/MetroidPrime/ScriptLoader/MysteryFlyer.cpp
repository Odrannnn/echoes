#include "MetroidPrime/ScriptLoader.hpp"

// MysteryFlyer - retail 0x8023283C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419640 and calls member 0 of it.
//
// This unit claims .text 0x8023283C..0x80232868 and .sbss 0x80419640..0x80419648. The 8-byte
// setter at 0x80232868 is its own unit, `Carve80232868.c`: REL modules import it by its retail
// name, so it stays `fn_80232868` verbatim, and a carve reproduces that name where a C++
// `SetLoader_MysteryFlyer` would mangle. It references `gLoader_MysteryFlyer` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MysteryFlyer;

CEntity* LoadMysteryFlyer(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_MysteryFlyer.value)(mgr, input, info);
}
