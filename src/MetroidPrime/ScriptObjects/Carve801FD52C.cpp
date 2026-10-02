// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:8268-8269` (`fn_801FD52C = .text:0x801FD52C; //
// size:0x84` and `fn_801FD5B0 = .text:0x801FD5B0; // size:0x38`), and the 188 bytes below are
// retail's own, read this run out of the disc with
// `python3 tools/dol_read.py 0x801FD52C 0xBC` (`orig/G2ME01/sys/main.dol`, `.text` at file offset
// 0x1FA32C for this address) and **not** out of `build/G2ME01/main.elf`, which holds our own bytes
// once this unit is in the link.  The body below is the C++ those bytes are the compilation of,
// and `tools/flip_test.sh` is what says so.
//
// .text 0x801FD52C..0x801FD5E8, 0xBC = 188 bytes, 2 functions:
//
//   fn_801FD52C    0x801FD52C  0x84   33 instructions   the 36-byte element's
//                                                       `rstl::vector<T>::~vector()`
//   fn_801FD5B0    0x801FD5B0  0x38   14 instructions   `rstl::destroy(It, It)`, the member walk
//                                                       that destructor hands its elements to
//
//   801fd52c  94 21 ff e0   stwu   r1,-32(r1)
//   801fd530  7c 08 02 a6   mflr   r0
//   801fd534  90 01 00 24   stw    r0,0x24(r1)
//   801fd538  93 e1 00 1c   stw    r31,0x1c(r1)
//   801fd53c  7c 9f 23 78   mr     r31,r4            ; the deleting flag, kept whole in r31
//   801fd540  93 c1 00 18   stw    r30,0x18(r1)
//   801fd544  7c 7e 1b 79   mr.    r30,r3            ; receiver guard: MWCC's null `this` test
//   801fd548  41 82 00 4c   beq    .+0x4c
//   801fd54c  80 1e 00 04   lwz    r0,4(r30)         ; mCount
//   801fd550  38 61 00 14   addi   r3,r1,0x14        ; &(the second copy of the begin iterator)
//   801fd554  80 be 00 0c   lwz    r5,0xc(r30)       ; mItems
//   801fd558  38 81 00 0c   addi   r4,r1,0x0c        ; &(the second copy of the end iterator)
//   801fd55c  1c 00 00 24   mulli  r0,r0,36          ; the element stride: 0x24 = 36 bytes
//   801fd560  7c a5 02 14   add    r5,r5,r0          ; end = mItems + mCount
//   801fd564  90 a1 00 0c   stw    r5,0xc(r1)        ; end, parameter copy
//   801fd568  80 1e 00 0c   lwz    r0,0xc(r30)       ; mItems again, for begin
//   801fd56c  90 a1 00 08   stw    r5,0x8(r1)        ; end, the temporary itself
//   801fd570  90 01 00 10   stw    r0,0x10(r1)       ; begin, the temporary itself
//   801fd574  90 01 00 14   stw    r0,0x14(r1)       ; begin, parameter copy
//   801fd578  48 00 00 39   bl     0x801fd5b0        ; fn_801FD5B0(begin, end), by value
//   801fd57c  80 7e 00 0c   lwz    r3,0xc(r30)       ; mItems
//   801fd580  48 0d 0e 09   bl     0x802ce388        ; CMemory::Free(mItems)
//   801fd584  7f e0 07 35   extsh. r0,r31            ; the flag, sign-extended to 16 bits
//   801fd588  40 81 00 0c   ble    .+0x0c            ; ... so `flag > 0` is the free test
//   801fd58c  7f c3 f3 78   mr     r3,r30
//   801fd590  48 0d 0d f9   bl     0x802ce388        ; CMemory::Free(self)
//   801fd594  80 01 00 24   lwz    r0,0x24(r1)
//   801fd598  7f c3 f3 78   mr     r3,r30            ; the receiver is returned
//   801fd59c  83 e1 00 1c   lwz    r31,0x1c(r1)
//   801fd5a0  83 c1 00 18   lwz    r30,0x18(r1)
//   801fd5a4  7c 08 03 a6   mtlr   r0
//   801fd5a8  38 21 00 20   addi   r1,r1,32
//   801fd5ac  4e 80 00 20   blr
//
//   801fd5b0  94 21 ff f0   stwu   r1,-16(r1)
//   801fd5b4  7c 08 02 a6   mflr   r0
//   801fd5b8  80 a4 00 00   lwz    r5,0(r4)          ; end.current
//   801fd5bc  90 01 00 14   stw    r0,0x14(r1)
//   801fd5c0  38 81 00 08   addi   r4,r1,0x8         ; &end's home slot
//   801fd5c4  80 03 00 00   lwz    r0,0(r3)          ; begin.current
//   801fd5c8  38 61 00 0c   addi   r3,r1,0xc         ; &begin's home slot
//   801fd5cc  90 a1 00 08   stw    r5,0x8(r1)
//   801fd5d0  90 01 00 0c   stw    r0,0xc(r1)
//   801fd5d4  48 00 00 15   bl     0x801fd5e8        ; fn_801FD5E8(begin, end), by value
//   801fd5d8  80 01 00 14   lwz    r0,0x14(r1)
//   801fd5dc  7c 08 03 a6   mtlr   r0
//   801fd5e0  38 21 00 10   addi   r1,r1,16
//   801fd5e4  4e 80 00 20   blr
//
// **fn_801FD52C is a deleting destructor in the port's stock shape with a 36-byte element's
// container teardown in the middle of it.**  `mr. r30,r3 / beq` guards the receiver,
// `CMemory::Free(self)` is reached only behind `extsh. r0,r31 / ble` (i.e. `flag > 0`), the receiver
// comes back in `r3`, and the middle is the vector's own parts read off the bytes: the count at
// `+0x4` (`lwz r0,4(r30)`), the capacity at `+0x8` (never read here) and the item array at `+0xC`
// (`lwz r5,0xc(r30)` and the second `lwz r0,0xc(r30)`) - exactly `rstl::vector`'s
// `mAllocator`/`mCount`/`mCapacity`/`mItems` (`include/rstl/vector.hpp:18-21`), the layout
// `src/MetroidPrime/ScriptObjects/Carve801FDB5C.c`'s own header already measures for this family.
// The `mulli r0,r0,36` at 0x801FD55C fixes the element stride at 0x24 = 36 bytes, which is what
// `src/MetroidPrime/Carve800045A0.c:20-31` documents for its 0xC twin: `mItems + mCount`, the
// buffer freed once, then the receiver.
//
// **fn_801FD5B0 is `rstl::destroy(It, It)` and is byte-identical to the `Matching` `fn_801FDBE0`
// of `src/MetroidPrime/ScriptObjects/Carve801FDB5C.c`, one instantiation over.**  Both are 0x38
// bytes and 14 instructions, both take two one-pointer structs **by value**, dereference them
// (`lwz r5,0(r4)` / `lwz r0,0(r3)`), copy each into their own frame at +0x8 and +0xC and hand the
// *addresses* of those copies to their own callee.  That by-value copy is load-bearing and is why
// this file models an iterator struct at all: a `void**` parameter is a pointer to the caller's
// cursor and emits neither store (`Carve801FDB5C.c`'s header measures the two spellings).  Its
// callee is `fn_801FD5E8` (`destroy_impl`, 0x50), claimed and `Matching` in
// `src/MetroidPrime/ScriptObjects/Carve801FD5E8.c` - so both `bl`s in this unit land on our own
// objects' symbols and the carve costs the port link **no new stand-in**.
//
// **The four stores at r1+0x08/+0x0C/+0x10/+0x14 are two by-value `pointer_iterator` arguments'
// home slots, and they are the whole difficulty of this item.**  Each pointer is stored twice:
// the temporary the source materialises (`SIt(mItems)` at +0x10, `SIt(mItems + mCount)` at +0x08)
// and the copy the by-value call actually passes (`addi r3,r1,0x14` / `addi r4,r1,0x0c` at
// 0x801FD550/0x801FD558).  `src/MetroidPrime/Carve800045A0.c`'s 0xC-element twin documents the same
// four stores and says removing them - a plain pointer loop - is 16 bytes short.
//
// **Two spellings that reach these bytes and one that only looks like it** (all three measured this
// run against retail's range, `fn_801FD52C`'s 33 instructions unless noted):
//
//   spelling                                                     verdict
//   this file: unnamed `SIt(mItems)` / `SIt(...)` arguments       33 instructions, byte-exact, and
//     with the class's constructor, in a `.cpp`                   the object defines nothing else
//                                                                (`.text` 0xBC, no other section)
//   named locals of the same constructed type in a `.cpp`         32 instructions, 0x80 vs 0x84:
//                                                                MWCC reuses one slot per argument
//   `(struct SIt){mItems}` compound literals in a `.c`            33 instructions, byte-exact, but
//                                                                an 8-byte `.sbss2` (`@14`, `@16`)
//                                                                reaches the object
//
// The `.c` compound literal is byte-exact in isolation and still cannot be promoted: MWCC's C mode
// gives a compound literal **static** storage duration, so the object carries two extra 4-byte
// objects retail's does not have, `tools/unit_fit.sh` answers `NOT CLAIMED BY splits.txt` and the
// DOL stops reproducing retail.  That is the same wall `docs/goal-notes/carve-801fdb5c.md` records
// for this shape, and the lesson is the general one: **byte-exact in isolation is not linkable** -
// judge with `unit_fit.sh` + `flip_test.sh`, never with an instruction diff.  The C++
// functional-cast temporary is a real object with automatic storage duration, so nothing extra
// reaches the object, and because both temporaries are materialised at the call *and* copied into
// the parameter area, both pointers land twice.  The constructor is declared and defined in the
// class, so it inlines to nothing:
//
//   build/binutils/powerpc-eabi-nm --defined-only -n build/G2ME01/src/MetroidPrime/ScriptObjects/Carve801FD52C.o
//   00000000 T fn_801FD52C
//   00000084 T fn_801FD5B0
//
// That is also why the unit is a `.cpp` and not a `.c`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  `fn_801FD5B0` (0x801FD5B0) is therefore declared **first**
// and `fn_801FD52C` (0x801FD52C) second; `python3 tools/check_decl_order.py --unit
// main/MetroidPrime/ScriptObjects/Carve801FD52C` prints `ok: 1 unit(s) checked, none emits its
// functions out of retail order`.
//
// Its own unit because a claim may not span an unclaimed gap.  In front of it `fn_801FD4B0`
// (0x801FD4B0, 0x7C) ends exactly where this claim starts and is unclaimed - that unit's own
// `src/MetroidPrime/ScriptObjects/Carve801FD4B0.cpp` stops at 0x801FD52C and calls this address
// with the `-1` flag; behind it `ScriptObjects/Carve801FD5E8.c` (Matching) starts exactly where
// this claim ends, so 0x801FD52C..0x801FD5E8 is exactly these two functions and nothing else
// (`fn_801FD420`, 0x801FD420, ends at 0x801FD4B0 in front; `fn_801FD638` starts at 0x801FD638
// behind).
//
// Retail names neither function: `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions stay `extern "C"` - a C++ one would mangle to
// `_Z<len>fn_801FD52C<len>...` and objdiff would pair nothing.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FD4B0.cpp` (0x801FD4B0..0x801FD52C) and
// `ScriptObjects/Carve801FD5E8.c` (0x801FD5E8..0x801FD638), which is the `ScriptObjects/`
// neighbourhood dtk's `auto_03_801FBD68_text.s` splits into.

