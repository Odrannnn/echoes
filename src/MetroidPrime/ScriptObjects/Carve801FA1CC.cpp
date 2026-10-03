// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt` (lines 8200-8201), the instructions are retail's
// own, still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FA1CC --stop-address=0x801FA2F4
// build/G2ME01/main.elf`, and the body below is the C++ those bytes are the compilation of.
// Before the claim existed dtk emitted them into `build/G2ME01/asm/auto_03_801F9848_text.s`,
// which is the listing that unit still carries for the 0x801F9848..0x801FA1CC part of it;
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FA1CC.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x801FA1CC..0x801FA2F4, 0x128 = 296 bytes, 2 functions:
//
//   fn_801FA1CC    0x801FA1CC  0xC0   48 instructions
//   fn_801FA28C    0x801FA28C  0x68   26 instructions
//
// **What the two are: one 0x40-byte-element `rstl::vector`'s own `reserve`, and the
// `uninitialized_copy` it moves the live elements with.**  Retail names neither, so both are read
// off the call edges and the argument registers - and each is a byte-for-byte twin of a symbol
// retail *does* name, elsewhere in the same DOL and already `Matching` in this tree:
//   reserve          twin of `reserve__Q24rstl50vector<13CInt32POINode,
//                      Q24rstl17rmemory_allocator>Fi`, `symbols.txt:11579`, 0x8029224C, 0xC0,
//                      `src/Kyoto/Animation/CAnimTreeSequence.cpp`
//   uninitialized_copy twin of `uninitialized_copy<Q24rstl120pointer_iterator<13CInt32POINode,
//                      ...>,P13CInt32POINode>__4rstlF...`, `symbols.txt:11580`, 0x8029230C, 0x68,
//                      same unit
// Measured this run by disassembling both 0x128-byte runs of `build/G2ME01/main.elf` and
// comparing the decoded words: **71 of the 74 words are identical**, and the three that differ are
// exactly the three `bl` words - `allocate__Q24rstl17rmemory_allocatorFi` at index 10, `Free__7CMemoryFPCv`
// at index 40 and the element's `construct` at index 60.  No data address differs, and the frame,
// the register moves, the loop bodies, the branch displacements and the epilogues are all the same
// words.  The `bl` to the copy helper at index 24 is byte-identical to the twin's own because the
// two callees sit at the same relative offset (+0xC0) inside their respective runs.
//
// **The vector's layout, and it is `rstl::vector`, not a guess.**  `fn_801FA1CC` reads `+0x8` for
// the growth test (`lwz r0,8(r3)` / `cmpw r28,r0` / `ble`, 0x801FA1E4-0x801FA1EC), `+0x4` for the
// live count (`lwz r0,4(r27)`, 0x801FA1F8 and again 0x801FA230) and `+0xC` for the base pointer
// (`lwz r4,12(r27)`, 0x801FA200), and writes `+0xC` and `+0x8` back at the end (0x801FA270-0x801FA274).
// That is `rstl::vector<T, Alloc>` at `include/rstl/vector.hpp:16-21` - `mAllocator`, `mCount`,
// `mCapacity`, `mItems` - and `Carve801F97C8.c`'s own header records the same four offsets for the
// `push_back_unsafe` of this very vector at 0x801F97C8, including that its `reserve` is this
// function.  `x00` is read by nothing here, so nothing is asserted about it beyond it being the
// allocator word the vector puts first.
//
// **The element is 0x40 = 64 bytes and is polymorphic**, both measured off this claim: the two
// `slwi ...,6` at 0x801FA1F0 and 0x801FA208 are `count * 64`, and the destroy loop steps the cursor
// by 64 (`addi r30,r30,64`, 0x801FA25C).  The element has a vtable because it is destroyed through
// one: `lwz r12,0x0(e)` / `lwz r12,0x8(r12)` / `mtctr r12` / `bctrl` with `r4 = -1`
// (0x801FA24C-0x801FA258) is the **deleting-destructor slot** - slot 1 of the vtable, and the `-1`
// is mwcc's own complete-object flag - which is what an explicit `p->~T()` compiles to for a class
// with a virtual destructor.  `Carve801F97C8.c` measured the same 0x40 stride and the same
// polymorphic owner from the other end.
//
// **So this unit is C++ even though retail names none of its functions.**  The deleting-destructor
// call needs a class type with a virtual destructor; written as an indirect call through a loaded
// vtable slot it does not come out in retail's shape.  The symbols stay unmangled because every
// definition is `extern "C"`; `powerpc-eabi-nm` on the object shows two `T fn_801FA<addr>` and
// three `U` (the three callees) and nothing else, which is the thing the plain-`.c` rule protects.
// `ScriptObjects/Carve801FF5A0.cpp` in this same directory is `extern "C"` for the same reason, and
// is this function's 0x24-stride twin.
//
// **The two iterator arguments are by value, and that is what the caller's stack shows.**
// `fn_801FA28C`'s own guard is `lwz r31,0(r3)` for `begin` and `mr r29,r4` for `end`, and the loop
// condition *re-reads* the bound from memory every iteration (`lwz r0,0(r29)` / `cmplw r31,r0` /
// `bne`, 0x801FA2C8-0x801FA2D0).  That is only reachable if both arrive **by value** as one-word
// classes: the callee gets the address of the caller's temporary and reloads through it.
// `fn_801FA1CC` materialises both on its own stack, one 8-byte slot per argument holding the same
// pointer twice - 0x8/0xc for `end`, 0x10/0x14 for `begin` - with `addi r4,r1,0xc` and
// `addi r3,r1,0x14` (0x801FA214 and 0x801FA204) the addresses handed over, and the `lwz r0,12(r27)`
// reload at 0x801FA21C between the stores: retail reads `mItems` once per temporary.  `SCarve801FF5A0Block`'s
// header records the identical eight stores on the identical point.
//
// **The destroy loop has to come from an inlined helper, and that is measured, not style.**  With
// the loop written inline in `fn_801FA1CC` the body is the right length and the right arithmetic and
// still differs in **five** words, all of it one thing: mwcceppc puts the new buffer in `r31` and
// the loop bound in `r29`, where retail has them the other way round (`mr r29,r3` / `mr r5,r29` /
// `add r31,r30,r0` / `cmplw r30,r31` / `stw r29,12(r27)` against `mr r31,r3` / `mr r5,r31` /
// `add r29,r30,r0` / `cmplw r30,r29` / `stw r31,12(r27)`).  Routing it through `sDestroy` below -
// retail's own `rstl::destroy`/`destroy_impl` shape at `include/rstl/construct.hpp:100-114` - is
// what fixes it, and it is byte-exact either way in the second function.  Six other spellings of the
// loop (raw `char*` cursors, `SElem*` cursor, `do`/`while`, a redundant `items` local, `void*` and
// `SElem*` new-buffer locals) were all measured and all still swap those two registers.
//
// **Its callees are all real, not stand-ins.**  `fn_801F9800` (0x801F9800, 0x20, `symbols.txt:8191`)
// is `rstl::construct` for this same element, defined by
// `src/MetroidPrime/ScriptObjects/Carve801F97C8.c`, a `Matching` unit, so the `bl` at 0x801FA2BC
// lands on a definition written in retail's bytes.  `allocate__Q24rstl17rmemory_allocatorFi`
// (0x802FDAB8, `symbols.txt:13824`) and `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) are
// declared here by their retail names and never defined, so this object carries neither an inline
// copy of the allocator nor a vtable.  Nothing in `src/MetroidPrime/PortLinkStubs.cpp` stands in
// for any of the three, and none of this file's two symbols needs a stand-in either.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL, with objdiff, with
// `tools/unit_fit.sh` and with the link all still green.  Only `tools/flip_test.sh` catches it, and
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/Carve801FA1CC` checks it
// without building.
//
// Its own unit, and nothing is split out of a neighbour to make it: `fn_801FA1CC` ends exactly
// where `fn_801FA28C` begins (0x801FA1CC + 0xC0 = 0x801FA28C) and `fn_801FA28C` ends exactly where
// retail's next function begins (0x801FA28C + 0x68 = 0x801FA2F4 = `fn_801FA2F4`, `symbols.txt:8202`,
// 0xD8, still unclaimed and left to dtk).  In front of the claim the nearest claimed range is
// `ScriptObjects/Carve801F97C8.c` (0x801F97C8..0x801F9848) and behind it
// `ScriptObjects/Carve801FBC58.c` (0x801FBC58..0x801FBD68), so the code is the
// `MetroidPrime/ScriptObjects/` neighbourhood - which is also where the vector's own
// `push_back_unsafe` at 0x801F97C8 is claimed.

/** `rstl::rmemory_allocator::allocate` and `CMemory::Free`, taken by their retail symbols rather
 *  than by including the headers: this object must define exactly the two functions above and
 *  nothing else, and either header would bring an inline copy of a callee in with it. */
extern "C" void* allocate__Q24rstl17rmemory_allocatorFi(int size);
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** `fn_801F9800` - retail `.text:0x801F9800`, 0x20 = 32 bytes: `rstl::construct` for the 0x40-byte
 *  element, defined for real by `src/MetroidPrime/ScriptObjects/Carve801F97C8.c` (a `Matching`
 *  unit), so this is a call into a body written in retail's bytes and not into a stand-in. */
extern "C" void fn_801F9800(void* dst, const void* src);

/** The one pointer of `rstl::pointer_iterator`, `current`.  What these bytes need of the class is
 *  exactly this: a struct of one pointer, passed by value, and - see the note above - built by a
 *  converting constructor so that `fn_801FA1CC`'s two temporaries come out as they do. */
struct SCarve801FA1CCIter {
  void* mCur;
  SCarve801FA1CCIter(void* cur) : mCur(cur) {}
};

/** The element: 0x40 bytes and polymorphic, and nothing else is asserted about it.  No field of it
 *  is read or written in either body - the copy goes through `fn_801F9800` and the destroy through
 *  the vtable - so the padding stands for whatever retail's class actually holds, and `sizeof`
 *  exists only to give the 64-byte stride `slwi ...,6` and `addi r30,r30,64` measure. */
struct SCarve801FA1CCElem {
  virtual ~SCarve801FA1CCElem();
  unsigned char x04_pad[0x3C];
};

/** The vector, as far as these two functions read it.  `x04` is the element count and `x0c` the base
 *  pointer, because the copy bound is `x0c + x04 * 0x40`; `x08` is the capacity `fn_801FA1CC`
 *  compares against with a **signed** `cmpw` and writes the requested capacity back into.  Same
 *  four words at the same offsets as `rstl::vector`'s (`include/rstl/vector.hpp:16-21`), which is
 *  the twin's class. */
struct SCarve801FA1CCVector {
  unsigned int x00;
  unsigned int x04;
  unsigned int x08;
  SCarve801FA1CCElem* x0c;
};

/** `rstl::destroy_impl(begin, end)` for this element, written as the inline helper retail's
 *  `destroy` inlines (`include/rstl/construct.hpp:100-114`).  It is not decoration: hoisting the loop
 *  into `fn_801FA1CC` itself is what costs the buffer/bound register swap documented above.  No
 *  `is_trivially_destructible` test here because the element is polymorphic - it cannot be
 *  trivially destructible - and retail emits no such test. */
static inline void sDestroy(SCarve801FA1CCElem* begin, SCarve801FA1CCElem* end) {
  for (SCarve801FA1CCElem* cur = begin; cur != end; ++cur) {
    cur->~SCarve801FA1CCElem();
  }
}

extern "C" void* fn_801FA28C(SCarve801FA1CCIter begin, SCarve801FA1CCIter end, void* dst);
extern "C" void fn_801FA1CC(SCarve801FA1CCVector* self, int newCapacity);

/** `rstl::uninitialized_copy(begin, end, out)`: one 0x40-strided walk that copy-constructs each
 *  element and returns the new end, which is what `fn_801FA1CC` leaves behind in r3.  The bound is
 *  re-read from `end` every iteration, so it must not be hoisted into a register. */
extern "C" void* fn_801FA28C(SCarve801FA1CCIter begin, SCarve801FA1CCIter end, void* dst) {
  char* in = (char*)begin.mCur;
  char* out = (char*)dst;
  for (; in != (char*)end.mCur; in += 64, out += 64) {
    fn_801F9800(out, in);
  }
  return out;
}

/** The vector's `reserve`.  `newCapacity <= x08` returns without allocating; otherwise it allocates
 *  `newCapacity * 0x40`, moves the live elements across with `fn_801FA28C`, destroys the old ones
 *  through their vtable, frees the old buffer, and stores the new buffer and capacity back.  Both
 *  ends are re-read from `self` after the copy instead of being held in locals: keeping them costs a
 *  fifth live register and a longer frame, which is the same trade `fn_801FF5A0`'s own header
 *  records.  `x04` is left alone - retail does not write it here either. */
extern "C" void fn_801FA1CC(SCarve801FA1CCVector* self, int newCapacity) {
  if (newCapacity <= (int)self->x08) {
    return;
  }
  char* const buffer = (char*)allocate__Q24rstl17rmemory_allocatorFi(newCapacity * 64);
  fn_801FA28C(SCarve801FA1CCIter(self->x0c),
              SCarve801FA1CCIter((char*)self->x0c + self->x04 * 64), buffer);
  sDestroy(self->x0c, self->x0c + self->x04);
  Free__7CMemoryFPCv(self->x0c);
  self->x0c = (SCarve801FA1CCElem*)buffer;
  self->x08 = (unsigned int)newCapacity;
}