#include "MetroidPrime/ScriptLoader.hpp"

// DarkSamusBattleStage - retail 0x80235DA0, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419658 and calls member 0 of it.
//
// This unit claims .text 0x80235DA0..0x80235DCC and .sbss 0x80419658..0x80419660. The 8-byte
// setter at 0x80235DCC is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_DarkSamusBattleStage;

CEntity* LoadDarkSamusBattleStage(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_DarkSamusBattleStage.value)(mgr, input, info);
}
