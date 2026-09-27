#include "Kyoto/Streams/CInputStream.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "rstl/math.hpp"
#include "string.h"

CInputStream::CInputStream(const void* ptr, unsigned long len)
: x4_buffer(const_cast< uchar* >(reinterpret_cast< const uchar* >(ptr)))
, xc_length(len)
, x10_owned(false) {
  x8_ptr = x4_buffer;
}

CInputStream::CInputStream(const void* ptr, unsigned long len, bool owned)
: x4_buffer(const_cast< uchar* >(reinterpret_cast< const uchar* >(ptr)))
, xc_length(len)
, x10_owned(owned) {
  x8_ptr = x4_buffer;
}

CInputStream::CInputStream(const SBufferAndSize& buffer, bool owned)
: x4_buffer(const_cast< uchar* >(reinterpret_cast< const uchar* >(buffer.x0_buffer)))
, xc_length(buffer.x4_size)
, x10_owned(owned) {
  x8_ptr = x4_buffer;
}

CInputStream::~CInputStream() {
  if (x10_owned) {
#ifdef TARGET_PC
    // **An owned buffer came from the game's heap, so the host has to free it from there.** Both
    // producers allocate with `CMemory::Alloc`: `CResLoaderLoadNewResourceSync.cpp:93`, the arm
    // of `fn_802FC63C` taken when the caller supplied no buffer, and
    // `Streams/DolphinCLZOInputStream.cpp:9`, the decompressed block. Under mwcceppc the
    // `delete[]` in the #else *is* `CMemory::Free` - `include/Kyoto/Alloc/CMemory.hpp:45-47`
    // defines the global `operator delete[]` that way under `__MWERKS__`, which is why
    // `nm build/G2ME01/src/Kyoto/Streams/CInputStream.o` reports `U Free__7CMemoryFPCv` and the
    // `x10_owned` arm of `__dt__12CInputStreamFv` relocates against it. On a host
    // `CMemory.hpp:39-42` deliberately leaves global new/delete to libstdc++, so that same
    // spelling hands a `CMemory` pointer to `free()` and dies in `munmap_chunk(): invalid
    // pointer` - which it did, at the end of `CEnvFxManager::Initialize`, where the boot's first
    // owned stream goes out of scope, and the boot with it. `CInputStream.cpp` is `Matching`
    // (`configure.py:1429`), so the matching build still compiles the #else unchanged.
    CMemory::Free(x4_buffer);
#else
    delete[] x4_buffer;
#endif
  }
}

void CInputStream::Get(void* dest, unsigned long len) {
  memcpy(dest, x8_ptr, len);
  x8_ptr += len;
}

const void* CInputStream::Get(unsigned long len) {
  const void* result = x8_ptr;
  x8_ptr += len;
  return result;
}

size_t CInputStream::ReadBytes(void* dest, size_t len) {
  size_t count = xc_length - (x8_ptr - x4_buffer);
  if (len < count) {
    count = len;
  }
  count = rstl::max_val(size_t(0), count);
  if (count != 0) {
    if (dest != nullptr) {
      memcpy(dest, x8_ptr, count);
    }
    x8_ptr += count;
  }
  return count;
}

float CInputStream::ReadFloat() {
  static float f;
  *reinterpret_cast< uint* >(&f) = ReadInt32();
  return f;
}

rstl::auto_ptr< uchar > CInputStream::ReleaseBuffer() {
  x10_owned = false;
  return rstl::auto_ptr< uchar >(x4_buffer);
}
