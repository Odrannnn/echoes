// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:3914-3917`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_800DFA60_text.s:2035-2114`, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x800E1548..0x800E163C, 0xF4 = 244 bytes, 4 functions:
//
//   fn_800E1548    0x800E1548  0x50    20 instructions
//   fn_800E1598    0x800E1598  0x60    24 instructions
//   fn_800E15F8    0x800E15F8  0x20     8 instructions
//   fn_800E1618    0x800E1618  0x24     9 instructions
//
// **What the four are: one deleting-destructor chain, each step calling the next.**  Retail
// names none of them, so this is read off the call edges and the argument registers:
//
//   fn_800E1618  takes a pointer in r3, materialises `li r4,-1` and calls fn_800E0B80.  The -1
//                is the "do not free me afterwards" flag of MWCC's deleting-destructor calling
//                convention, so this is `rstl::destroy_impl<T>(T*)`.
//   fn_800E15F8  takes a pointer in r3 and forwards it unchanged: that is the shape of
//                `rstl::destroy<T>(T*)` and of `__sys_free`, and here it is the former,
//                because the callee takes the -1.
//   fn_800E1598  is the only one of the four that reads the object.  `lwz r0,0(r29)` is a count
//                and `addi r31,r29,4` is the first element, and the walk from element to
//                element is in strides of 8 with `rstl::destroy` called on each: that is
//                `rstl::reserved_vector<T, N>::destroy_elements()` (see the identical loop in
//                `include/rstl/reserved_vector.hpp:97-105`), whose `int mCount` is the word at
//                +0 and whose inline `uchar mData[N * sizeof(T)]` is the array at +4.
//   fn_800E1548  is that convention's other half: `mr. r30,r3 / beq` guards the receiver, r4 is
//                kept in r31 and re-tested with `extsh.`, the member teardown runs, and only a
//                positive flag reaches `CMemory::Free`.  So it is `operator delete`-by-
//                destructor: destroy the members, then free the object.
//
// The eight-byte element is what fixes `T`'s size: the stride at 0x800E15C8 is `addi r31,r31,8`
// and nothing else in the function touches it, so the vector's `T` is 8 bytes.  It is not named
// here - retail has no name for it - and `SCarve800E1548` below is only the two words the bytes
// read.
//
// **The `mr r3,r30` at 0x800E157C is the return value**, not a leftover: it sits *after* both
// early exits and *before* the frame is torn down, so all four paths return the receiver.
// `void*` plus `return self` is what reproduces it (`Carve80255A0C.c:134-141` is the same shape
// at the same three places).
//
// The `li r4,-1` in fn_800E1618 is where the *chain* starts, and `bl fn_800E0B80` at 0x800E0AB4
// is its second caller, so fn_800E0B80 is the out-of-line destructor this -1 flag belongs to.
// It is declared, never defined here: it is in the same unclaimed `auto_*` range and no unit
// claims it.
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
// split` fails with "Cyclic dependency ... link order"), and because neither neighbour belongs
// to this chain: `fn_800E142C` (0x11C = 284 bytes, the caller of fn_800E1548 at 0x800E1464)
// ends the run beneath, and `fn_800E163C` (0x58 bytes,
// `lwz r3,0(r30) / li r4,1 / bl fn_800E131C`) starts the run above and is a *different*
// destructor - it passes `1`, not `-1`, to its own callee, so it is not a member of this chain.
//
// The directory is retail's own, taken from the nearest claimed range: the claim immediately
// below is `MetroidPrime/CSimpleShadow.cpp` (`.text` 0x800DF268..0x800DFA60) and the one above
// is `MetroidPrime/CWorldShadow.cpp` (`.text` 0x800E17E4..0x800E24D0), so this address sits in
// that unit's neighbourhood.  For an anonymous function that is the only evidence there is, and
// it beats a lane picking the directory it happened to own.  The claim starts at 0x800E1548
// rather than at the `auto_*` unit's own start of 0x800DFA60, which is what keeps `dtk dol
// split` from reporting a link-order cycle against `CSimpleShadow.cpp`.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  **Not
 *  claimed by any unit**, so dtk's own `auto_*` object supplies the bytes in the DOL link and
 *  the one `bl` below resolves to retail's address.  Declared, never defined here.
 *  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it under that name for the host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x800E0B80, `symbols.txt:3896`, size 0x64: the out-of-line destructor the -1 flag is for.
 *  Unnamed in retail and in the same unclaimed range; declared, never defined here. */
extern void fn_800E0B80(void* ptr, int flag);

/** The two words `fn_800E1598` reads: `mCount` at +0 is `rstl::reserved_vector`'s own first
 *  member and `mData` at +4 is its inline element array, so the element type is 8 bytes.  The
 *  array is declared one element long because the bound comes from `mCount`, not from a
 *  compile-time size - the real one is `N * 8` and `N` is not in these bytes.  Declared **above**
 *  the prototypes below, not below them: a `struct` named inside a parameter list is scoped to
 *  that list, and the host build then rejects the definition as a conflicting type. */
struct SCarve800E1548 {
  int mCount;
  unsigned char mData[8];
};

void fn_800E1618(void* ptr);
void fn_800E15F8(void* ptr);
void fn_800E1598(struct SCarve800E1548* self);
void* fn_800E1548(void* self, short flag);

void fn_800E1618(void* ptr) { fn_800E0B80(ptr, -1); }

void fn_800E15F8(void* ptr) { fn_800E1618(ptr); }

void fn_800E1598(struct SCarve800E1548* self) {
  unsigned char* ptr = self->mData;
  int i;
  for (i = 0; i < self->mCount; ++i) {
    fn_800E15F8(ptr);
    ptr += 8;
  }
}

void* fn_800E1548(void* self, short flag) {
  if (self) {
    fn_800E1598((struct SCarve800E1548*)self);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