/** One pointer of `rstl::pointer_iterator`, whose serialized class in this tree -
 *  `include/rstl/pointer_iterator.hpp:63`, with `T* current` at `:59` - is exactly this one field,
 *  and whose one-argument constructor (`pointer_iterator(T* begin)`,
 *  `include/rstl/pointer_iterator.hpp:72`) is why the temporaries below have automatic storage
 *  duration.  Retail names the class only
 *  inside mangled twin symbols; what these bytes need of it is this, that it is one pointer passed
 *  by value, and that it is constructed from a pointer rather than assembled by a compound literal.
 *  The element type is not asserted - it never appears in either body, only its 36-byte stride. */
struct SCarve801FD52CIterator {
  void* current;
  SCarve801FD52CIterator(void* p) : current(p) {}
};

/** The 36-byte element this vector walks, used only so that `mItems + mCount` scales by 0x24 the
 *  way the `mulli r0,r0,36` at 0x801FD55C measures.  Nothing here reads its fields and nothing is
 *  asserted about its class; the destructor that walks it is `fn_801FD5B0`'s callee, not this. */
struct SCarve801FD52CElement {
  int x00[9];
};

/** The container this destructor teardown reads.  `x04_count`/`x0c_items` are the `lwz r0,4(r30)`
 *  and `lwz r5,0xc(r30)` above; `x00` and `x08` are never read here but are what puts those two at
 *  their offsets (`rstl::vector`'s `mAllocator`/`mCount`/`mCapacity`/`mItems`,
 *  `include/rstl/vector.hpp:18-21`). */
