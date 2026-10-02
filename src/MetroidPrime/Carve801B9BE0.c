// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt` (lines 7191-7194), the instructions are the
// ones dtk emitted into `build/G2ME01/asm/auto_03_801B94B8_text.s` before the claim existed, and
// are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801B9BE0 --stop-address=0x801B9C68
// build/G2ME01/main.elf`.  The body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/Carve801B9BE0.s` is this unit's own generated listing, not the
// retail one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x801B9BE0..0x801B9C68, 0x88 = 136 bytes, 3 functions:
//
//   fn_801B9BE0    0x801B9BE0  0x20     8 instructions
//   fn_801B9C00    0x801B9C00  0x28    10 instructions
//   fn_801B9C28    0x801B9C28  0x40    16 instructions
//
// **What the three are: the copy-construct chain for one 0x70-byte element.**  fn_801B9BE0 is a
// forwarding frame (prologue, `bl fn_801B9C00` with r3/r4 untouched, epilogue), fn_801B9C00 is the
// null-guarded `construct` (`cmplwi r3,0` / `beq +0x8` / `bl fn_801B9C28`), and fn_801B9C28 is the
// element's own copy constructor - it copies the word at +0x0, copy-constructs the member at +0x4
// through fn_801B9C68 and returns the destination.  Retail names none of them, so this is read off
// the call edges and the argument registers, not off a name - and each is byte-for-byte the shape
// of a symbol retail *does* name, elsewhere in the DOL, with **only the `bl` destination
// differing** (none of the three touches a data address):
//
//   fn_801B9BE0  twin `__sys_free` (0x80008A28, 0x20 bytes, `src/MetroidPrime/main.cpp`), which is
//                the same eight instructions with a different callee.
//   fn_801B9C00  twin `fn_80004D5C` (0x80004D5C, 0x28 bytes,
//                `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`, the header there calls it
//                `rstl::construct` for the 16-byte `SGameStateBlock`): identical instruction for
//                instruction, the same `cmplwi r3,0`, the same `beq +0x8` and the same callee
//                shape.
//   fn_801B9C28  twin `fn_80026F28` (0x80026F28, 0x40 bytes, `src/MetroidPrime/CAnimData.cpp`, the
//                copy constructor of `TAdditiveAnimEntry`): 16 instructions, identical including
//                the schedule `lwz r0,0(r4)` / `addi r4,r4,4` / `stw r31,12(r1)` / `mr r31,r3` /
//                `addi r3,r31,4` / `stw r0,0(r31)` / `bl` / `mr r3,r31`.
//
// The element's size and the +0x4 member are not inferred from the byte counts alone: the caller
// `fn_801B9B18` (0x801B9B18, `size:0xC8`, ending exactly where this claim begins) sets
// `r3 = element, r4 = source` and calls fn_801B9BE0 inside a loop that advances **both** pointers
// by 0x70 (`mr r3,r27` / `mr r4,r28` at 0x801B9B90..0x801B9B94, `addi r27,r27,0x70` /
// `addi r28,r28,0x70` at 0x801B9B9C..0x801B9BA4), and the same stride appears as
// `mulli r0,r3,0x70` (0x801B9B80) in the element count it derives from the vector it inserts into.
//
// **The one function this unit calls and does not define is fn_801B9C68** (0x801B9C68,
// `size:0x148` = 328 bytes, `symbols.txt:7194`), the +0x4 member's own copy constructor.  It is
// 0x0 bytes past this claim's end, so it stays retail's and dtk supplies it from its own
// `auto_03_801B9C68_text` object - which is why the declaration below is `extern` and not a body:
// claiming it would enlarge the range for nothing, and its own two-byte-element loops are a
// separate spelling question.  The port's link does not carry that object, which is why
// `src/MetroidPrime/PortLinkStubs.cpp`'s `stub_187` stands in for it there.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the functions on either side
// of this run are not trivial.
//
// The directory is retail own, taken from the nearest claimed ranges: the carve below it,
// `MetroidPrime/Carve801B94B4.c` (0x801B94B4..0x801B94B8, 0x728 bytes before this claim), and the
// one above it, `MetroidPrime/Carve801BC900.c` (0x801BC900..0x801BC908, 0x2C98 bytes after), are
// both in `MetroidPrime/`, and the nearest claimed non-carve range below is
// `MetroidPrime/Cameras/CBallCameraTransitionState.cpp` (0x801B730C..0x801B73E4, 0x27FC bytes
// before this claim).  For an anonymous function that is the only evidence there is, and it beats
// a lane picking the directory it happened to own.

/** 0x801B9C68, `symbols.txt:7194`, the +0x4 member's copy constructor fn_801B9C28 forwards to.
 *  Also unclaimed, so also supplied by dtk's own `auto_03_801B9C68_text` object. */
extern void fn_801B9C68(void* dst, const void* src);

/** The `T` of the vector these copy-construct into: 0x70 bytes, a word at +0x0 and the rest at
 *  +0x4.  Nothing in this unit reads a field of it but that word - the 0x70 is the stride
 *  fn_801B9B18 advances by and the divisor of its `mulli`, not a size read off a byte count. */
struct SCarve801B9BE0Elem {
  int x00;
  unsigned char x04[0x6C];
};

struct SCarve801B9BE0Elem* fn_801B9C28(struct SCarve801B9BE0Elem* dst,
                                       const struct SCarve801B9BE0Elem* src);
void fn_801B9C00(struct SCarve801B9BE0Elem* dst, const struct SCarve801B9BE0Elem* src);
void fn_801B9BE0(struct SCarve801B9BE0Elem* dst, const struct SCarve801B9BE0Elem* src);

struct SCarve801B9BE0Elem* fn_801B9C28(struct SCarve801B9BE0Elem* dst,
                                       const struct SCarve801B9BE0Elem* src) {
  dst->x00 = src->x00;
  fn_801B9C68(&dst->x04, &src->x04);
  return dst;
}

void fn_801B9C00(struct SCarve801B9BE0Elem* dst, const struct SCarve801B9BE0Elem* src) {
  if (dst != 0) {
    fn_801B9C28(dst, src);
  }
}

void fn_801B9BE0(struct SCarve801B9BE0Elem* dst, const struct SCarve801B9BE0Elem* src) {
  fn_801B9C00(dst, src);
}
