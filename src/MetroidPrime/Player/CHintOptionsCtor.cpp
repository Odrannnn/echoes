#include "types.h"

#include "MetroidPrime/Player/CHintOptions.hpp"

// Retail 0x80180738, 0x24 = 36 bytes, and the whole body is six stores and a `blr`. It is the
// default constructor of the class at `CGameState+0xC4` - `CGameState`'s `CHintOptions` member -
// reached from `CGameState::CGameState(CInputStream&, int)` (retail fn_80144140, 0x80144140) at
// 0x801441EC, and it is the same six stores the class's stream constructor `fn_801805EC`
// (0x801805EC, 0x14C) begins with.
//
// Named for the address because retail's symbol table has no name for it; the symbol the port
// emits is `fn_80180738`, which is what `config/G2ME01/symbols.txt` already calls it, so nothing
// is renamed and the REL modules that reference it are untouched. It is an `extern "C"` function
// rather than `CHintOptions::CHintOptions()` because a C++ constructor mangles to
// `__ct__9CHintOptionsFv` and objdiff would have nothing in the retail object to pair it with.
//
// The single fact that makes this matchable rather than 95% is the member *order* and the fact
// that +0x00 is not one of them: retail stores +0x04, +0x08, +0x0C, +0x10, +0x14, +0x15 in that
// order and never touches +0x00, so the six initialisations below are in declaration order and
// `x00_unk` is left alone. Writing a zero initialiser for `x00_unk` adds a seventh store and
// costs 4 bytes.
extern "C" void fn_80180738(CHintOptions* self) {
  self->x04_count = 0;
  self->x08_cap = 0;
  self->x0c_data = nullptr;
  self->x10_unk = -1;
  self->x14 = false;
  self->x15 = false;
}
