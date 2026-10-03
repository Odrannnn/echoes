// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7322-7323`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801C13F8_text.s:1720-1762` before the claim
// existed (the same range is now `build/G2ME01/asm/MetroidPrime/Carve801C2D74.s`), and the bodies
// below are the C++ those bytes are the compilation of.
//
// .text 0x801C2D74..0x801C2DFC, 0x88 = 136 bytes, 2 functions:
//
//   fn_801C2D74    0x801C2D74  0x68  26 instructions  `rstl::uninitialized_copy`
//   fn_801C2DDC    0x801C2DDC  0x20   8 instructions  `rstl::construct< T >`
//
// **Both are byte-shape twins of `Matching` functions in this tree, read out of their own sources
// rather than guessed.**  The seed named them; the reading confirms it instruction for instruction.
// Compared with `tools`-independent byte comparison of the two `.fn` blocks in the `.s` files, each
// pair differs in **exactly one word** - the `bl` - and in nothing else:
//
//   fn_801C2D74  is `fn_8028E754` (`src/Kyoto/Animation/CAnimationSet.cpp:182`, `Matching`): the
//                same 26 instructions with the same `lwz r31,0(r3)` reading `begin` out of the
//                caller's copy, the same `mr r29,r4` keeping `end`, the same bottom-of-loop test
//                `lwz r0,0(r29) / cmplw r31,r0 / bne`, and the same literal `addi ..,0x44` step on
//                both cursors.  Only word 12, the `bl`, differs (`48 00 00 39` against
//                `4B FF E5 25`).  It is `rstl::uninitialized_copy< It, T* >`, the template at
//                `include/rstl/construct.hpp:116-127`; the twin writes the same loop by hand
//                because its `It` is a class that goes by hidden pointer.
//   fn_801C2DDC  is `fn_80004438` (`src/MetroidPrime/Carve80004438.c:95-97`, `Matching`): a frame
//                and one unconditional `bl` and nothing else, the eight instructions of
//                `include/rstl/construct.hpp:74-77`'s `construct`, whose whole body is
//                `construct_impl(dest, src)`.  Only word 3, the `bl`, differs.
//
// **The element is `CRagDoll::CRagDollParticle` and the step is its size, both measured.**  The
// loop's stride is the literal `0x44`, and `include/MetroidPrime/CRagDoll.hpp:197` asserts
// `NESTED_CHECK_SIZEOF(CRagDoll, CRagDollParticle, 0x44)` - the same relation the twin carries
// between its `addi ..,0x44` and `CHECK_SIZEOF(CAnimPOIData, 0x44)`.  The callee settles it
// independently: `fn_801C2DDC`'s `bl` at 0x801C2DE8 targets
// `construct_impl<Q28CRagDoll16CRagDollParticle>__4rstlFPvRCQ28CRagDoll16CRagDollParticle`
// (`symbols.txt:5843`, 0x8015EB80, 0x28, weak), which **our own tree already defines** in
// `MetroidPrime/CRagDoll.cpp` (`.text` 0x8015DDE8..0x80160C20, `splits.txt:980`) - so the call
// resolves in the DOL link without a stub.  `rstl::construct<Q28CRagDoll16CRagDollParticle>`
// (0x8015EB60, 0x20, `scope:local`, `symbols.txt:5842`) is these same eight instructions, so
// retail emitted this function twice; nothing here claims to know why.
//
// **The two iterators are parameters by value, and that is load-bearing**, exactly as the twin's
// header records at `CAnimationSet.cpp:174-181`: a class goes by hidden pointer, so `begin` and
// `end` arrive as `r3`/`r4` and `end` is **re-read from the caller's frame inside the loop**
// (`lwz r0,0(r29)`), which is retail's fifteenth instruction.  `fn_801C2CBC` (0x801C2CBC, 0xB8,
// `symbols.txt:7321`) is the only caller of `fn_801C2D74` in retail (`grep -rn 'bl fn_801C2D74'
// build/G2ME01/asm/`), and at 0x801C2D24 it builds both iterators on its own stack
// (`addi r3,r1,0x14 / addi r4,r1,0xc / mr r5,r31`) before the call, which is the hidden-pointer
// convention seen from the caller side.  The destination is `r5` and the returned `mr r3,r30` is
// the end cursor, so this is the `uninitialized_copy` that returns `out` for an empty range.
//
// **The element type is declared locally and never defined, for the reason `Carve8000447C.cpp`
// gives at its lines 50-59.**  Two things are needed from `CRagDoll::CRagDollParticle` and
// neither is its layout: its **name**, because the callee's mangled symbol spells
// `Q28CRagDoll16CRagDollParticle` and `CIngBoostBallGuardian3790.cpp:32-42` records that such a
// name is **not declarable** as an `extern "C"` identifier (the `<`, `,` and `>` in it are not
// identifier characters and mwcceppc has no `__asm__` symbol renaming); and its **size**, because
// the loop steps `0x44`.  `rstl::construct_impl` is declared as a function template and **never
// defined**, so the one call mangles to retail's own MWCC symbol and resolves against
// `MetroidPrime/CRagDoll.cpp`'s definition - and nothing is emitted for the declaration.  Including
// `rstl/construct.hpp` instead would put an *inline definition* of `construct_impl` in this
// translation unit, which mwceppc may either inline into `fn_801C2DDC` (wrong bytes) or outline as
// a local weak copy retail's range does not contain (`tools/unit_fit.sh` would say so).  For the
// same reason `rstl::pointer_iterator` is declared locally: `include/rstl/pointer_iterator.hpp`
// includes `rstl/construct.hpp`, and nothing here may see that definition.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `python3 tools/check_decl_order.py --unit
// MetroidPrime/Carve801C2D74.cpp` is the cheap check.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so `extern "C"` keeps them unmangled - a C++ definition
// without it would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why
// the unit is a `.cpp` rather than a `.c`: the call above cannot be written in C at all.
//
// Its own unit because a claim may not span an unclaimed gap.  This run sits at the **tail** of the
// 0x801C13F8..0x801C2DFC hole that dtk covers with `auto_03_801C13F8_text`, so carving splits that
// object into 0x801C13F8..0x801C2D74 and this one.  Above the claim `fn_801C2DFC` (0x801C2DFC, 0x370,
// `symbols.txt:7324`) begins and is unclaimed; the next claimed range is
// `MetroidPrime/Player/CGrappleArm.cpp` at 0x801C316C.  The directory is retail's own, taken from
// the nearest claimed range: below is `MetroidPrime/Carve801C13F4.c` (0x801C13F4..0x801C13F8),
// above is `MetroidPrime/Player/CGrappleArm.cpp`.  For an anonymous function that is the only
// evidence there is, and it beats a lane picking the directory it happened to own.
//
// The body is inside `#ifdef __MWERKS__` for the reason `CMysteryFlyerRelTail2.cpp` and
// `CMysteryFlyerRelTail.cpp` give in `files.cmake`: `MetroidPrime/CRagDoll.cpp` is not in the port
// build (`tools/check_files_cmake.py:605-606` excludes it with a measured reason), so a host
// compilation of this file would add one undefined symbol to the port's link for a function no
// host source calls - `fn_801C2CBC`, the only caller, is itself inside the unclaimed dtk range.  The
// host branch is empty by design; the DOL branch is the whole file.

