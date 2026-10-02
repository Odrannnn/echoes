// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:8276`, the instructions are retail's own, read this run
// out of the disc (`orig/G2ME01/sys/main.dol`, via `tools/dol_read.py 0x801FD7D4 0x84`, then
// `build/binutils/powerpc-eabi-objdump -D -b binary -EB -m powerpc:common --adjust-vma=0x801FD7D4`)
// and **not** out of `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link.
// Before the claim existed these 132 bytes were the third `.fn` block of dtk's
// `build/G2ME01/asm/auto_03_801FD6F0_text.s`, and the unit that file belonged to was left with
// `fn_801FD6F0` and `fn_801FD774` once this claim and `Carve801FD858.c` took the three behind them.
// `build/G2ME01/asm/MetroidPrime/ScriptObjects/Carve801FD7D4.s` is this unit's own generated
// listing, not the retail one - it is our compile, so it can only confirm, never establish, what
// retail had.  The body below is the C those bytes are the compilation of.
//
// .text 0x801FD7D4..0x801FD858, 0x84 = 132 bytes, 1 function:
//
//   fn_801FD7D4    0x801FD7D4  0x84    33 instructions
//
// **What it is: the deleting destructor of a block that owns a strided element array.**  Retail
// names it only as the `fn_<addr>` placeholder (`symbols.txt` carries no better name), so it is
// read off a twin - and the twin is exact.  `fn_801FD998` (0x801FD998, 0x84,
// `src/MetroidPrime/ScriptObjects/Carve801FD998.c`, a `Matching` unit) is 33 instructions too, and
// comparing the two ranges **word by word out of the disc** leaves **30 of 33 instruction words
// identical**, with these three differing:
//
//   +0x30  `mulli r0,r0,0x24` here   vs  `mulli r0,r0,0x2c` there   (the element stride)
//   +0x54  `bl Free__7CMemoryFPCv`    vs  the same word, 0x1C4 apart   (position, not semantics)
//   +0x64  `bl Free__7CMemoryFPCv`    vs  the same word, 0x1C4 apart   (position, not semantics)
//
// and 0x1C4 is exactly 0x801FD998 - 0x801FD7D4, so those two differ only because the two functions
// sit that far apart.  Everything else - the frame, `mr r31,r4`, `mr. r30,r3` / `beq 0x801FD83C`,
// the count load at +4 and buffer load at +0xC, the four iterator stores at +0x8/+0xC/+0x10/+0x14
// and their order, `bl 0x801FD858` at +0x4C, `extsh. r0,r31` / `ble`, `mr r3,r30` and the epilogue -
// is the same word for word.  Even the `bl` word is identical (`48 00 00 39`), because each
// function's inner walk sits 0x84 bytes past its own destructor: 0x801FD7D4 + 0x4C + 0x39*4 =
// 0x801FD858 here and 0x801FD998 + 0x4C + 0x39*4 = 0x801FDA1C there.  That is why the two
// load-bearing spellings (`volatile firstCopy`/`lastCopy`; `end` before both copy assignments;
// `&first`/`&last` passed) are copied from that unit instead of re-derived: `Carve801FD998.c`
// measures what each of them is worth, and this body is that unit's `fn_801FD998` with the stride
// changed from 0x2C to 0x24 and the callee from `fn_801FDA1C` to `fn_801FD858`.
//
// **The stride is 36 = 0x24, measured independently of the `mulli` it fixes.**  The walk this
// function forwards its iterators to reaches `fn_801FD8E0` for one element per turn and steps
// `addi r31,r31,0x24` (`src/MetroidPrime/ScriptObjects/Carve801FD858.c`, a `Matching` unit), and
// that file's claim starts exactly where this one ends, so the two are one another's witnesses.
// The same 0x24 is one of the four strides the block comment on
// `src/MetroidPrime/PortLinkStubs.cpp:1222-1229` tabulates for this address's family of
// destructors.  Nothing is asserted about the element's class: its members are not read here, so no
// struct is spelled and the buffer is walked as `unsigned char*`.
//
// **Its callee is real, not a stand-in.**  `fn_801FD858` (0x801FD858, 0x38) is defined by
// `src/MetroidPrime/ScriptObjects/Carve801FD858.c`, a `Matching` unit whose `.text` starts exactly
// where this claim ends, so `bl 0x801FD858` lands on a definition we wrote, in retail's bytes, and
// retiring this unit's stub in `src/MetroidPrime/PortLinkStubs.cpp` costs the port link nothing -
// the symbol is defined by that object instead.  `Free__7CMemoryFPCv` (0x802CE388) is
// `Kyoto/Alloc/CMemory.cpp`'s, also ours.
//
// **Its one caller** is `fn_801FD4B0` (0x801FD4B0, 0x7C,
// `src/MetroidPrime/ScriptObjects/Carve801FD4B0.cpp`, a `Matching` unit): the only `bl
// fn_801FD7D4` in the DOL is at 0x801FD4F0, `addi r3,r30,0x10` / `li r4,-1`, the third of that
// destructor's four member teardowns at +0x10/+0x20/+0x30.  So the flag arrives as MWCC's "destroy,
// do not free me afterwards", which is what `Carve801FD998.c` records for the same shape, and the
// receiver is a member at +0x10 of an unnamed class.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  With one function
// the rule cannot be violated here, and `tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/Carve801FD7D4` - which takes any unit name and resolves the object
// under `build/G2ME01/src/`, `.c` included - answers `ok: 1 unit(s) checked` and has nothing left to
// find; but it is the rule that keeps a later second function in this unit from breaking it, and
// `tools/flip_test.sh` is the check that actually decides.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// casts, which are compile-time only and leave the object byte-identical.
//
// Its own unit, and not merged with `Carve801FD858.c` below it, for two measured reasons.  **A claim
// may not span an unclaimed gap**: in front of this one `fn_801FD774` (0x801FD774, 0x60 =
// `symbols.txt:8275`) ends exactly at 0x801FD7D4 and is still unclaimed, left to dtk, and behind
// `Carve801FD858.c` starts exactly at 0x801FD858.  **And one translation unit cannot hold both
// declarations of `fn_801FD858`**: retail's caller here passes `&first`/`&last`, the addresses of
// this frame's slots, while the by-value copy inside that function dereferences r3/r4 itself, so
// the two spellings are `void fn_801FD858(const void*, const void*)` (this file's `extern`) and
// `void fn_801FD858(Iterator, Iterator)` (that file's).  One symbol cannot be declared both ways,
// which is what the item's `reason` predicted before either was compiled.
//
// The directory is retail's own, taken from the nearest claimed ranges: the last claimed range
// below this address is `ScriptObjects/Carve801FD67C.cpp` (0x801FD67C..0x801FD6F0) and the first
// above it is the `Matching` `ScriptObjects/Carve801FD8E0.c` (0x801FD8E0..0x801FD924), so the code
// is the `MetroidPrime/ScriptObjects/` neighbourhood, which is where the item's seeder put it and
// where the twin that fixes the shape lives three units along in it.

