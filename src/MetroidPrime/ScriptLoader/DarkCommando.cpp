#include "MetroidPrime/ScriptLoader.hpp"

// DarkCommando - retail 0x80235DD4, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x80419660 and calls member 0 of it.
//
// This unit claims .text 0x80235DD4..0x80235E00 and .sbss 0x80419660..0x80419668. The 8-byte
// setter at 0x80235E00 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_DarkCommando;

CEntity* LoadDarkCommando(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  return (*gLoader_DarkCommando.value)(mgr, input, info);
}
