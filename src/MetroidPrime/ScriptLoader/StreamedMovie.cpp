#include "MetroidPrime/ScriptLoader.hpp"

// StreamedMovie - retail 0x80229FC4, 44 bytes, a vtable thunk: it loads the loader-struct
// pointer out of the .sbss slot at 0x804195A8 and calls member 0 of it.
//
// This unit claims .text 0x80229FC4..0x80229FF0 and .sbss 0x804195A8..0x804195B0. The 8-byte
// setter at 0x80229FF0 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit.
// docs/research/rel_loaders.md has every loader in this family.

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

SLoaderSlot gLoader_StreamedMovie;

CEntity* LoadStreamedMovie(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  return (*gLoader_StreamedMovie.value)(mgr, input, info);
}
