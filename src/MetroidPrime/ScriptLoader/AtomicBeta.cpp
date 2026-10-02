#include "MetroidPrime/ScriptLoader.hpp"

// AtomicBeta - retail 0x80232870, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419648 and calls member 0 of it.
//
// This unit claims .text 0x80232870..0x8023289C and .sbss 0x80419648..0x80419650. The 8-byte
// setter at 0x8023289C is its own unit, `Carve8023289C.c`: REL modules import it by its
// retail name, so it stays `fn_8023289C` verbatim, and a carve reproduces that name where a C++
// `SetLoader_AtomicBeta` would mangle. It references `gLoader_AtomicBeta` as `extern`
// rather than claiming `.sbss` again.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_AtomicBeta;

CEntity* LoadAtomicBeta(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_AtomicBeta.value)(mgr, input, info);
}
