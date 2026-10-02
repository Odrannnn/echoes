// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8229-8231`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801FA3CC_text.s` before the claim existed, and are
// still readable with
// `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FBC58 --stop-address=0x801FBD68
// build/G2ME01/main.elf`.  The body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FBC58.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.
//
// .text 0x801FBC58..0x801FBD68, 0x110 = 272 bytes, 3 functions:
//
//   fn_801FBC58    0x801FBC58  0x54    21 instructions
//   fn_801FBCAC    0x801FBCAC  0x84    33 instructions
//   fn_801FBD30    0x801FBD30  0x38    14 instructions
//
// **What the three are: one deleting-destructor step, the block's clear, and the iterator copy
// that carries it.**  Retail names none of them - `symbols.txt` carries the `fn_<addr>`
// placeholder for all three - so every shape here is read off the call edges and the argument
// registers, and each is byte-for-byte a symbol retail *does* name, elsewhere in the DOL.  The
// three twins, verified instruction by instruction with objdump against `main.elf`, not recalled:
//
//   fn_801FBC58  ==  fn_80004744  0x80004744  0x54   src/MetroidPrime/Carve80004744.c
//   fn_801FBCAC  ==  __dt__Q24rstl82vector<Q24rstl38pair<Ui,Q24rstl20rc_ptr<10IMetaTrans>>,...>Fv
//                           0x80030E08  0x84   src/MetroidPrime/Factories/CCharacterFactory.cpp
//   fn_801FBD30  ==  fn_80004864  0x80004864  0x38   src/MetroidPrime/main.cpp:1252
//
// only their `bl` destinations differ.  The fourth twin, `fn_801FDBE0` at 0x801FDBE0 (0x38, same
// bytes), is documented in src/MetroidPrime/ScriptObjects/Carve801FDB5C.c:41-49 and is the mirror
// image of fn_801FBD30 - it is `destroy(It,It)` spelled with by-value **struct** parameters, so
// there the pairs come from the *parameters*, while here they come from the *call*.
//
// **The stride is 12 and the element is not a pointer.**  `fn_801FBCAC`'s `mulli r0,r0,12` fixes
// `T` at 0xC bytes; `fn_801FBD68` - the walk, which is **not** claimed here and is the first thing
// above this claim - walks with `addi r31,r31,0xc` and calls `fn_801FFE4C` on `r31+4`, so the
// reference-counted member of `T` is at +4.  4 + 8 is `rstl::pair<unsigned int,
// rstl::rc_ptr<IMetaTrans> >`, which is why the walk releases rather than frees.  Nothing here
// reads a field of `T`, so `T` is not modelled at all.
//
// **fn_801FBC58 is one step of the deleting-destructor chain, identical to its twin.**  `mr. r30,r3`
// / `beq` is MWCC's receiver guard, `extsh. r0,r31` / `ble` is the `flag > 0` test that decides
// whether the receiver itself is freed (`li r4,-1` at the call sites means "do not free me
// afterwards"), and `lwz r3,0xc(r30)` is the single thing destroyed: a plain pointer at +0xC, not
// an item array - there is no count load and no loop.  `mr r3,r30` in the epilogue is the return
// of the receiver, as in every member of the family.  Its twin src/MetroidPrime/Carve80004744.c
// documents the chain and its three call sites.
//
// **fn_801FBCAC is the same shape with the block's clear in front of the destructor tail**, and
// it is `src/MetroidPrime/main.cpp:1267-1280` (`fn_800068F4`) transposed: the same four frame
// copies over the same four slots (`last` at +0xC, `lastCopy` at +0x8, `firstCopy` at +0x10,
// `first` at +0x14) and the same `extsh.`/`ble`/`Free(self)` tail, with `x04_count`/`x0c_data` the
// `SGameStateBlock` fields `CGameStateBlocks.hpp:22-26` already measured for `fn_800068F4`.  The
// `bl fn_801FBD30` replaces `fn_800068F4`'s `bl fn_80004864` and the second `Free` frees the
// block's data - so this is the **clear**, not a plain destructor.
//
// Two spellings in that body are load-bearing, and both were compiled and scored here:
//
//   - the `volatile` on `firstCopy` and `lastCopy`.  Without it mwcceppc folds the copies and
//     emits two `stw`s where retail has four: 31 instructions against retail's 33, `beq` to +0x64
//     instead of +0x68, `stw r0,0x10` missing, `addi r3,r1,0x10` where retail has `0x14`.  The
//     two copies are the *unused* arguments of the walk - retail passes r1+0x14 and r1+0x0C and
//     stores the same two values into r1+0x08 and r1+0x10 - so the source keeps four address-taken
//     iterators and hands two of them over.  `volatile` is what stops the register allocator from
//     proving the copies redundant.
//   - the `end` temporary, and **which variable the two copies are assigned from**.  Written as
//     `last = base + count * 12; lastCopy = last;` the multiply and the sum both land in r0;
//     introducing `end` first and assigning both copies from **it** keeps `base` in the
//     accumulator register as retail has it (`mulli r0,r0,12 / add r5,r5,r0`).  Both shapes were
//     compiled: the same 33 instructions either way, and 0x0C/`lwz r0,12(r30)` versus the other.
//
// **`first`, not `firstCopy`, is the non-volatile one whose address is taken.**  Passing
// `&firstCopy` / `&last` gives 32 instructions and folds `first` away entirely; the byte-exact
// call passes `&first` and `&last` (`addi r3,r1,0x14` / `addi r4,r1,0xc`), leaving `firstCopy` /
// `lastCopy` as the volatile stores.  Measured both ways.
//
// **fn_801FBD30's parameters must be `const void*`, and that is the whole trick.**  Its 14
// instructions are the by-value **struct parameter copy** that the `bl fn_801FBD68` forces -
// `lwz r5,0(r4)` / `addi r4,r1,8` and `lwz r0,0(r3)` / `addi r3,r1,0xc`, i.e. each argument is
// dereferenced into a frame slot and the *address of the slot* is what the callee gets, because
// MWCC passes a struct that fits in a register by reference.  Those copies live in **this**
// function's frame, so this function's parameter type decides whether they land on retail's slots.
// Measured here, all three functions in the unit otherwise identical:
//
//   `struct SCarve801FBC58Iterator` by value - fn_801FBD30 byte-exact, but fn_801FBCAC builds a
//       fresh argument copy per argument: 34 instructions and a 0x30 frame against retail's 33/0x20
//   `void**`                         - fn_801FBCAC byte-exact, but fn_801FBD30's stores interleave
//       with its loads (5 of 14 instructions wrong) and the copy lands in r0, which the LR spill
//       has taken
//   **`const void*`**                - byte-exact at both ends
//
// `const void*` is right because retail's argument registers are already pointers *to* the
// iterators.  And **dereferencing inside the argument list is the other half**: spelling the same
// thing as two named locals first emits the same 14 instructions with a 32-byte frame against
// retail's 16, because the explicit locals stop being the argument slots the frame was sized for.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order").  The claim stops at 0x801FBD68 and both
// boundaries are function edges: below `fn_801FBBC4` (`symbols.txt:8228`, `size:0x94`) ends
// exactly at 0x801FBC58, above `fn_801FBD68` (`symbols.txt:8232`) begins exactly at 0x801FBD68.
//
// The directory is retail's own, taken from the nearest claimed ranges: 0x801FBC58 is 0x2C08 bytes
// into `MetroidPrime/ScriptObjects/CUnknown90.cpp`'s neighbourhood (0x801F9050..0x801F9190, the
// nearest claimed range below), and `Carve801FDB5C.c` above starts at 0x801FDBE0.  For an
// anonymous function that is the only evidence there is.

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** The one pointer of `rstl::pointer_iterator`, which is what `fn_801FBD68`'s two parameters are.
 *  Retail names the iterator's class only inside the mangled twin symbols above; what the bytes
 *  need of it is exactly this - a struct of one pointer, passed by value. */
