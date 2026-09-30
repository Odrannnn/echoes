#ifndef _RSTL_RESERVED_VECTOR
#define _RSTL_RESERVED_VECTOR

#include "types.h"

#include "rstl/construct.hpp"
#include "rstl/pointer_iterator.hpp"

class CInputStream;
class COutputStream;

namespace rstl {

//!< Selects the constructor that leaves `mCount` alone. See `reserved_vector(preserved_t)`'s note.
struct preserved_t {};

template < typename T, int N >
class reserved_vector {
public:
  // Public like `rstl::vector`'s members: retail's `reserved_vector<T, N>::operator=` is an
  // out-of-line symbol that no caller can name (a template instantiation is emitted under its
  // mangled name, so objdiff never pairs it with the retail symbol), so it has to be written out
  // by hand in a .cpp under an `extern "C"` name, and that code needs the members. Access is
  // codegen-neutral.
  int mCount;
  uchar mData[N * sizeof(T)];

public:
  // typedef pointer_iterator< T, reserved_vector< T, N >, void > iterator;
  // typedef const_pointer_iterator< T, reserved_vector< T, N >, void > const_iterator;
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef T value_type;

  inline iterator begin() { return iterator(data()); }
  inline const_iterator begin() const { return const_iterator(data()); }
  inline iterator end() { return iterator(data() + mCount); }
  inline const_iterator end() const { return const_iterator(data() + mCount); }

  reserved_vector() : mCount(0) {}

  //!< Constructs without writing `mCount` at all.
  //!
  //!< Exists for exactly one caller: `CGMFrontEnd`'s copy constructor
  //!< (`src/MetroidPrime/Player/CGameState.cpp`), whose **body** immediately calls
  //!< `fn_80143CD4(&mPlayers, &other.mPlayers)` - `rstl::reserved_vector`'s `operator=`, which
  //!< stores the count itself before copying. Retail's 140 bytes at 0x80143C48 contain no store
  //!< to `+0x20`, so the `mCount(0)` the mem-init list would emit is dead in retail and the
  //!< compiler keeps it here.
  //!
  //!< **The obvious alternative - deleting `mCount(0)` from the default constructor - is wrong,
  //!< and it is not a close call.** Measured on the whole tree: it takes
  //!< `__ct__11CGMFrontEndFRC11CGMFrontEnd` 88.86% -> 100.00%, and costs eight `Matching` units a
  //!< function each (`CCollisionPrimitive::InternalCollideBoolean` 100 -> 98.52,
  //!< `CPASAnimInfo::CPASAnimInfo(int)` 100 -> 50.00, `CMidiManager::__sinit_CMidiManager_cpp` and
  //!< `CStaticAudioPlayer::__sinit_CStaticAudioPlayer_cpp` 100 -> 75.33, `CDvdFile::TryARAMFile`
  //!< 100 -> 98.04, `CTextRenderBuffer`'s copy constructor 100 -> 98.06, `CControlMapper`'s
  //!< 100 -> 94.35, `CPlayMovie(int)` 100 -> 99.75) plus the Tweaks REL's
  //!< `__ct__15CTweakPlayerResFRC18SLdrTweakPlayerRes` 100 -> 0.00, and `main.dol` stops hashing to
  //!< `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`. The header is shared by every class with a
  //!< `reserved_vector` member, and each of those eight places genuinely needs its zero. This
  //!< constructor is additive: it adds no member, moves nothing, and the default constructor every
  //!< other use site reaches is unchanged, so no other unit's bytes move.
  reserved_vector(preserved_t) {}
  explicit reserved_vector(const T& value) : mCount(N) { uninitialized_fill_n(data(), N, value); }
  explicit reserved_vector(int count, const T& value) : mCount(count) {
    uninitialized_fill_n(data(), count, value);
  };
  reserved_vector(const reserved_vector& other) : mCount(other.mCount) {
    uninitialized_copy_n(other.data(), mCount, data());
  }
  reserved_vector(CInputStream& in);

  reserved_vector& operator=(const reserved_vector& other);

  void clear() {
    destroy_elements();
    mCount = 0;
  }

  ~reserved_vector() { destroy_elements(); }

  void push_back(const T& in) {
    construct(data() + mCount, in);
    ++mCount;
  }

  void pop_back() {
    destroy(&data()[mCount - 1]);
    --mCount;
  }

  inline T* data() { return reinterpret_cast< T* >(mData); }
  inline const T* data() const { return reinterpret_cast< const T* >(mData); }
  inline bool empty() const { return size() == 0; }
  inline int size() const { return mCount; }
  inline int capacity() const { return N; }
  inline T& front() { return data()[0]; }
  inline const T& front() const { return data()[0]; }
  inline T& back() { return at(mCount - 1); }
  inline const T& back() const { return at(mCount - 1); }
  inline T& operator[](int idx) { return data()[idx]; }
  inline const T& operator[](int idx) const { return data()[idx]; }
  inline T& at(int idx) { return data()[idx]; }
  inline const T& at(int idx) const { return data()[idx]; }
  iterator erase(iterator it);

  void resize(int count, const T& item = T()) {
    if (mCount == count) {
      return;
    }
    if (mCount <= count) {
      uninitialized_fill_n(data() + mCount, count - mCount, item);
    } else {
      destroy(data() + count, data() + mCount);
    }
    mCount = count;
  }

  void PutTo(COutputStream& out) const;

private:
  void destroy_elements() {
    if (is_trivially_destructible< T >::value) {
      return;
    }
    T* ptr = data();
    for (int i = 0; i < mCount; ++i) {
      destroy(&ptr[i]);
    }
  }
};



template < typename T, int N >
inline reserved_vector< T, N >& reserved_vector< T, N >::operator=(const reserved_vector& other) {
  if (this != &other) {
    destroy_elements();
    uninitialized_copy(other.data(), other.data() + other.size(), data());
    mCount = other.mCount;
  }
  return *this;
}

template < typename T, int N >
typename reserved_vector< T, N >::iterator reserved_vector< T, N >::erase(iterator it) {
  if (it >= begin() && it < end()) {
    for (iterator j = it; j < end() - 1; ++j) {
      *j = *(j + 1);
    }
    destroy(end() - 1);
    --mCount;
    return it;
  }
  return end();
}

} // namespace rstl

#endif // _RSTL_RESERVED_VECTOR
