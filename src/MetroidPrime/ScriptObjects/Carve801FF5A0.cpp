// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `config/G2ME01/symbols.txt` (lines 8342-8345),
// the instructions are the ones dtk emitted into `build/G2ME01/asm/auto_03_801FF4C4_text.s`
// (`.text:0xDC` through `.text:0x1F4`) before the claim existed, and are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF5A0 --stop-address=0x801FF720
// build/G2ME01/main.elf`.  The body below is the C++ those bytes are the compilation of, and
// `tools/flip_test.sh` is what says so.
//
// .text 0x801FF5A0..0x801FF720, 0x180 = 384 bytes, 4 functions:
//
//   fn_801FF5A0    0x801FF5A0  0xAC    43 instructions   the 0x24-element block's reserve
//   fn_801FF64C    0x801FF64C  0x20     8 instructions   forwarder to fn_801FF66C
//   fn_801FF66C    0x801FF66C  0x4C    19 instructions   destroy_impl(begin, end)
//   fn_801FF6B8    0x801FF6B8  0x68    26 instructions   uninitialized_copy(begin, end, dst)
//
// **What the four are: one 0x24 = 36-byte-element block's own `reserve` and the three helpers
// it calls.**  Retail names none of them, so this is read off the call edges and the argument
// registers - and each is byte-for-byte the shape of a symbol retail *does* name, elsewhere in
// this same DOL, so the bodies below are those symbols' C++ with the callees and the stride
// this copy uses:
//
//   fn_801FF5A0  twin of `fn_801466F4` (0x801466F4, 0xAC, Matching in
//                `src/MetroidPrime/Player/CGameState.cpp`) - instruction for instruction the same
//                apart from the four call targets and the two argument addresses.
//   fn_801FF6B8  twin of `fn_8014680C` (0x8014680C, 0x68, Matching, same file); `fn_80142718`
//                is this copy's `fn_801FEE40`.
//   fn_801FF66C  twin of `fn_801467C0` (0x801467C0, 0x4C, Matching, same file); `fn_801435D4` is
//                this copy's `fn_801FD638`.
//   fn_801FF64C  the `fn_801467A0` forwarder (0x801467A0, 0x20, Matching, same file) - and the
//                same 8-instruction shape as `__sys_free` (0x80008A28, Matching in
//                `src/MetroidPrime/main.cpp`), which is why `fn_801FF64C` takes **two** pointers
//                and forwards both in r3/r4 untouched rather than one.
//
// `fn_801FEE40` and `fn_801FD638` (0x20 bytes each, `symbols.txt:8321` and `:8271`) are the
// element copy and the element destructor: each is the MWCC `destroy<T>(T*)` shape - prologue,
// one `bl`, epilogue, no register shuffling - forwarding to `fn_801FEE60` / `fn_801FD658`.
//
// **The argument struct is load-bearing, and it is why `fn_801FF6B8` takes a struct at all.**
// Retail's guard is `lwz r31,0(r3)` for `begin` and `mr r29,r4` for `end`, and the loop
// condition *re-reads* the bound from memory every iteration (`lwz r0,0(r29)` / `cmplw r31,r0`
// / `bne`, 0x801FF6F4-0x801FF6FC).  That is only reachable if both iterators arrive **by value**
// as one-word classes: the callee gets the address of the caller's temporary and reloads
// through it.  `fn_801FDB5C.c` in this same directory is the same argument on the same point.
// `fn_801FF5A0` materialises both of those temporaries on its own stack, one 8-byte slot per
// argument holding the same pointer twice - 0x8/0xc for `end`, 0x10/0x14 for `begin`, with
// `addi r4,r1,0xc` and `addi r3,r1,0x14` (0x801FF5E0-0x801FF604) the addresses handed over.
// `fn_801466F4`'s own comment records the same eight stores at 0x80146734-0x80146758, **and the
// `lwz r0,0xc(r29)` reload at 0x801FF5F8 between them**: retail reads `x0c_data` twice, once
// per temporary.
//
// **So this unit is C++ even though retail names none of its functions.**  `SStateIter`'s
// converting constructor is what makes mwcceppc build those two-word temporaries and refuse to
// common the two loads, and every other unnamed carve in this tree is a `.c` only because it
// does not need this.  Written as plain C with a one-word struct passed by value, the same four
// bodies are **not** byte-exact, measured against dtk's own bytes:
//   - `fn_801FF5A0`: 27 of 43 words differ.  mwcceppc's C mode common-subexpressions the two
//     `x0c` reads, so it never reloads and emits **42** instructions instead of 43, and the two
//     argument slots land interleaved (begin at 0xc/0x14, end at 0x8/0x10) rather than in two
//     contiguous 8-byte slots.  Reversing the two assignments, and casting `self` on the second
//     read, change nothing - measured.
//   - `fn_801FF66C`: 6 of 19 words differ, all of it one thing: C mode puts the walked cursor
//     in r30 and the bound in r31 where retail has them the other way round.  `while` and
//     `for` spellings both do it.
//   The other two (`fn_801FF64C`, `fn_801FF6B8`) are byte-exact in C, so it is specifically the
//   C-mode register allocation and CSE, not the struct-by-value ABI, that is the difference.
// The symbols stay unmangled because every definition is `extern "C"`; `powerpc-eabi-nm` on the
// object shows four `T fn_801FF<addr>` and nothing else, which is the thing the `.c` rule is
// protecting.  Thirteen other `Carve*.cpp` units in this tree are `extern "C"` for the same
// reason.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// **The same four functions repeat over this range**: the 0xAC/0x20/0x4C/0x68 run at 0x801FF5A0,
// then 0x801FF720, 0x801FF8A0 and 0x801FFA20, with the helper triplet also standing alone at
// 0x801FF7CC/0x801FF7EC/0x801FF838, 0x801FF94C/0x801FF96C/0x801FF9B8 and
// 0x801FFACC/0x801FFAEC/0x801FFB38.  One template, emitted per script-object type, which is
// what fixes the shapes here.  Only the first round is claimed; the rest stay retail's and dtk
// fills them.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the functions on
// either side of this run are not trivial.
//
// The directory is retail own, taken from the nearest claimed range: this address is 0x6B50
// bytes into `MetroidPrime/ScriptObjects/CUnknown90.cpp` (0x801F9050..0x801F9190), so the code
// is that unit neighbourhood.  For an anonymous function that is the only evidence there is,
// and it beats a lane picking the directory it happened to own.