/** 0x802CE388, `symbols.txt:12992`: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp` (`.text` 0x802CE224..0x802CE72C), so our own tree supplies it.
 *  Declared, never defined here.  `src/Kyoto/Alloc/PortMwccNew.cpp:34` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** 0x801FD858, `symbols.txt:8277`, 0x38 = 56 bytes: the `rstl::destroy<It, It>` step this body
 *  hands its two iterators to, defined for real by `src/MetroidPrime/ScriptObjects/
 *  Carve801FD858.c` (a `Matching` unit whose claim starts exactly where this one ends).  Retail's
 *  bytes pass each argument as the address of a caller frame slot, which is why it is declared
 *  here as the two pointers it takes - the spelling `Carve801FD998.c` measured for its own twin
 *  `fn_801FDA1C` - rather than as a by-value struct, which needs a copy the caller does not
 *  build. */
extern void fn_801FD858(const void* first, const void* last);

/** The block this function tears down.  Only the two fields its bytes read are modelled: the count
 *  at +4 and the `void*` at +0xC - retail's `rstl::vector` layout, the same one `fn_801FD998`
 *  walks in `Carve801FD998.c`.  The words before them are padding so the fields land on retail's
 *  displacements; what they hold is not this unit's business, because the caller hands a subobject
 *  at a fixed offset inside a larger class. */
struct SCarve801FD7D4Block {
  int x00;
  unsigned int x04_count;
  int x08;
  void* x0c_data;
};

void* fn_801FD7D4(struct SCarve801FD7D4Block* self, short flag) {
  if (self) {
    unsigned int count = self->x04_count;
    unsigned char* first;
    unsigned char* volatile firstCopy;
    unsigned char* last;
    unsigned char* volatile lastCopy;
    unsigned char* end = (unsigned char*)self->x0c_data + count * 36;
    last = end;
    lastCopy = end;
    firstCopy = (unsigned char*)self->x0c_data;
    first = (unsigned char*)self->x0c_data;
    fn_801FD858(&first, &last);
    Free__7CMemoryFPCv(self->x0c_data);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}