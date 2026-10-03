// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:10484-10486`, the instructions are retail's own, read
// this run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x80255900
// --stop-address=0x80255A0C build/G2ME01/main.elf`, and the body below is the C those bytes are the
// compilation of.  Before the claim existed dtk emitted them into
// `build/G2ME01/asm/auto_03_80255128_text.s`, whose `.fn fn_80255900` / `.fn fn_80255984` /
// `.fn fn_802559BC` blocks are lines 619-654, 657-672 and 675-698 of that file.
// `build/G2ME01/asm/WorldFormat/Carve80255900.s` is this unit's own generated listing, not the retail
// one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x80255900..0x80255A0C, 0x10C = 268 bytes, 3 functions:
//
//   fn_80255900    0x80255900  0x84    33 instructions
//   fn_80255984    0x80255984  0x38    14 instructions
//   fn_802559BC    0x802559BC  0x50    20 instructions
//
// **What the three are: one deleting destructor of a strided block, the `rstl::destroy<It,It>` step
// it hands its iterators to, and the element walk that step runs.**  Retail names none of them -
// `symbols.txt` carries the `fn_<addr>` placeholder for all three - so every shape here is read off a
// **byte-shape twin already matched in this tree**, and each twin comparison was run this session as
// a `diff` of two objdump listings read out of the disc, not recalled.  All three are exact:
//
//   fn_80255900 == fn_801FD998 (0x801FD998, 0x84, `ScriptObjects/Carve801FD998.c`, `Matching`):
//                 33 instruction words, the only differences are **two `bl`s and two branches, and
//                 nothing else** - `bl 80255984`/`Free__7CMemoryFPCv` here against
//                 `bl 801FDA1C`/`Free__7CMemoryFPCv` there, and the `beq`/`ble` displacements that
//                 name the local label.  Even the `mulli r0,r0,0x2c` stride word is the same word.
//   fn_80255984 == fn_801FDA1C (0x801FDA1C, 0x38, `ScriptObjects/Carve801FDA1C.c`, `Matching`):
//                 14 words, the single difference is the `bl` target - and its **encoding**
//                 `48 00 00 15` is identical, because the callee sits the same 0x14 bytes past the
//                 `bl` in both.
//   fn_802559BC == fn_801FDA54 (0x801FDA54, 0x50, `ScriptObjects/Carve801FDA1C.c`, `Matching`):
//                 20 words, the differences are the `b`, the `bl` and the `bne` displacements, and
//                 the `bl` **encoding** `48 00 00 2d` is again identical for the same reason.
//
// So the load-bearing spellings come from those units rather than from re-derivation:
// `ScriptObjects/Carve801FD998.c` measures what `volatile firstCopy`/`lastCopy` and `end` before both
// copy assignments are worth, and `ScriptObjects/Carve801FDA1C.c` measures that the loop bound is an
// inequality between two pointers (`lwz r0,0(r30)` / `cmplw r31,r0` / `bne`), not an index against a
// count, because MWCC passes each iterator struct by reference.
//
// **How `fn_80255984` is declared is load-bearing, and it was measured here, not copied.**  Retail's
// own bytes make it a **pointer-taking** step: `lwz r5,0(r4)` / `addi r4,r1,8` / `lwz r0,0(r3)` /
// `addi r3,r1,12` are each argument dereferenced into a frame slot, and the address of that slot is
// what the `bl` passes on.  Written that way it reproduces 33/33 + 14/14 + 20/20 instructions.  Written
// the obvious way instead - `fn_80255984` taking `SCarve80255900Iterator` **by value** and the caller
// passing `first, last` - it scores 10/33 and 1/14 with a **48-byte frame instead of retail's 32**:
// by-value forces a copy that spills two more slots.  Same for declaring the iterators as that struct
// type rather than `unsigned char*` (9/33).  So `fn_80255984` takes `const void*` and dereferences, and
// `fn_80255900` passes `&first` / `&last` of plain pointer locals - which is exactly what
// `ScriptObjects/Carve801FD998.c` does for its own `fn_801FDA1C`, and why the comment there calls the
// arguments "the address of a caller frame slot".
//
// **The stride is 44 = 0x2C, measured twice inside this claim and independently of the word that
// fixes it.**  `fn_80255900` computes its end pointer with `mulli r0,r0,0x2c`; `fn_802559BC`, the walk
// those iterators reach, steps `addi r31,r31,44`.  Nothing is asserted about the element's class: its
// members are read nowhere in this claim, so no struct is spelled and the element pointer is passed
// as `void*`.
//
// **Both callees are real, not stand-ins.**  `fn_80255984` is defined here, in this object.  The
// other, `fn_80255A0C` (0x80255A0C, 0x20), is defined by `src/WorldFormat/Carve80255A0C.c`, a
// `Matching` unit whose `.text` starts exactly where this claim ends, so `bl 0x802559E0` lands on a
// definition we wrote in retail's bytes and nothing in `src/MetroidPrime/PortLinkStubs.cpp` stands in
// for either symbol (measured: `grep` for the three `fn_` names over `src/` and `include/` returns
// nothing outside this file and that one).  `Free__7CMemoryFPCv` (0x802CE388) is
// `Kyoto/Alloc/CMemory.cpp`'s, also ours.
//
// **The flag arrives as -1, and the one caller is measured.**  The only `bl fn_80255900` in the DOL
// is at 0x802558D0, inside `fn_80255878` (0x80255878, 0x88, unclaimed and left to dtk), which is the
// last of five member teardowns at +0x00/+0x10/+0x20/+0x30/+0x40 and passes `li r4,-1`: MWCC's "destroy,
// do not free me afterwards", behind the same `extsh. r0,r31` / `ble` this function itself uses.  That
// is also why the receiver here is the whole object and the flag gates the final `CMemory::Free(self)`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Here that is fn_802559BC first, then fn_80255984, then
// fn_80255900.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source in
// `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit casts,
// which are compile-time only and leave the object byte-identical.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol split`
// fails with "Cyclic dependency ... link order"), and because neither neighbour is part of this chain:
// `fn_80255878` (0x88 bytes) ends the run below and `fn_80255A0C` - the callee - starts the one above,
// in the unit named above.  Both boundaries are function edges, so nothing is split here, and the
// claim does not span an unclaimed gap: `WorldFormat/CAreaRenderOctTree.cpp` ends at 0x80255128 and
// this claim starts at 0x80255900, 0x7D8 bytes later and the address of a `symbols.txt` function.
//
// The directory is retail's own, taken from the nearest claimed range: the range below is
// `WorldFormat/CAreaRenderOctTree.cpp` (0x80254BAC..0x80255128) and the one above is
// `WorldFormat/Carve80255A0C.c` (0x80255A0C..0x80255B28), so this address sits in that unit's
// neighbourhood.

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x80255A0C, `symbols.txt:10487`, 0x20 = 32 bytes: `rstl::destroy<T>` for the 0x2C-byte element,
 *  defined for real by `src/WorldFormat/Carve80255A0C.c`, whose claim starts exactly where this one
 *  ends.  The call at 0x802559E0 is the only callee edge out of `fn_802559BC`. */
