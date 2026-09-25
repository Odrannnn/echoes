#include "MetroidPrime/ScriptLoader.hpp"

// IngSpaceJumpGuardian - retail 0x8021DC00, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804194F8 and calls member 0 of it.
//
// This unit claims .text 0x8021DC00..0x8021DC2C and .sbss 0x804194F8..0x80419500. The 8-byte
// setter at 0x8021DC2C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_IngSpaceJumpGuardian;

CEntity* LoadIngSpaceJumpGuardian(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_IngSpaceJumpGuardian.value)(mgr, input, info);
}
