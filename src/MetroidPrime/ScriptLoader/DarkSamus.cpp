#include "MetroidPrime/ScriptLoader.hpp"

// DarkSamus - retail 0x802188B8, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193D0 and calls member 0 of it.
//
// This unit claims .text 0x802188B8..0x802188E4 and .sbss 0x804193D0..0x804193D8. The 8-byte
// setter at 0x802188E4 is claimed by the `Matching` unit
// `src/MetroidPrime/ScriptLoader/Carve802188E4.c`, which keeps the retail name the DarkSamus
// module imports it by; this unit still owns the slot.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_DarkSamus;

CEntity* LoadDarkSamus(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_DarkSamus.value)(mgr, input, info);
}
