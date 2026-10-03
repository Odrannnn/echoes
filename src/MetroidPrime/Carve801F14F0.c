// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address
// and size come from `config/G2ME01/symbols.txt:8020`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_801F0D24_text.s:568-577` before the claim existed (the
// same range is now `build/G2ME01/asm/MetroidPrime/Carve801F14F0.s`), and the body below is the
// C those bytes are the compilation of.
//
// .text 0x801F14F0..0x801F1510, 0x20 = 32 bytes, 1 function:
//
//   fn_801F14F0    0x801F14F0  0x20     8 instructions   frame + one `bl`
//
// **It is a byte-shape twin of `fn_80004438`** (`src/MetroidPrime/Carve80004438.c:95-97`,
// 0x80004438, 0x20, `Matching`): the same eight instructions in the same order, with
// `bl fn_801F1510` in place of `bl fn_80004458`.  The twin's whole body is
// `void fn_80004438(void* self) { fn_80004458(self); }` - a frame, one call, no load, no test,
// no return value - so the spelling here is the twin's, with two arguments because the callee
// this copy calls reads a second register.
//
// **Both arguments are forwarded, and that is measured, not assumed.**  `fn_801F14F0` writes no
// register between the prologue and its `bl`: `r3` still holds the caller's first argument and
// `r4` still holds the caller's second, unchanged.  Its only retail caller, `fn_801F14A8`
// (0x801F14A8, 0x48, still unclaimed), sets `r3` from `r31 + *(r31)*0x1C + 4` at 0x801F14BC-0x801F14C8
// and then calls at 0x801F14CC with `r4` untouched - the source pointer is the caller's own
// second parameter.  So the two-parameter spelling is the one that forwards both.
//
// `fn_801F1510` (0x801F1510, 0x44, `symbols.txt:8021`) is **above** this claim and is therefore
// declared, never defined here.  Retail's 17 instructions, read from the dtk listing above, are a
// null test on `r3` and then a member-wise copy of a 0x19 = 25-byte aggregate from `r4` to `r3`:
// two `short`s at +0x00/+0x04, four `float`s at +0x08/+0x0C/+0x10/+0x14 and a `char` at +0x18,
// each loaded and stored interleaved, ending in `blr` with no frame of its own.  That is an
// assignment operator's body, but its own 0x44 bytes are a spelling job of its own, which is why
// this claim stops where it does; the `bl` at 0x801F14FC is retail's, so the carve cannot drop
// the call.  For the DOL dtk's own `auto_*` object still defines it (the range above this claim is
// untouched); for the port link it is the announced stand-in `stub_carve801f14f0_0` in
// `src/MetroidPrime/PortLinkStubs.cpp`, the same trade `stub_carve80256d1c_0` makes for
// `Carve80256D1C.c`'s callee.  Nothing here claims `fn_801F1510` is decompiled.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A single function is descending by construction, but the
// file is written in that shape so that a later extension of the claim can extend downward.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: below it,
// 0x801F0D24..0x801F14F0 (`fn_801F0D24` .. `fn_801F14A8`) is unclaimed, and above it,
// 0x801F1510..0x801F1554 (`fn_801F1510`, `fn_801F1554`) is too.  The directory is retail's own,
// taken from the nearest claimed ranges: below is `MetroidPrime/CRELFileManager.cpp`
// (0x801F0518..0x801F0D24) and above is `MetroidPrime/Carve801F3690.c`
// (0x801F3690..0x801F3694).

/** 0x801F1510, `symbols.txt:8021`, 0x44 = 68 bytes: the 25-byte aggregate's assignment
 *  operator, the function this file's `fn_801F14F0` calls.  Declared, never defined here; the
 *  port link's stand-in is `stub_carve801f14f0_0` in `src/MetroidPrime/PortLinkStubs.cpp`, an
 *  announced empty body.  The two `void*` parameters are `r3` (the destination) and `r4` (the
 *  source) in the order the body reads them; it tests `r3` for null before copying. */
extern void fn_801F1510(void* self, const void* src);

/** `fn_801F14F0` - retail `.text:0x801F14F0`, 0x20 = 32 bytes: a frame and one call, no load,
 *  no test and no return value, forwarding `r3` and `r4` to `fn_801F1510` unchanged.  Its
 *  measured twin is `fn_80004438` (0x80004438, `MetroidPrime/Carve80004438.c`, `Matching`), the
 *  same eight instructions with a different `bl` target. */
void fn_801F14F0(void* self, const void* src);

void fn_801F14F0(void* self, const void* src) { fn_801F1510(self, src); }