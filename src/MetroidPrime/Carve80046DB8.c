// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:1322-1325`, and the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80045CDC_text.s` while this range was still
// unclaimed - the body below is the C those bytes are the compilation of.  Claiming it splits
// that asm unit in two, and the retail bytes are at 0x80046DB8 in `build/G2ME01/main.elf`.
//
// .text 0x80046DB8..0x80046F88, 0x1D0 = 464 bytes, 4 functions:
//
//   fn_80046DB8    0x80046DB8  0x74    29 instructions
//   fn_80046E2C    0x80046E2C  0x74    29 instructions
//   fn_80046EA0    0x80046EA0  0x74    29 instructions
//   fn_80046F14    0x80046F14  0x74    29 instructions
//
// **What the four are: three `rstl::list<T>::~list()` copies and one `do_erase(node*)`.**  Retail
// names none of them, so this is read off the bodies and the call edges, not guessed:
//
//   fn_80046DB8  `~list()` - `mr. r29,r3 / beq` guards the receiver, `r30` holds the `-1` of
//                MWCC's deleting-destructor convention and is re-tested with `extsh.` at 0x80046DFC,
//                so only a positive flag reaches `CMemory::Free`.
//   fn_80046E2C  `~list()` again, **byte-identical to fn_80046DB8 apart from its two `bl`
//                displacements** (0x80046E60 and 0x80046E7C vs 0x80046DEC and 0x80046E08).  The
//                seed's twin list, matched in `src/MetroidPrime/CWorld.cpp` at 0x800527FC.
//   fn_80046EA0  `do_erase(node*)` - a different body: `subi r0,r4,0x1 ; stw r0,0x14(r30)` is the
//                `mCount` decrement, and the result travels back in `r31`, not `r29`.  Its only
//                caller is `fn_800418B8` (0x800418B8, `MetroidPrime/CStateManager.s:13236`), which
//                walks a range with it: `bl fn_80046EA0 ; mr r0,r3 ; cmplw r0,r31 ; bne` - the
//                `erase(first, last)` shape.
//   fn_80046F14  `~list()` a third time, the `mGraveyard` copy - see the callers below.
//
// `include/rstl/list.hpp:251` (`~list()`) and `:263` (`do_erase`) are the source all four are
// instantiations of, and `src/MetroidPrime/CWorld.cpp` already matches the same two bodies
// byte-for-byte at 0x80052788/0x800527FC, as does `src/MetroidPrime/ScriptObjects/Carve801E3864.c`
// for four more copies of them; that is why this file reproduces their logic instead of inventing a
// bodyshape.  The member offsets the bytes pin down (and `include/rstl/list.hpp` documents):
// `mStart` at +4, `mEnd` at +8 and `mCount` at +0x14, i.e. `rmemory_allocator` (empty, at +0)
// followed by the four node pointers and the count, `node { mPrev, mNext }` at +0/+4, and an
// **empty `~T()`** - none of the four emits an element destructor call, so every element type
// here is trivially destructible.
//
// Who they belong to is measured too, from the five `bl` edges in the DOL:
//
//   fn_80046DB8  five callers, four units.  `__dt__13CStateManagerFv` passes `+0x8D4` at
//                0x80042B58; `CGameArea`'s dtor passes `+0x1C4` at 0x8005DFF4; `CScriptTrigger`
//                passes it a stack local at 0x800726B4, and a list node's `mItem` at 0x800733A8 and
//                at `node+0xC` 0x80073428.  A list this widely shared is an unnamed element type,
//                so no better name than the address is available.
//   fn_80046E2C  one caller, 0x80229BF8 in `auto_03_80229BC4_text.s`: a deleting destructor that
//                stores `lbl_803B86A8` (`symbols.txt:18397`, `type:object size:0x10`) as its
//                vtable and then destroys the list at `self+4`.
//   fn_80046EA0  one caller, `fn_800418B8` as above.
//   fn_80046F14  one caller, `__dt__13CStateManagerFv` passes `+0x1608` at 0x80042B24, and
//                `include/MetroidPrime/CStateManager.hpp:348` names that member
//                `rstl::list< rstl::reserved_vector< CEntity*, 32 > > mGraveyard`.  That is the one
//                of the four whose element type is known, and it is consistent: a
//                `reserved_vector< CEntity*, 32 >` is a pointer array with no destructor of its own,
//                hence the empty `~T()` the bytes show.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the functions on either side
// of this run are not trivial: below, `do_erase__Q24rstl43list<9TUniqueId,...>` at 0x80046D44 is
// a *named* symbol, so it needs its own mangling and a `.cpp`; above, `fn_80046F88` (0x80046F88,
// 0x8C) walks nodes whose element has an out-of-line destructor, which the `~list()` shape here
// does not cover.
//
// The directory is retail's own, taken from the nearest claimed range: the claim below is
// `MetroidPrime/Carve80045CD4.c` (0x80045CD4..0x80045CDC) and the one above is
// `MetroidPrime/CEntity.cpp` (0x800476A8), so this address sits in that unit's neighbourhood.  For
// an anonymous function that is the only evidence there is, and it beats a lane picking the
// directory it happened to own.

