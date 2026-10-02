#include "Kyoto/Animation/CAnimCharacterSet.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

// The functions below are retail's *unnamed* functions in this unit - the ones dtk names
// `fn_<address>` because retail's own symbol table has no name for them. Each is already
// byte-identical to code this translation unit emits, but under the mangled name mwceppc gives a
// class destructor, a template instantiation or an inline member, and objdiff pairs functions by
// name, so all of them score 0.00% for that reason alone. `src/Kyoto/Animation/CAnimationSet.cpp`
// documents the technique and the reasoning at length; the short version is that no C++
// declaration can rename a template instantiation, so the only way to give objdiff a symbol to
// pair is to write the function out under an `extern "C"` name.
//
// The bodies are not transcribed disassembly: each is the body the class headers already spell,
// and each was read out of `build/G2ME01/obj/Kyoto/Animation/CAnimCharacterSet.o` - retail's own
// object, with its relocations resolved - before it was written. A private member is reached
// through the class's own `const` accessor and `const_cast`ed back, which costs no instruction
// and keeps the member's name where a reader can check it. The one exception is
// `CAnimationSet::mDefaultTransition`: the accessor spelling folds the `rc_ptr` member's null
// test and its address into a single register, and retail has two - see the note on
// `fn_8028EBB8` below and `docs/research/raw_offsets.md`.
//
// A body calls its neighbour *by retail's name* rather than inlining it, because a `bl` is a
// `bl` in both objects and objdiff compares the instruction rather than the symbol it relocates
// to. That is measured, not assumed: `fn_8028DA3C` here pairs with retail's own `fn_8028DA3C`
// through a `bl` that in our object is written against a `rstl::vector< CAnimPOIData >`.
//
// The same is true of a data relocation, which is why `fn_8028EC7C` reaches 100.00% while
// naming mwceppc's `@stringBase0` against retail's `lbl_803AED58`. That is also why the one function
// whose *name* has no C++ spelling at all, `fn_8028E8C4`, could be written anyway: the vtable it
// stores is named `lbl_803B9240` in retail and `__vt__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>`
// by mwceppc, and neither name is reachable from C++ - but both spell the same three instruction
// words, so the table is declared under retail's name and the function is written out by hand. Its
// note below has the two details that took the search.
//
// **Definitions are in descending retail offset**, which is what mwcceppc needs in order to emit
// them in ascending order (see "Declare in reverse" in `docs/RUNNING_THE_DECOMP.md`, and
// `tools/check_decl_order.py`).

typedef rstl::pair< int, CCharacterInfo > CharPair;
typedef rstl::vector< CharPair > CharVec;
typedef rstl::pointer_iterator< CharPair, CharVec, rstl::rmemory_allocator > CharIter;
typedef CAnimationSet::AdditiveAnimationList AdditiveVec;

extern "C" void* fn_8028EBB8(void* self, int flag);
extern "C" void* fn_8028EB60(void* self, int flag);
extern "C" void fn_8028EB3C(void* p);
extern "C" void fn_8028EB1C(void* p);
extern "C" void fn_8028EACC(CharIter begin, CharIter end);
extern "C" void fn_8028EA94(CharIter begin, CharIter end);
extern "C" void* fn_8028EA10(void* self, int flag);
extern "C" void* fn_8028E9B8(void* self, int flag);
extern "C" void* fn_8028E954(void* self, int flag);
extern "C" void* fn_8028ED18(void* self, int flag);

// The three vector deleting destructors `fn_8028EBB8` calls by retail's name. They are defined,
// under these names, in `src/Kyoto/Animation/CAnimationSet.cpp`; declaring them here is what
// keeps the `bl` in this object pointed at retail's `fn_8028DA3C` rather than at a mangled
// `rstl::vector` destructor.
extern "C" void* fn_8028DA3C(void* self, int flag);
extern "C" void* fn_8028DC60(void* self, int flag);
extern "C" void* fn_8028DD7C(void* self, int flag);
extern "C" void* fn_8028DFB8(void* self, int flag);

