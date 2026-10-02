// CSwarmBasicsLeafCache.cpp - SwarmBasics' (module 80) octree-node vector's copy machinery,
// `.text 0x5B90..0x5C8C`, 0xFC = 252 bytes, four functions. The range sits inside the module's
// unclaimed `auto_00_00004280_text` run (0x4280..0xB30C) and is carved out here as its own unit,
// so `build/report.json` counts these four instead of leaving them retail bytes.
//
// **What the four are.** One `rstl::reserved_vector` chain over one element type. The element is
// `CAreaOctTree::Node` (`NESTED_CHECK_SIZEOF(CAreaOctTree, Node, 0x24)`,
// `include/WorldFormat/CAreaOctTree.hpp:91`): `fn_80_5C40` is its copy constructor and the other
// three are the vector's `uninitialized_copy_n` (`fn_80_5B90`), `rstl::construct` (`fn_80_5BF8`)
// and `rstl::construct_impl` (`fn_80_5C18`). The 0x24 stride is in the bytes twice and agrees
// with this module's own `push_back` (`mulli r0,r0,36` at 0x5A5C).
//
// **The element is a local struct, and that is deliberate.** `CAreaOctTree::Node` exists and is
// laid out and checked above, but its members are private (`mPtr`, `mOwner`, `mNodeType`), so
// reaching them from a free function means either a friend declaration in a shared header or raw
// offsets - and `tools/check_raw_offsets.py` fails the gate on a raw offset in a file it has no
// section for. A local struct with the members named, in the same order and at the same offsets,
// reproduces retail's bytes and keeps this file at zero raw-offset sites.
//
// **The six floats are six named members, not an array.** Measured: an array copy is a *word*
// copy - all nine words `lwz`/`stw` - and retail's `fn_80_5C40` is nine `lfs`/`stfs`/`lwz` pairs
// in the `CAABox`'s own member order, with `f1` and `f0` alternating. Six scalars give that;
// `float mAabb[6]` does not.
//
// **Why the claim is 0x5B90..0x5C8C and not the whole family - and this is the expensive part of
// the item.** The run continues on both sides and *both* sides are blocked, for different and
// measured reasons:
//
//  * `fn_80_5B4C` (0x5B4C, 68 bytes) is the vector's copy constructor, it reaches 100% like the
//    four here, and it is still not in the claim. Extending the range down to 0x5B4C makes
//    `DigitalGuardian`, `ElitePirate`, `Lumite` and `SandBoss` come out of `dtk rel make` with
//    different bytes - four module hashes fail while `main.dol`'s sha1 and the per-function diff
//    are both clean. Measured by narrowing the claim one function at a time: 0x5B90..0x5C8C is
//    **87 files OK**, 0x5BF8..0x5C8C is not, 0x5B4C..0x5C8C is not, and 0xB088..0xB0F0 (a claim
//    in the same `auto_00_00004280_text` run, 0x8f0 earlier in the file) is **87 files OK** -
//    so it is this particular range and not "any new claim in this module", and it is not the
//    object's bytes either: the same four fail with the object marked `NonMatching` and its
//    `.o` left out of the link, and they pass with a one-line comment added to
//    `CSwarmBasicsHooks.cpp`, which re-runs the same global `dtk rel make` (all 86 `.rel` files
//    come out of **one** `makerel` rule, so any module's link re-runs all of them) and matches.
//  * `fn_80_5C8C` (0x5C8C, 88 bytes) reaches 100% too, but it calls `fn_80_589C` at 0x589C, and
//    `fn_80_58EC` (95.85%) and `fn_80_5AC0` (98.00%) are below it. **In both the only difference
//    is register allocation** - the same 35 instructions in the same order, with the count in
//    `r6`/the cursor in `r3` where this build puts them in `r5`/`r6`. Seven spellings of the
//    loop (a local bound, `destroy` vs `destroy_impl`, `&mData[i]` vs `mData + i`, an explicit
//    element cursor, a `data()` accessor, and the same object under `mw_version="GC/1.3.2"`,
//    which also costs four of the four functions here) leave both scores unchanged, so the
//    variable is not the source. Claiming either would put different registers in the link and
//    break SwarmBasics' own hash. `fn_80_5C8C` cannot be claimed separately from what it calls
//    either: `tools/probe_sources.sh` runs `tools/link_check.sh --strict`, which fails when the
//    port's undefined count grows, and a `Matching` object calling a symbol nothing defines is
//    exactly one new undefined name.
//
// **Nothing outside the four is referenced.** `fn_80_5C18` -> `fn_80_5C40`, `fn_80_5BF8` ->
// `fn_80_5C18`, `fn_80_5B90` -> `fn_80_5BF8`, and `fn_80_5C40` calls nothing: the claim's object
// has no undefined symbol at all. That matters twice over here - it keeps `files.cmake` free of
// new host references, and it keeps SwarmBasics' own import-stub layout untouched.
//
// **One contiguous range per unit is the shape that survives** - `docs/RUNNING_THE_DECOMP.md`,
// "One unit cannot claim two discontiguous ranges".
//
// **This file is listed in `files.cmake`, and its host branch defines nothing at all** - the
// arrangement `ScriptGuiTail.cpp`, `CLumiteRelTail.cpp` and `CElitePirateVecCopy.cpp` use. All
// four are module-local `fn_80_*` code addresses with no PC-side definition, so a host body would
// add four undefined references and close none.
//
// Definitions are in **descending** retail text order: mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim, so ascending would
// permute the module's bytes with objdiff still at 100% and the module hash breaking. Checked
// with `python3 tools/check_decl_order.py --unit MetroidPrime/Enemies/CSwarmBasicsLeafCache.cpp`.
//
// The names are retail's own `fn_80_<off>` strings from
// `config/G2ME01/rels/SwarmBasics/symbols.txt:92-94`, so no rename is needed and the definitions
// are `extern "C"` to stay unmangled - objdiff pairs by name.

