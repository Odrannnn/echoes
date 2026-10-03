// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:10495-10496`, the instructions are
// retail's own, read this run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x80255C54
// --stop-address=0x80255D2C build/G2ME01/main.elf`, and the bodies below are the C those bytes are
// the compilation of.  Before the claim existed dtk emitted them into
// `build/G2ME01/asm/auto_03_80255B28_text.s`, whose `.fn fn_80255C54` / `.fn fn_80255CA8` blocks are
// lines 105-128 and 131-168 of that file.
// `build/G2ME01/asm/WorldFormat/Carve80255C54.s` is this unit's own generated listing, not the
// retail one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x80255C54..0x80255D2C, 0xD8 = 216 bytes, 2 functions:
//
//   fn_80255C54    0x80255C54  0x54   21 instructions
//   fn_80255CA8    0x80255CA8  0x84   33 instructions
//
// **What the two are: one `rstl::vector<T>::~vector()` for a 0x20-byte `T`, and the deleting
// destructor that hands it `-1`.**  Retail names neither, so both shapes are read off **byte-shape
// twins already matched in this tree**, and each comparison was run this session as a `diff` of two
// objdump listings read out of the disc, not recalled:
//
//   fn_80255C54 == fn_800045A0 (0x800045A0, 0x54, `src/MetroidPrime/Carve800045A0.c`, `Matching`):
//                 21 instruction words, and the **only** differences are the two `bl`
//                 displacements - `bl fn_80255CA8` against `bl fn_800045F4`, and `bl
//                 802ce388`/`Free__7CMemoryFPCv` in both.  Even the `bl` **encoding**
//                 `48 00 00 31` is identical, because the callee sits the same 0x14 bytes past
//                 the `bl` in both.
//   fn_80255CA8 == __dt__Q24rstl49vector<12SAreaSurface,Q24rstl17rmemory_allocator>Fv
//                 (0x8005C028, 0x84, `src/MetroidPrime/CGameArea.cpp`): 33 instruction words, and
//                 the only differences are the two `bl` displacements - `Free__7CMemoryFPCv` in
//                 both.  That twin is the `rstl::vector<T>::~vector()` this is: same shape as
//                 `fn_800045F4` in the same unit, with the element stride changed from 0x0C to
//                 **0x20**, and `slwi r0,r0,5` is exactly the shape change that stride 32 = 2^5
//                 produces (a power-of-two stride is a shift; 0x0C is `mulli r0,r0,0xc`).  The
//                 store order the shift form emits is also the twin's, and not `fn_800045F4`'s -
//                 so this file's spelling is measured against the shift-stride twin and not
//                 against the multiply-stride one.
//
// **The `+0x04` / `+0x0C` pair is `rstl::vector`'s own layout** (`include/rstl/vector.hpp:18-21`
// is `mAllocator, mCount, mCapacity, mItems`), and **the stride is the only thing in these bytes
// that fixes `sizeof(T)`**: `slwi r0,r0,5` in the destructor and `addi r4,r4,32` in the loop.  That
// is 32 bytes, which is `CHECK_SIZEOF(SAreaSurface, 0x20)`
// (`include/MetaRender/IRenderer.hpp:38`), so the array is retail's `rstl::vector< SAreaSurface >`
// - but nothing in this claim *names* that type, so the struct below stays anonymous and carries
// only its size, exactly as `MetroidPrime/Carve8000432C.cpp:116-118` does for its own.
//
// **The loop has an empty body and must still exist.**  `b cond / addi r4,r4,32 / cond / bne` with
// nothing between the increment and the test is `rstl::destroy_impl` over a **trivially
// destructible** element (`include/rstl/construct.hpp:100-109`): the loop survives, the per-element
// call inlines to nothing.  Written as a counted loop against `mCount` the compiler deletes it
// entirely; the bound here is an inequality between two pointers (`include/rstl/pointer_iterator.hpp`
// is a struct of one pointer, and `Carve800045A0.c`'s `SIt` is the same shape spelled in C).
//
// **The four stores at +0x08/+0x0C/+0x10/+0x14 are load-bearing** - each of the two pointer values
// is written twice and never read back.  They are the two by-value `pointer_iterator` home slots
// the inliner leaves behind, one copy per parameter of the `destroy`/`destroy_impl` pair, which is
// why `Carve800045A0.c:127-135` spells that pair as two nested `static inline` functions.  Writing
// the walk as a plain pointer loop instead drops all four stores and is 16 bytes short
// (measured on the matched twin `fn_800045F4`, whose header records the same).
//
// **The flag arrives as -1, and both callers are measured.**  `fn_80255C54`'s only `bl` in the DOL
// is at 0x802558A0, inside `fn_80255878` (0x80255878, 0x88, unclaimed and left to dtk), which
// passes `addi r3,r30,0x40 / li r4,-1`: it is the first of five member teardowns at
// +0x40/+0x30/+0x20/+0x10/+0x00 and the -1 is MWCC's "destroy, do not free me afterwards".
// `fn_80255CA8`'s other caller is 0x80256C14, inside `fn_80256BD8` (unclaimed), which destroys a
// 0x10-byte stack temporary the same way.  The flag is a **`short`**: retail's tail is
// `extsh. r0,r31 / ble`, which an `int` parameter would make `cmpwi r31,0`.
//
// **Both callees are real, not stand-ins.**  `fn_80255CA8` is defined here, in this object.
// `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) is claimed by
// `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it;
// declared, never defined here.  Measured: `grep` for `fn_80255C54` and `fn_80255CA8` over `src/`
// and `include/` returns nothing outside this file, so no `src/MetroidPrime/PortLinkStubs.cpp`
// duplicate has to be deleted for this carve.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that; `python3 tools/check_decl_order.py --unit
// main/WorldFormat/Carve80255C54` is the cheap check.  Here that is `fn_80255CA8` first, then
// `fn_80255C54`.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol split`
// fails with "Cyclic dependency ... link order"), and because neither neighbour is part of this
// chain.  Both boundaries are function edges, so nothing is split here, and the claim does not span
// an unclaimed gap: it is exactly `fn_80255C54` (0x54) followed by `fn_80255CA8` (0x84), and
// 0x80255C54 + 0xD8 = 0x80255D2C, the address of the next `symbols.txt` function.  The claim starts
// 0x12C bytes **after** the unit below ends (`WorldFormat/Carve80255A0C.c`, 0x80255A0C..0x80255B28)
// and ends 0x1440 bytes **before** the one above (`WorldFormat/Carve80256D1C.c`, 0x80256D1C), which
// is what keeps `dtk dol split` from reporting a link-order cycle against either neighbour.  The
// functions left in the gap above the claim - `fn_80255D2C` (0x7C) and the weak
// `rstl::vector<short>::~vector()` at 0x80255C00 - are not carved here.
//
// The directory is retail's own, taken from the nearest claimed range: the claim below is
// `WorldFormat/Carve80255A0C.c` (0x80255A0C..0x80255B28) and the one above is
// `WorldFormat/Carve80256D1C.c` (0x80256D1C..0x80256D64), so this address sits in that unit's
// neighbourhood.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** The 0x20-byte element `fn_80255CA8` walks in strides of 32.  Nothing here reads a field of it -
 *  its destructor is trivial, which is why the walk has an empty body - so the struct carries
 *  only the size the stride fixes.  `SAreaSurface` is retail's name for a type of that size
 *  (`CHECK_SIZEOF(SAreaSurface, 0x20)`, `include/MetaRender/IRenderer.hpp:38`), and nothing in
 *  this claim names it, so it is not spelled here. */
