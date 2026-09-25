#include "MetroidPrime/ScriptLoader.hpp"

// RsfAudio - retail 0x80227B00, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419588 and calls member 0 of it.
//
// This unit claims .text 0x80227B00..0x80227B2C and .sbss 0x80419588..0x80419590. The 8-byte
// setter at 0x80227B2C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_RsfAudio;

CEntity* LoadRsfAudio(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_RsfAudio.value)(mgr, input, info);
}