#include "types.h"

namespace {

/** `CAreaOctTree::Node` - 0x24 bytes: the `CAABox` (six floats, 0x00..0x17), the tree pointer
 *  `mPtr` at 0x18, the owning `CAreaOctTree&` at 0x1c and `ETreeType mNodeType` at 0x20. Only
 *  `fn_80_5C40` writes them here; `mPtr` is named for the layout and nothing in this unit reads
 *  it. */
struct SNode {
  float mMinX;
  float mMinY;
  float mMinZ;
  float mMaxX;
  float mMaxY;
  float mMaxZ;
  void* mPtr;
  void* mOwner;
  int mNodeType;
};

} // namespace

#ifdef __MWERKS__

extern "C" {

// .text 0x5C40, 0x4C = 76 bytes: `CAreaOctTree::Node`'s copy constructor, out of line in retail
// (0x802461A0 in the DOL - see `fn_80248F2C`'s note in src/WorldFormat/CMetroidAreaCollider.cpp
// :893, which reaches it through a `bl`) and named `fn_80_5C40` here. Member order, no frame,
// and the interleaving is the compiler's: `lfs f1,0` / `lfs f0,4` / `stfs f1,0` / `lfs f1,8` /
// `stfs f0,4`, then the same pair of registers for each following word.
#pragma dont_inline on
void fn_80_5C40(SNode* dest, const SNode& src);
#pragma dont_inline reset

// .text 0x5C18, 0x28 = 40 bytes: `rstl::construct_impl< CAreaOctTree::Node >`. Placement new's
// own null test on the destination (`cmplwi r3,0` / `beq`, hoisted above the frame's `stw lr`),
// then one `bl`. Written as the explicit call rather than as `new (dest) SNode(src)` so that no
// weak COMDAT copy of the copy constructor is emitted into this object - the claim has room for
// four named functions and no fifth.
void fn_80_5C18(void* dest, const SNode& src);

// .text 0x5BF8, 0x20 = 32 bytes: `rstl::construct< CAreaOctTree::Node >` - a frame and one
// unconditional `bl` to the previous function and nothing else.
void fn_80_5BF8(void* dest, const SNode& src);

// .text 0x5B90, 0x68 = 104 bytes: `uninitialized_copy_n` over the 0x24-byte node. `(src, count,
// dest)` in r3/r4/r5, the three cursors in r31/r30/r29, a `b` to the bottom test so a zero count
// copies nothing and still returns `dest`, and the cursors advance *after* the construct. The
// same 26 instructions as `fn_80248EA4` (src/WorldFormat/CMetroidAreaCollider.cpp:917), which
// is its measured twin, with the 0x24 stride.
void* fn_80_5B90(const SNode* src, int count, SNode* dest);

// The definitions that must not be folded into their callers: `fn_80_5C18` calls `fn_80_5C40`,
// `fn_80_5BF8` calls `fn_80_5C18` and `fn_80_5B90` calls `fn_80_5BF8`, and retail has a real `bl`
// for each.
#pragma dont_inline on

void fn_80_5C40(SNode* dest, const SNode& src) { *dest = src; }

void fn_80_5C18(void* dest, const SNode& src) {
  if (dest != nullptr) {
    fn_80_5C40(static_cast< SNode* >(dest), src);
  }
}

void fn_80_5BF8(void* dest, const SNode& src) { fn_80_5C18(dest, src); }

void* fn_80_5B90(const SNode* src, int count, SNode* dest) {
  const SNode* it = src;
  SNode* cur = dest;
  for (int remaining = count; remaining != 0; --remaining, ++it, ++cur) {
    fn_80_5BF8(cur, *it);
  }
  return cur;
}

#pragma dont_inline reset

} // extern "C"

#endif // __MWERKS__