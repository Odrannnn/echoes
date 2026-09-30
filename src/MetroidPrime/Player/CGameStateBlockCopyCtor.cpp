/**
 * `fn_80004AA0` - retail `.text:0x80004AA0`, `size:0xFC` = 252 bytes: the copy constructor of the
 * 16-byte `SGameStateBlock` (`include/MetroidPrime/Player/CGameStateBlocks.hpp`), which is
 * `rstl::vector<unsigned char>` field for field. It is the body `rstl/vector.hpp`'s
 * `vector(const vector&)` has - count and capacity copied, a null buffer when both are zero,
 * otherwise `allocate(capacity)` and a byte copy of `count` bytes - and it returns `this`.
 *
 * **The byte copy is `rstl::uninitialized_copy_n`, not a hand-written loop** - the exact call
 * `include/rstl/vector.hpp:125` makes. A hand-written `for (int n = count; n != 0; --n) *to++ =
 * *from++;` is 94.05% and stops there: mwcceppc's loop-idiom transform runs on both, and the
 * unrolled 8-byte-then-tail body is identical, but the hand-written form allocates the trip count
 * to `r0` and then has to keep a second copy for the `andi. r3,r3,7` tail (`mr r3,r0` plus
 * `srwi. r0,r0,3`), where retail has `srwi. r0,r3,3` on a count that stayed in `r3` - and it
 * gives the pointers the other way round, `r4` = source, where retail has `r4` = destination.
 * `uninitialized_copy_n` (`include/rstl/construct.hpp:127`) takes the count as its second
 * argument and carries it in the register retail uses, so the whole object matches.
 *
 * **The two `== 0` tests are signed.** Retail tests them with `cmpwi` (0x80004AC8, 0x80004AD4),
 * not `cmplwi`, because `rstl::vector`'s `mCount`/`mCapacity` are `int`; the `static_cast< int >`
 * on the reads is what puts `cmpwi` in the object. `SGameStateBlock` declares the two words `u32`
 * (it is a view onto a block that is `int mCount` in one overlay and a byte size in another), and
 * `== 0` is the only operation done on them here, so the cast is exact.
 *
 * Callers: `fn_80004D5C` (the null-guarded construct, `CGameStateBlockConstruct.cpp`), which
 * `CGameState`'s default constructor reaches six times through `fn_80144924`, and
 * `CMain::ResetGameState` (`src/MetroidPrime/CMainResetGameState.cpp`).
 */

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

#include "rstl/construct.hpp"
#include "rstl/rmemory_allocator.hpp"

extern "C" {
SGameStateBlock* fn_80004AA0(SGameStateBlock* self, const SGameStateBlock* src) {
  self->x04_count = src->x04_count;
  self->x08_cap = src->x08_cap;
  if (static_cast< int >(src->x04_count) == 0 && static_cast< int >(src->x08_cap) == 0) {
    self->x0c_data = 0;
  } else {
    self->x0c_data = rstl::rmemory_allocator::allocate(self->x08_cap);
    rstl::uninitialized_copy_n(static_cast< const unsigned char* >(src->x0c_data),
                               static_cast< int >(self->x04_count),
                               static_cast< unsigned char* >(self->x0c_data));
  }
  return self;
}
} // extern "C"