/** 0x802CE388, `symbols.txt:12992`: retail's `CMemory::Free(void const*)`, size 0x64.  The four
 *  `bl`s in this range all resolve there - checked by decoding the displacements out of
 *  `build/G2ME01/main.elf` at 0x80046DEC, 0x80046E60, 0x80046EE8 and 0x80046F48, all 0x802CE388.
 *  Not claimed by any unit, so dtk's own `auto_*` object supplies the bytes in the DOL link.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it under that name for
 *  the host link. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `rstl::list<T>::node`, `include/rstl/list.hpp:74`: two pointers, prev then next.  Declared
 *  above the prototypes below, not below them: a `struct` named inside a parameter list is scoped
 *  to that list, and the host build then rejects the definition as a conflicting type. */
struct SCarve80046DB8Node {
  struct SCarve80046DB8Node* mPrev; /* +0 */
  struct SCarve80046DB8Node* mNext; /* +4 */
};

/** `rstl::list<T, rmemory_allocator>`: the allocator is empty and sits at +0, then the four node
 *  pointers, then `mCount` at +0x14 - the offsets the `do_erase` body reads. */
struct SCarve80046DB8List {
  void* mAllocator;                      /* +0, empty class: no bytes of its own */
  struct SCarve80046DB8Node* mStart;     /* +4 */
  struct SCarve80046DB8Node* mEnd;       /* +8 */
  struct SCarve80046DB8Node* mEmptyPrev; /* +0xC, `list`'s self-linked empty node */
  struct SCarve80046DB8Node* mEmptyNext; /* +0x10 */
  int mCount;                            /* +0x14 */
};

void* fn_80046F14(struct SCarve80046DB8List* self, short flag);
void* fn_80046EA0(struct SCarve80046DB8List* self, struct SCarve80046DB8Node* item);
void* fn_80046E2C(struct SCarve80046DB8List* self, short flag);
void* fn_80046DB8(struct SCarve80046DB8List* self, short flag);

void* fn_80046F14(struct SCarve80046DB8List* self, short flag) {
  if (self) {
    struct SCarve80046DB8Node* cur = self->mStart;
    while (cur != self->mEnd) {
      struct SCarve80046DB8Node* it = cur;
      struct SCarve80046DB8Node* next = cur->mNext;
      cur = next;
      Free__7CMemoryFPCv(it);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80046EA0(struct SCarve80046DB8List* self, struct SCarve80046DB8Node* item) {
  struct SCarve80046DB8Node* result = item->mNext;
  if (item == self->mStart) {
    self->mStart = result;
  }
  item->mPrev->mNext = item->mNext;
  item->mNext->mPrev = item->mPrev;
  Free__7CMemoryFPCv(item);
  self->mCount--;
  return result;
}

void* fn_80046E2C(struct SCarve80046DB8List* self, short flag) {
  if (self) {
    struct SCarve80046DB8Node* cur = self->mStart;
    while (cur != self->mEnd) {
      struct SCarve80046DB8Node* it = cur;
      struct SCarve80046DB8Node* next = cur->mNext;
      cur = next;
      Free__7CMemoryFPCv(it);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_80046DB8(struct SCarve80046DB8List* self, short flag) {
  if (self) {
    struct SCarve80046DB8Node* cur = self->mStart;
    while (cur != self->mEnd) {
      struct SCarve80046DB8Node* it = cur;
      struct SCarve80046DB8Node* next = cur->mNext;
      cur = next;
      Free__7CMemoryFPCv(it);
    }
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