struct SCarve80255C54Elem {
  unsigned int x00[8];
};

/** The array header `fn_80255CA8` reads: the count at +0x04 and the item buffer at +0x0C, with the
 *  words between them left as they are.  `rstl::vector`'s own layout is
 *  `include/rstl/vector.hpp:18-21`.  Declared **above** the prototypes below, not below them: a
 *  `struct` named inside a parameter list is scoped to that list, and the host build then rejects
 *  the definition as a conflicting type.  mwcceppc only warns, so the matching build is green while
 *  `tools/probe_sources.sh` is not. */
struct SCarve80255C54Vec {
  int mAllocator;
  int mCount;
  int mCapacity;
  struct SCarve80255C54Elem* mItems;
};

/** One `rstl::pointer_iterator`, which is what the by-value parameter list of the `destroy` pair
 *  is made of, and what the two home-slot stores per pointer are. */
struct SCarve80255C54It {
  struct SCarve80255C54Elem* current;
};

/** `rstl::destroy` of an element with a trivial destructor: nothing to call, so nothing is called.
 *  `static inline` because retail's object carries only the two functions it names. */
static inline void DestroyElem(struct SCarve80255C54Elem* p) {}

/** `rstl::destroy_impl`, `include/rstl/construct.hpp:100-109`: the loop that survives with an
 *  empty body because the bound is an inequality between two pointers. */
static inline void DestroyImpl(struct SCarve80255C54It begin, struct SCarve80255C54It end) {
  struct SCarve80255C54Elem* cur = begin.current;
  struct SCarve80255C54Elem* last = end.current;
  for (; cur != last; ++cur) {
    DestroyElem(cur);
  }
}

/** `rstl::destroy`, `include/rstl/construct.hpp:111-114`: its whole body is the `destroy_impl`
 *  call, and the two nested inlines are what leave four home-slot stores behind. */
static inline void Destroy(struct SCarve80255C54It begin, struct SCarve80255C54It end) {
  DestroyImpl(begin, end);
}

void* fn_80255CA8(struct SCarve80255C54Vec* self, short flag);
void* fn_80255C54(void* self, short flag);

void* fn_80255CA8(struct SCarve80255C54Vec* self, short flag) {
  if (self) {
    struct SCarve80255C54It begin;
    struct SCarve80255C54It end;
    begin.current = self->mItems;
    end.current = self->mItems + self->mCount;
    Destroy(begin, end);
    Free__7CMemoryFPCv(self->mItems);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80255C54(void* self, short flag) {
  if (self) {
    fn_80255CA8((struct SCarve80255C54Vec*)self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}