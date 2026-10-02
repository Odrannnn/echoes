#ifndef _RSTL_VECTOR
#define _RSTL_VECTOR

#include "types.h"

#include "rstl/allocator_auto_ptr.hpp"
#include "rstl/iterator.hpp"
#include "rstl/pointer_iterator.hpp"
#include "rstl/rmemory_allocator.hpp"
class CInputStream;
class COutputStream;

namespace rstl {

template < typename T, typename Alloc = rmemory_allocator >
class vector {
public:
  Alloc mAllocator;
  int mCount;
  int mCapacity;
  T* mItems;

public:
  typedef Alloc allocator_type;
  typedef pointer_iterator< T, vector< T, Alloc >, Alloc > iterator;
  typedef const_pointer_iterator< T, vector< T, Alloc >, Alloc > const_iterator;
  typedef int size_type;
  typedef T value_type;

  iterator begin() { return iterator(this, mItems); }
  const_iterator begin() const { return const_iterator(this, mItems); }
  iterator end() {
    T* const end = data() + mCount;
    return iterator(end);
  }
  const_iterator end() const { return const_iterator(this, data() + size()); }
  vector(const Alloc& alloc = Alloc())
  : mAllocator(alloc), mCount(0), mCapacity(0), mItems(nullptr) {}
  // The count elements are uninitialized until the caller fills the storage.
  vector(int count) : mCount(0), mCapacity(0), mItems(0) {
    if (count > 0)
      reserve(count);
    mCount = count;
  }
  vector(int count, const T& v, const Alloc& alloc = Alloc())
  : mAllocator(alloc), mCount(count), mCapacity(count) {
    mAllocator.allocate(mItems, mCount);
    uninitialized_fill_n(mItems, count, v);
  }

  vector(const vector& other);
  vector(CInputStream& in, const Alloc& alloc = Alloc());
  template < typename It >
  vector(It first, It last, const Alloc& alloc = Alloc())
  : mAllocator(alloc), mCount(0), mCapacity(0) {
    mCount = mCapacity = rstl::distance(first, last);
    mAllocator.allocate(mItems, mCount);
    rstl::uninitialized_copy(first, last, mItems);
  }
  ~vector();

  inline void resize(int size, const T& in = T());
  void assign(int size, const T& in = T());
  void reserve(int size);
  iterator insert(iterator it, const T& value);

  template < typename from_iterator >
  void insert(iterator it, from_iterator begin, from_iterator end) {
    insert_into(it, rstl::distance(begin, end), begin);
  }

  // iterator erase(iterator it);
  // iterator erase(iterator first, iterator last);

  iterator erase(iterator it);
  iterator erase(iterator first, iterator last);

  // Retail's inlined `push_back` keeps its zero-capacity fallback: `fn_800E95EC` has
  // `cmpw mCapacity,mCount / bne skip` and then `cmpwi cap,0 / li 4 / beq / slwi cap,1` before the
  // `reserve` call (0x800E9610-0x800E962C), so this does too. What matches retail here is the test's
  // shape - `mCapacity == mCount`, emitted as that `cmpw`/`bne` rather than `mCount >= mCapacity` -
  // `mCount++` fused into the element store, and the element read into a local, because retail's
  // copy loads it *after* the growth test (0x800E963C) rather than at the top of the block.
  // Retail folds the fallback away at one call site of its own (`CGMMultiplayer::ChooseSpawnPoint`,
  // 0x80196C80) and this tree does not reproduce that fold, so that call site spells its growth out
  // instead - see CGMMultiplayer.cpp.
  void push_back(const T& in) {
    if (mCapacity == mCount) {
      reserve(mCapacity != 0 ? mCapacity * 2 : 4);
    }
    const T value = in;
    rstl::construct(mItems + mCount++, value);
  }

  void push_back_unsafe(const T& in) { rstl::construct(mItems + mCount++, in); }

  void pop_back() {
    destroy(mItems + mCount - 1);
    --mCount;
  }

  inline vector& operator=(const vector& other);

  void clear();

  T* data() { return mItems; }
  const T* data() const { return mItems; }
  int size() const { return mCount; }
  bool empty() const { return mCount == 0; }
  int capacity() const { return mCapacity; }
  T& at(int idx) { return mItems[idx]; }
  const T& at(int idx) const { return mItems[idx]; }
  T& front() { return at(0); }
  const T& front() const { return at(0); }
  T& back() { return at(mCount - 1); }
  const T& back() const { return at(mCount - 1); }
  T& operator[](int idx) { return mItems[idx]; }
  const T& operator[](int idx) const { return mItems[idx]; }

