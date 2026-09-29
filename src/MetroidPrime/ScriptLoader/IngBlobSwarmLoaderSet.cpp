// Retail `fn_8022E134`, .text 0x8022E134..0x8022E13C, 8 bytes:
//
//     8022e134  stw  r3,-26520(r13)
//     8022e138  blr
//
// `-26520(r13)` off `_SDA_BASE_` = 0x8041FD80 is 0x804195E8, the `gLoader_IngBlobSwarm`
// slot owned by `MetroidPrime/ScriptLoader/IngBlobSwarm.cpp` (`.sbss 0x804195E8..0x804195F0`)
// and dereferenced by `LoadIngBlobSwarm`. This is its setter: `gLoader_IngBlobSwarm.value =
// loader`; `value` is at +0 of the 8-byte slot, so the store has no displacement.
//
// This must be a separate unit: IngBlobSwarm.cpp claims .text 0x8022E108..0x8022E134, directly
// below this function, and one unit cannot claim two discontiguous ranges in one section. The REL
// imports this function as `fn_8022E134`, so keep its retail name and C linkage.
#include "MetroidPrime/ScriptLoader.hpp"

struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

extern SLoaderSlot gLoader_IngBlobSwarm;

extern "C" void fn_8022E134(FScriptLoader* loader) { gLoader_IngBlobSwarm.value = loader; }
