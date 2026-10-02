// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:93-97`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80004B9C_text.s` before the claim existed (they are now
// two files, `auto_03_80004B9C_text.s` and `auto_03_80004CD4_text.s`), and the body below is the C
// those bytes are the compilation of.
//
// .text 0x80004C4C..0x80004D5C, 0x110 = 272 bytes, 5 functions:
//
//   fn_80004C4C    0x80004C4C  0x20     8 instructions   `rstl::destroy< SGameStateBlock >(T*)`
//   fn_80004C6C    0x80004C6C  0x24     9 instructions   `rstl::destroy_impl< SGameStateBlock >(T*)`
//   fn_80004C90    0x80004C90  0x44    17 instructions   `SGameStateSlots`'s copy constructor
//   fn_80004CD4    0x80004CD4  0x68    26 instructions   the element copy loop it calls
//   fn_80004D3C    0x80004D3C  0x20     8 instructions   `construct< SGameStateBlock >`, forwarder
//
// **What the five are: the tail of the 3-element saved-game block's machinery.**  The block is
// `SGameStateSlots` in `include/MetroidPrime/Player/CGameStateBlocks.hpp` -
// `{ int x00_count; SGameStateBlock x04_blk[3]; }`, 0x34 bytes, whose 16-byte element the two
// `Matching` neighbours below it already own (`Player/CGameStateBlockDtor.cpp` is `fn_80004A4C`,
// `Player/CGameStateBlockCopyCtor.cpp` is the element copy `fn_80004AA0`).  Each function is read
// off its call edge and its argument registers:
//
//   fn_80004C4C  is a frame and one unconditional `bl fn_80004C6C` - no load, no test, no return
//                value - so it is the `destroy` of the pair, whose whole body is
//                `destroy_impl(in)` (`include/rstl/construct.hpp:95-99`).  `fn_80004BEC`
//                (0x80004BEC, 0x60, unclaimed) is its caller: the element walk that
//                `include/rstl/reserved_vector.hpp` spells as `destroy_elements()`, stepping 0x10
//                from +4 and calling this on each element (`bl fn_80004C4C` at 0x80004C18).  It is
//                also the same 8 instructions as `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`)
//                and as `destroy< 11CTweakValue >__4rstlFP11CTweakValue` (0x80006830), apart from
//                the one `bl`.
//   fn_80004C6C  materialises `li r4,-1` and calls `fn_80004A4C`, taking nothing else from its own
//                argument - that is `destroy_impl`'s `in->~T()`, and the -1 is MWCC's
//                "destroy, do not free me afterwards" flag, so the callee is the *deleting*
//                destructor of the element.  **The twin is exact**: `destroy_impl< 11CTweakValue
//                >__4rstlFP11CTweakValue` (0x80006850, 0x24) is these nine instructions word for
//                word with `bl __dt__11CTweakValueFv` in place of `bl fn_80004A4C`.
//   fn_80004C90  takes `(dst, src)`, reads the source's word at +0, stores it at the destination's
//                +0, then hands `src+4` (first argument), that count (second) and `dst+4` (third)
//                to `fn_80004CD4` and returns the destination.  Those are exactly
//                `reserved_vector`'s copy constructor - count first, then the element array -
//                and **its twin is exact too**: `fn_80248E60` (0x80248E60, 0x44,
//                `src/WorldFormat/CMetroidAreaCollider.cpp:950`) is `self->mCount =
//                other->mCount; fn_80248EA4(other->data(), self->mCount, self->data()); return
//                self;`, instruction for instruction identical to this one apart from the `bl`
//                target.  **The count reload at 0x80004CB4 is the measurement, not a decoration.**
//                Retail loads the source's word once (`lwz r0,0(r4)`), stores it, and then
//                *re-loads it from the destination* (`lwz r4,0(r31)`) to pass as `fn_80004CD4`'s
//                count - it does not keep the value it already has in `r0`.  So the bound argument
//                is written on the destination, exactly as `fn_80248E60` does, and that is what
//                puts `dst` in `r31` and the two cursors in `r3`/`r5`.
//   fn_80004CD4  is the element-wise copy the constructor calls: `(first, count, result)` in
//                r3/r4/r5, a count-first loop that `construct`s each element and returns the end
//                cursor.  **Its twin is `fn_80248EA4`** (0x80248EA4, 0x68,
//                `src/WorldFormat/CMetroidAreaCollider.cpp:915`), `reserved_vector`'s
//                `uninitialized_copy_n` for a 36-byte element: the same 26 instructions, same
//                0x20 frame, same three cursors in `r31`/`r30`/`r29`, same `b` to the bottom test,
//                same `subi`/`addi` increment order - only the `bl` target and the stride differ
//                (0x10 here, 0x24 there).  The one detail the twin's `remaining != 0` spelling
//                carries is the loop being entered at its **bottom** test, which is retail's
//                `b .L_80004D14`: a zero count copies nothing and still returns `result`.
//   fn_80004D3C  is the same 8-instruction forwarder shape as `fn_80004C4C` and `__sys_free`, with
//                one call to `fn_80004D5C` - the element's null-guarded `construct`
//                (`Player/CGameStateBlockConstruct.cpp`, a `Matching` unit, 0x28 bytes).  It is
//                the callee `fn_80004CD4` calls once per element.
//
// **Why the claim is the whole contiguous tail and not only the three the item named.**  The item's
// range (0x80004C4C..0x80004CD4) is the three functions the seeder twin-checked, and it stops one
// function short of the end of the run.  Claiming only those three leaves the copy constructor's
// callee `fn_80004CD4` undefined in the port, which is one **new** undefined symbol - and
// `tools/link_check.sh --strict`, run by `tools/probe_sources.sh` in the gate, fails when the
// port's undefined count *grows* against its recorded baseline: measured 291 -> 292, `STRICT FAIL -
// regression gate ... (GREW)`.  The two functions above are in the same `auto_*` run, `fn_80004D3C`
// is `fn_80004CD4`'s only callee, and writing both leaves this object calling only symbols
// `files.cmake` already defines: `fn_80004A4C` by `Player/CGameStateBlockDtor.cpp` and
// `fn_80004D5C` by `Player/CGameStateBlockConstruct.cpp`.  So the claim is 0x80004C4C..0x80004D5C,
// the whole tail, and the port's undefined count does not move.  Nothing here is a stub: all five
// bodies are the C their bytes compile from, and `tools/flip_test.sh` says so.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range is
// `auto_03_80004B9C_text`: `__dt__80004B9C` (0x80004B9C, 0x50) and `fn_80004BEC` (0x80004BEC, 0x60),
// neither of which any unit claims.  Above it, at 0x80004D5C, is
// `Player/CGameStateBlockConstruct.cpp`'s claim, so this is the whole of what remains unclaimed
// between them.
//
// The directory is retail's own, taken from the nearest claimed ranges: `Player/CGameStateBlockDtor.cpp`
// (0x80004A4C..0x80004AA0), `Player/CGameStateBlockCopyCtor.cpp` (0x80004AA0..0x80004B9C) and
// `Player/CGameStateBlockConstruct.cpp` (0x80004D5C..0x80004D84) are the block's other units, so
// this address is in the `MetroidPrime/Player/` neighbourhood.