struct SCarve801FBC58Iterator {
  void* current;
};

/** The block `fn_801FBC58` tears down and `fn_801FBCAC` clears.  Only the two fields these two
 *  functions read are modelled: the count at +4 and the `void*` at +0xC.  The words before them
 *  are padding so the fields land on retail's displacements; what they hold is not this unit's
 *  business, because the callers hand a subobject at a fixed offset inside a larger class. */
struct SCarve801FBC58Block {
  int x00;
  unsigned int x04_count;
  int x08;
  void* x0c_data;
};

/** 0x801FBD68, `symbols.txt:8232`, size 0x68: the walk fn_801FBD30 forwards to.  **Not** claimed
 *  here - it starts exactly at this unit's claim end, so it stays retail's and dtk supplies it
 *  from its own `auto_03_801FBD68_text.o`.  Declared, never defined here. */
void fn_801FBD68(struct SCarve801FBC58Iterator begin, struct SCarve801FBC58Iterator end);
void fn_801FBD30(const void* first, const void* last);
void* fn_801FBCAC(struct SCarve801FBC58Block* self, short flag);
void* fn_801FBC58(struct SCarve801FBC58Block* self, short flag);

void fn_801FBD30(const void* first, const void* last) {
  fn_801FBD68(*(struct SCarve801FBC58Iterator*)first,
              *(struct SCarve801FBC58Iterator*)last);
}

void* fn_801FBCAC(struct SCarve801FBC58Block* self, short flag) {
  if (self) {
    unsigned int count = self->x04_count;
    unsigned char* first;
    unsigned char* volatile firstCopy;
    unsigned char* last;
    unsigned char* volatile lastCopy;
    unsigned char* end = (unsigned char*)self->x0c_data + count * 12;
    last = end;
    lastCopy = end;
    firstCopy = (unsigned char*)self->x0c_data;
    first = (unsigned char*)self->x0c_data;
    fn_801FBD30(&first, &last);
    Free__7CMemoryFPCv(self->x0c_data);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

void* fn_801FBC58(struct SCarve801FBC58Block* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(self->x0c_data);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
