// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:6845-6847`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801997B4_text.s:1557-1611`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8019AC78..0x8019AD24, 0xAC = 172 bytes, 3 functions:
//
//   fn_8019AC78    0x8019AC78  0x20     8 instructions   the forwarder
//   fn_8019AC98    0x8019AC98  0x28    10 instructions   the null-guarded construct
//   fn_8019ACC0    0x8019ACC0  0x64    25 instructions   the element's copy constructor
//
// **What the three are: a `rstl::construct` / copy-constructor pair over an anonymous 0x26-byte
// element.**  Retail names none of them, so the element is declared locally below and only the
// bytes these functions touch are given members; each function is read off its call edge and its
// argument registers:
//
//   fn_8019AC78  is a frame and one unconditional `bl fn_8019AC98` - no load, no test, no return
//                value - which is the shape `include/rstl/construct.hpp:117-120` gives the in-class
//                `construct`, and the shape `fn_80004D3C` (0x80004D3C, 0x20,
//                `src/MetroidPrime/Player/Carve80004C4C.c`, a `Matching` unit) is these 8
//                instructions word for word with the `bl` retargeted.  `__sys_free` (0x80008A28)
//                is the same shape a third time.
//   fn_8019AC98  tests its *first* argument for null (`cmplwi r3,0x0`, i.e. the destination, not
//                the source) and only then calls `fn_8019ACC0` with both arguments untouched, so
//                it is `rstl::construct`'s null guard.  **The twin is exact**: `fn_80004D5C`
//                (0x80004D5C, 0x28, `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, a
//                `Matching` unit) is these 10 instructions word for word with `bl fn_80004AA0`
//                where the `bl` here is.  Retail's own instruction order is reproduced too: the
//                `cmplwi` sits between `mflr r0` and the `stw r0,0x14(r1)`.
//   fn_8019ACC0  copies 0x26 bytes member by member - `lwz/stw` for the words, one `lfs/stfs`
//                for the float at +0x08, `lbz/stb` for the four bytes at +0x20, +0x21, +0x24 and
//                +0x25, and nothing at +0x22/+0x23 - so it is the element's implicit copy
//                constructor, which is what `fn_8019AC98` calls.  It keeps both pointers in `r3`
//                and `r4` for the whole body (no `mr` into a callee-saved register) and runs a
//                two-deep load/store pipeline, which is MWCC's schedule for a member-wise copy.
//
// **Why the claim is 0x8019AC78..0x8019AD24 and not only the two functions the item named.**
// `fn_8019AC98` calls `fn_8019ACC0`, and `fn_8019ACC0` is claimed by no unit - it is not in
// `src/`, not in `configure.py`, not in `config/G2ME01/splits.txt`.  A claim that stopped at
// 0x8019ACC0 therefore *adds* `fn_8019ACC0` to the port's link gap, and
// `tools/link_check.sh --strict`, which `tools/probe_sources.sh` runs inside the gate, fails
// when that count grows: measured 291 against the recorded 291 in
// `docs/research/port_link_baseline.txt:3`.  `fn_8019ACC0` is contiguous with the item's range
// and is the one function above it, so claiming it costs nothing and keeps the count where it is
// - the same reason `src/MetroidPrime/Player/Carve80004C4C.c:66-77` took the whole tail rather
// than the three functions its item named.  Above 0x8019AD24 the run continues with `fn_8019AD24`
// (0x5C, the same copy with `r7`/`r0` instead of `r5`/`r0`), which this item does not reach and
// which is not written.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`, and why the `.c` is compiled as C by the port's host build (where C++ would
// mangle it) and syntax-checked as C++ by `tools/probe_sources.sh`; both accept this file as
// written.
//
// Its own unit because a claim may not span unclaimed bytes: below this range, the run
// 0x801997B4..0x8019AC78 is still dtk's `auto_03_801997B4_text`, and above it
// 0x8019AD24..0x8019CE6C is the rest of the same unclaimed run.
//
// The directory is retail's own, taken from the nearest claimed ranges:
// `MetroidPrime/Carve801997B0.c` (0x801997B0..0x801997B4) and `MetroidPrime/Carve8019CE6C.c`
// (0x8019CE6C..0x8019CE70) are both in `MetroidPrime/`, so this address is in the `MetroidPrime/`
// neighbourhood.

/** The 0x26-byte element these three functions move around.  Only the members the bytes below
 *  touch are named, plus the two padding bytes at +0x22/+0x23: **without them the struct packs
 *  to 0x24 and the last two `lbz`/`stb` pairs land on +0x23 and +0x24 instead of retail's +0x24
 *  and +0x25** - measured, `main.dol` sha1 `28ead22fd4668cf1aab02d9f8161b33c54ed44c3` against
 *  `6ef9b491d0cc08bc81a124fdedb8bfaec34d0010`, four bytes differing at 0x8019AD0C, 0x8019AD14,
 *  0x8019AD18 and 0x8019AD1C, which is objdiff's 99.84% on `fn_8019ACC0`.  `float` at +0x08 is
 *  measured too: it is the one member `fn_8019ACC0` moves with `lfs`/`stfs` rather than
 *  `lwz`/`stw`. */
struct SUnknown8019 {
  unsigned int x00;
  unsigned int x04;
  float x08;
  unsigned int x0c;
  unsigned int x10;
  unsigned int x14;
  unsigned int x18;
  unsigned int x1c;
  unsigned char x20;
  unsigned char x21;
  unsigned char x22Pad;
  unsigned char x23Pad;
  unsigned char x24;
  unsigned char x25;
};

/** `fn_8019ACC0` - retail `.text:0x8019ACC0`, 0x64 = 100 bytes, 25 instructions: the element's
 *  implicit copy constructor.  Neither pointer is copied into a callee-saved register - retail
 *  addresses both objects off `r3`/`r4` for the whole body - and it returns nothing, so the body
 *  is the plain member-wise copy with no `return self`. */
void fn_8019ACC0(void* self, const void* src);

void fn_8019ACC0(void* self, const void* src) {
  struct SUnknown8019* dst = (struct SUnknown8019*)self;
  const struct SUnknown8019* from = (const struct SUnknown8019*)src;
  dst->x00 = from->x00;
  dst->x04 = from->x04;
  dst->x08 = from->x08;
  dst->x0c = from->x0c;
  dst->x10 = from->x10;
  dst->x14 = from->x14;
  dst->x18 = from->x18;
  dst->x1c = from->x1c;
  dst->x20 = from->x20;
  dst->x21 = from->x21;
  dst->x24 = from->x24;
  dst->x25 = from->x25;
}

/** `fn_8019AC98` - retail `.text:0x8019AC98`, 0x28 = 40 bytes, 10 instructions: `rstl::construct`
 *  for the element above - a null test on the *destination* and one call to its copy constructor.
 *  Its twin is `fn_80004D5C` (0x80004D5C, 0x28), byte for byte apart from the `bl` target. */
void fn_8019AC98(void* self, const void* src);

void fn_8019AC98(void* self, const void* src) {
  if (self != 0) {
    fn_8019ACC0(self, src);
  }
}

/** `fn_8019AC78` - retail `.text:0x8019AC78`, 0x20 = 32 bytes, 8 instructions: the in-class
 *  `construct` forwarder, a frame and one call to `fn_8019AC98` above and nothing else, as
 *  `include/rstl/construct.hpp:117-120` spells it.  Its twin is `fn_80004D3C` (0x80004D3C,
 *  0x20), byte for byte apart from the `bl` target. */
void fn_8019AC78(void* self, const void* src);

void fn_8019AC78(void* self, const void* src) { fn_8019AC98(self, src); }