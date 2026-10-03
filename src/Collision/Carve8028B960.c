// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:11397`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8028B914_text.s:85-114` before the claim existed (the same
// range is now `build/G2ME01/asm/Collision/Carve8028B960.s`), and the body below is the C those
// bytes are the compilation of.
//
// .text 0x8028BA18..0x8028BA80, 0x68 = 104 bytes, 1 function:
//
//   fn_8028BA18    0x8028BA18  0x68    26 instructions   `rstl::uninitialized_copy(It, It, T*)`
//
// **Its byte-shape twin is matched in this tree and its source is the reading.**  The seed named it;
// the reading confirms it instruction for instruction:
// `uninitialized_copy<Q24rstl118pointer_iterator<12SAreaSurface,Q24rstl49vector<12SAreaSurface,
// Q24rstl17rmemory_allocator>,Q24rstl17rmemory_allocator>,P12SAreaSurface>__4rstlFQ24rstl118
// pointer_iterator<...>Q24rstl118pointer_iterator<...>P12SAreaSurface` (0x8005F300, 0x68,
// `src/MetroidPrime/CGameArea.cpp`, `Matching`) is the same twenty-six instructions with only the call
// target and the caller-slot addresses different - `rstl::construct<SAreaSurface>` there,
// `fn_8028B8F4` here - and this tree's own header spells the body:
// `include/rstl/construct.hpp:117-127` is `uninitialized_copy(It begin, It end, T out)`, which copies
// `begin` into a cursor, keeps the output pointer in a second cursor, and per element calls
// `construct(tmp, *cur)` before `++cur, ++tmp`.  Those are exactly this function's instructions:
// `lwz r31, 0(r3)` for `It cur = begin`, `mr r30, r5` for `T tmp = out`, the `bl` for the construct,
// the two `addi ...,0x20` for the increments and `cmplw r31, 0(r29)` for the cursor comparison.
// `end` stays in r29 and is re-read every iteration (`lwz r0, 0(r29)`) rather than hoisted, which is
// why the third parameter is spelled `struct It*` here and not a loaded pointer.
//
// **Both arguments are pointers to a one-word iterator object, and that is measured, not assumed.**
// `rstl::pointer_iterator<T, Vec, Alloc>` has exactly one member, the `T* current`
// (`include/rstl/pointer_iterator.hpp:19-73`), and retail's own copy loads `0x0(r3)` for the cursor
// and `0x0(r29)` for the end, so each argument points at such an object - the caller materialises
// them in its frame, which is what `fn_8028B960` below does at `r1+0x0C` and `r1+0x14`.
//
// **The 0x20-byte element is measured from the code that walks it.**  The loop's own
// `addi r31, r31, 0x20` and `addi r30, r30, 0x20` are the step, and the one retail caller of this
// function (measured with `grep -rn 'bl fn_8028BA18' build/G2ME01/asm/`, which returns exactly one
// hit, `auto_03_8028B914_text.s:59`) is `fn_8028B960` at 0x8028B9C8, whose `slwi r3, r30, 5` sizes
// the buffer at `newSize * 32` and whose destroy loop steps by `addi r4, r4, 0x20`.
//
// `fn_8028B8F4` (0x8028B8F4, `symbols.txt:11394`, 0x20 = 32 bytes) is retail's
// `rstl::construct<SAreaSurface>` for this element and **this file does not define it**: it belongs
// to `src/Collision/Carve8028B8BC.c`, which already claims 0x8028B8BC..0x8028B914 and is `Matching`,
// and it forwards to `fn_8028B914` (0x8028B914, 0x4C, still inside dtk's own object, whose copy of
// the bytes is `auto_03_8028B914_text.s:14-31`; for the port link the announced stand-in is
// `stub_8028b8bc_0` in `src/MetroidPrime/PortLinkStubs.cpp`).  `fn_8028B8BC` in that same unit is the
// only other retail caller of `fn_8028B8F4` (`Collision/Carve8028B8BC.s:19`), so the backward call
// from this unit is retail's own direction: the callee is the lower address.
//
// `fn_8028B960` (0x8028B960, `symbols.txt:11396`, 0xB8 = 184 bytes) is **below** this claim and is
// therefore not defined here - dtk's `auto_03_8028B914_text.o` still owns it, and this file is not
// in `configure.py` for it.  What retail's bytes show, read off
// `auto_03_8028B914_text.s:32-84`: `rstl::vector<SAreaSurface>::reserve(int)`.  It returns
// unchanged when `newSize <= mCapacity` at +0x08 (`cmpw r30, r0 / ble`), otherwise it allocates
// `newSize * 32` bytes through `allocate__Q24rstl17rmemory_allocatorFi`, builds the four stack words
// its `uninitialized_copy` call takes (this function's `begin` and `end` iterators at `r1+0x14` and
// `r1+0x0C`, plus two dead copies at `r1+0x10` and `r1+0x08` holding the same two pointers), calls
// this function at 0x8028B9C8, then walks the old buffer with a loop that **calls nothing** and
// steps by 0x20 - that is `destroy(mItems, mItems + mCount)` for a type with a trivial destructor -
// frees the old buffer with `Free__7CMemoryFPCv` and stores the new pointer and capacity.  **Its
// body is not claimed decompiled by this unit and its bytes are not this file's claim.**  It is the
// next carve in this hole; the reading above is what a lane needs, and the reason this claim stops
// where it does is that a claim may not span an unclaimed gap - and `fn_8028B960` will not compile
// to these bytes in plain C with the spellings tried (see the notes file for this item): the same
// four stores with the same offsets are reachable, but mwcceppc then materialises the two argument
// addresses in the other order (`addi r4, r1, 0xC` before `addi r3, r1, 0x14`), which cascades into
// a different temp register (`r5`/`r0` where retail has `r4`/`r6`) and a different position for the
// second read of `+0x0C`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  This file defines one function, so it cannot be permuted; the
// rule is stated so the next carve in this hole keeps to it.
//
// Retail names this function.  `symbols.txt` carries the `fn_8028BA18` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_8028BA18v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: this run sits inside the
// 0x8028B780..0x8028BC64 hole that dtk covers with `auto_03_8028B780_text`, and the functions on
// either side of it are not trivial - `fn_8028B914` below and the 0xB8-byte reserve below that.
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Collision/Carve8028B8BC.c` (0x8028B8BC..0x8028B914), above is
// `Kyoto/Basics/CStopwatch.cpp` (0x8028BC64..0x8028BD3C).

/** 0x8028B8F4, `symbols.txt:11394`, 0x20 = 32 bytes, `src/Collision/Carve8028B8BC.c`'s
 *  `fn_8028B8F4`: retail's `rstl::construct<SAreaSurface>` - a frame and one unconditional `bl` to
 *  `fn_8028B914` - which is the call this loop makes once per element.  `Matching` in that unit, and
 *  the same object the DOL already links, so the call below resolves without any stand-in. */
extern void fn_8028B8F4(void* dest, const void* src);

/** `rstl::pointer_iterator`'s only member: retail loads `+0x0` off both arguments and nothing else,
 *  and both are one word wide, so one member is the whole of what this file needs to model. */
struct SCarve8028B960Iterator {
  const char* x00_current;
};

/** Copies each element `*begin` into `out`, 0x20 bytes per element, and returns the output cursor
 *  one past the last element written - retail returns it in r3 (`mr r3, r30`, 0x8028BA64) and its
 *  only caller ignores the value. */
void* fn_8028BA18(const struct SCarve8028B960Iterator* begin, struct SCarve8028B960Iterator* end,
                  void* out);

void* fn_8028BA18(const struct SCarve8028B960Iterator* begin, struct SCarve8028B960Iterator* end,
                  void* out) {
  const char* cur = begin->x00_current;
  char* tmp = (char*)out;
  for (; cur != end->x00_current; cur += 0x20, tmp += 0x20) {
    fn_8028B8F4(tmp, cur);
  }
  return tmp;
}