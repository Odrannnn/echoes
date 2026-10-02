// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:644-645`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80024D68_text.s:114-167` before the claim existed, and the
// body below is the C those bytes are the compilation of.
//
// .text 0x80024EBC..0x80024F6C, 0xB0 = 176 bytes, 2 functions:
//
//   fn_80024EBC    0x80024EBC  0x50    20 instructions   the vector's deleting destructor
//   fn_80024F0C    0x80024F0C  0x60    24 instructions   `rstl::reserved_vector<T, N>::destroy_elements()`
//
// **What the pair is: the saved palette block's own deleting destructor and the element walk it
// calls.**  The object is `CTextRenderBuffer::mPalettes`, `rstl::reserved_vector< SFontPalette, 64 >`
// (`include/Kyoto/Text/CTextRenderBuffer.hpp:93`) - the count word at `+0x00` and the elements
// inline from `+0x04` (`include/rstl/reserved_vector.hpp:22-23`).  Three things outside this range
// name the pair and fix the types:
//
//   `__dt__17CTextRenderBufferFv` (0x80024AE8, 0x88, `asm/auto_03_80023FFC_text.s:749-785`) tears
//   the object down and calls `fn_80024EBC(self + 0x50, -1)` at 0x80024B10, so `+0x50` is
//   `mPalettes` and the flag is MWCC's "destroy, do not free me afterwards" - which is why the
//   free-through-to-self below is dead at that call site.  It is not dead in the source.
//   `fn_8027C3D8` (0x8027C3D8, 0x68, `asm/Kyoto/Text/CGuiTextSupport.s:1442-1470`) is
//   `reserved_vector`'s copy: it calls `fn_80024F0C` on the source and then computes the end of the
//   source with `mulli r0, r0, 0x2c` (0x8027C40C) - the same 0x2c stride as this file's loop, read
//   here from the *caller*.
//   `__dt__Q217CTextRenderBuffer12SFontPaletteFv` (0x80024FB0, 0xCC, `symbols.txt:648`) is the
//   element's deleting destructor, called from `fn_80024F6C`/`fn_80024F8C` in
//   `src/MetroidPrime/Carve80024F6C.c` - the unit whose range begins exactly where this one ends.
//   `NESTED_CHECK_SIZEOF(CTextRenderBuffer, SFontPalette, 0x2c)`
//   (`include/Kyoto/Text/CTextRenderBuffer.hpp:101`) is what pins the stride to the element type.
//
// **Both functions have an exact byte-shape twin in a `Matching` unit, and that is what identifies
// them.**  `src/MetroidPrime/Player/Carve80004B9C.c` is the same two shapes for the same two
// headers, and its object - `build/G2ME01/asm/MetroidPrime/Player/Carve80004B9C.s` - is these
// instructions with three operands retargeted: `__dt__80004B9C` (0x80004B9C, 0x50, 20
// instructions) against `fn_80024EBC`, and `fn_80004BEC` (0x80004BEC, 0x60, 24 instructions) against
// `fn_80024F0C`.
//
//   80004b9c  stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) / stw r31,0xc(r1)
//   80004bac  mr   r31,r4          ; the flag, live from entry
//   80004bb0  stw  r30,0x8(r1)
//   80004bb4  mr.  r30,r3          ; `this`
//   80004bb8  beq  0x80004bd0      ; if (this == 0) return
//   80004bbc  bl   fn_80004BEC      ; destroy_elements(this)      <- bl fn_80024F0C here
//   80004bc0  extsh. r0,r31         ; the flag, sign-extended to 16
//   80004bc4  ble  0x80004bd0      ; if (flag <= 0) return
//   80004bc8  mr   r3,r30
//   80004bcc  bl   Free__7CMemoryFPCv
//   80004bd0  epilogue, `mr r3,r30` ; returns `this`
//
// The loop is the same twenty-four instructions twice over, and only the stride and the callee
// differ:
//
//   80004bec  stwu r1,-0x20(r1) / mflr r0 / stw r0,0x24(r1) / stw r31,0x1c(r1) / stw r30,0x18(r1)
//   80004c00  li   r30,0x0         ; the index
//   80004c04  stw  r29,0x14(r1)
//   80004c08  mr   r29,r3          ; the receiver
//   80004c0c  addi r31,r29,0x4     ; the cursor, the inline elements
//   80004c10  b    0x80004c24      ; entered at the bottom test
//   80004c14  mr   r3,r31
//   80004c18  bl   fn_80004C4C     ; destroy one element          <- bl fn_80024F6C here
//   80004c1c  addi r31,r31,0x10    ; stride 16                    <- addi r31,r31,0x2c here
//   80004c20  addi r30,r30,0x1     ; ++i
//   80004c24  lwz  r0,0x0(r29)     ; the count is re-read every iteration
//   80004c28  cmpw r30,r0
//   80004c2c  blt  0x80004c14
//   80004c30  epilogue
//
// Three details of the twin carry over unchanged and are what fix the register allocation, so the
// two bodies below are the twin's bodies with the stride and the callees changed:
//
//   - **The return type is measured, not assumed.**  The epilogue is `mr r3,r30`, and spelled with
//     a `void` return mwcceppc drops it: the object comes out 19 instructions / 76 bytes and every
//     instruction after the `ble` is shifted.  So `fn_80024EBC` returns its receiver.
//   - **The flag is a `short`.**  `extsh.` is the halfword sign-extend, so the test is `flag > 0` on
//     a 16-bit parameter; the flag never leaves `r4`, which is why the frame saves `r30` and `r31`
//     and nothing else, and why the receiver is tested exactly once (`mr. r30,r3 ; beq`).
//   - **The cursor is declared before the index, and the increments run `it += stride, ++i`.**
//     `unsigned char* it; int i;` puts the index in `r30` and the cursor in `r31`, retail's
//     `li r30,0` / `addi r31,r29,0x4`; the other declaration order swaps both registers and 8 of
//     the 24 instructions differ, and `++i, it += stride` emits the two `addi`s the other way round.
//     The count is re-read from the receiver on every iteration (`lwz r0,0x0(r29)` at the bottom
//     test, `cmpw r30,r0`), so the loop test is spelled on `self->mCount` and not on a saved copy.
//     The loop is entered at its bottom test, so a zero count calls nothing and still returns.
//
// **Why the claim is exactly these two.**  A claim may not span an unclaimed gap.  Above
// 0x80024EBC the run ends at 0x80024F6C, where `src/MetroidPrime/Carve80024F6C.c` (a `Matching`
// unit, `0x80024F6C..0x80024FB0`) starts; below 0x80024EBC is the rest of dtk's
// `auto_03_80024D68_text`, unclaimed.  0x80024EBC..0x80024F6C is the whole contiguous run, and both
// its functions are written below.
//
// **Both callees are declared, never defined here, and both are already ours**, so this object adds
// no name to the port's undefined set: `fn_80024F6C` by `src/MetroidPrime/Carve80024F6C.c`, and
// `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) by `src/Kyoto/Alloc/CMemory.cpp`.
// `src/MetroidPrime/PortLinkStubs.cpp` defines neither, so there is no duplicate to delete.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of the two.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`, and why the `.c` is compiled as C by the port's host build (where C++ would mangle
// it) and syntax-checked as C++ by `tools/probe_sources.sh`; both accept this file as written.
//
// The directory is retail's own, taken from the nearest claimed range above:
// `src/MetroidPrime/Carve80024F6C.c` (0x80024F6C..0x80024FB0) sits in `MetroidPrime/`, so this
// address is in the `MetroidPrime/` neighbourhood.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x80024F6C, `symbols.txt:646`, 0x20 = 32 bytes: `rstl::destroy< SFontPalette >`, the
 *  one-element teardown this file's loop calls.  Defined by `src/MetroidPrime/Carve80024F6C.c`, a
 *  `Matching` unit whose range starts exactly where this one ends.  Declared, never defined here. */
