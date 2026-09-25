#include "MetroidPrime/ScriptLoader.hpp"

// MediumIng - retail 0x80218A40, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804193F8 and calls member 0 of it.
//
// This unit claims .text 0x80218A40..0x80218A6C and .sbss 0x804193F8..0x80419400. The 8-byte
// setter at 0x80218A6C is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_MediumIng;

CEntity* LoadMediumIng(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_MediumIng.value)(mgr, input, info);
}
