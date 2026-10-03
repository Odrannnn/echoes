// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:11393-11394`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8028B780_text.s:98-126` before the claim existed
// (the same range is now `build/G2ME01/asm/Collision/Carve8028B8BC.s`), and the bodies below are
// the C those bytes are the compilation of.
//
// .text 0x8028B8BC..0x8028B914, 0x58 = 88 bytes, 2 functions:
//
//   fn_8028B8BC    0x8028B8BC  0x38    13 instructions   `rstl::vector<T>::push_back_unsafe(const T&)`
//   fn_8028B8F4    0x8028B8F4  0x20     8 instructions   `rstl::construct<T>(void*, const T&)`
//
// **Both are byte-shape twins of matched functions in this tree, read out of their own sources
// rather than guessed.**  The seed named them; the reading confirms it instruction for instruction.
//
//   fn_8028B8BC  is `push_back_unsafe__Q24rstl49vector<12SAreaSurface,Q24rstl17rmemory_allocator>
//                FRC12SAreaSurface` (0x8005C148, 0x38, `src/MetroidPrime/CGameArea.cpp`, `Matching`):
//                the same thirteen instructions word for word, the same `slwi r0, r6, 5` - a
//                0x20-byte element - the same `construct(mItems + mCount++, in)` from
//                `include/rstl/vector.hpp:95`, with only the `bl` target different (this copy's
//                `fn_8028B8F4`, the twin's `construct<12SAreaSurface>__4rstlFPvRC12SAreaSurface`).
//                The neighbour below, `src/Collision/Carve8028B728.c`, is the same function over a
//                0x50-byte element and is spelled the same way apart from the element size.
//   fn_8028B8F4  is a frame and one unconditional `bl` and nothing else, as
//                `include/rstl/construct.hpp:73-75` spells it, so the body is a forward.  The
//                seed's own twin is `fn_80004438` (0x80004438, 0x20,
//                `src/MetroidPrime/Carve80004438.c`, `Matching`), the same eight instructions with
//                a different `bl` target - `rstl::destroy<T>` in that file, `rstl::construct<T>`
//                here - which is why this one is written forward to its own callee and not as a
//                destructor call.  `src/Collision/Carve8028B728.c`'s `fn_8028B760` is that same
//                eight-instruction shape again, over a 0x50-byte element.
//
// **The 0x20-byte element is measured from the code that walks it, not assumed.**  The only other
// caller of `fn_8028B8F4` in retail (measured with `grep -rn 'bl fn_8028B8F4' build/G2ME01/asm/`)
// is the loop at 0x8028BA40 in `fn_8028BA18`, which does `addi r31, r31, 0x20` and
// `addi r30, r30, 0x20` on its two cursors and calls with them, so retail copies 0x20 bytes per
// element and the `slwi r0, r6, 5` above agrees.  The only caller of `fn_8028B8BC` is the loop at
// 0x8028B850 in `fn_8028B7FC` (0x8028B7FC, 0xC0), which fills a 0x20-byte stack object at `r1+8`
// with two `CSegId`s, `fn_8028383C`, a `CVector3f` and a `float`, then pushes it - the same shape
// as `fn_8028B674`'s 0x50-byte vector below in this directory, which zeroes +0x04/+0x08/+0x0C,
// stores a capacity into +0x08 and then loops `bl fn_8028B728`.  That is why this file's struct
// models +0x04 and +0x0C and leaves the words between them as padding: it is retail's
// `rstl::vector<T>` layout, the same one `fn_80178270` walks in `CMemoryCard.cpp`.
//
// `fn_8028B914` (0x8028B914, `symbols.txt:11395`, 0x4C = 76 bytes) is **above** this claim and is
// therefore declared, never defined here.  Its bytes are retail's own copy for this element: a
// null-destination early return, then the source's bytes at +0x00 and +0x01, its words at +0x08 and
// +0x0C, and four floats at +0x10/+0x14/+0x18/+0x1C - the fields `fn_8028B7FC` fills - with +0x04
// skipped.  Those 0x4C bytes are a spelling job of their own, which is why the claim stops where
// it does, and its `bl` at 0x8028B900 is retail's, so the carve cannot drop the call.  For the
// DOL, dtk's own `auto_*` object defines it; for the port link it is the announced stand-in
// `stub_8028b8bc_0` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade
// `stub_8028b728_0` makes for `Carve8028B728.c`'s callee.  **Nothing here claims `fn_8028B914` is
// decompiled**, and the null check in it is why the two bodies below are the whole of what retail
// compiled for this range.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: this run sits inside the
// 0x8028B780..0x8028BC64 hole that dtk covers with `auto_03_8028B780_text`, and the functions
// either side of it in that hole are not trivial - the 0x50-byte `push_back_unsafe` pair below and
// `fn_8028B914` above.  The directory is retail's own, taken from the nearest claimed ranges:
// below is `Collision/Carve8028B728.c` (0x8028B728..0x8028B780), above is
// `Kyoto/Basics/CStopwatch.cpp` (0x8028BC64..0x8028BD3C).

/** 0x8028B914, `symbols.txt:11395`, 0x4C = 76 bytes: retail's copy for this 0x20-byte element,
 *  the function `fn_8028B8F4` forwards to.  Declared, never defined here; the port link's stand-in
 *  is `stub_8028b8bc_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an announced empty body.  Both
 *  parameters are pointers: retail tests the destination for null, then reads `+0x00`, `+0x01`,
 *  `+0x08`, `+0x0C` and the four floats at `+0x10`..`+0x1C` off the source into the destination. */
extern void fn_8028B914(void* dest, const void* src);

/** The block `fn_8028B8BC` pushes onto.  Only the two fields its bytes read are modelled: the
 *  count at +0x04 and the `void*` at +0x0C.  The words between them are padding, and what they
 *  hold is not this unit's business - `fn_8028B674` writes a capacity to +0x08 itself, and the
 *  caller hands the subobject in at a fixed offset inside a larger class. */
struct SCarve8028B8BCVector {
  int x00;
  unsigned int x04_count;
  int x08;
  void* x0c_items;
};

void fn_8028B8BC(struct SCarve8028B8BCVector* self, const void* in);
void fn_8028B8F4(void* dest, const void* src);

void fn_8028B8F4(void* dest, const void* src) { fn_8028B914(dest, src); }

void fn_8028B8BC(struct SCarve8028B8BCVector* self, const void* in) {
  unsigned int count = self->x04_count;
  void* items = self->x0c_items;
  unsigned int offset = count << 5;
  self->x04_count = count + 1;
  fn_8028B8F4((char*)items + offset, in);
}