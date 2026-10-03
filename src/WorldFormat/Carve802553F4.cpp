// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:10473-10474`, and the instructions were read off
// the pristine disc with
//
//   python3 tools/dol_read.py 0x802553F4 0x24 orig/G2ME01/sys/main.dol
//   .text @ 0x802553f4  (file 0x2521f4, 36 bytes)
//   hex : 94 21 ff f0 7c 08 02 a6 90 01 00 14 38 a1 00 08 48 00 00 15 80 01 00 14 7c 08 03 a6 38 21 00 10 4e 80 00 20
//
// - the same bytes dtk emitted into `build/G2ME01/asm/auto_03_80255128_text.s:232-243` while the
// range was still that object's, which is now `auto_03_80255128_text.s` for 0x80255128..0x802553F4
// and `auto_03_80255418_text.s` for 0x80255418..0x802554BC.  The body below is the C++ those bytes
// are the compilation of.  `build/G2ME01/asm/WorldFormat/Carve802553F4.s` is this unit's own
// generated listing, not the retail one - it is our compile, so it can only confirm, never
// establish, what retail had.
//
// .text 0x802553F4..0x80255418, 0x24 = 36 bytes, 1 function:
//
//   fn_802553F4    0x802553F4  0x24    9 instructions
//
//   802553f4  stwu r1,-0x10(r1)      802553fc  addi r5,r1,0x8
//   802553f8  mflr r0                80255404  bl __ct__Q24rstl36vector<f,
//   802553fc  stw  r0,0x14(r1)                        Q24rstl17rmemory_allocator>
//   80255408  lwz  r0,0x14(r1)                 FR12CInputStreamRCQ24rstl17rmemory_allocator
//   8025540c  mtlr r0                80255410  addi r1,r1,0x10
//                                  80255414  blr
//
// **It is a byte-shape twin of `CInputStream::Get<rstl::basic_string<char, rstl::char_traits<char>,
// rstl::rmemory_allocator>>`** (`symbols.txt`, 0x80051FC4, `size:0x24`, weak, listed at
// `build/G2ME01/asm/MetroidPrime/CWorld.s:4253-4263`): these nine instructions word for word, and
// **the `bl` encoding is `48 00 00 15` in both** only because the callee sits the same 0x14 bytes
// past the `bl` in each.  That twin is **not** in a `Matching` unit - `main/MetroidPrime/CWorld` is
// `NonMatching` at 89.58% - but the twin function itself measures **100.0%** there in
// `build/report.json`, and the same entry carries its demangled name,
// `CInputStream::Get<rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator>>(
// const TType<rstl::basic_string<...> >&)`, which is the identification below arrived at
// independently.  Its source is `include/Kyoto/Streams/CInputStream.hpp:117-120`
//
//   template < typename T > inline T CInputStream::Get(const TType< T >& type) { return T(*this); }
//
// so the shape is "construct a `T` from the stream and hand it back by value", and **what `T` is
// here is fixed by the callee's own name and by the caller, not guessed**: `T` is
// `rstl::vector<float>`, whose stream constructor is declared at `include/rstl/vector.hpp:52` as
// `vector(CInputStream& in, const Alloc& alloc = Alloc())` and defined at
// `include/Kyoto/Streams/CInputStream.hpp:196-203`.  Retail's name for it is the callee at
// 0x80255418 (`symbols.txt:10474`, `size:0xA4`), one byte-range above this claim.
//
// **The registers are the sret convention for a class return value, and that is measured, not
// assumed.**  `r3` is the caller's result slot - the callee stores its first four words straight to
// `0x4(r3)`, `0x8(r3)`, `0xc(r3)` (0x80255440-0x80255448) - and `r4` arrives untouched, which is
// why retail spends no instruction moving it.  `addi r5,r1,0x8` is the default `rmemory_allocator`
// argument's temporary, which is why the frame is 0x10 bytes and not less: `rstl::vector`'s
// allocator is the empty type, so the temporary is never written, only its address passed.  The
// one caller settles it.  `grep -rn "bl fn_802553F4" build/G2ME01/asm/` returns exactly one call
// site, 0x8025539C inside `fn_80255370` (0x80255370, 0x84), which reaches it with
// `addi r3,r1,0xc` (the frame's own `rstl::vector<float>` temporary) and `r4` = `r31`, the stream
// it saved at 0x80255384 from its own `r4`, and then hands the same `r1+0xc` to `fn_8015B780` at
// 0x802553A8.  So `fn_802553F4` is `in.Get<rstl::vector<float> >()` written into that temporary.
//
// **The unit is a `.cpp` with `extern "C"`, not the `.c` the seed named, and the callee's name is
// the reason.**  MWCC mangles that constructor
// `__ct__Q24rstl36vector<f,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator`,
// which a C declaration cannot spell at all - `<`, `>` and `,` are not identifier characters - and
// the alias that would dodge it, `extern void f(...) asm("<mangled>")`, is **rejected by mwcceppc in
// both C and C++ mode** (measured, and recorded at `src/MetroidPrime/Carve801285DC.c:47-48` and
// `src/MetroidPrime/Carve80281310.c:60`).  `extern "C"` is what keeps `fn_802553F4` verbatim and
// unmangled, which is the seed's actual concern; it is the arrangement
// `src/MetroidPrime/Carve8024492C.cpp` and `src/MetroidPrime/Carve801EF730.cpp` already use for
// Matching carves, and both of those also return a class by value.
//
// **`CInputStream.hpp` is deliberately NOT included.**  It is where the constructor is *defined*
// (line 196), and including it makes this translation unit emit a weak out-of-line copy of that
// constructor and of `vector<float>::reserve` into its own `.text` - measured: the same body
// compiled with the include produces a 0xA4+0x1E `.text` holding `fn_802553F4` **and** two symbols
// retail's object for this claim does not define, and then the `bl` below would resolve to that
// local copy instead of to retail's 0x80255418.  The **explicit specialization declaration** below
// says "this specialization exists, it is not defined here", which is what turns the call into an
// external reference and leaves the object's `.text` exactly the 0x24 bytes retail has.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  With one function the order cannot be wrong, and the rule is
// recorded because the next carve added to this file would break it.  The cheap check is
// `python3 tools/check_decl_order.py --unit WorldFormat/Carve802553F4.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit where a neighbouring
// unit's `.text` ends.  The claim is a **proper interior slice** of dtk's
// `auto_03_80255128_text` (0x80255128..0x802554BC): it starts 0x2CC bytes in, where
// `WorldFormat/CAreaRenderOctTree.cpp` stopped claiming at 0x80255128, and it ends exactly where
// `__ct__Q24rstl36vector<f,...>` begins (0x80255418), with `WorldFormat/Carve802554BC.c`
// (0x802554BC..0x802554D0) as the next claim above.  Neither neighbour is absorbed, and starting
// 0x2CC bytes into the auto range rather than at its first byte is what keeps `dtk dol split` from
// reporting a link-order cycle (`docs/RUNNING_THE_DECOMP.md`, "The carve vein").  The directory is
// retail's own, taken from the nearest claimed ranges: below is `WorldFormat/CAreaRenderOctTree.cpp`
// (0x80254BAC..0x80255128) and above is `WorldFormat/Carve802554BC.c` (0x802554BC..0x802554D0).
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C++ *and* `extern "C"`: a
// `C++`-mangled one would be `_Z<len>fn_802553F4<args>v` and objdiff would pair nothing.
//
// The carve's own `block` is `.text` and nothing else: no data, no vtable, no string - the object
// has one section, `.text` 0x24 (measured with `powerpc-eabi-objdump -h`).