CAnimCharacterSet::CAnimCharacterSet(CInputStream& in)
: mVersion(in.Get< ushort >()), mCharacterSet(in), mAnimationSet(in) {}

// `fn_8028ED18` - retail `.text:0x8028ED18`, 0x64 = 100 bytes, unnamed. It is
// `rstl::auto_ptr< CAnimCharacterSet >::~auto_ptr()`, the *deleting* destructor mwceppc generates
// for the `CFactoryFnReturn` member.
//
//     57c  lbz   r0,0(r30) / cmplwi r0,0 / beq        ; if (!mHas) skip
//     588  lwz   r3,4(r30) / li r4,1 / bl fn_8028E954 ; delete mItem
//     594  extsh. r0,r31 / ble                         ; the delete flag, a *signed halfword*
//     59c  mr    r3,r30 / bl Free__7CMemoryFPCv
//
// Unlike `~auto_ptr< IObj >` (which is in the same unit and does have a null test on the item)
// this one has none: `mItem` is only read when `mHas` is set, and `~CAnimCharacterSet` copes with
// a null `this` itself. The flag is sign-extended from 16 bits *by the function*, so the source
// compares a `short`.
extern "C" void* fn_8028ED18(void* self, int flag) {
  if (self != nullptr) {
    rstl::auto_ptr< CAnimCharacterSet >* ap =
      static_cast< rstl::auto_ptr< CAnimCharacterSet >* >(self);
    if (ap->owner()) {
      fn_8028E954(ap->get(), 1);
    }
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028EC7C` - retail `.text:0x8028EC7C`, 0x9C = 156 bytes, unnamed. It is
// `TObjOwnerDerivedFromIObj< CAnimCharacterSet >::GetNewDerivedObject(const rstl::auto_ptr<>&)`,
// whose whole body is `return rs_new TObjOwnerDerivedFromIObj< T >(obj);`. Written out because
// mwceppc can only emit that instantiation under its mangled name.
//
//     4c0  stwu  r1,-16(r1) / li r3,8 / bl __nw__        ; new (rs_new) TObjOwnerDerivedFromIObj
//     4f4  cmplwi r3,0 / beq                             ; the null-return tail
//     4fc  stw   __vt__4IObj,0(r3)                       ; IObj's vptr
//     51c  stw   __vt__CObjOwnerDerivedFromIObjUntyped,0(r3)
//     520  stb   r5,0(r31)                               ; obj.mHas = false  (auto_ptr::release)
//     528  stw   r4,4(r3)                                ; m_objPtr = obj.mItem
//     52c  stw   lbl_803B9240,0(r3)                      ; this class's own vptr, set last
//     53c  stb   r0,0(r30) / stw r3,4(r30)               ; the auto_ptr< TObjOwnerDerivedFromIObj >
//
// The return type is the class itself, so `r3` is the ABI's hidden return slot and `r4` is `obj`.
// The `new` placement string relocation is `@stringBase0` here against retail's `lbl_803AED58`,
// and that is not a difference: the object already carried the same `"\?\?(\?\?)"` literal for
// `FAnimCharacterSet` before this change, and objdiff compares the instruction rather than the
// symbol a relocation names (measured - see `fn_8028EC50` below).
extern "C" rstl::auto_ptr< TObjOwnerDerivedFromIObj< CAnimCharacterSet > >
fn_8028EC7C(const rstl::auto_ptr< CAnimCharacterSet >& obj) {
  return rs_new TObjOwnerDerivedFromIObj< CAnimCharacterSet >(obj);
}

// `fn_8028EC50` - retail `.text:0x8028EC50`, 0x2C = 44 bytes, unnamed. It is
// `TToken< CAnimCharacterSet >::GetIObjObjectFor(const rstl::auto_ptr<>&)`, which is
// `return GetNewDerivedObject(obj);` - a frame, the result slot kept in `r31`, one `bl`, the
// epilogue.
//
//     4a8  bl    fn_8028EC7C
//
// Our `bl` is `GetNewDerivedObject__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>...`, mwceppc's
// out-of-line copy of retail's `fn_8028EC7C`, and the function still scores 100.00%: objdiff
// compares the instruction, not the symbol the relocation names. `GetNewDerivedObject` is *not*
// inlined here - mwceppc emits it out of line - so the same spelling is 27.64% for `fn_8028EC7C`
// itself and has to be written out separately, as above.
extern "C" rstl::auto_ptr< TObjOwnerDerivedFromIObj< CAnimCharacterSet > >
fn_8028EC50(const rstl::auto_ptr< CAnimCharacterSet >& obj) {
  return TObjOwnerDerivedFromIObj< CAnimCharacterSet >::GetNewDerivedObject(obj);
}

// `fn_8028EBB8` - retail `.text:0x8028EBB8`, 0x98 = 152 bytes, unnamed. It is
// `CAnimationSet::~CAnimationSet()`, and every offset in it is a member of `CAnimationSet`
// (0x64 bytes, checked in the header).
//
//     41c  addi  r3,r30,84  / li r4,-1 / bl fn_8028DA3C  ; mEventSets,      vector< CAnimPOIData >
//     428  addi  r3,r30,68  / li r4,-1 / bl fn_8028DC60  ; mHalfTransitions, vector< CHalfTransition >
//     434  addi  r3,r30,44  / li r4,-1 / bl ~vector<pair<uint, CAdditiveAnimationInfo> >
//     440  addic. r0,r30,36 / beq                         ; if (mDefaultTransition) ...
//     448  addi  r3,r30,36  / bl ReleaseData<rc_ptr<IMetaTrans> >
//     450  addi  r3,r30,20  / li r4,-1 / bl fn_8028DD7C  ; mTransitions,    vector< CTransition >
//     45c  addi  r3,r30,4   / li r4,-1 / bl fn_8028DFB8  ; mAnimations,     vector< CAnimation >
//     468  extsh. r0,r31 / ble / mr r3,r30 / bl Free      ; the delete flag
//
// Members are destroyed in reverse declaration order, which is what the header's order already
// gives, and the two `rc_ptr` words at +36 are tested rather than assumed: `~rc_ptr` is inlined
// into its own null test and `ReleaseData` call, and `ReleaseData` itself has no null test on
// `mPtr`, so the test belongs here.
//
// `mDefaultTransition` is reached as `base + 36` rather than through `GetDefaultTransition()`,
// and that is the one measured reason this file has a raw offset in it. mwceppc inlines
// `~rc_ptr` into its null test on `this` followed by the `ReleaseData` call, and when `this` is a
// known member of the enclosing class it keeps the address in one register for both - the `bl`
// is then `addic. r3,r30,36 / beq / bl`, two instructions short of retail's. Spelled as a byte
// offset the compiler cannot see the member, so it computes the address a second time into `r3`
// after the `addic.` has used it, and the three instructions match. This is the sole kind-B site
// in the file; see `docs/research/raw_offsets.md`.
extern "C" void* fn_8028EBB8(void* self, int flag) {
  if (self != nullptr) {
    CAnimationSet* set = static_cast< CAnimationSet* >(self);
    char* base = static_cast< char* >(self);
    fn_8028DA3C(base + 84, -1);
    fn_8028DC60(base + 68, -1);
    const_cast< AdditiveVec& >(set->GetAdditiveAnimInfoList()).~AdditiveVec();
    // Spelled as the member's own destructor call, not as `if (p) p->ReleaseData()`: mwceppc
    // inlines `~rc_ptr` into its own null test on `this` followed by the `ReleaseData` call, and
    // it needs the address in `r3` for the call after using it for the `addic.`, so the source
    // computes the address twice. A hand-written null test reuses `r3` instead and is 8 bytes
    // shorter than retail's.
    reinterpret_cast< rstl::rc_ptr< IMetaTrans >* >(base + 36)->~rc_ptr();
    (void)0;
    fn_8028DD7C(base + 20, -1);
    fn_8028DFB8(base + 4, -1);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028EB60` - retail `.text:0x8028EB60`, 0x58 = 88 bytes, unnamed. It is
// `rstl::pair< int, CCharacterInfo >::~pair()` as the deleting destructor: the `int` first member
// is trivial, so only the `CCharacterInfo` at +4 is destroyed, and then the flag tail.
//
//     3c4  addi  r3,r30,4 / li r4,-1 / bl __dt__14CCharacterInfoFv
//     3d0  extsh. r0,r31 / ble / mr r3,r30 / bl Free
//
// `0xFC` = 252 = `sizeof(pair<int, CCharacterInfo>)`, and that is also the stride `fn_8028EACC`
// below walks, so the two agree on the layout without either of them hard-coding it.
extern "C" void* fn_8028EB60(void* self, int flag) {
  if (self != nullptr) {
    static_cast< CharPair* >(self)->second.~CCharacterInfo();
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028EB3C` - retail `.text:0x8028EB3C`, 0x24 = 36 bytes, unnamed. It is
// `rstl::destroy_impl< pair<int, CCharacterInfo >* >`: a frame, `li r4,-1` - the `~pair` deleting
// flag meaning *do not* free the element, since the element came out of a vector - and one
// unconditional `bl` to `fn_8028EB60`. The `is_trivially_destructible` test is not here: nothing
// in the tree declares `pair<int, CCharacterInfo>` trivially destructible, so there is nothing to
// fold away and the call stands on its own.
extern "C" void fn_8028EB3C(void* p) { fn_8028EB60(p, -1); }

// `fn_8028EB1C` - retail `.text:0x8028EB1C`, 0x20 = 32 bytes, unnamed. It is
// `rstl::destroy< pair<int, CCharacterInfo >* >`: a frame and one unconditional `bl` to
// `fn_8028EB3C`, nothing else. Written as a call rather than as `rstl::destroy(...)` because
// `destroy` and `destroy_impl` are both in-class-inline and mwceppc folds them into one another -
// and folding `destroy_impl` here would fold in its `is_trivially_destructible` test too, which
// is retail's *previous* function and not this one. It builds no iterators on the stack, unlike
// `fn_8028EA94` below, because this instantiation is over a raw pointer and not over
// `pointer_iterator`.
extern "C" void fn_8028EB1C(void* p) { fn_8028EB3C(p); }

// `fn_8028EACC` - retail `.text:0x8028EACC`, 0x50 = 80 bytes, unnamed. It is
// `rstl::destroy_impl< pointer_iterator< pair<int, CCharacterInfo >, ... > >`, the per-element
// body `~vector<pair<int, CCharacterInfo>>` calls.
//
//     320  lwz   r31,0(r3)                  ; begin.get_pointer()
//     328  mr    r30,r4                     ; the *end* iterator, by hidden pointer
//     32c  b     33c                        ; the test comes first
//     330  mr    r3,r31 / bl fn_8028EB1C     ; destroy(&*cur)
//     338  addi  r31,r31,252                ; ++cur  (0xFC = sizeof(pair<int, CCharacterInfo>))
//     33c  lwz   r0,0(r30) / cmplw r31,r0 / bne 330
//
// The loop is entered at its bottom test, so an empty range destroys nothing. The end cursor is
// reloaded from the iterator every iteration rather than kept in a register, which is what leaves
// the begin cursor in `r31` and the end iterator in `r30`; the two `pointer_iterator`s arrive by
// hidden pointer, so `r3` and `r4` are *pointers to* the cursors.
extern "C" void fn_8028EACC(CharIter begin, CharIter end) {
  CharPair* cur = begin.get_pointer();
  while (cur != end.get_pointer()) {
    fn_8028EB1C(cur);
    ++cur;
  }
}

// `fn_8028EA94` - retail `.text:0x8028EA94`, 0x38 = 56 bytes, unnamed. It is
// `rstl::destroy< pointer_iterator< pair<int, CCharacterInfo >, ... > >`: the two iterators are
// built on the stack at 8(r1)/12(r1) out of the two cursors loaded from `r3`/`r4`, and
// `fn_8028EACC` is called on them. The `addi r4,r1,8 / addi r3,r1,12` pair is the by-value pass
// of two `pointer_iterator`s, which are classes and so go on the stack.
extern "C" void fn_8028EA94(CharIter begin, CharIter end) { fn_8028EACC(begin, end); }

// `fn_8028EA10` - retail `.text:0x8028EA10`, 0x84 = 132 bytes, unnamed. It is
// `rstl::vector< pair<int, CCharacterInfo > >::~vector()`, the *deleting* destructor mwceppc
// generates, called once, from `CCharacterSet`'s own deleting destructor.
//
//     274  lwz   r0,4(r30) / lwz r5,12(r30) / mulli r0,r0,252 / add r5,r5,r0
//     28c  stw   r5,12(r1) / lwz r0,12(r30) / stw r5,8(r1) / stw r0,16(r1) / stw r0,20(r1)
//     2a0  bl    fn_8028EA94                          ; destroy(begin(), end())
//     2a4  lwz   r3,12(r30) / bl Free__7CMemoryFPCv   ; mAllocator.deallocate(mItems)
//     2ac  extsh. r0,r31 / ble / mr r3,r30 / bl Free   ; the delete flag
//
// `mulli 252` is 0xFC = `sizeof(pair<int, CCharacterInfo>)`, and `mItems` is at +12 and `mCount`
// at +4, which is `rstl::vector`'s own layout. Three details, all measured: the flag is
// sign-extended from 16 bits by the *function*, so the source compares a `short` and not an
// `int`; it stays in `r4` throughout, so only `r30`/`r31` are saved; and the function returns
// `this`, which is the deleting-destructor convention and costs nothing.
extern "C" void* fn_8028EA10(void* self, int flag) {
  if (self != nullptr) {
    CharVec* vec = static_cast< CharVec* >(self);
    fn_8028EA94(CharIter(vec->mItems), CharIter(vec->mItems + vec->mCount));
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028E9B8` - retail `.text:0x8028E9B8`, 0x58 = 88 bytes, unnamed. It is
// `CCharacterSet::~CCharacterSet()`: the one non-trivial member is the
// `rstl::vector< pair<int, CCharacterInfo> >` at +4 (`mTableCount` is a `ushort` at +0), and then
// the same flag tail every deleting destructor in this unit has.
//
//     21c  addi  r3,r30,4 / li r4,-1 / bl fn_8028EA10
//     228  extsh. r0,r31 / ble / mr r3,r30 / bl Free
extern "C" void* fn_8028E9B8(void* self, int flag) {
  if (self != nullptr) {
    CCharacterSet* set = static_cast< CCharacterSet* >(self);
    fn_8028EA10(
      &const_cast< rstl::vector< rstl::pair< int, CCharacterInfo > >& >(set->GetCharacterList()),
      -1);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028E954` - retail `.text:0x8028E954`, 0x64 = 100 bytes, unnamed. It is
// `CAnimCharacterSet::~CAnimCharacterSet()`, the destructor of the class this unit is named for.
// `mVersion` is a `ushort` at +0, `mCharacterSet` (0x14 bytes) at +4 and `mAnimationSet` (0x64
// bytes) at +24, which is the 0x7c the header's `CHECK_SIZEOF` states.
//
//     1b8  addi  r3,r30,24 / li r4,-1 / bl fn_8028EBB8
//     1c4  addi  r3,r30,4  / li r4,-1 / bl fn_8028E9B8
//     1d0  extsh. r0,r31 / ble / mr r3,r30 / bl Free
extern "C" void* fn_8028E954(void* self, int flag) {
  if (self != nullptr) {
    CAnimCharacterSet* set = static_cast< CAnimCharacterSet* >(self);
    fn_8028EBB8(&const_cast< CAnimationSet& >(set->GetAnimationSet()), -1);
    fn_8028E9B8(&const_cast< CCharacterSet& >(set->GetCharacterSet()), -1);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028E8C4` - retail `.text:0x8028E8C4`, 0x90 = 144 bytes, unnamed - is the *deleting*
// destructor of `TObjOwnerDerivedFromIObj< CAnimCharacterSet >`. Written out by hand under
// `extern "C"` because no C++ declaration can rename a template instantiation's destructor: mwceppc
// emits the very same 144 bytes under `__dt__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>Fv`,
// at the very same offset in `.text`, word for word, and objdiff scores that **0.00%** purely
// because it pairs functions by symbol name.
//
//     108  stwu  r1,-16(r1) / mflr r0 / stw r0,20(r1) / stw r31,12(r1)
//     118  mr    r31,r4 / stw r30,8(r1) / mr. r30,r3 / beq 17c      ; if (!self) goto epilogue
//     128  lis   r3,lbl_803B9240@ha / addi r0,r3,..@l / stw r0,0(r30)
//     134  lwz   r3,4(r30) / cmplwi r3,0 / beq 148                  ; if (!m_objPtr) ...
//     140  li    r4,1 / bl fn_8028E954                              ; ~CAnimCharacterSet(m_objPtr, 1)
//     148  cmplwi r30,0 / beq 16c
//     150  lis   r3,__vt__31CObjOwnerDerivedFromIObjUntyped@ha / addi r0,r3,.. / stw r0,0(r30)
//     15c  beq   16c                                                  ; dead: the flags above still hold
//     160  lis   r3,__vt__4IObj@ha / addi r0,r3,.. / stw r0,0(r30)
//     16c  extsh. r0,r31 / ble 17c / mr r3,r30 / bl Free            ; the delete flag
//
// Two things stopped this from being written before, and neither is what
// `docs/goal-notes/progress-unit-canimcharacterset.md` concluded (it records 58.31% for a direct
// destructor call and 53.19% for `delete`, both of which drop the three vptr stores).
//
// **1. The three vptr stores are writable by hand, and *which* table is stored does not matter.**
// mwceppc emits them only inside a real destructor, but `lis r3,X@ha / addi r0,r3,X@l / stw r0,0(r30)`
// is the same three instruction words for every `X` - the address lives in the relocation, and
// `functionRelocDiffs=none` (`configure.py`'s `progress_report_args`) is why `fn_8028EC7C` scores
// 100.00% against retail's `lbl_803AED58` while our relocation names `@stringBase0`. All three tables
// are nameable as plain data: `lbl_803B9240` **is** in `config/G2ME01/symbols.txt` (line 18458,
// `.data:0x803B9240`, 0x10 bytes - it is the table mwceppc emits as
// `__vt__45TObjOwnerDerivedFromIObj<17CAnimCharacterSet>`, whose mangled name contains `<` and `>` and
// so cannot be declared in C++, but the table itself is an ordinary DOL object).
// `src/MetroidPrime/CConsoleOutputWindowCtor.cpp` is the precedent for the
// `*reinterpret_cast<void**>(this) = const_cast<char*>(table)` spelling and for these exact words.
//
// **2. The dead `beq` at 15c needs the second test written as an empty branch, not a second `if`.**
// Retail's own mwceppc CSE'd the second `cmplwi r30,0` of its base-destructor sequence but kept the
// branch, and mwceppc will not CSE two source-level `if (self != nullptr)` tests - measured, that
// spelling reaches **97.08%**, 37 instructions to retail's 36, with the extra `cmplwi r30,0` at +0x5c.
// Writing the inner test as `if (self == nullptr) { } else { ...store... }` gives both branches the
// same target block, which is what lets mwceppc reuse the compare, and the function is byte-identical.
// A third spelling, `if (self) { ... } else { ... }` around both stores, is 36 instructions and 98.19%
// - right count, wrong shape.
extern "C" {
extern const char lbl_803B9240[];                              // .data 0x803B9240, this class's table
extern const char __vt__31CObjOwnerDerivedFromIObjUntyped[];  // .data 0x803B16E8
extern const char __vt__4IObj[];                               // .data 0x803B16DC
} // extern "C"

extern "C" void* fn_8028E8C4(void* self, int flag) {
  if (self != nullptr) {
    void** vtable = reinterpret_cast< void** >(self);
    TObjOwnerDerivedFromIObj< CAnimCharacterSet >* owner =
      static_cast< TObjOwnerDerivedFromIObj< CAnimCharacterSet >* >(self);
    *vtable = const_cast< char* >(lbl_803B9240);
    if (owner->GetContents() != nullptr) {
      fn_8028E954(owner->GetContents(), 1);
    }
    if (self != nullptr) {
      *vtable = const_cast< char* >(__vt__31CObjOwnerDerivedFromIObjUntyped);
      // The empty `if` is load-bearing: it is what makes this branch share its target with the one
      // above, so mwceppc reuses that compare and emits retail's dead `beq` with no `cmplwi` of its
      // own. See note 2 above.
      if (self == nullptr) {
      } else {
        *vtable = const_cast< char* >(__vt__4IObj);
      }
    }
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_8028E820` - retail `.text:0x8028E820`, 0xA4 = 164 bytes, unnamed. It is
// `CFactoryFnReturn::CFactoryFnReturn(CAnimCharacterSet*)`, whose member-initialiser list is
// `obj(TToken< T >::GetIObjObjectFor(ptr).release())`.
//
//     64   neg/or/srwi                                  ; auto_ptr<CAnimCharacterSet>(ptr): mHas
//     84   addi r3,r1,8  / addi r4,r1,16 / stb r0,16(r1)
//     94   bl    fn_8028EC50                             ; obj at 8(r1), ptr at 16(r1)
//     98   lwz   r3,12(r1) / li r0,0 / stb r0,8(r1)     ; auto_ptr<TObjOwnerDerivedFromIObj>::release
//     b0   stb   r0,0(r31) / stw r3,4(r31)               ; self->obj = auto_ptr<IObj>(r3)
//     b8   lbz   r0,8(r1) / cmplwi r0,0 / beq            ; ~auto_ptr<TObjOwnerDerivedFromIObj>
//     d0   lwz   r12,0(r3) / lwz r12,8(r12) / bctrl      ; ...which deletes through IObj's vtable
//     ec   bl    fn_8028ED18                             ; ~auto_ptr<CAnimCharacterSet>(16(r1), -1)
//     f4   mr    r3,r31                                  ; a constructor returns `this`
//
// `CFactoryFnReturn` is nothing but that one `rstl::auto_ptr< IObj > obj`, so `self` is spelled as
// the member and the two member stores are `auto_ptr`'s own public fields. Three orderings in
// retail's body are what the spelling has to reproduce, and each was measured:
//
//   - **The member is built before the temporaries die.** `IObj* p = ...release(); obj->mItem = p;`
//     puts both stores *after* `~auto_ptr<CAnimCharacterSet>` and reaches 46.32%; keeping them in
//     the same full-expression that built the temporaries puts them back before it.
//   - **The `if (mHas)` of the released `auto_ptr` is not folded.** With the release in a separate
//     statement mwceppc knows the flag is false and drops the test (`b`/`beq` over dead code) - the
//     same 46.32% spelling. In this one it keeps the load and the test.
//   - **`r3` comes back as `self`.** A constructor returns `this`, so the body ends `mr r3,r31`;
//     returning the `auto_ptr` by value instead reaches 97.56% with that one instruction missing.
//
// `fn_8028EC50` is reached here rather than `fn_8028EC7C` because the *source* is
// `TToken<T>::GetIObjObjectFor`, and `GetIObjObjectFor` is what `CFactoryFnReturn`'s constructor
// calls; `bl` targets do not affect the score (see `fn_8028EC50` above).
extern "C" rstl::auto_ptr< IObj >* fn_8028E820(void* self, CAnimCharacterSet* ptr) {
  rstl::auto_ptr< IObj >* obj = static_cast< rstl::auto_ptr< IObj >* >(self);
  IObj* p;
  obj->mItem =
    (obj->mHas = (p = TToken< CAnimCharacterSet >::GetIObjObjectFor(ptr).release()) != nullptr, p);
  return static_cast< rstl::auto_ptr< IObj >* >(self);
}

CFactoryFnReturn FAnimCharacterSet(const SObjectTag& tag, CInputStream& in,
                                         const CVParamTransfer& xfer) {
  return rs_new CAnimCharacterSet(in);
}
