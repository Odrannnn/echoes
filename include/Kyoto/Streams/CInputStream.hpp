#ifndef _CINPUTSTREAM
#define _CINPUTSTREAM

#include "types.h"

#include "stddef.h"

#include "rstl/auto_ptr.hpp"
#include "rstl/pair.hpp"

class CInputStream;

// Port: host byte order. Retail's readers are the PowerPC's own big-endian load (`lwz`), so the
// value they return is the big-endian word in the buffer. The port's host is little-endian, and
// every pak read (version word, table counts, resource entries, string lengths) comes through
// this reader, so the conversion lives here. mwcceppc does not define TARGET_PC, so the matching
// build keeps retail's plain load.
#ifdef TARGET_PC
struct SObjectTag;
// The compressed-stream loaders read the decompressed-size prefix off `mPtr` by hand; they are
// declared with C linkage before the class so the friend declarations below refer to them.
extern "C" void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf);
extern "C" void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);

inline uint cinput_stream_read_be32(const void* ptr) {
  const uchar* bytes = static_cast< const uchar* >(ptr);
  return (uint(bytes[0]) << 24) | (uint(bytes[1]) << 16) | (uint(bytes[2]) << 8) | uint(bytes[3]);
}
inline u16 cinput_stream_read_be16(const void* ptr) {
  const uchar* bytes = static_cast< const uchar* >(ptr);
  return u16((uint(bytes[0]) << 8) | uint(bytes[1]));
}
#endif

template < typename T >
struct TType {};

template < typename T >
inline TType< T > TGetType(const T&) {
  return TType< T >();
}

class CInputStream {
public:
  struct SBufferAndSize {
    const void* mBuffer;
    unsigned long mSize;

    SBufferAndSize(const void* buffer, unsigned long size) : mBuffer(buffer), mSize(size) {}
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

#ifdef TARGET_PC
  friend void* fn_802FC4D8(void* resLoader, const SObjectTag& tag, void* buf);
  friend void* fn_802FC63C(void* resLoader, const SObjectTag& tag, void* extBuf);
#endif

  template < typename T >
  T Get(const TType< T >& type = TType< T >());

  int ReadInt32() {
    int* result = reinterpret_cast< int* >(mPtr);
    mPtr += sizeof(int);
#ifdef TARGET_PC
    return static_cast< int >(cinput_stream_read_be32(result));
#else
    return *result;
#endif
  }
  u64 ReadInt64() {
    u64* result = reinterpret_cast< u64* >(mPtr);
    mPtr = reinterpret_cast< uchar* >(result + 1);
#ifdef TARGET_PC
    return (static_cast< u64 >(cinput_stream_read_be32(result)) << 32) |
           cinput_stream_read_be32(reinterpret_cast< uchar* >(result) + 4);
#else
    return *result;
#endif
  }
  u16 ReadUint16() {
    u16* result = reinterpret_cast< u16* >(mPtr);
    mPtr = reinterpret_cast< uchar* >(result + 1);
#ifdef TARGET_PC
    return cinput_stream_read_be16(result);
#else
    return *result;
#endif
  }
  short ReadInt16() { return static_cast< short >(ReadUint16()); }
  u8 ReadUint8() {
    const u8 result = *mPtr++;
    return result;
  }
  char ReadInt8() { return static_cast< char >(ReadUint8()); }
  bool ReadBool() { return ReadUint8() != 0; }
  uint GetReadPosition() const { return mPtr - mBuffer; }

private:
  uchar* mBuffer;
  uchar* mPtr;
  unsigned long mLength;
  bool mOwned;
};

CHECK_SIZEOF(CInputStream, 0x14)

template < typename T >
inline T CInputStream::Get(const TType< T >& type) {
  return T(*this);
}

template <>
inline bool CInputStream::Get< bool >(const TType< bool >& type) {
  return ReadBool();
}

template <>
inline char CInputStream::Get< char >(const TType< char >& type) {
  return ReadInt8();
}

template <>
inline unsigned char CInputStream::Get< unsigned char >(const TType< unsigned char >& type) {
  return ReadUint8();
}

template <>
inline signed char CInputStream::Get< signed char >(const TType< signed char >& type) {
  return ReadInt8();
}

template <>
inline int CInputStream::Get< int >(const TType< int >& type) {
  return ReadInt32();
}

template <>
inline uint CInputStream::Get< uint >(const TType< uint >& type) {
  return ReadInt32();
}

template <>
inline unsigned long CInputStream::Get< unsigned long >(const TType< unsigned long >& type) {
  return ReadInt32();
}

// Port: on an LP64 host `u64` is `unsigned long`, which already has its (retail, 32-bit)
// specialization above; `ReadInt64` covers the 64-bit read there.
#if !(defined(TARGET_PC) && __SIZEOF_LONG__ == 8)
template <>
inline u64 CInputStream::Get< u64 >(const TType< u64 >& type) {
  const uint high = ReadInt32();
  const uint low = ReadInt32();
  return (static_cast< u64 >(high) << 32) | low;
}
#endif

template <>
inline float CInputStream::Get< float >(const TType< float >& type) {
  return ReadFloat();
}

template <>
inline short CInputStream::Get< short >(const TType< short >& type) {
  return ReadInt16();
}

template <>
inline ushort CInputStream::Get< ushort >(const TType< ushort >& type) {
  return ReadUint16();
}

// rstl
template < typename L, typename R >
inline rstl::pair< L, R >::pair(CInputStream& in)
: first(in.Get(TGetType(first))), second(in.Get(TGetType(second))) {}

#include "rstl/vector.hpp"
template < typename T, typename Alloc >
inline rstl::vector< T, Alloc >::vector(CInputStream& in, const Alloc& allocator)
: mCount(0), mCapacity(0), mItems(nullptr) {
  int count = in.ReadInt32();
  reserve(count);
  for (int i = 0; i < count; i++) {
    push_back_unsafe(in.Get< T >());
  }
}

#include "rstl/reserved_vector.hpp"
template < typename T, int N >
inline rstl::reserved_vector< T, N >::reserved_vector(CInputStream& in) : mCount(in.ReadInt32()) {
  for (int i = 0; i < mCount; i++) {
    construct(&data()[i], in.Get(TType< T >()));
  }
}

#include "rstl/red_black_tree.hpp"
template < typename T, typename P, int U, typename S, typename Cmp, typename Alloc >
inline rstl::red_black_tree< T, P, U, S, Cmp, Alloc >::red_black_tree(
    CInputStream& in, const S& selector, const Cmp& cmp, const Alloc& alloc)
: mSelector(selector), mCmp(cmp), mAllocator(alloc), mCount(0) {
  const int count = in.Get< int >();
  for (int i = 0; i < count; ++i) {
    insert(in.Get< P >());
  }
}

#endif // _CINPUTSTREAM
