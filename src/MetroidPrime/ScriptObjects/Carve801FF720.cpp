// Carved out of an unclaimed dtk `auto_*` range by a goal lane.  Every number here is measured:
// the addresses and sizes come from `config/G2ME01/symbols.txt` (lines 8346-8349), the
// instructions are the ones dtk emitted into `build/G2ME01/asm/auto_03_801FF720_text.s` before
// the claim existed, and are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FF720 --stop-address=0x801FF8A0
// build/G2ME01/main.elf`.  The body below is the C++ those bytes are the compilation of, and
// `tools/flip_test.sh` is what says so.
//
// .text 0x801FF720..0x801FF8A0, 0x180 = 384 bytes, 4 functions:
//
//   fn_801FF720    0x801FF720  0xAC    43 instructions   the 0x24-element block's reserve
//   fn_801FF7CC    0x801FF7CC  0x20     8 instructions   forwarder to fn_801FF7EC
//   fn_801FF7EC    0x801FF7EC  0x4C    19 instructions   destroy_impl(begin, end)
//   fn_801FF838    0x801FF838  0x68    26 instructions   uninitialized_copy(begin, end, dst)
//
// **What the four are: the same 36-byte-element block's `reserve` and its three helpers, emitted a
// second time for another script-object type.**  This is the very next 0x180 run of the same
// template as `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` (0x801FF5A0..0x801FF720,
// `Matching`), and the measurement that says so is a direct one - disassemble both ranges out of
// `build/G2ME01/main.elf` and compare word by word:
//
//   $ objdump -d --start-address=0x801FF5A0 --stop-address=0x801FF720 build/G2ME01/main.elf > a
//   $ objdump -d --start-address=0x801FF720 --stop-address=0x801FF8A0 build/G2ME01/main.elf > b
//   96 instructions each, 384 bytes each, **4 differing words, all of them `bl`s**
//     a: 0x801FF690 bl fn_801FD638   |  b: 0x801FF810 bl fn_801FD8E0
//     a: 0x801FF6E8 bl fn_801FEE40   |  b: 0x801FF868 bl fn_801FEA98
//     a: 0x801FF5D0 bl allocate__Q24rstl17rmemory_allocatorFi
//     a: 0x801FF624 bl Free__7CMemoryFPCv           (the same two absolute targets in b)
//
// So this file is that file's body with two callee names changed, and nothing else.  The two
// differing callees are each the MWCC `destroy<T>(T*)` / element-copy forwarder shape, 0x20 bytes
// (`symbols.txt:8279` for `fn_801FD8E0`, `:8311` for `fn_801FEA98`).  `fn_801FEE40` is the
// 36-byte element's copy here as it is there; `fn_801FD638` is the destructor.  The internal call
// edges line up one for one as well: `fn_801FF720` calls `fn_801FF838` at 0x801FF788 and
// `fn_801FF7CC` at 0x801FF79C, `fn_801FF7CC` calls `fn_801FF7EC` at 0x801FF7D8.
//
// **The argument struct is load-bearing, and it is why `fn_801FF838` takes a struct at all.**
// Retail's guard is `lwz r31,0(r3)` for `begin` and `mr r29,r4` for `end`, and the loop condition
// *re-reads* the bound from memory every iteration (`lwz r0,0(r29)` / `cmplw r31,r0` / `bne`,
// 0x801FF874-0x801FF87C).  That is only reachable if both iterators arrive **by value** as
// one-word classes: the callee gets the address of the caller's temporary and reloads through it.
// `fn_801FF720` materialises both of those temporaries on its own stack, one 8-byte slot per
// argument holding the same pointer twice - 0x8/0xc for `end`, 0x10/0x14 for `begin`, with
// `addi r4,r1,0xc` and `addi r3,r1,0x14` (0x801FF760-0x801FF784) the addresses handed over - and
// the `lwz r0,0xc(r29)` reload at 0x801FF778 between them: retail reads `x0c` twice, once per
// temporary, and refuses to common the two loads.
//
// **So this unit is C++ even though retail names none of its functions.**  `SStateIter`'s
// converting constructor is what makes mwcceppc build those two-word temporaries and refuse to
// common the two loads; the sibling records the measurement that plain C with a one-word struct
// passed by value is *not* byte-exact (42 instructions instead of 43, argument slots interleaved,
// and the destroy loop's cursor/bound registers the wrong way round).  The `.cpp` is therefore
// inherited from a measured sibling, not chosen.  The symbols stay unmangled because every
// definition is `extern "C"`; `powerpc-eabi-nm` on the object shows four `T fn_801FF<addr>` and
// nothing else, which is the thing the `.c` convention is protecting.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// **The same four functions repeat again above this range**: at 0x801FF8A0 and 0x801FFA20, with
// the helper triplet also standing alone at 0x801FF94C/0x801FF96C/0x801FF9B8 and
// 0x801FFACC/0x801FFAEC/0x801FFB38.  One template, emitted per script-object type, which is what
// fixes the shapes here.  Only this round is claimed; the rest stay retail's and dtk fills them.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the sibling's claim
// already owns 0x801FF5A0..0x801FF720.
//
// The directory is retail's own, taken from the nearest claimed range: this address is 0x66D0
// bytes into `MetroidPrime/ScriptObjects/CUnknown90.cpp` (0x801F9050..0x801F9190), so the code
// is that unit's neighbourhood.  For an anonymous function that is the only evidence there is,
// and it beats a lane picking the directory it happened to own.

