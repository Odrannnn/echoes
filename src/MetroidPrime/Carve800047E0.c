// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:83-84`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_80004798_text.s:32-86`, and the bodies below are the C
// those bytes are the compilation of.
//
// .text 0x800047E0..0x8000489C, 0xBC = 188 bytes, 2 functions:
//
//   fn_80004864    0x80004864  0x38    14 instructions
//   fn_800047E0    0x800047E0  0x84    33 instructions
//
// **What the two are: an `rstl::vector` deleting destructor for a 12-byte element and the
// `rstl::destroy` it forwards to.**  Retail names neither of them, so this is read off the call
// edges and the argument registers; both bodies are byte-shape twins of functions already at
// 100.00% in this tree (measured instruction by instruction, not by percentage alone):
//
//   fn_800047E0  `mr. r30,r3 / beq` guards the receiver, r4 is kept in r31 and re-tested with
//                `extsh.`, `lwz r0,4(r30)` is the count and `lwz r5,0xc(r30)` the item array
//                (`include/rstl/vector.hpp:18-21` is `mAllocator, mCount, mCapacity, mItems`),
//                `mulli r0,r0,0xc` is the element stride, the four stores at `r1+0x08..0x14` are
//                the by-value iterator home slots, then `Free__7CMemoryFPCv(mItems)` and, only for
//                a positive flag, `Free__7CMemoryFPCv(self)` - MWCC's deleting-destructor
//                convention, `mr r3,r30` in the epilogue being the return of the receiver.
//                **Exact twin:** `__dt__Q24rstl82vector<Q24rstl38pair<Ui,Q24rstl20rc_ptr<
//                10IMetaTrans>>,Q24rstl17rmemory_allocator>Fv` (0x80030E08, 0x84 = 132 bytes,
//                100.00% in `main/MetroidPrime/Factories/CCharacterFactory`), these 33
//                instructions word for word - including `mulli r0,r0,0xc`, the same 12-byte stride -
//                with only its `bl` target different.
//   fn_80004864  `lwz r5,0(r4) / addi r4,r1,0x8 / lwz r0,0(r3) / addi r3,r1,0xc` then the two
//                `stw`: it copies the two iterators onto its own frame and forwards them, which
//                is `include/rstl/construct.hpp:111-114` - `destroy(It begin, It end)` whose whole
//                body is `destroy_impl(begin, end)`.  **Exact twin:** `fn_800043B0` (0x800043B0,
//                0x38) in `MetroidPrime/Carve8000432C.cpp` (`Matching`, 3/3), these 14
//                instructions with a different `bl` target.
//
// **The element is 12 bytes and the callee `fn_8000489C` is this copy's own `destroy_impl`.**
// 0xC is fixed twice, by `mulli r0,r0,0xc` in the destructor and by `addi r31,r31,0xc` in the
// callee's loop (`build/G2ME01/asm/auto_03_80004798_text.s:106`), and neither is a shift.
// `fn_8000489C` (0x8000489C, 0x60 = 96 bytes, `symbols.txt:85`) walks `first` to `last` calling
// `__dt__6CTokenFv` (0x8030154C, `symbols.txt:13922`) on each element's first word behind a
// `cmplwi r31,0` null test - the same walk `src/MetroidPrime/main.cpp:1237-1241` already
// documents from the other side, for `fn_800068F4`'s clear of `CGameState::x1f4`.  That is the
// reading which makes the three pieces agree: **this carve's `fn_800047E0` is that same
// block's destructor.**  Its one caller is 0x800041D0 in `__dt__10CGameStateFv` (0x8000419C,
// 0x190, `auto_03_8000408C_text.s:107-109`), `addi r3,r30,0x1f4 / li r4,-1` - 0x1F4 is
// `CGameState::x1f4` (`include/MetroidPrime/Player/CGameState.hpp:307`), the block `fn_800068F4`
// zeroes at `stw r0,504(r31)` (`main.cpp:1248`) and the one `src/MetroidPrime/PortBoot.cpp:473`
// names in a `PORT_FRAME_STOP`.  `fn_80004864`'s other caller is that `fn_800068F4`
// (`MetroidPrime/main.s:1593`), so the two copies of the walk are 0x80006934 and 0x80004888.
//
// **`fn_8000489C` is the one call this carve cannot own and does not drop.**  It is 0xC0 bytes
// above the claim and is claimed by nobody, so the `bl` resolves against dtk's own
// `auto_03_80004798_text.o`.  `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`, size 0x64)
// is claimed by `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), and
// `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for the host.  Both are declared, never defined
// here, so this object carries two functions and nothing else.
//
// **The four stores at `r1+0x08/0x0C/0x10/0x14` are load-bearing, and the `const` in `end` is
// load-bearing too.**  The stores are the by-value `SIt` home slots: `fn_80004864` takes its pair
// by value, and MWCC passes a by-value class argument as a pointer to a caller-side temporary, so
// the call needs a temporary per `begin()`/`end()` result *and* a second one per parameter.  That
// is also why the argument registers are `addi r3,r1,0x14` and `addi r4,r1,0x0C`: the second
// pair.  Naming the iterators in locals and passing them lands the parameter copies at +0x08 and
// +0x0C instead, which is the same 132 bytes with a different frame (measured on the twin in
// `src/MetroidPrime/Carve8000432C.cpp`, whose header records both spellings and their scores).
// Separately, retail reads the buffer twice (`lwz r5,0xc(r30)` at 0x80004808 and
// `lwz r0,0xc(r30)` at 0x8000481C), which is what the `SElem800047E0* const last` local in
// `End800047E0` produces; drop it and the two reads collapse into one and the function is 4 bytes
// short.  So the destructor call is written as the call, the way
// `include/rstl/vector.hpp:139-142` writes it, and not through named locals.
//
// The iterators are a **local 4-byte struct**, not `rstl::pointer_iterator`: it has no out-of-line
// members, so the object carries two functions and nothing else, and naming them keeps both bodies
// reading as the `rstl` ones they are.
//
// Retail names none of this.  `symbols.txt:83-84` carries the `fn_<addr>` placeholders and this
// file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>...` and objdiff would pair nothing.  That is also why the unit is a
// `.c` rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `tools/check_decl_order.py --unit
// main/MetroidPrime/Carve800047E0` is the cheap check.
//
// Its own unit because a claim may not span an unclaimed gap and may not sit where a neighbouring
// `Matching` unit's `.text` ends: below is `MetroidPrime/Carve80004744.c` (0x80004744..0x80004798),
// then dtk keeps 0x80004798..0x800047E0 for the real-named `__dt__9CGameModeFv`
// (`symbols.txt:82`, 0x48); above, 0x8000489C.. is `fn_8000489C`, unclaimed and dtk's.  The
// directory is retail's own, taken from those neighbours: this address sits in the
// `MetroidPrime/` neighbourhood.  The claim starts at 0x800047E0 and not at the `auto_*` unit's own
// 0x80004798, which is what keeps `dtk dol split` from reporting a link-order cycle.

