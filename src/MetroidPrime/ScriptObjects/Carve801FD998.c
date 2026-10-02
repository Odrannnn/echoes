// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8282`, the instructions are retail's own, read this
// run with `build/binutils/powerpc-eabi-objdump -d --start-address=0x801FD998
// --stop-address=0x801FDA1C build/G2ME01/main.elf`, and the body below is the C those bytes are
// the compilation of.  Before the claim existed dtk emitted them into
// `build/G2ME01/asm/auto_03_801FD998_text.s`, whose `.fn fn_801FD998` block is that listing; the
// unit that file belonged to was left with this one function after
// `ScriptObjects/Carve801FDA1C.c` claimed the two behind it.  `build/G2ME01/asm/
// MetroidPrime/ScriptObjects/Carve801FD998.s` is this unit's own generated listing, not the
// retail one - it is our compile, so it can only confirm, never establish, what retail had.
//
// .text 0x801FD998..0x801FDA1C, 0x84 = 132 bytes, 1 function:
//
//   fn_801FD998    0x801FD998  0x84    33 instructions
//
// **What it is: the deleting destructor of a block that owns a strided element array.**  Retail
// names it only as the `fn_<addr>` placeholder, so it is read off a twin rather than off its own
// code - and the twin is exact.  `fn_801FBCAC` (0x801FBCAC, 0x84, `ScriptObjects/Carve801FBC58.c`,
// a `Matching` unit) is 33 instructions too, and comparing the two ranges **read out of the disc**
// (not out of our build) leaves **30 of 33 instruction words identical**, with these three
// differing:
//
//   +0x30  `mulli r0,r0,0x2c` here      vs  `mulli r0,r0,0x0c` there   (the element stride)
//   +0x54  `bl Free__7CMemoryFPCv`      vs  the same word, +0x1CEC apart (position, not semantics)
//   +0x64  `bl Free__7CMemoryFPCv`      vs  the same word, +0x1CEC apart (position, not semantics)
//
// Everything else - the frame, `mr r31,r4`, `mr. r30,r3` / `beq`, the count load at +4 and buffer
// load at +0xC, the four iterator stores at +0x8/+0xC/+0x10/+0x14 and their order, the
// `bl fn_801FDA1C` (its displacement is 0x84 here and there alike, so even that word is
// identical), `extsh. r0,r31` / `ble`, `mr r3,r30` and the epilogue - is the same word for word.
// That is why the two load-bearing spellings (`volatile firstCopy`/`lastCopy`; `end` before both
// copy assignments; `&first`/`&last` passed) are copied from that unit instead of re-derived:
// `Carve801FBC58.c` measures what each of them is worth, and this body is that unit's
// `fn_801FBCAC` with the stride changed from 0xC to 0x2C and the callee from `fn_801FBD30` to
// `fn_801FDA1C`.
//
// **The stride is 44 = 0x2C, measured independently of the `mulli` it fixes.**  The walk this
// function forwards its iterators to reaches `fn_801FDAA4` for one element per turn and steps
// `addi r31,r31,0x2c` (`ScriptObjects/Carve801FDA1C.c`, a `Matching` unit), and that file plus
// `ScriptObjects/Carve801FDAA4.c` fix the element size at 0x2C from the walk's own bytes.  The
// same 0x2C is one of the four strides `ScriptObjects/Carve801FD4B0.cpp` tabulates for this
// address's family of destructors.  Nothing is asserted about the element's class: its members are
// not read here, so no struct is spelled and the buffer is walked as `unsigned char*`.
//
// **Its callee is real, not a stand-in.**  `fn_801FDA1C` (0x801FDA1C, 0x38) is defined by
// `src/MetroidPrime/ScriptObjects/Carve801FDA1C.c`, a `Matching` unit whose `.text` starts exactly
// where this claim ends, so `bl 0x801FD9E4` lands on a definition we wrote, in retail's bytes, and
// retiring this unit's stub in `src/MetroidPrime/PortLinkStubs.cpp` costs the port link nothing -
// the symbol is defined by this object instead.  `Free__7CMemoryFPCv` (0x802CE388) is
// `Kyoto/Alloc/CMemory.cpp`'s, also ours.
//
// **Its one caller** is `fn_801FD4B0` (0x801FD4B0, 0x7C, `ScriptObjects/Carve801FD4B0.cpp`, a
// `Matching` unit): the only `bl fn_801FD998` in the DOL is at 0x801FD4E4, `addi r3,r30,0x20` /
// `li r4,-1`, the second of that destructor's four member teardowns at +0x10/+0x20/+0x30.  So the
// flag arrives as MWCC's "destroy, do not free me afterwards", which is what
// `Carve801FD924.cpp` records for the same shape, and the receiver is a member at +0x20 of an
// unnamed class.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  With one function
// the rule cannot be violated here, and `tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/Carve801FD998` matches `.cpp` names only, so it says nothing; but it
// is the rule that keeps a later second function in this unit from breaking it.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap, and it can join neither neighbour:
// in front of it `ScriptObjects/Carve801FD924.cpp` ends exactly at 0x801FD998 (`symbols.txt:8281`
// has `fn_801FD924` at 0x801FD924 + 0x74), behind it `ScriptObjects/Carve801FDA1C.c` starts
// exactly at 0x801FDA1C.  Both boundaries are function edges, so nothing is split here.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FD924.cpp` (0x801FD924..0x801FD998) and `ScriptObjects/Carve801FDA1C.c`
// (0x801FDA1C..0x801FDAA4), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood, and
// the twin that fixes the shape lives two units along in it.

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FDA1C, `symbols.txt:8283`, 0x38 = 56 bytes: the `rstl::destroy<It, It>` step this body
 *  hands its two iterators to, defined for real by `src/MetroidPrime/ScriptObjects/
 *  Carve801FDA1C.c` (a `Matching` unit whose claim starts exactly where this one ends).  Retail's
 *  bytes pass each argument as the address of a caller frame slot, which is why it is declared
 *  here as the two pointers it takes - the spelling `Carve801FBC58.c` measured for its own twin
 *  `fn_801FBD30` - rather than as a by-value struct, which needs a copy the caller does not
 *  build. */
extern void fn_801FDA1C(const void* first, const void* last);

/** The block this function tears down.  Only the two fields its bytes read are modelled: the count
 *  at +4 and the `void*` at +0xC - retail's `rstl::vector` layout, the same one `fn_801FBCAC`
 *  walks in `Carve801FBC58.c`.  The words before them are padding so the fields land on retail's
 *  displacements; what they hold is not this unit's business, because the callers hand a subobject
 *  at a fixed offset inside a larger class. */
struct SCarve801FD998Block {
  int x00;
  unsigned int x04_count;
  int x08;
  void* x0c_data;
};

void* fn_801FD998(struct SCarve801FD998Block* self, short flag) {
  if (self) {
    unsigned int count = self->x04_count;
    unsigned char* first;
    unsigned char* volatile firstCopy;
    unsigned char* last;
    unsigned char* volatile lastCopy;
    unsigned char* end = (unsigned char*)self->x0c_data + count * 44;
    last = end;
    lastCopy = end;
    firstCopy = (unsigned char*)self->x0c_data;
    first = (unsigned char*)self->x0c_data;
    fn_801FDA1C(&first, &last);
    Free__7CMemoryFPCv(self->x0c_data);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}