/** `rstl::rmemory_allocator::allocate` and `CMemory::Free`, taken by their mangled names rather
 *  than by including the headers: this object must define exactly the four functions above and
 *  nothing else, and either header would bring an inline copy of the callee in with it.  Both
 *  addresses are retail's own (`symbols.txt`, and the `bl` at 0x801FF750 / 0x801FF7A4). */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FEA98, 0x20 bytes: the 0x24-byte element's copy, called with the destination first
 *  (`mr r3,r30` then `mr r4,r31`, 0x801FF860-0x801FF864). */
extern "C" void fn_801FEA98(void* dst, const void* src);

/** 0x801FD8E0, 0x20 bytes: the same element's destructor, called with the element alone
 *  (`mr r3,r31`, 0x801FF80C). */
extern "C" void fn_801FD8E0(void* elem);

/** The one pointer of the block's element iterator, `current`.  What the bytes need of the class
 *  is exactly this: a struct of one pointer, passed by value, and - see the note above - built
 *  by a converting constructor so that `fn_801FF720`'s two temporaries come out as they do. */
struct SStateIter {
  void* mCur;
  SStateIter(void* cur) : mCur(cur) {}
};

/** The block itself, as far as these four functions read it.  `x04` is the element count and
 *  `x0c` the base pointer, because the loop bound is `x0c + x04 * 0x24`; `x08` is the capacity
 *  `fn_801FF720` compares against with a **signed** `cmpw` and writes back the requested
 *  capacity into.  Nothing reads `x00`, so nothing is asserted about it.  Same three words at
 *  the same offsets as `SGameStateBlock`'s (`include/MetroidPrime/Player/CGameStateBlocks.hpp`),
 *  which is the twin's block; only the element stride differs, 0x24 here and there. */
struct SCarve801FF720Block {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  void* x0c;
};

extern "C" void* fn_801FF838(SStateIter begin, SStateIter end, void* dst);
extern "C" void fn_801FF7EC(char* begin, char* end);
extern "C" void fn_801FF7CC(char* begin, char* end);

/** `uninitialized_copy`: one 0x24-strided walk that copy-constructs each element and returns
 *  the new end, which is what `fn_801FF720` leaves in r3.  The bound is re-read from `end`
 *  every iteration, so it must not be hoisted into a register. */
extern "C" void* fn_801FF838(SStateIter begin, SStateIter end, void* dst) {
  char* in = static_cast< char* >(begin.mCur);
  char* out = static_cast< char* >(dst);
  for (; in != static_cast< char* >(end.mCur); in += 36, out += 36) {
    fn_801FEA98(out, in);
  }
  return out;
}

/** `destroy_impl`: the same 0x24-strided walk with the element destructor instead.  Retail
 *  jumps to the condition first (`b .L_801FF818`) rather than testing on entry. */
extern "C" void fn_801FF7EC(char* begin, char* end) {
  for (char* p = begin; p != end; p += 36) {
    fn_801FD8E0(p);
  }
}

/** The two-argument forwarder.  Both pointers go across in r3/r4 with no shuffling at all,
 *  which is why it takes `begin, end` and not the one pointer `__sys_free`'s shape suggests. */
extern "C" void fn_801FF7CC(char* begin, char* end) { fn_801FF7EC(begin, end); }

/** The block's `reserve`.  `capacity <= x08` returns without allocating; otherwise it allocates
 *  `capacity * 0x24`, moves the live elements across with `fn_801FF838`, destroys the old ones
 *  with `fn_801FF7CC`, frees the old buffer and stores the new buffer and capacity back.  Both
 *  ends are re-read from `self` after the copy instead of being held in locals: keeping them
 *  costs a fifth live register, `stmw r27,28(r1)` and a 0x90-byte frame - the same trade
 *  `fn_801FF5A0` records in its own comment. */
extern "C" void fn_801FF720(SCarve801FF720Block* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08)) {
    return;
  }
  char* const buffer =
      static_cast< char* >(allocate__Q24rstl17rmemory_allocatorFi(capacity * 36));
  fn_801FF838(SStateIter(self->x0c),
              SStateIter(static_cast< char* >(self->x0c) + self->x04 * 36), buffer);
  fn_801FF7CC(static_cast< char* >(self->x0c),
              static_cast< char* >(self->x0c) + self->x04 * 36);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = buffer;
  self->x08 = static_cast< unsigned int >(capacity);
}
