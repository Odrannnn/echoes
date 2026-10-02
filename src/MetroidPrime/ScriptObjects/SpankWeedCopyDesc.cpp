// SpankWeedCopyDesc.cpp - a carve of SpankWeed's (module 73) `.text 0x00003B7C..0x00003BE4`:
// one function, `fn_73_3B7C`, 0x68 = 104 bytes, 26 instructions. It sits inside the module's
// unclaimed `auto_00_000003F4_text` run (0x3F4..0x3DA8) and is carved out here as its own unit, so
// `build/report.json` counts it instead of leaving it retail bytes.
//
// **What it is.** `rstl::uninitialized_copy` over this module's 0x68-byte record: one walk that
// copy-constructs every element through this copy's own `fn_73_25EC` and returns the new end. The
// twin this copy is written from - `src/MetroidPrime/ScriptObjects/CElitePirateVecCopy.cpp`,
// `Matching`, the same `rstl::uninitialized_copy` over a 0x68-byte record in module 15 - measures
// 100% with the same declaration, so the 104-byte stride, the by-value iterators and the re-read
// bound are read there rather than re-derived. What is this module's own is the callee:
// `fn_73_25EC` is at `.text:0x25EC` and is the copy constructor of
// `CJointCollisionDescription` (0x68 bytes, `include/MetroidPrime/Collision/
// CJointCollisionDescription.hpp:81`, `CHECK_SIZEOF(CJointCollisionDescription, 0x68)`), which is
// what `fn_73_3ABC` one stride-range below - `rstl::vector<CJointCollisionDescription,
// rmemory_allocator>::reserve` - copies through, and whose `rstl::string mName` at +0x2c that
// `reserve`'s destroy loop hands to `internal_dereference`.
//
// The record type is not declared here, and that is deliberate, for the reason
// `CElitePirateVecCopy.cpp` gives: a struct with a real copy constructor would make mwcceppc emit a
// weak inline copy of it into this object, which the 0x3B7C..0x3BE4 claim has no room for and
// which `tools/unit_fit.sh` would then list as a function retail's object does not define. Nothing
// here reads an offset, so the unit stays layout-immune.
//
// **The declaration that produced the shape** (the thing to copy, per the item): the two iterators
// arrive **by value, as one-word classes**, and that is what the bytes are -
//
//   00003B7C  stwu r1,-0x20(r1)
//   00003B80  mflr r0
//   00003B84  stw  r0,0x24(r1)
//   00003B88  stw  r31,0x1c(r1)
//   00003B8C  lwz  r31,0x0(r3)      <- begin, the address of the caller's temporary
//   00003B90  stw  r30,0x18(r1)
//   00003B94  mr   r30,r5           <- dst
//   00003B98  stw  r29,0x14(r1)
//   00003B9C  mr   r29,r4           <- end, kept by address
//   ...
//   00003BB8  lwz  r0,0x0(r29)      <- the bound is RE-READ every iteration
//   00003BBC  cmplw r31,r0
//   00003BC0  bne  .L
//
// `rstl::pointer_iterator<T, vector<T, Alloc>, Alloc>` is one word wide
// (`include/rstl/pointer_iterator.hpp:59`), so `SSpankIter` below is that class reduced to its
// member and built by a converting constructor - which is what makes `fn_73_3ABC` materialise the
// two-word temporaries the `lwz` above then read. The reload at the loop head is why the bound
// must not be hoisted into a local.
//
// Written as a plain function under the module's own `fn_73_3B7C` name rather than as a template
// instantiation, for the reason `CElitePirateVecCopy.cpp` gives: `symbols.txt` names the module's
// text `fn_73_3B7C` (it is a TU-local weak instantiation that dtk could not name), so renaming it
// to its mangled form would change a relocation in the module, and the `extern "C"` definition
// keeps the module's symbol table exactly as dtk split it.
//
// Definitions are in descending retail text order (only one here), because mwcceppc emits them in
// reverse source order and mwldeppc keeps the object's `.text` order verbatim - ascending would
// permute the module's bytes with objdiff still at 100% and the module hash breaking. Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/SpankWeedCopyDesc.cpp`.
//
// This unit needs `mw_version="GC/2.7"`, and that is half of what makes it match: REL objects
// default to GC/1.3.2 and that code generator schedules the by-value `begin` load differently from
// the one retail used. Same per-object override `CElitePirateVecCopy.cpp` and
// `CLumiteRelTail.cpp` use, and for the same reason.
//
// **No `force_active:` entry is needed**: `fn_73_3B7C` is called from `fn_73_3ABC` in the module's
// own unclaimed code, so it is not the orphan that dead-stripped `ScriptCoin`'s tail.
//
// Listed in `files.cmake` for the reason `CElitePirateVecCopy.cpp` gives: this unit defines no
// RELMain/RELExit, so it does not collide in a flat link, and the one relocation outside itself is
// behind the `#ifdef __MWERKS__` guard the port cannot resolve - `fn_73_25EC` is module code at
// `.text 0x25EC` with no PC-side definition. `powerpc-eabi-nm -u` on the host object therefore
// prints nothing and the port's undefined count does not move.

#include "types.h"

// Everything is behind `__MWERKS__`, as in `CElitePirateVecCopy.cpp`: the callee is a guest module
// symbol with no PC-side definition, and the host build compiles this file to nothing so the port's
// link is unchanged.
#ifdef __MWERKS__

#include "Kyoto/Alloc/CMemory.hpp"

// .text 0x25EC, 0x20 bytes, unclaimed: this module's own copy constructor for the 0x68-byte
// `CJointCollisionDescription`, called with the destination first (`mr r3,r30` then `mr r4,r31`).
// It lives in the module's own unclaimed code, so it stays retail's and is declared, not defined.
extern "C" void fn_73_25EC(void* dst, const void* src);

namespace {

/** `rstl::pointer_iterator<T, vector<T, Alloc>, Alloc>`'s single member, `current`. */
struct SSpankIter {
  void* mCur;
  explicit SSpankIter(void* cur) : mCur(cur) {}
};

} // namespace

extern "C" {

// .text 0x3B7C, 0x68 bytes.  `rstl::uninitialized_copy`: one 0x68-strided walk that
// copy-constructs each record and returns the new end, which is what `fn_73_3ABC` leaves in r3.
// The bound is re-read from `end` every iteration, so it must not be hoisted into a register.
void* fn_73_3B7C(SSpankIter begin, SSpankIter end, void* out) {
  char* cur = static_cast< char* >(begin.mCur);
  char* dst = static_cast< char* >(out);
  for (; cur != static_cast< const char* >(end.mCur); cur += 104, dst += 104) {
    fn_73_25EC(dst, cur);
  }
  return dst;
}

} // extern "C"

#endif // __MWERKS__