/** The head of the block this file copies and tears down: the element count at +0x00, with the
 *  element array itself at +0x04 - `SGameStateSlots` in
 *  `include/MetroidPrime/Player/CGameStateBlocks.hpp`, of which only the one word is read here.
 *  The array is reached by the `+ 4` below rather than with a member, because spelling the latter
 *  as a `SGameStateBlock` array would drag in the element type and the count is the only word
 *  these bytes touch. */
struct SBlockHead {
  int mCount;
};

/** 0x80004A4C, `symbols.txt:90`, 0x54 = 84 bytes: the 16-byte element's deleting destructor, the
 *  function this file's `fn_80004C6C` calls with the `-1` flag.  Defined by
 *  `src/MetroidPrime/Player/CGameStateBlockDtor.cpp`, a `Matching` unit.  Declared, never defined
 *  here. */
extern void fn_80004A4C(void* self, int flag);

/** 0x80004D5C, `symbols.txt:98`, 0x28 = 40 bytes: the element's null-guarded `construct`, the
 *  callee of `fn_80004D3C` below.  Defined by `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`,
 *  a `Matching` unit.  Declared, never defined here. */
extern void fn_80004D5C(void* self, const void* src);

/** `fn_80004D3C` - retail `.text:0x80004D3C`, 0x20 = 32 bytes: `rstl::construct< SGameStateBlock >`
 *  for one element, a frame and one call to `fn_80004D5C` and nothing else - the shape
 *  `include/rstl/construct.hpp:117-120` gives the in-class `construct`.  Its twin is
 *  `fn_80004C4C` below (and `__sys_free`, 0x80008A28), the same 8 instructions with a different
 *  `bl` target. */
