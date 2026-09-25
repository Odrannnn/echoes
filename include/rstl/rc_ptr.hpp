#ifndef _RSTL_RC_PTR
#define _RSTL_RC_PTR

#include "types.h"
#include "Kyoto/Alloc/CMemory.hpp"

namespace rstl {

// ---------------------------------------------------------------------------
// Retail's `rstl::rc_ptr<T>` is `{ T* x0_ptr; int* x4_refCount; }` - eight bytes, with the
// refcount a *separate four-byte CMemory allocation* rather than a field of a shared control
// block. The evidence and the disassembly it comes from are in docs/research/rc_ptr.md; the four
// measurements that pin it down are:
//
//   fn_80049010                 the copy constructor: 0x24 bytes, `lwz r5,0(r4) ; lwz r0,4(r4) ;
//                               stw r5,0(r3) ; stw r0,4(r3)` then `++*(u32*)0(r3+4)`
//   ReleaseData (0x80008FA4)    `lwz r4,4(r3) ; ... ; CMemory::Free(*(u32**)(r3+4))` and
//                               `delete x0_ptr` through vtable slot 2 with argument 1
//   CIOWinManager::AddIOWin     its by-value `ncrc_ptr<CIOWin>` parameter is passed *by
//                               pointer* to an 8-byte caller temporary (Itanium ABI: a
//                               non-trivial class), and `operator new(16)` is
//                               `IOWinPQNode { rc_ptr(8); int; node* }`
//   MakeMsg::CreateFrameEnd     `new(4) ; *(int*)r3 = 1` stored at +4 of the rc_ptr, and the
//                               rc_ptr's two words stored at +8/+0xc of the 16-byte
//                               CArchitectureMessage
//
// The port's `CRefData` control block is gone: there is no retail object with those semantics to
// map onto, and modelling one costs an extra indirection on every dereference and an extra free
// on every release, which is exactly the difference between the port's 87.84% `ReleaseData` and
// retail's 100%.
// ---------------------------------------------------------------------------

// A default-constructed `rc_ptr` is `{nullptr, &sNullRefCount}`. Retail has no such object either
// (it is absent from symbols.txt and from every dtk object), but its `ReleaseData` has no null
// test on the refcount pointer - it does `lwz r4,4(r3) ; lwz r3,0(r4)` - so a null `rc_ptr` has
// to point at a real word, or a default-constructed one would fault on destruction. The count is
// large enough that `--*x4_refCount` can never reach zero, so a null `rc_ptr` never deletes
// anything. Defined in src/MetroidPrime/PortGlobals.cpp, which no DOL unit claims.
extern int sNullRefCount;

template < typename T >
class rc_ptr {
public:
  rc_ptr() : x0_ptr(nullptr), x4_refCount(&sNullRefCount) {}
  rc_ptr(const T* ptr) : x0_ptr(const_cast< T* >(ptr)), x4_refCount(AllocRefCount()) {}
  // Retail's copy constructor is out-of-line at fn_80049010 (0x80049010, 0x24 bytes) and mwcceppc
  // emits a call to it from `RemoveAllIOWins`, `RemoveIOWin` and four other CIOWinManager methods -
  // while *inlining* the identical eight instructions in `IOWinPQNode::IOWinPQNode` (0x80049D58)
  // and four more places. Both are this compiler's own decision on the same definition, so the
  // declaration is left inline and the layout is what makes the choice come out right; forcing
  // it out of line by hiding the definition would change retail's inlined sites too.
  rc_ptr(const rc_ptr& other) : x0_ptr(other.x0_ptr), x4_refCount(other.x4_refCount) {
    ++(*x4_refCount);
  }
  ~rc_ptr() { ReleaseData(); }
  rc_ptr& operator=(const rc_ptr& other) {
    if (x4_refCount != other.x4_refCount) {
      ReleaseData();
      x0_ptr = other.x0_ptr;
      x4_refCount = other.x4_refCount;
      ++(*x4_refCount);
    }
    return *this;
  }
  T* GetPtr() const { return x0_ptr; }
  bool IsNull() const { return x0_ptr == nullptr; }
  template < typename U >
  void Assign(const U* ptr) {
    const T* base = ptr;
    ReleaseData();
    x0_ptr = const_cast< T* >(base);
    x4_refCount = AllocRefCount();
  }
  void ReleaseData();
  void reset() {
    ReleaseData();
    x0_ptr = nullptr;
    x4_refCount = &sNullRefCount;
  }
  T* operator->() const { return x0_ptr; }
  T& operator*() const { return *x0_ptr; }
  operator bool() const { return x0_ptr != nullptr; }

private:
  // `rs_new` is `new ("\?\?(\?\?)", nullptr)`, i.e. CMemory::Alloc with the file/line operands
  // retail's `__nw__FUlPCcPCc` carries: `MakeMsg::CreateFrameEnd` does `li r3,4 ; bl __nw__ ;
  // li r0,1 ; stw r0,0(r3)`, which is `new int(1)`.
  static int* AllocRefCount() { return rs_new int(1); }
  // Retail frees the word with `CMemory::Free` (`Free__7CMemoryFPCv`), not with
  // `operator delete` - 0x80008FEC is `lwz r3,4(r31) ; bl Free__7CMemoryFPCv` and there is no
  // second free. In the PC build `rs_new` is the *host* `new` and `CMemory::Free` goes to
  // `CGameAllocator::Free`, a real emulated heap, so the two must not be mixed there.
  static void FreeRefCount(int* ptr) {
#if defined(__MWERKS__)
    CMemory::Free(ptr);
#else
    delete ptr;
#endif
  }

  T* x0_ptr;
  int* x4_refCount;
};

template < typename T >
void rc_ptr< T >::ReleaseData() {
  if (--(*x4_refCount) <= 0) {
    delete x0_ptr;
    FreeRefCount(x4_refCount);
  }
}

template < typename T >
class ncrc_ptr : public rc_ptr< T > {
public:
  ncrc_ptr() {}
  ncrc_ptr(T* ptr) : rc_ptr< T >(ptr) {}
  ncrc_ptr(const rc_ptr< T >& other) : rc_ptr< T >(other) {}
  ncrc_ptr& operator=(const rc_ptr< T >& other) {
    rc_ptr< T >::operator=(other);
    return *this;
  }
  template < typename U >
  ncrc_ptr& operator=(const U* ptr) {
    rc_ptr< T >::Assign(ptr);
    return *this;
  }
};

template < typename T >
bool operator==(const rc_ptr< T >& left, const rc_ptr< T >& right) {
  return left.GetPtr() == right.GetPtr();
}

} // namespace rstl

#endif // _RSTL_RC_PTR
