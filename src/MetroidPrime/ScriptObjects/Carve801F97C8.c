// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt` (lines 8190-8192), the instructions are the
// ones dtk emitted into `build/G2ME01/asm/auto_03_801F9190_text.s` before the claim existed, and
// are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801F97C8 --stop-address=0x801F9848
// build/G2ME01/main.elf`.  The body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801F97C8.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x801F97C8..0x801F9848, 0x80 = 128 bytes, 3 functions:
//
//   fn_801F97C8    0x801F97C8  0x38    14 instructions
//   fn_801F9800    0x801F9800  0x20     8 instructions
//   fn_801F9820    0x801F9820  0x28    10 instructions
//
// **What the three are: `rstl::vector<T>::push_back_unsafe` and the two halves of the
// `rstl::construct` it forwards to.**  Retail names none of them, so this is read off the call
// edges and the argument registers, not off a name - and each is byte-for-byte the shape of a
// symbol retail *does* name, elsewhere in the DOL:
//
//   fn_801F97C8  loads the count at +0x4 and the item pointer at +0xC of its r3, increments the
//                count, and calls fn_801F9800 with `items + oldCount * 0x40`.  That is
//                `vector<T>::push_back_unsafe(const T&)` for a `T` of 0x40 bytes, and its
//                byte-identical twin is
//                `push_back_unsafe__Q24rstl50vector<13CInt32POINode,Q24rstl17rmemory_allocator>FRC13CInt32POINode`
//                at 0x80299E64 (0x38 bytes, `src/Kyoto/Animation/CSequenceHelper.cpp`'s unit) -
//                the same instruction schedule and the same offsets, with only the `bl`
//                destination differing.
//   fn_801F9800  forwards both arguments in a plain prologue/call/epilogue frame; its twin is
//                `__sys_free` at 0x80008A28 (0x20 bytes, `src/MetroidPrime/main.cpp`), which is
//                the same eight instructions with a different callee.
//   fn_801F9820  is the null test on the destination and the call: `if (dst != 0) fn_801F9848(dst,
//                src)`.  Its twin is `fn_80004D5C` at 0x80004D5C (0x28 bytes,
//                `src/MetroidPrime/Player/CGameStateBlockConstruct.cpp`), which the header there
//                calls `rstl::construct` for the 16-byte `SGameStateBlock`: instruction for
//                instruction identical apart from the very same two things (`cmplwi r3,0`, `beq`
//                +0x8, and the callee).
//
// The neighbours in this same auto range confirm the reading, and none of it is inferred from the
// `slwi` alone.  `SetupColliders__20CCameraColliderGroupFfffif` (`symbols.txt:8189`, 0x801F9628,
// size 0x1A0 - it ends exactly where this claim begins) builds an element in its own frame, calls
// fn_801F97C8 with `r3 = itself + 0x4, r4 = &element` (0x801F9728..0x801F9730), and then runs the
// inlined destructor on the temporary: `addic. r0,r1,0x20 / beq / stw lbl_803B6564,0x20(r1)`.  The
// same vector's `reserve` (fn_801FA1CC, 0x801FA1CC, size 0xC0) reads the count at +0x4, the
// capacity at +0x8 and the item pointer at +0xC - the offsets this file's struct mirrors - and
// destroys each element through the vtable with `lwz r12,0x0(e) ; lwz r12,0x8(r12) ; bctrl` and
// `r4 = -1` (0x801FA244..0x801FA258), the deleting-destructor slot.  So the vector is the
// `rstl::vector` at +0x4 of its owner and `T` is a 0x40-byte polymorphic class, which is why the
// stride is 0x40 rather than a byte count read off the `slwi`.
//
// **The one function this unit *calls* is not in it, and it does not need to be.**  fn_801F9848
// (0x801F9848, `size:0x88`) is the element's copy constructor - the `construct` target - and it
// is 0x0 bytes past this claim's end, so it stays retail's and dtk supplies it from its own
// `auto_03_801F9848_text` object.  That is why the declaration below is `extern` and not a body:
// claiming it would enlarge the range for nothing, and its own two float-copy loops are a
// separate spelling question.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the functions on either
// side of this run are not trivial.
//
// The directory is retail own, taken from the nearest claimed range below: 0x801F97C8 is 0x778
// bytes past the end of `MetroidPrime/ScriptObjects/CUnknown90.cpp`
// (0x801F9050..0x801F9190).  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.

/** 0x801F9848, `symbols.txt:8193`, the copy constructor fn_801F9820 forwards to.  Also
 *  unclaimed, so also supplied by dtk's own `auto_*` object. */
extern void fn_801F9848(void* dst, const void* src);

/** The `T` of the vector this pushes into: 0x40 bytes, polymorphic (its owner destroys elements
 *  through a vtable slot).  Nothing in this unit reads a field of it, only its size - the stride
 *  - which the `slwi ...,6` in fn_801F97C8 measures. */
struct SCarve801F97C8Elem {
  unsigned char unk00[0x40];
};

/** The prefix of the owning vector that these bytes touch: count at +0x4, item pointer at +0xC.
 *  The same layout is visible in fn_801FA1CC's `reserve` (count `lwz r0,0x4(r27)`, capacity
 *  `lwz r0,0x8(r3)`, items `lwz r4,0xc(r27)`), which is why the names are not guesses. */
struct SCarve801F97C8Vector {
  int x0;
  int count;
  int capacity;
  struct SCarve801F97C8Elem* items;
};

void fn_801F9820(void* dst, const void* src);
void fn_801F9800(void* dst, const void* src);
void fn_801F97C8(struct SCarve801F97C8Vector* self, const void* src);

void fn_801F9820(void* dst, const void* src) {
  if (dst != 0) {
    fn_801F9848(dst, src);
  }
}

void fn_801F9800(void* dst, const void* src) {
  fn_801F9820(dst, src);
}

void fn_801F97C8(struct SCarve801F97C8Vector* self, const void* src) {
  fn_801F9800(self->items + self->count++, src);
}
