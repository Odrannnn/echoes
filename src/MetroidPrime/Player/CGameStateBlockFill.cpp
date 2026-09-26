/**
 * `fn_80142BA4` - retail `.text:0x80142BA4`, `size:0x154` = 340 bytes: refill the 16-byte
 * `SGameStateBlock` with `count` copies of one byte. `clear` (`fn_80142914`), `reserve(count)`
 * (`fn_801465EC`), then `count` unchecked appends of `*src` - mwcceppc's 8-way unroll of a loop
 * whose body re-reads the count and the buffer each time, which is `push_back_unsafe`.
 *
 * `CGameState`'s default constructor reaches it twice: through `fn_80142DD4`
 * (`CGameStateSlotDefaults.cpp`, `lbl_804183DD`) and `fn_80142CF8` (`CGameStateSysOptsPutTo.cpp`,
 * `lbl_804183DF`), each with 32 bytes - the buffer a `CMemoryStreamOut` then serialises into.
 */

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

extern "C" void fn_80142914(SGameStateBlock* self);
extern "C" void fn_801465EC(SGameStateBlock* self, int size);

extern "C" {
void fn_80142BA4(SGameStateBlock* self, int count, const unsigned char* src) {
  fn_80142914(self);
  fn_801465EC(self, count);
  // The element address is formed first and the byte stored through it - `construct(data +
  // count++, *src)` with the construct inlined. Ranked with `tools/try_batch.py`: indexing
  // `data[count++]` is 49 instructions out and a named count 40; this is the exact match.
  for (int i = 0; i < count; ++i) {
    unsigned char* p = static_cast< unsigned char* >(self->x0c_data) + self->x04_count++;
    *p = *src;
  }
}
} // extern "C"