  void PutTo(COutputStream& out) const;

protected:
  template < typename In >
  inline void insert_into(iterator at, int n, In in);
};

template < typename T, typename Alloc >
inline vector< T, Alloc >::vector(const vector& other)
: mAllocator(other.mAllocator), mCount(other.mCount), mCapacity(other.mCapacity) {
  if (other.mCount == 0 && other.mCapacity == 0) {
    mItems = nullptr;
  } else {
    mAllocator.allocate(mItems, mCapacity);
    uninitialized_copy_n(other.mItems, mCount, mItems);
  }
}

template < typename T, typename Alloc >
inline vector< T, Alloc >::~vector() {
  destroy(begin(), end());
  mAllocator.deallocate(mItems);
}

template < typename T, typename Alloc >
void vector< T, Alloc >::assign(int size, const T& in) {
  clear();
  reserve(size);
  for (int i = 0; i < size; ++i) {
    push_back_unsafe(in);
  }
}

template < typename T, typename Alloc >
inline void vector< T, Alloc >::resize(int size, const T& in) {
  if (mCount != size) {
    if (size > mCount) {
      reserve(size);
      uninitialized_fill_n(mItems + mCount, size - mCount, in);
    } else {
      destroy(begin() + size, end());
    }
    mCount = size;
  }
}

template < typename T, typename Alloc >
void vector< T, Alloc >::reserve(int newSize) {
  if (newSize <= mCapacity) {
    return;
  }

  T* newData;
  mAllocator.allocate(newData, newSize);
  uninitialized_copy(begin(), end(), newData);
  destroy(mItems, mItems + mCount);
  mAllocator.deallocate(mItems);
  mItems = newData;
  mCapacity = newSize;
}

template < typename T, typename Alloc >
typename vector< T, Alloc >::iterator vector< T, Alloc >::insert(iterator it, const T& value) {
  typename iterator::difference_type diff = it.operator->() - mItems;
  const_counting_iterator< T > in(&value, 0);
  insert_into(it, 1, in);
  return iterator(mItems) + diff;
}

template < typename T, typename Alloc >
template < typename In >
void vector< T, Alloc >::insert_into(iterator at, int n, In in) {
  T* oldData = mItems;
  In input = in;

  if (mCount + n <= mCapacity) {
    long atIdx = at - begin();
    int moveCount = mCount - atIdx;
    int i = moveCount - 1;
    for (; i >= 0; --i) {
      construct(oldData + atIdx + n + i, data()[atIdx + i]);
      destroy(oldData + atIdx + i);
    }
    for (i = 0; i < n; ++input, ++i) {
      construct(oldData + atIdx + i, *input);
    }
    mCount += n;
  } else {
    int newCapacity = mCapacity ? mCapacity * 2 : 4;
    while (newCapacity < mCount + n) {
      newCapacity *= 2;
    }

    T* newData;
    mAllocator.allocate(newData, newCapacity);
    long atIdx = at - begin();
    T* const newItems = newData;
    int newIdx = 0;
    // `i` is a `long` to match `atIdx` (a `difference_type`). With `int i` the guard
    // sign-extends the 32-bit induction variable and mwcceppc materialises the zero it
    // compares against, so the first copy loop is entered through an explicit `cmpwi`
    // retail does not have; with `long i` the `cmpwi` folds into the `addze.` that
    // divides the byte distance. Costs nothing, matches retail in both instantiations.
    for (long i = 0; i < atIdx; ++newIdx, ++i) {
      construct(newItems + newIdx, data()[i]);
    }
    for (int i = 0; i < n; ++input, ++newIdx, ++i) {
      construct(newItems + newIdx, *input);
    }
    for (int i = atIdx; i < size(); ++newIdx, ++i) {
      construct(newItems + newIdx, data()[i]);
    }

    destroy(oldData, oldData + size());
    mAllocator.deallocate(mItems);
    mItems = newData;
    mCapacity = newCapacity;
    mCount += n;
  }
}

template < typename T, typename Alloc >
inline vector< T, Alloc >& vector< T, Alloc >::operator=(const vector< T, Alloc >& other) {
  if (this == &other)
    return *this;
  clear();
  if (other.size() == 0) {
    mAllocator.deallocate(mItems);
    mCount = 0;
    mCapacity = 0;
    mItems = nullptr;
  } else {
    reserve(other.size());
    uninitialized_copy(other.mItems, other.mItems + other.mCount, data());
    mCount = other.mCount;
  }
  return *this;
}

template < typename T, typename Alloc >
typename vector< T, Alloc >::iterator vector< T, Alloc >::erase(iterator it) {
  return erase(it, it + 1);
}

template < typename T, typename Alloc >
typename vector< T, Alloc >::iterator vector< T, Alloc >::erase(iterator first, iterator last) {
  destroy(first, last);

  const typename iterator::difference_type tmp = first - begin();

  int newCount = tmp;

  for (iterator it = last, moved = iterator(mItems + tmp); it != end();
       ++moved, ++newCount, ++it) {
    construct(&*moved, *it);
    destroy(&*it);
  }
  mCount = newCount;

  return first;
}
template < typename T, typename Alloc >
void vector< T, Alloc >::clear() {
  destroy(begin(), end());
  mCount = 0;
}

} // namespace rstl

#endif // _RSTL_VECTOR
