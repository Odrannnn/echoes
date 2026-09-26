/**
 * `fn_801465EC` - retail `.text:0x801465EC`, `size:0x108` = 264 bytes:
 * `rstl::vector<unsigned char>::reserve` on the 16-byte `SGameStateBlock`, and the body is
 * `rstl/vector.hpp`'s `reserve`: nothing if the new size fits, otherwise `allocate(size)`, the
 * `count` old bytes copied across, the old buffer freed (`CMemory::Free`), and the new buffer and
 * capacity stored.
 */

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "rstl/rmemory_allocator.hpp"

extern "C" {
void fn_801465EC(SGameStateBlock* self, int size) {
  if (size <= static_cast< int >(self->x08_cap)) {
    return;
  }
  unsigned char* data = static_cast< unsigned char* >(rstl::rmemory_allocator::allocate(size));
  const unsigned char* from = static_cast< const unsigned char* >(self->x0c_data);
  const unsigned char* end = from + self->x04_count;
  unsigned char* to = data;
  for (; from != end; ++from, ++to) {
    *to = *from;
  }
  CMemory::Free(self->x0c_data);
  self->x0c_data = data;
  self->x08_cap = size;
}
} // extern "C"
