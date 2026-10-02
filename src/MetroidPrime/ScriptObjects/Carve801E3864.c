// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7802-7805`, and the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_801E2AF0_text.s` while this range was still
// unclaimed - the body below is the C those bytes are the compilation of.  Claiming it split
// that asm unit in two, `auto_03_801E2AF0_text.s` (0x801E2AF0..0x801E3864) and
// `auto_03_801E3A34_text.s` (0x801E3A34..0x801E3E34), i.e. exactly the pieces either side of
// this claim; the retail bytes are at 0x801E3864 in `build/G2ME01/main.elf`.
//
// .text 0x801E3864..0x801E3A34, 0x1D0 = 464 bytes, 4 functions:
//
//   fn_801E3864    0x801E3864  0x74    29 instructions
//   fn_801E38D8    0x801E38D8  0x74    29 instructions
//   fn_801E394C    0x801E394C  0x74    29 instructions
//   fn_801E39C0    0x801E39C0  0x74    29 instructions
//
// **What the four are: two `rstl::list` members, written out twice each.**  Retail names none of
// them, so this is read off the bodies and the call edges, not guessed:
//
//   fn_801E3864  `rstl::list<T>::do_erase(node*)` for the list at +0x44 of the class below.
//   fn_801E38D8  the same list's deleting destructor `~list()` (the `-1` flag of MWCC's
//                deleting-destructor convention: `mr. r29,r3 / beq` guards the receiver, r4 is
//                re-tested with `extsh.`, and only a positive flag reaches `CMemory::Free`).
//   fn_801E394C  `do_erase` again, differing from fn_801E3864 **only in its one `bl` target** -
//                the list at +0x5C, a second instantiation.
//   fn_801E39C0  that second list's `~list()`.
//
// `include/rstl/list.hpp:263` (`do_erase`) and `:251` (`~list()`) are the source both copies are
// instantiations of, and `src/MetroidPrime/CWorld.cpp` already matches the same two bodies
// byte-for-byte at 0x80052788/0x800527FC, which is why this file reproduces their logic instead
// of inventing a bodyshape.  The member offsets the bytes pin down (and `include/rstl/list.hpp`
// documents): `mStart` at +4, `mEnd` at +8 and `mCount` at +0x14, i.e. `rmemory_allocator` (empty
// at +0) followed by the four node pointers and the count, `node { mPrev, mNext }` at +0/+4, and
// an **empty `~T()`** - neither copy emits a destructor call, so both element types are trivially
// destructible.
//
// Who they belong to is measured too.  `fn_801E2E30` (0x801E2E30, the class's deleting
// destructor) stores `lbl_803B7640` at +0 - its vtable, `config/G2ME01/symbols.txt:18322`,
// `type:object size:0x20`, unclaimed - destroys the list at +0x5C with `li r4,-1 ; bl
// fn_801E39C0`, the list at +0x44 with `li r4,-1 ; bl fn_801E38D8`, and then the `CEntity` base
// with `li r4,0 ; bl __dt__7CEntityFv`, so the +0x44 and +0x5C lists are members of one
// `CEntity`-derived class.  `fn_801E3414` (0x801E3414, same class) has the only other calls to
// these four in the whole DOL: `addi r3,r30,0x5c ; bl fn_801E394C` at 0x801E3570 and 0x801E3668,
// and `addi r3,r30,0x44 ; bl fn_801E3864` at 0x801E3600, each reached after a walk of a chain
// through +4 comparing the halfword at +8 against a `TUniqueId`.  The class itself is still
// unnamed: nothing claims 0x801E2AF0, and `lbl_803B7640` is the only thing that names it.
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
// side of this run are not trivial: below, `fn_801E374C` (0x801E374C, 0x118) ends the run;
// above, `fn_801E3A34` (0x801E3A34, 0x158) starts the next one.
//
// The directory is retail's own, taken from the nearest claimed range: the claim below is
// `MetroidPrime/ScriptObjects/Carve801E2AE8.c` and the one above
// `MetroidPrime/ScriptObjects/Carve801E3E34.c`, so this address sits in that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is, and it beats a
// lane picking the directory it happened to own.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  Not
 *  claimed by any unit, so dtk's own `auto_*` object supplies the bytes in the DOL link and the
 *  `bl`s below resolve to retail's address.  Declared, never defined here.
 *  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it under that name for the host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `rstl::list<T>::node`, `include/rstl/list.hpp`: two pointers, prev then next.  Declared above
 *  the prototypes below, not below them: a `struct` named inside a parameter list is scoped to
 *  that list, and the host build then rejects the definition as a conflicting type. */
struct SCarve801E3864Node {
  struct SCarve801E3864Node* mPrev; /* +0 */
  struct SCarve801E3864Node* mNext; /* +4 */
};

/** `rstl::list<T, rmemory_allocator>`: the allocator is empty and sits at +0, then the four node
 *  pointers, then `mCount` at +0x14 - the offsets the two `do_erase` bodies read. */
struct SCarve801E3864List {
  void* mAllocator;                      /* +0, empty class: no bytes of its own */
  struct SCarve801E3864Node* mStart;     /* +4 */
  struct SCarve801E3864Node* mEnd;       /* +8 */
  struct SCarve801E3864Node* mEmptyPrev; /* +0xC, `list`'s self-linked empty node */
  struct SCarve801E3864Node* mEmptyNext; /* +0x10 */
  int mCount;                            /* +0x14 */
};

void* fn_801E39C0(struct SCarve801E3864List* self, short flag);
void* fn_801E394C(struct SCarve801E3864List* self, struct SCarve801E3864Node* item);
void* fn_801E38D8(struct SCarve801E3864List* self, short flag);
void* fn_801E3864(struct SCarve801E3864List* self, struct SCarve801E3864Node* item);

void* fn_801E39C0(struct SCarve801E3864List* self, short flag) {
  if (self) {
    struct SCarve801E3864Node* cur = self->mStart;
    while (cur != self->mEnd) {
      struct SCarve801E3864Node* it = cur;
      struct SCarve801E3864Node* next = cur->mNext;
      cur = next;
      Free__7CMemoryFPCv(it);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_801E394C(struct SCarve801E3864List* self, struct SCarve801E3864Node* item) {
  struct SCarve801E3864Node* result = item->mNext;
  if (item == self->mStart) {
    self->mStart = result;
  }
  item->mPrev->mNext = item->mNext;
  item->mNext->mPrev = item->mPrev;
  Free__7CMemoryFPCv(item);
  self->mCount--;
  return result;
}

void* fn_801E38D8(struct SCarve801E3864List* self, short flag) {
  if (self) {
    struct SCarve801E3864Node* cur = self->mStart;
    while (cur != self->mEnd) {
      struct SCarve801E3864Node* it = cur;
      struct SCarve801E3864Node* next = cur->mNext;
      cur = next;
      Free__7CMemoryFPCv(it);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_801E3864(struct SCarve801E3864List* self, struct SCarve801E3864Node* item) {
  struct SCarve801E3864Node* result = item->mNext;
  if (item == self->mStart) {
    self->mStart = result;
  }
  item->mPrev->mNext = item->mNext;
  item->mNext->mPrev = item->mPrev;
  Free__7CMemoryFPCv(item);
  self->mCount--;
  return result;
}
