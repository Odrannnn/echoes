#include "MetroidPrime/ScriptLoader.hpp"

// Coin - retail 0x8021FA54, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419530 and calls member 0 of it.
//
// This unit claims .text 0x8021FA54..0x8021FA80 and .sbss 0x80419530..0x80419538. The 8-byte
// setter at 0x8021FA80 is a separate `Matching` unit, `CoinLoaderSet.cpp`: the name is fixed
// because the REL module imports it, but the *file* is not - a unit may not
// claim two discontiguous ranges in one section.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_Coin;

CEntity* LoadCoin(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_Coin.value)(mgr, input, info);
}
