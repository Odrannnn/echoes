#ifndef _CINPUTSTREAM
#define _CINPUTSTREAM

#include "types.h"

#include "stddef.h"

#include "rstl/auto_ptr.hpp"
#include "rstl/pair.hpp"

class CInputStream;
// Only the name is needed for the two friend declarations below; the real definition is
// `Kyoto/SObjectTag.hpp`, which this header deliberately does not include (it is reached from
// `rstl/vector.hpp`'s stream constructor, and a cycle is not worth a dependency for a friend).
struct SObjectTag;
// These two have to be declared at namespace scope with C linkage *before* the class, for the
// same reason `Kyoto/CResLoader.hpp` does it: the friend declarations inside the class name them,
// and a friend declaration is the *first* declaration of the function if nothing precedes it -
// which gives it C++ linkage and then conflicts with the `extern "C"` in `CResLoader.hpp`.
// mwcceppc reads the `extern` in `friend extern "C" f(...)` as a storage class and rejects it.
extern "C" void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf);
extern "C" void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);
template < typename T >
struct TType {};
template < typename T >
T cinput_stream_helper(const TType< T >& type, CInputStream& in);

template < typename T >
inline TType< T > TGetType(const T&) {
  return TType< T >();
}

class CInputStream {
public:
  struct SBufferAndSize {
    const void* x0_buffer;
    unsigned long x4_size;

    SBufferAndSize(const void* buffer, unsigned long size) : x0_buffer(buffer), x4_size(size) {}
  };

  CInputStream(const void* ptr, unsigned long len);
  CInputStream(const void* ptr, unsigned long len, bool owned);
  CInputStream(const SBufferAndSize& buffer, bool owned);
  virtual ~CInputStream();

  float ReadFloat();
  size_t ReadBytes(void* dest, size_t len);
  void Get(void* dest, unsigned long len);
  const void* Get(unsigned long len);
  rstl::auto_ptr< uchar > ReleaseBuffer();

  // `CResLoader`'s two compressed-stream loaders (`fn_802FC4D8`, `fn_802FC63C`) read the four-byte
  // decompressed-size prefix off the front of the memory stream by hand, and the three
  // instructions that do it are retail's own:
  //
  //     802fc14c:  lwz  r6,8(r7)      ; x8_ptr
  //     802fc154:  addi r0,r6,4
  //     802fc158:  stw  r0,8(r7)      ; x8_ptr += 4
  //     802fc15c:  lwz  r30,0(r6)     ; *x8_ptr
  //
  // which is `Get(4)`'s expansion - but **`Get` is defined in `Kyoto/Streams/CInputStream.cpp`**,
  // a different translation unit, so a caller here cannot inline it and spelling it that way emits
  // a call that changes the object. These two are friends instead, and read the member directly.
  // The cost is that the *source* is not retail's - the bytes are, which is what the unit needs.
  friend void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf);
  friend void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);

  template < typename T >
  T Get() {
    TType< T > type;
    return cinput_stream_helper(type, *this);
  }
  template < typename T >
  T Get(const TType< T >& type) {
    return cinput_stream_helper(type, *this);
  }

  int ReadInt32() {
    int* result = reinterpret_cast< int* >(x8_ptr);
    x8_ptr = reinterpret_cast< uchar* >(result + 1);
    return *result;
  }
  u16 ReadUint16() {
    u16* result = reinterpret_cast< u16* >(x8_ptr);
    x8_ptr = reinterpret_cast< uchar* >(result + 1);
    return *result;
  }
  short ReadInt16() { return static_cast< short >(ReadUint16()); }
  u8 ReadUint8() {
    u8* result = x8_ptr;
    x8_ptr = result + 1;
    return *result;
  }
  char ReadInt8() { return static_cast< char >(ReadUint8()); }
  bool ReadBool() { return ReadUint8() != 0; }
  uint GetReadPosition() const { return x8_ptr - x4_buffer; }

private:
  uchar* x4_buffer;
  uchar* x8_ptr;
  unsigned long xc_length;
  bool x10_owned;
};

CHECK_SIZEOF(CInputStream, 0x14)

template < typename T >
inline T cinput_stream_helper(const TType< T >& type, CInputStream& in) {
  return T(in);
}
template <>
inline bool cinput_stream_helper(const TType< bool >& type, CInputStream& in) {
  return in.ReadBool();
}
template <>
inline char cinput_stream_helper(const TType< char >& type, CInputStream& in) {
  return in.ReadInt8();
}

template <>
inline unsigned char cinput_stream_helper(const TType< unsigned char >& type, CInputStream& in) {
  return in.ReadUint8();
}

template <>
inline signed char cinput_stream_helper(const TType< signed char >& type, CInputStream& in) {
  return in.ReadInt8();
}

template <>
inline int cinput_stream_helper(const TType< int >& type, CInputStream& in) {
  return in.ReadInt32();
}
template <>
inline uint cinput_stream_helper(const TType< uint >& type, CInputStream& in) {
  return in.ReadInt32();
}
template <>
inline unsigned long cinput_stream_helper(const TType< unsigned long >& type, CInputStream& in) {
  return in.ReadInt32();
}
template <>
inline float cinput_stream_helper(const TType< float >& type, CInputStream& in) {
  return in.ReadFloat();
}
template <>
inline short cinput_stream_helper(const TType< short >& type, CInputStream& in) {
  return in.ReadInt16();
}
template <>
inline ushort cinput_stream_helper(const TType< ushort >& type, CInputStream& in) {
  return in.ReadUint16();
}

// rstl
template < typename L, typename R >
inline rstl::pair< L, R >::pair(CInputStream& in)
: first(in.Get(TGetType(first))), second(in.Get(TGetType(second))) {}

#include "rstl/vector.hpp"
template < typename T, typename Alloc >
rstl::vector< T, Alloc >::vector(CInputStream& in, const Alloc& allocator)
: x4_count(0), x8_capacity(0), xc_items(nullptr) {
  int count = in.ReadInt32();
  reserve(count);
  for (int i = 0; i < count; i++) {
    push_back_unsafe(in.Get< T >());
  }
}

#include "rstl/reserved_vector.hpp"
template < typename T, int N >
inline rstl::reserved_vector< T, N >::reserved_vector(CInputStream& in) : x0_count(in.ReadInt32()) {
  for (int i = 0; i < x0_count; i++) {
    construct(&data()[i], in.Get(TType< T >()));
  }
}

#endif // _CINPUTSTREAM