#ifdef __MWERKS__

namespace rstl {

/** Named only so `ParticleIter` below reads as the template it stands in for; nothing here uses
 *  it, and `include/rstl/rmemory_allocator.hpp` is not included for the reason `construct_impl`
 *  below is not. */
class rmemory_allocator;

/** `rstl::construct_impl< T >(void*, const T&)` - `include/rstl/construct.hpp:50-56` is the real
 *  one.  **Declared and never defined here, deliberately:** that header's *inline definition*
 *  would let mwceppc inline the placement new into `fn_801C2DDC` (wrong bytes) or emit a local
 *  weak copy of `construct_impl<Q28CRagDoll16CRagDollParticle>...` into this object, which retail's
 *  range does not define (`tools/unit_fit.sh`).  Declared bare, the one call mangles to
 *  `construct_impl<Q28CRagDoll16CRagDollParticle>__4rstlFPvRCQ28CRagDoll16CRagDollParticle` -
 *  `symbols.txt:5843`, 0x8015EB80 - and resolves against `MetroidPrime/CRagDoll.cpp`, which our
 *  own tree compiles.  Nothing is emitted for the declaration. */
template < typename T > void construct_impl(void* dest, const T& src);

/** `rstl::pointer_iterator< T, Vec, Alloc >` - `include/rstl/pointer_iterator.hpp:62-99` is the
 *  real one, reduced to the member and the accessor these bytes use, and declared locally for the
 *  same reason: that header includes `rstl/construct.hpp`, whose `construct_impl` definition must
 *  not be visible here.  The two cursors arrive by hidden pointer because a class goes by value that
 *  way, which is what makes retail re-read `end` inside the loop.  `Vec` and `Alloc` are unused by
 *  the accessor and are named only so this reads as the template it stands in for. */
template < typename T, typename Vec, typename Alloc >
class pointer_iterator {
public:
  pointer_iterator() : current(nullptr) {}
  pointer_iterator(T* begin) : current(begin) {}
  T* get_pointer() const { return current; }
  T& operator*() const { return *get_pointer(); }

private:
  T* current;
};

} // namespace rstl

