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
// Above the two words sits `rstl::CRcPtrData`, a **non-template** class, so that retail's
// out-of-line copy constructor exists at all. That is a modelling decision, not a convenience:
// `fn_80049010` carries no mangled name in the map while `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv`
// does, so retail emitted one copy constructor for every `T`, which is only possible if the words
// are not in the template. See the class's own comment and `src/rstl/rc_ptr_copy.cpp`.
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

// ---------------------------------------------------------------------------
// The two words, in a **non-template** class.
//
// Retail's copy constructor is out of line at `fn_80049010` (0x80049010, 0x24 = 36 bytes) and the
// map gives it no mangled name, while `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv` *is* named - so
// retail emitted one copy constructor for every `T` between them, which is only possible if the
// words live in a class that is not itself a template. Putting them in one here reproduces that:
// `CRcPtrData` is not a template, so a single out-of-line symbol serves every `rc_ptr<T>` and
// `fn_80049010` becomes a real function this tree can own.
//
//   80049010:  lwz  r5,0(r4)   ; r5 = other->x0_ptr
//   80049014:  lwz  r0,4(r4)   ; r0 = other->x4_refCount
//   80049018:  stw  r5,0(r3)
//   8004901c:  stw  r0,4(r3)
//   80049020:  lwz  r4,4(r3)   ; the AddRef goes through the *second* word
//   80049024:  lwz  r3,0(r4)
//   80049028:  addi r0,r3,1
//   8004902c:  stw  r0,0(r4)
//   80049030:  blr
//
// Defined in `src/rstl/rc_ptr_copy.cpp`, which claims exactly those 36 bytes and is **`NonMatching`
// because it is 97.22%, not 100%**: mwcceppc allocates the AddRef above to r5/r4 where retail
// uses r4/r3. The same class inlined gets r4/r3, so this is the out-of-line register allocator and
// not the source; twenty body spellings and every `-O`/`-pragma` combination leave it alone, and
// `docs/research/rc_ptr.md` has the table. Until it is 100% nothing in the DOL may call it, so
// `CIOWinManager::RemoveAllIOWins` - which *is* byte-exact - stays `NonMatching` too.
//
// The class adds no members and no vtable, so `rc_ptr<T>` is still 8 bytes and no other class in
// the tree changes size. MWCC does not encode base classes, so no mangled name in the tree changed
// either.
// ---------------------------------------------------------------------------
class CRcPtrData {
public:
  /// The default constructor does **nothing**: `rc_ptr<T>` writes both words itself. An
  /// initialising base constructor here is not eliminated by mwcceppc - it emitted four dead
  /// instructions at the head of every inline copy (`li r0,0 ; stw r0,0(r3) ; li r0,0 ;
  /// stw r0,4(r3)`) and took `IOWinPQNode::IOWinPQNode` from 100% to 63.64% and
  /// `CObjectReference`'s two constructors from 100% to 83.57%/80.59%. Measured, both ways.
  CRcPtrData() {}
  CRcPtrData(const CRcPtrData& other);

  /// Asks for the **call** rather than the expansion. Retail's own compiler makes both choices
  /// from one definition: it calls out in `RemoveAllIOWins` (0x80049A18, twice), `RemoveIOWin`
  /// (0x80049A98) and four more `CIOWinManager` methods, and inlines the identical nine
  /// instructions in `IOWinPQNode::IOWinPQNode` (0x80049D58), in `fn_80049034` twice and in
  /// `fn_8004935C`'s neighbours. All 15 of its call sites in the DOL are `CIOWinManager` methods
  /// and `main.elf` has no other caller, so the definition was in no header - which is what the
  /// non-template base above reproduces. One definition cannot be both at once, so `rc_ptr<T>`'s
  /// own copy constructor stays `inline` for the sites retail inlined, and a site that retail
  /// called out of line spells `rc_ptr< T >(rstl::CRcPtrData::OutOfLine, src)`.
  struct OutOfLine {
  };

  void* x0_ptr;
  int* x4_refCount;
};

template < typename T >
class rc_ptr : public CRcPtrData {
public:
  rc_ptr() {
    x0_ptr = nullptr;
    x4_refCount = & sNullRefCount;
  }
  rc_ptr(const T* ptr) : CRcPtrData() {
    x0_ptr = const_cast< T* >(ptr);
    x4_refCount = AllocRefCount();
  }
  // Retail's *inlined* copy: the same nine instructions, expanded. See `CRcPtrData::OutOfLine`
  // for why both spellings exist.
  rc_ptr(const rc_ptr& other) : CRcPtrData() {
    x0_ptr = other.x0_ptr;
    x4_refCount = other.x4_refCount;
    ++(*x4_refCount);
  }
  rc_ptr(OutOfLine, const CRcPtrData& other) : CRcPtrData(other) {}
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
  T* GetPtr() const { return static_cast< T* >(x0_ptr); }
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
  T* operator->() const { return static_cast< T* >(x0_ptr); }
  T& operator*() const { return *static_cast< T* >(x0_ptr); }
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
};

template < typename T >
void rc_ptr< T >::ReleaseData() {
  if (--(*x4_refCount) <= 0) {
    // The static type is what picks the deleting destructor, and retail's is virtual through
    // vtable slot 2 with argument 1 - so the cast is free and the `cmplwi`/`beq` null test is
    // mwcceppc's own (writing it by hand makes it emit the test twice; see docs/research/rc_ptr.md).
    delete static_cast< T* >(x0_ptr);
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
