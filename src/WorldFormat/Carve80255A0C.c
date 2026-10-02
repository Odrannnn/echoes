// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones dtk
// emitted into `build/G2ME01/asm/auto_03_80255128_text.s` before the claim existed (lines
// 700-789) and are still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x80255A0C --stop-address=0x80255B28
// build/G2ME01/main.elf`, and the body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/WorldFormat/Carve80255A0C.s` is this unit's own generated listing, not the
// retail one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x80255A0C..0x80255B28, 0x11C = 284 bytes, 4 functions:
//
//   fn_80255A0C    0x80255A0C  0x20    8 instructions
//   fn_80255A2C    0x80255A2C  0x24    9 instructions
//   fn_80255A50    0x80255A50  0x54   21 instructions
//   fn_80255AA4    0x80255AA4  0x84   33 instructions
//
// **What the four are: one free chain, each step calling the next.**  Retail names none of
// them, so this is read off the call edges and the argument registers, not off a name:
//
//   fn_80255A0C  takes a pointer in r3 and does nothing but forward it to fn_80255A2C.
//                That is the shape of `__sys_free`.
//   fn_80255A2C  takes a pointer in r3, materialises `li r4,-1` and calls fn_80255A50.
//                That is `rstl::destroy_impl<T>(T*)`: the -1 is the "do not free me
//                afterwards" flag of MWCC's deleting-destructor calling convention.
//   fn_80255A50  is that convention's other half.  `mr. r30,r3 / beq` guards the receiver,
//                r4 is kept in r31 and re-tested with `extsh.`, the member teardown runs with
//                -1, and only a positive flag reaches `CMemory::Free`.  That is
//                `operator delete`-by-destructor: destroy the members, then free the object.
//   fn_80255AA4  is the member teardown, and the only one of the four that reads the object:
//                `lwz r0,4(r30)` is a count, `lwz r3,0xc(r30)` is a block, `mulli r0,r0,0x14`
//                makes the end, and the walk from block to end in strides of 0x14 has an
//                **empty body** - the elements are 0x14 = 20 bytes and their destructor is
//                trivial, so `rstl::destroy_impl` has nothing to call per element but keeps its
//                loop.  Then `CMemory::Free(block)`, then `CMemory::Free(self)` behind the same
//                positive-flag test.
//
// The +0x4 / +0xC / +0x14 triple is `rstl::vector<T>`'s own layout once the empty allocator
// takes the word at +0 (`include/rstl/vector.hpp`: `mAllocator`, `mCount`, `mCapacity`,
// `mItems`), so fn_80255AA4 is a destructor whose only member teardown is that vector, and
// fn_80255A50 is the `operator delete` that calls it.  The class is not named in retail and is
// not guessed here; `SCarve80255A0COwner` below is only the four words the bytes read.
//
// **The four stores at +0x08/+0x0C/+0x10/+0x14 are load-bearing, and they are what
// `volatile` is for.**  Retail writes each of the two values twice - `stw r3,0x14`,
// `stw r3,0x8`, `stw r0,0x10`, `stw r0,0xc` - and reads none of them back.  In C++ those are
// the copies `rstl::destroy(begin(), end())` leaves behind; in C the only way measured to keep
// a store the register allocator would otherwise delete is to make the destination volatile,
// and the only way to get the store *order* retail has is to write the four fields in the order
// `d, a, c, b`.  Declaring four separate volatile pointers instead reloads them inside the
// loop (measured: 37 instructions against retail's 33), and a plain non-volatile struct drops
// all four stores.
//
// **The loop has to exist.**  Written as a counted loop the compiler removes it, because nothing
// in the body can have an effect; retail keeps it, so the bound is an inequality between two
// pointers rather than an index against the count.
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
// split` fails with "Cyclic dependency ... link order"), and because neither neighbour is part
// of this chain: `fn_802559BC` (0x50 bytes) ends the run below and `fn_80255B28` (0x84 bytes)
// starts the one above.  The second is the same *size* as `fn_80255AA4` and 19 of its 33
// instructions match, which is not the same function and is not carved here.
//
// The directory is retail own, taken from the nearest claimed range: the range below is
// `WorldFormat/CAreaRenderOctTree.cpp` (0x80254BAC..0x80255128) and the one above is
// `WorldFormat/CCollisionPrimitiveData.cpp` (0x80257A14), so this address sits in that unit's
// neighbourhood.  The claim starts at 0x80255A0C rather than at a unit boundary, which is what
// keeps `dtk dol split` from reporting a link-order cycle against `CAreaRenderOctTree.cpp`.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.
 *  **Not claimed by any unit**, so `dtk`'s own `auto_*` object supplies the bytes in the DOL
 *  link and the two `bl`s below resolve to retail's address.  Declared, never defined here.
 *  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it under that name for the host link, the same
 *  way it already does for `__nw__FUlPCcPCc`. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** The four words `fn_80255AA4` reads.  `x00` is the empty allocator `rstl::vector` puts
 *  there, `count`/`capacity`/`items` are its `mCount`/`mCapacity`/`mItems`; retail names the
 *  owner nothing, so nothing here is a guess about a class.  Declared **above** the prototypes
 *  below, not below them: a `struct` named inside a parameter list is scoped to that list, and
 *  the host build then rejects the definition as a conflicting type.  mwcceppc only warns, so
 *  the matching build is green while `tools/probe_sources.sh` is not. */
struct SCarve80255A0COwner {
  unsigned int x00;
  unsigned int count;
  unsigned int capacity;
  void* items;
};

/** Four volatile copies of the two values the walk runs between.  See the header: the stores
 *  are retail's and they are never read back. */
struct SCarve80255A0CCopies {
  unsigned char* a;
  unsigned char* b;
  unsigned char* c;
  unsigned char* d;
};

void* fn_80255AA4(struct SCarve80255A0COwner* self, short flag);
void* fn_80255A50(void* self, short flag);
void fn_80255A2C(void* ptr);
void fn_80255A0C(void* ptr);

void* fn_80255AA4(struct SCarve80255A0COwner* self, short flag) {
  if (self) {
    unsigned int count = self->count;
    void* data = self->items;
    unsigned char* end = (unsigned char*)data + count * 0x14;
    volatile struct SCarve80255A0CCopies copies;
    unsigned char* cur;
    copies.d = (unsigned char*)data;
    copies.a = (unsigned char*)data;
    copies.c = end;
    copies.b = end;
    for (cur = (unsigned char*)data; cur != end; cur += 0x14) {
    }
    Free__7CMemoryFPCv(data);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80255A50(void* self, short flag) {
  if (self) {
    fn_80255AA4((struct SCarve80255A0COwner*)self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void fn_80255A2C(void* ptr) { fn_80255A50(ptr, -1); }

void fn_80255A0C(void* ptr) { fn_80255A2C(ptr); }