/** `CRagDoll::CRagDollParticle` as far as these 136 bytes need it, and no further.  The real class
 *  is `include/MetroidPrime/CRagDoll.hpp:44-77`, whose eleven members these bytes never read: the
 *  loop only steps a pointer by `sizeof`, and the callee it hands the element to is retail's own
 *  `construct_impl`, declared above and defined in another unit.  **What is reproduced here is the
 *  name and the size** - the name because the callee's mangled symbol spells
 *  `Q28CRagDoll16CRagDollParticle` and that spelling is not declarable any other way
 *  (`CIngBoostBallGuardian3790.cpp:32-42`), the size because `NESTED_CHECK_SIZEOF(CRagDoll,
 *  CRagDollParticle, 0x44)` (`CRagDoll.hpp:197`) is the literal `addi ..,0x44` in the loop and in
 *  `fn_801C2CBC`'s `mulli r0, r0, 0x44`.  Nothing here claims the layout below that size is known. */
class CRagDoll {
public:
  class CRagDollParticle {
  public:
    unsigned char x00_payload[0x44];
  };
};

typedef rstl::pointer_iterator< CRagDoll::CRagDollParticle, CRagDoll::CRagDollParticle,
                                rstl::rmemory_allocator > ParticleIter;

/** `fn_801C2DDC` - retail `.text:0x801C2DDC`, 0x20 = 32 bytes: `rstl::construct< T >` for this
 *  element, which `include/rstl/construct.hpp:74-77` spells as `construct_impl(dest, src)` and
 *  nothing else - a frame, one call, the epilogue.  Retail emitted the same eight instructions at
 *  0x8015EB60 (`rstl::construct<Q28CRagDoll16CRagDollParticle>`, `symbols.txt:5842`). */
extern "C" void fn_801C2DDC(void* dest, const CRagDoll::CRagDollParticle& src);

extern "C" void fn_801C2DDC(void* dest, const CRagDoll::CRagDollParticle& src) {
  rstl::construct_impl(dest, src);
}

/** `fn_801C2D74` - retail `.text:0x801C2D74`, 0x68 = 104 bytes: `rstl::uninitialized_copy< It,
 *  T* >` (`include/rstl/construct.hpp:116-127`) over this element, written out because the real
 *  template would pull in `rstl::construct_impl`'s definition.  **Both iterators are parameters by
 *  value, and that is the whole difference from retail** - the twin's header records it as
 *  92.69% -> 100.00% on `fn_8028E754`: a by-value class goes by hidden pointer, so the test
 *  re-reads `end` out of the caller's frame after the `bl`, which is what
 *  `lwz r0,0x0(r29) / cmplw r31,r0 / bne` is.  The loop is entered at its bottom test, so an empty
 *  range copies nothing and still returns `out`. */
extern "C" CRagDoll::CRagDollParticle* fn_801C2D74(ParticleIter begin, ParticleIter end,
                                                  CRagDoll::CRagDollParticle* out);

extern "C" CRagDoll::CRagDollParticle* fn_801C2D74(ParticleIter begin, ParticleIter end,
                                                  CRagDoll::CRagDollParticle* out) {
  CRagDoll::CRagDollParticle* it = begin.get_pointer();
  CRagDoll::CRagDollParticle* cur = out;
  for (; it != end.get_pointer(); ++it, ++cur) {
    fn_801C2DDC(cur, *it);
  }
  return cur;
}

#endif // __MWERKS__