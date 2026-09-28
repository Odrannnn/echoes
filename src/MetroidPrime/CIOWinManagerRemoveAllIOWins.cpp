/**
 * `CIOWinManager::RemoveAllIOWins`, retail 0x80049A18, 0x80 = 128 bytes.
 *
 * **Written, 128 bytes, byte-for-byte, in a `NonMatching` unit.** The body is retail's and the
 * layout is retail's: two bottom-tested walks, one over `mDrawRoot` and one over `mPumpRoot`,
 * each copy-constructing an `rstl::rc_ptr<CIOWin>` **from the node's own `mIowin` member** and
 * passing it to `RemoveIOWin` by reference. That the copy is taken from the node's address rather
 * than from a `rc_ptr*` is itself the proof that the member is at offset 0 of a 16-byte node.
 *
 * The copy is spelled
 * `rstl::rc_ptr< CIOWin >(rstl::CRcPtrData::OutOfLine(), AsCRcPtrData(&node->mIowin))` because
 * retail emits `bl fn_80049010` here - an **out-of-line copy constructor** - while it *inlines* the
 * identical nine instructions in `IOWinPQNode::IOWinPQNode` (0x80049D58) and in four more places.
 * `rstl::CRcPtrData` holds `rc_ptr`'s two words and its copy constructor is the out-of-line one;
 * the tag is how this one site asks for the call instead of the expansion. See
 * `include/rstl/rc_ptr.hpp` and `docs/research/rc_ptr.md`.
 *
 * **`NonMatching` because `fn_80049010` itself is not byte-exact yet.**
 * `src/rstl/rc_ptr_copy.cpp` emits the right nine instructions but allocates the AddRef to r5/r4
 * where retail uses r4/r3 - mwcceppc's out-of-line register allocator differs from the one it
 * uses for an inlined expansion, and no spelling of the source changes it (measured: twenty
 * spellings of the body, four class shapes, and every `-O`/`-pragma` combination tried). Four of
 * the nine instructions differ, in register operands only. Until that is 100% the copy constructor
 * cannot be `Matching`, so nothing in the DOL may call it, so this unit cannot be `Matching`
 * either.
 *
 * Retail's own compiler makes both choices: it *inlines* the same nine instructions in
 * `IOWinPQNode::IOWinPQNode` (0x80049D58), in `fn_80049034` and in four more places, and calls out
 * only here and in `RemoveIOWin`, `fn_80049244`, `fn_8004935C`, `fn_80049764` and `fn_80049884` -
 * all 15 call sites are `CIOWinManager` methods, and `main.elf` has no other caller. So retail's
 * definition was not visible in a header, and it was not a weak template instantiation either
 * (`nm` reports `80049010 T`, a strong global, as it does for
 * `ReleaseData__Q24rstl15rc_ptr<6CIOWin>Fv`).
 *
 * It is also the *only* one of the four frame-loop functions the copy constructor blocks.
 * `AddIOWin`, `PumpMessages` and `CInputGenerator::Update` never call 0x80049010; they inline the
 * copy.
 */

#include "MetroidPrime/CIOWin.hpp"

#include "MetroidPrime/CIOWinManager.hpp"

// `rstl::CRcPtrData` is a **same-layout view** of the first two words of any `rc_ptr<T>`:
// upstream's `rc_ptr` keeps `mPtr` at +0 and `mRefCount` at +4, which is where retail's
// `x0_ptr` and `x4_refCount` are, so a `reinterpret_cast` reaches the out-of-line copy without
// changing the class. See `include/rstl/rc_ptr.hpp`.
static inline const rstl::CRcPtrData& AsCRcPtrData(const void* owner) {
  return *reinterpret_cast< const rstl::CRcPtrData* >(owner);
}

void CIOWinManager::RemoveAllIOWins() {
  while (mDrawRoot) {
    rstl::rc_ptr< CIOWin > win(rstl::CRcPtrData::OutOfLine(), AsCRcPtrData(&mDrawRoot->mIowin));
    RemoveIOWin(win);
  }
  while (mPumpRoot) {
    rstl::rc_ptr< CIOWin > win(rstl::CRcPtrData::OutOfLine(), AsCRcPtrData(&mPumpRoot->mIowin));
    RemoveIOWin(win);
  }
}
