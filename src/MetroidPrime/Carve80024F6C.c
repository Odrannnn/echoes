// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:644-648`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80024D68_text.s:169-192` before the claim existed, and the
// body below is the C those bytes are the compilation of.
//
// .text 0x80024F6C..0x80024FB0, 0x44 = 68 bytes, 2 functions:
//
//   fn_80024F6C    0x80024F6C  0x20     8 instructions   `rstl::destroy< SFontPalette >`
//   fn_80024F8C    0x80024F8C  0x24     9 instructions   `rstl::destroy_impl< SFontPalette >`
//
// **What the two are: the element-destruction pair for `CTextRenderBuffer::SFontPalette`.**  The
// element type is measured off the unclaimed function directly below them, `fn_80024F0C`
// (0x80024F0C, 0x60, `asm/auto_03_80024D68_text.s:139-167`), which is the walk that calls this
// file's `fn_80024F6C` once per element: `i` from 0 (`li r30,0`) against the count word at `+0x00`
// (`lwz r0,0x0(r29)`), a cursor from `+0x04` (`addi r31,r29,0x4`) stepped by **0x2c**
// (`addi r31,r31,0x2c`), and `cmpw`/`blt`, so the loop is signed.  That is
// `rstl::reserved_vector<T, N>::destroy_elements()`
// (`include/rstl/reserved_vector.hpp:97-105`) and the 0x2c stride names `T`: it is
// `NESTED_CHECK_SIZEOF(CTextRenderBuffer, SFontPalette, 0x2c)`
// (`include/Kyoto/Text/CTextRenderBuffer.hpp:101`), and `CTextRenderBuffer::mPalettes` is
// `rstl::reserved_vector< SFontPalette, 64 >` (same header, line 93).  So `fn_80024F6C` is
// `destroy` over `destroy_impl` for that element.  The function above that one, `fn_80024EBC`
// (0x80024EBC, 0x50, `asm/auto_03_80024D68_text.s:114-137`), is the vector's deleting destructor:
// a receiver guard, `bl fn_80024F0C`, `extsh. r0,r31`, and `Free__7CMemoryFPCv(self)`.
//
// Each function is read off its call edge and its argument registers:
//
//   fn_80024F8C  materialises `li r4,-1` and calls
//                `__dt__Q217CTextRenderBuffer12SFontPaletteFv` (0x80024FB0, `symbols.txt:648`,
//                0xCC, `scope:weak`) with the receiver untouched, taking nothing else from its own
//                argument - that is `destroy_impl`'s `in->~T()`
//                (`include/rstl/construct.hpp:85-90`), and the -1 is MWCC's "destroy, do not free
//                me afterwards" flag, which the callee's own `extsh. r0,r31` proves is 16-bit, so
//                the callee is the element's *deleting* destructor.  **The twin is exact**:
//                `fn_80004C6C` (0x80004C6C, 0x24) in `src/MetroidPrime/Player/Carve80004C4C.c`, a
//                `Matching` unit, is these nine instructions word for word with `bl fn_80004A4C`
//                where the `bl` here is, and `__dt__Q213CFontImageDefFv` - the neighbour carve
//                `MetroidPrime/Carve80024D24.c` at 0x80024D44 - is a third copy of the same nine.
//                Retail's instruction order is reproduced too: `li r4,-1` sits between `mflr r0`
//                and the `stw r0,0x14(r1)`.
//   fn_80024F6C  is a frame and one unconditional `bl fn_80024F8C` - no load, no test, no return
//                value, nothing else taken from its arguments - so it is `destroy`'s
//                `destroy_impl(in)` (`include/rstl/construct.hpp:92-95`).  **Its twin is exact**:
//                `fn_80004C4C` (0x80004C4C, 0x20, the same file) and `fn_80004D3C` (0x80004D3C,
//                0x20, the same file) are these eight instructions word for word with the `bl`
//                retargeted, and `fn_80024D24` below in `MetroidPrime/Carve80024D24.c` is a fourth.
//
// **Why the claim is exactly these two.**  The callee
// `__dt__Q217CTextRenderBuffer12SFontPaletteFv` at 0x80024FB0 is a real-named `weak` function, not
// part of this pair, and would need a mangled definition in a unit of its own - and a claim may not
// span unclaimed bytes.  Below 0x80024F6C, the run 0x80024EBC..0x80024F6C is `fn_80024EBC` and
// `fn_80024F0C`, neither of which is written, so the claim stops where they start.  Above
// 0x80024FB0, 0x80024FB0..0x80025D3C is the rest of `auto_03_80024D68_text`, unclaimed.
// 0x80024F6C..0x80024FB0 is the whole contiguous run that is written and byte-exact.
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
// Its own unit because a claim may not span an unclaimed gap: below this range,
// 0x80024EBC..0x80024F6C, is still dtk's `auto_03_80024D68_text`, and above it 0x80024FB0 is that
// real-named `weak` destructor, which no unit claims.
//
// The directory is retail's own, taken from the nearest claimed range below:
// `MetroidPrime/Carve80024D24.c` (0x80024D24..0x80024D68) and `MetroidPrime/Carve80024B70.c`
// (0x80024B70..0x80024C18) sit in `MetroidPrime/`, so this address is in the `MetroidPrime/`
// neighbourhood.

/** 0x80024FB0, `symbols.txt:648`, 0xCC = 204 bytes: `CTextRenderBuffer::SFontPalette`'s deleting
 *  destructor, the callee of `fn_80024F8C` below.  Retail's own body checks the four
 *  `rstl::auto_ptr< CGraphicsPalette >` members at `+0x0c`, `+0x14`, `+0x1c` and `+0x24`
 *  (`include/Kyoto/Text/CTextRenderBuffer.hpp:51-54`, each auto_ptr guarded by an
 *  `addic.`/`lbz`/`cmplwi` null test before `bl __dt__16CGraphicsPaletteFv`) and then frees the
 *  receiver when the 16-bit flag is positive.  It is unclaimed by any unit, so dtk's own `auto_*`
 *  object supplies its bytes in the DOL link; this file only declares it. */
extern void __dt__Q217CTextRenderBuffer12SFontPaletteFv(void* self, short flag);

/** `fn_80024F8C` - retail `.text:0x80024F8C`, 0x24 = 36 bytes: `rstl::destroy_impl` for the
 *  0x2c-byte element, i.e. the element's destructor called with the "do not free me" flag.  The
 *  twin is `fn_80004C6C` (0x80004C6C, 0x24), these nine instructions word for word with
 *  `bl fn_80004A4C` in place of the `bl` here. */
void fn_80024F8C(void* self);

void fn_80024F8C(void* self) { __dt__Q217CTextRenderBuffer12SFontPaletteFv(self, -1); }

/** `fn_80024F6C` - retail `.text:0x80024F6C`, 0x20 = 32 bytes: `rstl::destroy` for the same
 *  element, a frame and one call to `fn_80024F8C` above and nothing else, as
 *  `include/rstl/construct.hpp:92-95` spells it.  Its measured twin is `fn_80004C4C` (0x80004C4C,
 *  0x20) in `src/MetroidPrime/Player/Carve80004C4C.c`, byte for byte apart from the `bl` target. */
void fn_80024F6C(void* self);

void fn_80024F6C(void* self) { fn_80024F8C(self); }