// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:8277-8278`, the instructions are retail's own at
// `.text 0x801FD858..0x801FD8E0`, read this run out of the disc
// (`orig/G2ME01/sys/main.dol`, via `tools/dol_read.py 0x801FD858 0x38` and `0x801FD890 0x50`, then
// `build/binutils/powerpc-eabi-objdump -D -b binary -EB -m powerpc:common --adjust-vma=0x801FD858`)
// and **not** out of `build/G2ME01/main.elf`, which holds our bytes once this unit is in the link.
// Before the claim existed these 136 bytes were the fourth and fifth `.fn` blocks of dtk's
// `build/G2ME01/asm/auto_03_801FD6F0_text.s`.  The body below is the C those bytes are the
// compilation of.
//
// .text 0x801FD858..0x801FD8E0, 0x88 = 136 bytes, 2 functions:
//
//   fn_801FD890    0x801FD890  0x50    20 instructions
//   fn_801FD858    0x801FD858  0x38    14 instructions
//
// **What the two are: one deleting-destructor step, and the element walk it hands its iterators
// to.**  Retail names neither - `symbols.txt` carries the `fn_<addr>` placeholder for both - so
// every shape here is read off the call edges and the argument registers, and each is byte for byte
// a function this tree already holds, compared word by word out of the disc, not recalled:
//
//   fn_801FD858  ==  fn_801FDA1C (0x801FDA1C, 0x38,
//                `src/MetroidPrime/ScriptObjects/Carve801FDA1C.c`, a `Matching` unit):
//                **all 14 instruction words identical, the `bl` included** (`48 00 00 15`, because
//                the callee sits 0x14 bytes past the `bl` in both).  Its body is the by-value
//                **struct parameter copy** the `bl` forces - `lwz r5,0(r4)` / `addi r4,r1,8` /
//                `lwz r0,0(r3)` / `addi r3,r1,0xc` - i.e. each argument is dereferenced into a
//                frame slot and the address of the slot is what the callee gets, because MWCC
//                passes a struct that fits in a register by reference.  `fn_801FDBE0` (0x801FDBE0,
//                0x38, `Carve801FDB5C.c`, also `Matching`) is the same 14 words with the same `bl`
//                word.
//   fn_801FD890  ==  fn_801FD5E8 (0x801FD5E8, 0x50,
//                `src/MetroidPrime/ScriptObjects/Carve801FD5E8.c`, a `Matching` unit):
//                **all 20 instruction words identical, the `bl` included**.  Its twin
//                `fn_801FDA54` (0x801FDA54, 0x50, `Carve801FDA1C.c`) is the same body with
//                `addi r31,r31,0x2c`, and `fn_801FDC18` (0x801FDC18, 0x50,
//                `Carve801FDB5C.c`) with `addi r31,r31,0x30`; those two differ from this one in 19
//                of 20 words, that one instruction being the only difference in any of the three.
//                That body is `rstl::destroy_impl<It,It>` written out as C: the loop bound is an
//                inequality between two pointers (`lwz r0,0(r30)` / `cmplw r31,r0` / `bne`), not
//                an index against a count, and the two `It` parameters are what produce it - MWCC
//                passes each struct by reference, so r3 is dereferenced once into r31 and walked
//                while r4 is kept in r30 and reloaded on every iteration.
//
// **The stride is 0x24 = 36 and that is the element size**, measured independently of the `addi` it
// fixes - by the two retail callers of the callee below.  One is this walk, `fn_801FD890` itself;
// the other is `fn_801FF7EC` (0x801FF7EC) at 0x801FF810, inside the `Matching`
// `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp`, and that file's own carve fixes 0x24 from the
// bytes it matches.  Ahead of this claim, `src/MetroidPrime/ScriptObjects/Carve801FD7D4.c`'s
// `mulli r0,r0,0x24` at 0x801FD804 is a second, independent measurement of the same 0x24, in the
// caller that hands this walk its two iterators.  Nothing is asserted about the element's class:
// the receiver never appears in either body of this claim, so no struct is spelled and the element
// pointer is passed as `void*`.
//
// **Its callee is real, not a stand-in.**  `fn_801FD8E0` (0x801FD8E0, 0x20,
//  `symbols.txt:8279`) is defined by `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c`, a `Matching`
// unit whose `.text` starts exactly where this claim ends, so `bl 0x801FD8E0` lands on a definition
// we wrote, in retail's bytes, and nothing in `src/MetroidPrime/PortLinkStubs.cpp` stands in for
// either symbol here.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Here that is `fn_801FD890` first, then `fn_801FD858`.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces both symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*` parameter on the callee, which is compile-time only and leaves the object byte-identical.
//
// Its own unit, and not merged with `Carve801FD7D4.c` in front of it, for two measured reasons.
// **A claim may not span an unclaimed gap**: behind it `fn_801FD8E0` is another unit's claim, and
// in front of it `Carve801FD7D4.c` ends exactly at 0x801FD858 (`symbols.txt:8277` puts
// `fn_801FD858` at 0x801FD858) while below that `fn_801FD774` (0x801FD774, 0x60) is still
// unclaimed and left to dtk.  **And one translation unit cannot hold both declarations of
// `fn_801FD858`**: retail's caller passes `&first`/`&last`, this address of a caller frame slot,
// while the function dereferences r3/r4 itself, so the two spellings are `void
// fn_801FD858(const void*, const void*)` (that file's `extern`) and `void
// fn_801FD858(Iterator, Iterator)` (this file's).  One symbol cannot be declared both ways.
//
// The directory is retail's own, taken from the nearest claimed ranges: the `Matching`
// `ScriptObjects/Carve801FD7D4.c` starts exactly at 0x801FD7D4 below and the `Matching`
// `ScriptObjects/Carve801FD8E0.c` at 0x801FD8E0 above, so the code is the
// `MetroidPrime/ScriptObjects/` neighbourhood, which is where the item's seeder put it and where
// both twins live.

/** 0x801FD8E0, `symbols.txt:8279`, 0x20 = 32 bytes: `rstl::destroy<T>` for the 0x24-byte element,
 *  defined for real by `src/MetroidPrime/ScriptObjects/Carve801FD8E0.c`, whose claim starts exactly
 *  where this one ends.  That file measures the element size from its own two retail callers - one
 *  of which is `fn_801FD890` above. */
extern void fn_801FD8E0(void* self);

/** The one pointer of `rstl::pointer_iterator`, which is what both functions' two parameters are.
 *  Retail names the iterator's class only inside mangled twin symbols elsewhere in the DOL; what
 *  these bytes need of it is exactly this - a struct of one pointer, passed by value. */
struct SCarve801FD858Iterator {
  void* current;
};

void fn_801FD890(struct SCarve801FD858Iterator begin, struct SCarve801FD858Iterator end);
void fn_801FD858(struct SCarve801FD858Iterator begin, struct SCarve801FD858Iterator end);

void fn_801FD890(struct SCarve801FD858Iterator begin, struct SCarve801FD858Iterator end) {
  char* cur = (char*)begin.current;
  while (cur != (char*)end.current) {
    fn_801FD8E0(cur);
    cur += 0x24;
  }
}

void fn_801FD858(struct SCarve801FD858Iterator begin, struct SCarve801FD858Iterator end) {
  fn_801FD890(begin, end);
}