/** `rstl::rmemory_allocator::allocate` and `CMemory::Free`, taken by their mangled names rather
 *  than by including the headers: this object must define exactly the four functions above and
 *  nothing else, and either header would bring an inline copy of the callee in with it.  Both
 *  addresses are retail's own (`symbols.txt`, and the `bl` at 0x801FF5D0 / 0x801FF624). */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FEE40, 0x20 bytes: the 0x24-byte element's copy, called with the destination first
 *  (`mr r3,r30` then `mr r4,r31`, 0x801FF6E0-0x801FF6E4). */
extern "C" void fn_801FEE40(void* dst, const void* src);

/** 0x801FD638, 0x20 bytes: the same element's destructor, called with the element alone
 *  (`mr r3,r31`, 0x801FF68C). */
extern "C" void fn_801FD638(void* elem);

/** The one pointer of the block's element iterator, `current`.  What the bytes need of the class
 *  is exactly this: a struct of one pointer, passed by value, and - see the note above - built
 *  by a converting constructor so that `fn_801FF5A0`'s two temporaries come out as they do. */
struct SStateIter {
  void* mCur;
  SStateIter(void* cur) : mCur(cur) {}
};

/** The block itself, as far as these four functions read it.  `x04` is the element count and
 *  `x0c` the base pointer, because the loop bound is `x0c + x04 * 0x24`; `x08` is the capacity
 *  `fn_801FF5A0` compares against with a **signed** `cmpw` and writes back the requested
 *  capacity into.  Nothing reads `x00`, so nothing is asserted about it.  Same three words at
 *  the same offsets as `SGameStateBlock`'s (`include/MetroidPrime/Player/CGameStateBlocks.hpp`),
 *  which is the twin's block; only the element stride differs, 0x24 here and there. */
struct SCarve801FF5A0Block {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  void* x0c;
};

extern "C" void* fn_801FF6B8(SStateIter begin, SStateIter end, void* dst);
extern "C" void fn_801FF66C(char* begin, char* end);
extern "C" void fn_801FF64C(char* begin, char* end);

/** `uninitialized_copy`: one 0x24-strided walk that copy-constructs each element and returns
 *  the new end, which is what `fn_801FF5A0` leaves in r3.  The bound is re-read from `end`
 *  every iteration, so it must not be hoisted into a register. */
extern "C" void* fn_801FF6B8(SStateIter begin, SStateIter end, void* dst) {
  char* in = static_cast< char* >(begin.mCur);
  char* out = static_cast< char* >(dst);
  for (; in != static_cast< char* >(end.mCur); in += 36, out += 36) {
    fn_801FEE40(out, in);
  }
  return out;
}

/** `destroy_impl`: the same 0x24-strided walk with the element destructor instead.  Retail
 *  jumps to the condition first (`b .L_801FF698`) rather than testing on entry. */
extern "C" void fn_801FF66C(char* begin, char* end) {
  for (char* p = begin; p != end; p += 36) {
    fn_801FD638(p);
  }
}

/** The two-argument forwarder.  Both pointers go across in r3/r4 with no shuffling at all,
 *  which is why it takes `begin, end` and not the one pointer `__sys_free`'s shape suggests. */
extern "C" void fn_801FF64C(char* begin, char* end) { fn_801FF66C(begin, end); }

/** The block's `reserve`.  `capacity <= x08` returns without allocating; otherwise it allocates
 *  `capacity * 0x24`, moves the live elements across with `fn_801FF6B8`, destroys the old ones
 *  with `fn_801FF64C`, frees the old buffer and stores the new buffer and capacity back.  Both
 *  ends are re-read from `self` after the copy instead of being held in locals: keeping them
 *  costs a fifth live register, `stmw r27,28(r1)` and a 0x90-byte frame - the same trade
 *  `fn_801466F4` records in its own comment. */
extern "C" void fn_801FF5A0(SCarve801FF5A0Block* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  char* const buffer =
      static_cast< char* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 36));
  fn_801FF6B8(SStateIter(self->x0c),
              SStateIter(static_cast< char* >(self->x0c) + self->x04 * 36), buffer);
  fn_801FF64C(static_cast< char* >(self->x0c),
              static_cast< char* >(self->x0c) + self->x04 * 36);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}