#include "rstl/vector.hpp"

namespace rstl {

/** Retail's 0xA4-byte out-of-line `rstl::vector<float>::vector(CInputStream&,
 *  const rmemory_allocator&)`, at 0x80255418 (`symbols.txt:10474`).  **Declared, never defined
 *  here**: nothing in this tree claims it, so the DOL link takes the bytes from dtk's own
 *  `auto_03_80255418_text.o` - the upper half of what used to be one `auto_03_80255128_text`
 *  range, split by this carve - and the `bl` below resolves to retail's address.  The declaration has
 *  to be an explicit specialization because `rstl::vector`'s own primary template only declares
 *  the member (`include/rstl/vector.hpp:52`) and the definition lives in
 *  `include/Kyoto/Streams/CInputStream.hpp:196`, which cannot be included here - see the header. */
template <>
vector< float >::vector(CInputStream&, const rmemory_allocator&);

#ifndef __MWERKS__
/** Port-only stand-in, and **it is one**: an empty body that says so.  Retail's own 0xA4 bytes for
 *  that constructor are a spelling job of their own and no unit claims them, and this file needs
 *  the call to resolve for the host link, because the host's own mangling of the constructor is
 *  nothing like MWCC's.  It is the same trade `src/MetroidPrime/Carve801EF730.cpp:197-215` makes
 *  for `fn_801EF7B0` and `src/MetroidPrime/Carve8024492C.cpp` for `fn_8024426C`, and it is the
 *  only reason the guard is `__MWERKS__` and not `TARGET_PC`: the matching build **must** take the
 *  symbol from dtk's object, and a second definition there is the duplicate
 *  `tools/gate.sh`'s `port link dups` step exists to catch.
 *  **Nothing that boots can reach this file.**  `fn_802553F4`'s only caller in retail is
 *  `fn_80255370` (one `bl` site, 0x8025539C, measured by grep over `build/G2ME01/asm/`), no unit
 *  claims that function, and the port's flat link carries no `auto_*` objects at all
 *  (`src/MetroidPrime/Carve801285DC.c:61-62` records that), so `fn_802553F4` has no caller there.
 *  The three initialisers are the ones the real constructor begins with
 *  (`include/Kyoto/Streams/CInputStream.hpp:197`); `mAllocator` is the empty `rmemory_allocator`
 *  and is never stored.  It deliberately does **not** read the stream: a stand-in that invented
 *  plausible contents would be the fake this repo forbids, and nothing can observe the difference. */
template <>
vector< float >::vector(CInputStream& in, const rmemory_allocator& alloc)
: mCount(0), mCapacity(0), mItems(nullptr) {
  (void)in;
  (void)alloc;
}
#endif

} // namespace rstl

extern "C" {

/** `fn_802553F4` - retail `.text:0x802553F4`, 0x24 = 36 bytes, 9 instructions: a frame, the
 *  default-allocator temporary's address, the one call and the epilogue.  Byte for byte the
 *  twin `CInputStream::Get<rstl::basic_string<...> >` (0x80051FC4, 0x24,
 *  `build/G2ME01/asm/MetroidPrime/CWorld.s:4253-4263`, 100.0% in `build/report.json`), so the body
 *  is that function's own `return T(*this);` (`include/Kyoto/Streams/CInputStream.hpp:118-120`)
 *  with this copy's `T`.  The class return is what puts the result slot in `r3`; writing `void`
 *  and a placement new instead would compile to the same nine instructions but would no longer be
 *  the function retail compiled, and `rstl::vector<float>` is what its caller at 0x8025539C
 *  expects. */
rstl::vector< float > fn_802553F4(CInputStream* in) { return rstl::vector< float >(*in); }

} // extern "C"