/** 0x802CE388, `symbols.txt:12992`, size 0x64: retail's `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:39` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** The 12-byte element, as far as these two bodies read it: nothing here names it and retail has
 *  no name for it either, so it is the anonymous 12-byte type whose size `mulli r0,r0,0xc` and
 *  `addi r31,r31,0xc` fix.  Only its size is modelled - the one field the callee
 *  (`fn_8000489C`, not ours) touches is the `CToken` at +0, and this carve never reads it. */
struct SElem800047E0 {
  int x00;
  int x04;
  int x08;
};

/** One 4-byte `rstl::pointer_iterator` (`include/rstl/pointer_iterator.hpp:58` is its single
 *  `T* current`), named so the by-value parameter list of the `destroy` call - which is what emits
 *  the four home-slot stores - can be written in C. */
struct SIt800047E0 {
  struct SElem800047E0* current;
};

/** 0x8000489C, `symbols.txt:85`, size 0x60: `rstl::destroy_impl` over the range - the 96-byte
 *  loop that calls `__dt__6CTokenFv` per 12-byte element.  **Unclaimed**, still in dtk's
 *  `auto_03_80004798_text.o`, so declared, never defined here. */
extern void fn_8000489C(struct SIt800047E0 begin, struct SIt800047E0 end);

/** The array header as far as these bytes read it: `+0x04` is the count (`mulli r0,r0,0xc` and the
 *  call's bound) and `+0x0C` the item array, which is `rstl::vector`'s `mItems` after its
 *  `mAllocator`/`mCount`/`mCapacity` (`include/rstl/vector.hpp:18-21`).  `+0x00` and `+0x08` are
 *  never touched here, so they are left as the two words between them. */
struct SVec800047E0 {
  int x00;
  int mCount;
  int x08;
  struct SElem800047E0* mItems;
};

/** `include/rstl/vector.hpp:27`'s `begin()`, verbatim in effect: the owner is passed and dropped,
 *  so it only supplies the constructor shape. */
static inline struct SIt800047E0 Begin800047E0(struct SVec800047E0* self) {
  struct SIt800047E0 it;
  it.current = self->mItems;
  return it;
}

/** `include/rstl/vector.hpp:29-32`'s `end()`, and the `T* const end` local inside it is what makes
 *  retail read the buffer twice (0x80004808 and 0x8000481C).  See the header. */
static inline struct SIt800047E0 End800047E0(struct SVec800047E0* self) {
  struct SElem800047E0* const last = self->mItems + self->mCount;
  struct SIt800047E0 it;
  it.current = last;
  return it;
}

void fn_80004864(struct SIt800047E0 begin, struct SIt800047E0 end);
void* fn_800047E0(struct SVec800047E0* self, short flag);

/** `fn_80004864` - retail `.text:0x80004864`, 0x38 = 56 bytes: `rstl::destroy(begin, end)` over
 *  the range, whose whole body is the `destroy_impl` call above. */
void fn_80004864(struct SIt800047E0 begin, struct SIt800047E0 end) { fn_8000489C(begin, end); }

/** `fn_800047E0` - retail `.text:0x800047E0`, 0x84 = 132 bytes: the array's deleting destructor.
 *  The call is written as the call and not through named locals; see the header for the
 *  measurement behind that.  The flag is a **`short`**: retail's tail is
 *  `extsh. r0,r31 / ble`, which an `int` would make `cmpwi r31,0`. */
void* fn_800047E0(struct SVec800047E0* self, short flag) {
  if (self) {
    fn_80004864((struct SIt800047E0){self->mItems}, (struct SIt800047E0){self->mItems + self->mCount});
    Free__7CMemoryFPCv(self->mItems);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}