void fn_80004D3C(void* dest, const void* src);

void fn_80004D3C(void* dest, const void* src) { fn_80004D5C(dest, src); }

/** `fn_80004CD4` - retail `.text:0x80004CD4`, 0x68 = 104 bytes: the element-wise copy loop,
 *  `uninitialized_copy_n` for the 16-byte element.  The twin is `fn_80248EA4` (0x80248EA4, 0x68),
 *  byte for byte apart from the `bl` target and the 0x24 stride, so the body below is that
 *  function's source with the element size this block's bytes use: `construct` each element, walk
 *  both cursors, return the end of the copy. */
void* fn_80004CD4(const void* first, int count, void* result);

void* fn_80004CD4(const void* first, int count, void* result) {
  const unsigned char* it = (const unsigned char*)first;
  unsigned char* cur = (unsigned char*)result;
  int remaining;
  for (remaining = count; remaining != 0; --remaining, it += 16, cur += 16) {
    fn_80004D3C(cur, it);
  }
  return cur;
}

/** `fn_80004C90` - retail `.text:0x80004C90`, 0x44 = 68 bytes: `SGameStateSlots`'s copy
 *  constructor, and the same 17 instructions as `fn_80248E60`
 *  (`WorldFormat/CMetroidAreaCollider.cpp:950`), its measured twin, apart from the `bl` target. */
void* fn_80004C90(void* self, const void* src);

void* fn_80004C90(void* self, const void* src) {
  struct SBlockHead* dst = (struct SBlockHead*)self;
  const struct SBlockHead* from = (const struct SBlockHead*)src;
  dst->mCount = from->mCount;
  fn_80004CD4((const unsigned char*)src + 4, dst->mCount, (unsigned char*)self + 4);
  return self;
}

/** `fn_80004C6C` - retail `.text:0x80004C6C`, 0x24 = 36 bytes: `rstl::destroy_impl` for the
 *  16-byte element, i.e. the element's destructor called with the "do not free me" flag.  The
 *  twin is `destroy_impl< 11CTweakValue >__4rstlFP11CTweakValue` (0x80006850, 0x24), which is
 *  these nine instructions word for word.  `-1` is an `int`, not a `short`: the twin's `li r4,-1`
 *  is the same instruction either way, and the callee's own definition takes an `int`. */
void fn_80004C6C(void* self);

void fn_80004C6C(void* self) { fn_80004A4C(self, -1); }

/** `fn_80004C4C` - retail `.text:0x80004C4C`, 0x20 = 32 bytes: `rstl::destroy` for the same
 *  element, a frame and one call to `fn_80004C6C` above and nothing else, as
 *  `include/rstl/construct.hpp:95-99` spells it.  Its measured twin is `__sys_free` (0x80008A28,
 *  0x20) in `src/MetroidPrime/main.cpp`, byte for byte apart from the `bl` target. */
void fn_80004C4C(void* self);

void fn_80004C4C(void* self) { fn_80004C6C(self); }