extern void fn_80024F6C(void* self);

/** The object both functions below tear down: the element count at +0x00 with the elements inline
 *  from +0x04 - `rstl::reserved_vector< SFontPalette, 64 >`, `CTextRenderBuffer::mPalettes` at
 *  `include/Kyoto/Text/CTextRenderBuffer.hpp:93`.  Only the one word is read here; the array is
 *  reached by the `+ 4` below rather than through a member, because spelling the latter as an
 *  element array would drag in the element type, which is exactly what this loop does not need. */
struct SPaletteHead {
  int mCount;
};

/** `fn_80024F0C` - retail `.text:0x80024F0C`, 0x60 = 96 bytes: `destroy_elements()` over the inline
 *  palettes, entered at its bottom test, as `include/rstl/reserved_vector.hpp:97-105` spells it with
 *  the stride its element needs.  Its measured twin is `fn_80004BEC` (0x80004BEC, 0x60), these
 *  twenty-four instructions with `addi r31,r31,0x10` and `bl fn_80004C4C` in place of the `addi` and
 *  the `bl` below. */
void fn_80024F0C(struct SPaletteHead* self);

void fn_80024F0C(struct SPaletteHead* self) {
  unsigned char* it;
  int i;
  for (it = (unsigned char*)self + 4, i = 0; i < self->mCount; it += 0x2c, ++i) {
    fn_80024F6C(it);
  }
}

/** `fn_80024EBC` - retail `.text:0x80024EBC`, 0x50 = 80 bytes: the deleting destructor of the
 *  palette vector, twenty instructions that are the element walk's own caller.  Its measured twin
 *  is `__dt__80004B9C` (0x80004B9C, 0x50) in `src/MetroidPrime/Player/Carve80004B9C.c`, a
 *  `Matching` unit, word for word with `bl fn_80004BEC` in place of the `bl` here.  The caller
 *  `__dt__17CTextRenderBufferFv` passes `-1`
 *  (`asm/auto_03_80023FFC_text.s:759-760`), so as in the rest of the family the free-through-to-self
 *  is dead at that call site - it is not dead in the source. */
void* fn_80024EBC(struct SPaletteHead* self, short flag);

void* fn_80024EBC(struct SPaletteHead* self, short flag) {
  if (self) {
    fn_80024F0C(self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