struct SCarve801FD52CVector {
  unsigned int x00_allocator;
  int x04_count;
  unsigned int x08_capacity;
  SCarve801FD52CElement* x0c_items;
};

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`, claimed by `Kyoto/Alloc/CMemory.cpp`
 *  in both builds, so our own tree supplies it.  Declared under retail's own emitted spelling so the
 *  call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FD5E8, `symbols.txt:8270`: `rstl::destroy_impl(It, It)` for this element, claimed and
 *  `Matching` in `src/MetroidPrime/ScriptObjects/Carve801FD5E8.c` (its twin there is `fn_801FDC18`).
 *  Declared, never defined here. */
extern "C" void fn_801FD5E8(SCarve801FD52CIterator begin, SCarve801FD52CIterator end);

/** 0x801FD5B0, `symbols.txt:8269`: `rstl::destroy(It, It)`, the walk this destructor hands its
 *  range to.  The callee of an unclaimed function, which is why the previous carve's
 *  `src/MetroidPrime/PortLinkStubs.cpp` block did not stand in for it - nothing referenced it until
 *  this unit did, and this unit defines it. */
extern "C" void fn_801FD5B0(SCarve801FD52CIterator begin, SCarve801FD52CIterator end);

extern "C" void* fn_801FD52C(SCarve801FD52CVector* self, short flag);

extern "C" void fn_801FD5B0(SCarve801FD52CIterator begin, SCarve801FD52CIterator end) {
  fn_801FD5E8(begin, end);
}

extern "C" void* fn_801FD52C(SCarve801FD52CVector* self, short flag) {
  if (self) {
    fn_801FD5B0(SCarve801FD52CIterator(self->x0c_items),
                SCarve801FD52CIterator(self->x0c_items + self->x04_count));
    Free__7CMemoryFPCv(self->x0c_items);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
