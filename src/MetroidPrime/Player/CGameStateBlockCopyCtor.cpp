/**
 * `fn_80004AA0` - retail `.text:0x80004AA0`, `size:0xFC` = 252 bytes: the copy constructor of the
 * 16-byte `SGameStateBlock` (`include/MetroidPrime/Player/CGameStateBlocks.hpp`), which is
 * `rstl::vector<unsigned char>` field for field. It is the body `rstl/vector.hpp`'s
 * `vector(const vector&)` has - count and capacity copied, a null buffer when both are zero,
 * otherwise `allocate(capacity)` and a byte copy of `count` bytes (mwcceppc's 8-way unroll) - and
 * it returns `this`.
 *
 * Callers: `fn_80004D5C` (the null-guarded construct, `CGameStateBlockConstruct.cpp`), which
 * `CGameState`'s default constructor reaches six times through `fn_80144924`, and
 * `CMain::ResetGameState` (`src/MetroidPrime/CMainResetGameState.cpp`).
 */

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

#include "rstl/rmemory_allocator.hpp"

extern "C" {
SGameStateBlock* fn_80004AA0(SGameStateBlock* self, const SGameStateBlock* src) {
  self->x04_count = src->x04_count;
  self->x08_cap = src->x08_cap;
  if (src->x04_count == 0 && src->x08_cap == 0) {
    self->x0c_data = 0;
  } else {
    self->x0c_data = rstl::rmemory_allocator::allocate(self->x08_cap);
    const unsigned char* from = static_cast< const unsigned char* >(src->x0c_data);
    unsigned char* to = static_cast< unsigned char* >(self->x0c_data);
    for (int n = self->x04_count; n != 0; --n) {
      *to++ = *from++;
    }
  }
  return self;
}
} // extern "C"
