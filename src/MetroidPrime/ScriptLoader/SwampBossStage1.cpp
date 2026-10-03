#include "MetroidPrime/ScriptLoader.hpp"

// SwampBossStage1 - retail 0x8022EC38, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419608 and calls member 0 of it.
//
// This unit claims .text 0x8022EC38..0x8022EC64 and .sbss 0x80419608..0x80419610. The 8-byte
// setter at 0x8022EC64 was deliberately NOT claimed when this unit was written - REL modules
// import it by its retail name, so it cannot be renamed - but it is carved out now as the
// `Matching` unit `MetroidPrime/ScriptLoader/Carve8022EC64.c`, which defines the unmangled
// `fn_8022EC64` and stores its argument into the slot declared here.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_SwampBossStage1;

CEntity* LoadSwampBossStage1(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_SwampBossStage1.value)(mgr, input, info);
}