extern void fn_80255A0C(void* self);

/** The one pointer of `rstl::pointer_iterator`, which is what `fn_802559BC`'s two parameters are.
 *  Retail names the iterator's class only inside a mangled twin symbol elsewhere in the DOL
 *  (`destroy<rstl::pointer_iterator<CTweakValue, rstl::vector<CTweakValue, ...> > >` in
 *  `MetroidPrime/main.cpp`); what these bytes need of it is exactly this - a struct of one pointer.
 *  `fn_80255984` takes its two arguments as `const void*` and casts, because the by-value form costs
 *  a 48-byte frame against retail's 32 (see the header).  Declared **above** the prototypes below,
 *  not below them: a `struct` named inside a parameter list is scoped to that list, and the host build
 *  then rejects the definition as a conflicting type.  mwcceppc only warns, so the matching build is
 *  green while `tools/probe_sources.sh` is not. */
struct SCarve80255900Iterator {
  void* current;
};

/** The block `fn_80255900` tears down.  Only the two fields its bytes read are modelled: the count at
 *  +0x04 and the `void*` at +0x0C - retail's `rstl::vector` layout, the same one `fn_801FD998` walks
 *  in `ScriptObjects/Carve801FD998.c`.  The words around them are padding so the fields land on
 *  retail's displacements; what they hold is not this unit's business, because the caller hands a
 *  subobject at a fixed offset inside a larger class. */
struct SCarve80255900Block {
  int x00;
  unsigned int x04_count;
  int x08;
  void* x0c_data;
};

void fn_802559BC(struct SCarve80255900Iterator begin, struct SCarve80255900Iterator end);
void fn_80255984(const void* first, const void* last);
void* fn_80255900(struct SCarve80255900Block* self, short flag);

void fn_802559BC(struct SCarve80255900Iterator begin, struct SCarve80255900Iterator end) {
  char* cur = (char*)begin.current;
  while (cur != (char*)end.current) {
    fn_80255A0C(cur);
    cur += 0x2C;
  }
}

void fn_80255984(const void* first, const void* last) {
  fn_802559BC(*(struct SCarve80255900Iterator*)first, *(struct SCarve80255900Iterator*)last);
}

void* fn_80255900(struct SCarve80255900Block* self, short flag) {
  if (self) {
    unsigned int count = self->x04_count;
    unsigned char* first;
    unsigned char* volatile firstCopy;
    unsigned char* last;
    unsigned char* volatile lastCopy;
    unsigned char* end = (unsigned char*)self->x0c_data + count * 0x2C;
    last = end;
    lastCopy = end;
    firstCopy = (unsigned char*)self->x0c_data;
    first = (unsigned char*)self->x0c_data;
    fn_80255984(&first, &last);
    Free__7CMemoryFPCv(self->x0c_data);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}