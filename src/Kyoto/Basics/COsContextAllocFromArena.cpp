// COsContext::AllocFromArena - retail 0x8028BFFC, 0x5C bytes.
//
// dtk left this `fn_8028BFFC`; it is COsContext::AllocFromArena, renamed in
// config/G2ME01/symbols.txt. The rename is what makes this unit possible at
// all: with the `fn_` name objdiff pairs nothing, so the object would score
// 0/0 and look like an empty unit rather than a 100% one.
//
// The identification is not a guess from the name. Three things pin it:
//   - retail `main` (0x801EFB00) never calls this directly, but the COsContext
//     constructor at 0x8028C09C does, with r4 = this->x2c_frameBufferSize and
//     r3 = this, i.e. exactly (this, size);
//   - the body bumps MEM1 with OSAllocFromArenaLo(sz, 32) and then refreshes
//     all three arena fields, which is what `GetBaseFreeRam()` - read by
//     CGameAllocator::Initialize - depends on;
//   - retail's own inlined `GetBaseFreeRam` in CGameAllocator::Initialize
//     (0x8030EA48) reads +0x1C/+0x20, the two fields this writes.
//
// Note the *order* of the two OSGetArenaLo calls: +0x20 is refreshed first and
// +0x18 second. That is retail's, and swapping them is still byte-identical to
// nothing - it is two stores, so the order is visible in the object.
#include "Kyoto/Basics/COsContext.hpp"

#include "dolphin/os.h"

void* COsContext::AllocFromArena(size_t sz) {
  void* ret = OSAllocFromArenaLo(sz, 32);
  x20_arenaLo2 = OSGetArenaLo();
  x18_arenaLo1 = OSGetArenaLo();
  x1c_arenaHi = OSGetArenaHi();
  return ret;
}
