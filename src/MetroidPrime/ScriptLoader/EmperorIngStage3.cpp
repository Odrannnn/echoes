#include "MetroidPrime/ScriptLoader.hpp"

// EmperorIngStage3 - retail 0x8022EB9C, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195F0 and calls member 0 of it.
//
// This unit claims .text 0x8022EB9C..0x8022EBC8 and .sbss 0x804195F0..0x804195F8. The 8-byte
// setter at 0x8022EBC8 is its own unit, `Carve8022EBC8.c`: REL modules import it by its
// retail name, so it stays `fn_8022EBC8` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_EmperorIngStage3` would mangle. It references `gLoader_EmperorIngStage3` as
// `extern` rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_EmperorIngStage3;

CEntity* LoadEmperorIngStage3(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_EmperorIngStage3.value)(mgr, input, info);
}
