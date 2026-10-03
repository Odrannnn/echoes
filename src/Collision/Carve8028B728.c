// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:11389-11390`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8028B1F4_text.s:403-431` before the claim existed
// (the same range is now `build/G2ME01/asm/Collision/Carve8028B728.s`), and the bodies below are
// the C those bytes are the compilation of.
//
// .text 0x8028B728..0x8028B780, 0x58 = 88 bytes, 2 functions:
//
//   fn_8028B728    0x8028B728  0x38    13 instructions   `rstl::vector<T>::push_back_unsafe(const T&)`
//   fn_8028B760    0x8028B760  0x20     8 instructions   `rstl::construct<T>(void*, const T&)`
//
// **Both are byte-shape twins of matched functions in this tree, read out of their own sources
// rather than guessed.**  The seed named them; the reading confirms it instruction for instruction.
//
//   fn_8028B728  is `push_back_unsafe__Q24rstl59vector<22CSaveWorldIntermediate,
//                Q24rstl17rmemory_allocator>FRC22CSaveWorldIntermediate` (0x80178270, 0x38,
//                `src/MetroidPrime/CMemoryCard.cpp`, `Matching`): the same thirteen instructions,
//                the same `mulli r0, r5, 0x50` - a 0x50-byte element - the same
//                `construct(mItems + mCount++, in)`, with only the `bl` target different.  The
//                template is `include/rstl/vector.hpp:95`.  The element size here is also 0x50,
//                and that is the whole of what the `mulli` carries: the two functions differ in
//                what they construct, not in how the arithmetic reads.
//   fn_8028B760  is `construct<22CSaveWorldIntermediate>__4rstlFPvRC22CSaveWorldIntermediate`
//                (0x801782A8, 0x20, same object): a frame and one unconditional `bl` and nothing
//                else, as `include/rstl/construct.hpp:73-75` spells it, so the body is a forward.
//                The seed's own twin is `fn_80004438` (0x80004438, 0x20,
//                `src/MetroidPrime/Carve80004438.c`, `Matching`), the same eight instructions with
//                a different `bl` target - `rstl::destroy<T>` in that file, `rstl::construct<T>`
//                here, which is why this one is written forward to its own callee and not as a
//                destructor call.
//
// **The 0x50-byte element is measured from the code that walks it, not assumed.**  The only
// callers of `fn_8028B760` in retail (measured with `grep -rn 'bl fn_8028B760'
// build/G2ME01/asm/`) are `fn_8028B728` at 0x8028B74C and the loop at 0x8028BB60 in
// `fn_8028BB20`, which does `addi r31, r31, 0x50` and `addi r30, r30, 0x50` on its two cursors
// and calls with them, so retail copies 0x50 bytes per element and the `mulli` above agrees.
// `fn_8028B674` at 0x8028B690..0x8028B6A4 is the other end of the story: it zeroes +0x04, +0x08
// and +0x0C of a subobject at `this + 0x10`, stores a capacity into +0x08, and then loops
// `bl fn_8028B728` - which is why this file's struct models +0x04 and +0x0C and leaves the words
// between them as padding.  That is retail's `rstl::vector<T>` layout, the same one
// `fn_80178270` walks in `CMemoryCard.cpp`.
//
// `fn_8028B780` (0x8028B780, `symbols.txt:11391`, 0x7C = 124 bytes) is **above** this claim and is
// therefore declared, never defined here.  Its bytes are retail's `rstl::construct_impl<T>` for
// this element: a null-receiver early return, then a byte at +0x00 and a byte at +0x01, the words
// at +0x08 and +0x0C, `__ct__12CTransform4fFRC12CTransform4f` at +0x10, and three floats at
// +0x40/+0x44/+0x48.  Those 0x7C bytes are a spelling job of their own, which is why the claim
// stops where it does, and its `bl` at 0x8028B76C is retail's, so the carve cannot drop the call.
// For the DOL, dtk's own `auto_*` object defines it; for the port link it is the announced
// stand-in `stub_8028b728_0` in `src/MetroidPrime/PortLinkStubs.cpp`, the same trade
// `stub_80004438_0` makes for `Carve80004438.c`'s callee.  **Nothing here claims `fn_8028B780` is
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
// 0x8028B1F4..0x8028BC64 hole that dtk covers with `auto_03_8028B1F4_text`, and the functions
// either side of it in that hole are not trivial - `fn_8028B674` below and `fn_8028B780` above.
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Collision/CMRay.cpp` (0x8028AF74..0x8028B1F4), above is `Kyoto/Basics/CStopwatch.cpp`
// (0x8028BC64..0x8028BD3C).

/** 0x8028B780, `symbols.txt:11391`, 0x7C = 124 bytes: `rstl::construct_impl<T>` for this
 *  0x50-byte element, the function `fn_8028B760` forwards to.  Declared, never defined here; the
 *  port link's stand-in is `stub_8028b728_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an announced
 *  empty body.  Both parameters are pointers: retail reads `+0x00`, `+0x01`, `+0x08`, `+0x0C`,
 *  `+0x10`, `+0x40`, `+0x44` and `+0x48` off the source and writes them into the destination. */
extern void fn_8028B780(void* dest, const void* src);

/** The block `fn_8028B728` pushes onto.  Only the two fields its bytes read are modelled: the
 *  count at +0x04 and the `void*` at +0x0C.  The words between them are padding, and what they
 *  hold is not this unit's business - `fn_8028B674` writes a capacity to +0x08 itself, and the
 *  caller hands the subobject in at a fixed offset inside a larger class. */
struct SCarve8028B728Vector {
  int x00;
  unsigned int x04_count;
  int x08;
  void* x0c_items;
};

void fn_8028B728(struct SCarve8028B728Vector* self, const void* in);
void fn_8028B760(void* dest, const void* src);

void fn_8028B760(void* dest, const void* src) { fn_8028B780(dest, src); }

void fn_8028B728(struct SCarve8028B728Vector* self, const void* in) {
  unsigned int count = self->x04_count;
  void* items = self->x0c_items;
  unsigned int offset = count * 0x50;
  self->x04_count = count + 1;
  fn_8028B760((char*)items + offset, in);
}