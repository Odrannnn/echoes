#include "MetroidPrime/ScriptLoader.hpp"

// Shredder - retail 0x8021F9B8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419518 and calls member 0 of it.
//
// This unit claims .text 0x8021F9B8..0x8021F9E4 and .sbss 0x80419518..0x80419520. The 8-byte
// setter at 0x8021F9E4 is not claimed either: it is Carve8021F9E4.c, which keeps its unmangled
// `fn_8021F9E4` so the module still imports the retail name.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Shredder;

CEntity* LoadShredder(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Shredder.value)(mgr, input, info);
}
