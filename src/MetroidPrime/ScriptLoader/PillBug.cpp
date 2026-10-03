#include "MetroidPrime/ScriptLoader.hpp"

// PillBug - retail 0x80200F04, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419370 and calls member 0 of it.
//
// This unit claims .text 0x80200F04..0x80200F30 and .sbss 0x80419370..0x80419378. The 8-byte
// setter at 0x80200F30 is its own unit, `Carve80200F30.c`: REL modules import it by its
// retail name, so it stays `fn_80200F30` verbatim, and a carve reproduces that name where a
// C++ `SetLoader_PillBug` would mangle. It references `gLoader_PillBug` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_PillBug;

CEntity* LoadPillBug(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_PillBug.value)(mgr, input, info);
}
