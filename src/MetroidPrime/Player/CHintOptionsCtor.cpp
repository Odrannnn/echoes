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
//
// **After the merge to upstream PrimeDecomp/echoes, this is a view and not a set of members.**
// Upstream's `CHintOptions` is `{ rstl::vector<SHintState> mHintStates; int mNextHintIdx;
// bool mInRezbitState; bool mScanDisplayActive; }`, 0x18 bytes, and each of the six offsets
// above is inside it: `rstl::vector` is four words at `+0x00`, so `+0x04`/`+0x08`/`+0x0C` are
// its end pointer, its capacity and its allocator, `+0x10` is `mNextHintIdx` (an `int`, set to
// -1 here, same value), and `+0x14`/`+0x15` are the two `bool`s. The three vector words are
// `rstl::vector`'s private members and mwcceppc deletes the construction of a member this
// translation unit never reads, so a member spelling cannot produce the six stores at all - which
// is the same finding the old header recorded, and the reason the struct below is the same
// declaration with the same types in the same order: the generated code is the six stores and
// nothing else.
namespace {
// The 0x18 bytes of `CGameState+0xC4`, laid out as this function writes them.
struct SHintOptionsRaw {
  int x00_unk;    //!< +0x00, left alone by the constructor
  int x04_count;  //!< +0x04, the element count
  int x08_cap;    //!< +0x08, the capacity the stream constructor loops to
  void* x0c_data; //!< +0x0C, 12-byte {int, float, float} elements
  int x10_unk;    //!< +0x10, -1 until a particular element type is seen
  bool x14;       //!< +0x14
  bool x15;       //!< +0x15
};
CHECK_SIZEOF(SHintOptionsRaw, 0x18)
} // namespace

extern "C" void fn_80180738(CHintOptions* self) {
  SHintOptionsRaw* raw = reinterpret_cast< SHintOptionsRaw* >(self);
  raw->x04_count = 0;
  raw->x08_cap = 0;
  raw->x0c_data = nullptr;
  raw->x10_unk = -1;
  raw->x14 = false;
  raw->x15 = false;
}
