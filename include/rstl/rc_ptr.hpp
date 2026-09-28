#ifndef _RSTL_RC_PTR
#define _RSTL_RC_PTR

#include "types.h"
#include "Kyoto/Alloc/CMemory.hpp"

namespace rstl {
// Port: the word a null `rc_ptr`'s refcount points at. Retail has no object for it either - it is
// absent from `config/G2ME01/symbols.txt` and from main.elf's symbol table - but retail's
// `ReleaseData` (0x80008FA4) has no null test on that pointer, so a default-constructed `rc_ptr`
// has to point at a real word or it faults when it dies. Upstream's `rc_ptr` points at
// `CRefData::sNull` instead and does not use this; the port's `rstl::rc_ptr_data_copy` paths and
// `src/MetroidPrime/PortGlobals.cpp` still define it, so it is declared here. A declaration is all
// this is: no upstream member is renamed and no `rc_ptr<T>` changes size.
extern int sNullRefCount;

class CRefData {
public:
  CRefData() : mRefCount(0) {}
#ifdef TARGET_PC
  // Port: lets `CRefData::sNull` be constant-initialised with retail's count (0x00FFFFFF at
  // .sdata 0x80418B98) before any static `rc_ptr` constructor runs. See PortGlobals.cpp.
  explicit constexpr CRefData(int count) : mRefCount(count) {}
#endif
  int AddRef() { return ++mRefCount; }
  int DelRef() { return --mRefCount; }

  int mRefCount;

  static CRefData sNull;
};

// ---------------------------------------------------------------------------
// Port: `rstl::CRcPtrData`, the **non-template** class that holds `rc_ptr`'s two words.
//
// Upstream models the refcount as a shared `CRefData` control block, which is a host-only
// convenience. Retail's `rc_ptr` is `{ T* x0_ptr; int* x4_refCount; }` - eight bytes, the
// refcount a *separate* four-byte `CMemory` allocation - and the out-of-line copy constructor
// retail carries at 0x80049010 has **no mangled name** in the map, while
// `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` does. One unmangled copy constructor serving every
// `T` is only possible if the words are not inside the template, which is what this class is
// for; `src/rstl/rc_ptr_copy.cpp` owns its definition and claims those 36 bytes.
//
// It is deliberately **not** a base class of `rc_ptr<T>`: upstream's own members `mPtr`/`mRefCount`
// already sit at +0 and +4, so this class is a same-layout view of them, reached by
// `reinterpret_cast`. Offsets and the eight-byte size of `rc_ptr<T>` are unchanged, and no
// upstream member is renamed, retyped or reordered.
// ---------------------------------------------------------------------------
class CRcPtrData {
public:
  /// Does nothing: `rc_ptr<T>` writes both words itself, and an initialising base constructor is
  /// not eliminated by mwcceppc.
  CRcPtrData() {}
  /// Retail's out-of-line copy, as a **static** member: mwcceppc reserves r3 for `this` in a
  /// non-static member function, which costs the AddRef two registers. Declared, not defined; the
  /// definition is in `src/rstl/rc_ptr_copy.cpp`.
  static void CopyInto(CRcPtrData* dest, const CRcPtrData& src);
  /// Retail's *inlined* copy and its *called* copy are the same nine instructions, and retail's
  /// compiler makes both choices. This tag is how a call site asks for the call.
  struct OutOfLine {
  };
  CRcPtrData(const CRcPtrData& other) { CopyInto(this, other); }

  void* x0_ptr;
  int* x4_refCount;
};

template < typename T >
class rc_ptr {
public:
  /// The out-of-line copy spelling: `bl CRcPtrData::CopyInto` where retail calls one, instead of
  /// the expansion `rc_ptr(const rc_ptr&)` above gives. Same ABI (`r3` = destination,
  /// `r4` = source) and same mangled name as the pre-merge base-class form.
  rc_ptr(CRcPtrData::OutOfLine, const CRcPtrData& other) {
    CRcPtrData::CopyInto(reinterpret_cast< CRcPtrData* >(this), other);
  }
  rc_ptr() : mPtr(nullptr), mRefCount(&CRefData::sNull.mRefCount) { ++*mRefCount; }
  rc_ptr(const T* ptr) : mPtr(ptr), mRefCount(rs_new int(1)) {}
  rc_ptr(const rc_ptr& other) : mPtr(other.mPtr), mRefCount(other.mRefCount) {
    ++*mRefCount;
  }
  ~rc_ptr() { ReleaseData(); }
  rc_ptr& operator=(const rc_ptr& other) {
    if (mPtr != other.mPtr) {
      ReleaseData();
      mPtr = other.mPtr;
      mRefCount = other.mRefCount;
      ++*mRefCount;
    }
    return *this;
  }
  T* GetPtr() const { return const_cast< T* >(mPtr); }
  bool IsNull() const { return GetPtr() == nullptr; }
  template < typename U >
  void Assign(const U* ptr) {
    const T* base = ptr;
    ReleaseData();
    mPtr = base;
    mRefCount = rs_new int(1);
  }
  void ReleaseData();
  void reset() {
    ReleaseData();
    mPtr = nullptr;
    mRefCount = &CRefData::sNull.mRefCount;
    ++*mRefCount;
  }
  T* operator->() const { return GetPtr(); }
  T& operator*() const { return *GetPtr(); }
  operator bool() const { return GetPtr() != nullptr; }

private:
  const T* mPtr;
  int* mRefCount;
};

template < typename T >
void rc_ptr< T >::ReleaseData() {
  if (--*mRefCount <= 0) {
    delete GetPtr();
    delete mRefCount;
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
inline bool operator==(const rc_ptr< T >& left, const rc_ptr< T >& right) {
  return left.GetPtr() == right.GetPtr();
}

} // namespace rstl

#endif // _RSTL_RC_PTR
