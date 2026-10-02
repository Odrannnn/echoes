// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:8283-8284`, and the instructions are retail's own at
// `.text 0x801FDA1C..0x801FDAA4`, read before this claim existed with
//
//   build/binutils/powerpc-eabi-objdump -d --start-address=0x801FDA1C --stop-address=0x801FDAA4 \
//     build/G2ME01/main.elf
//
// (dtk had disassembled the disc into `build/G2ME01/asm/auto_03_801FD998_text.s`, which carries
// both functions as `.fn` blocks).  Once this unit is in the link that range holds our bytes, so
// the listing above can only confirm, never establish, what retail had.  The body below is the C
// those bytes are the compilation of.
//
// .text 0x801FDA1C..0x801FDAA4, 0x88 = 136 bytes, 2 functions:
//
//   fn_801FDA1C    0x801FDA1C  0x38    14 instructions
//   fn_801FDA54    0x801FDA54  0x50    20 instructions
//
// **What the two are: one deleting-destructor step, and the element walk it hands its iterators
// to.**  Retail names neither - `symbols.txt` carries the `fn_<addr>` placeholder for both - so
// every shape here is read off the call edges and the argument registers, and each is byte for byte
// a function this tree already holds, compared word by word with objdump, not recalled:
//
//   fn_801FDA1C  ==  fn_801FBD30 (0x801FBD30, 0x38, `ScriptObjects/Carve801FBC58.c`, a `Matching`
//                unit): **all 14 instruction words identical, the `bl` included** (`48 00 00 15`),
//                because the callee sits 0x14 bytes past the `bl` in both.  Its body is the
//                by-value **struct parameter copy** the `bl` forces - `lwz r5,0(r4)` / `addi
//                r4,r1,8` / `lwz r0,0(r3)` / `addi r3,r1,0xc` - i.e. each argument is dereferenced
//                into a frame slot and the address of the slot is what the callee gets, because
//                MWCC passes a struct that fits in a register by reference.  `fn_801FDBE0`
//                (0x801FDBE0, 0x38, `ScriptObjects/Carve801FDB5C.c`, a `Matching` unit) is the
//                same 14 words with the same `bl` word.
//   fn_801FDA54  ==  fn_801FDC18 (0x801FDC18, 0x50, `ScriptObjects/Carve801FDB5C.c`, a `Matching`
//                unit): **19 of 20 words identical**, the one difference being the element stride,
//                `addi r31,r31,0x2c` here against `addi r31,r31,0x30` there.  `fn_801FD5E8`
//                (0x801FD5E8, 0x50, `ScriptObjects/Carve801FD5E8.c`, a `Matching` unit) is the
//                same body again with `addi r31,r31,0x24`; its callee also sits 0x2C past its
//                `bl`, so the `bl` word `48 00 00 2d` is the same in all three.  That body is
//                `rstl::destroy_impl<It,It>` written out as C: the loop bound is an inequality
//                between two pointers (`lwz r0,0(r30)` / `cmplw r31,r0` / `bne`), not an index
//                against a count, and the two `It` parameters are what produce it - MWCC passes
//                each struct by reference, so r3 is dereferenced once into r31 and walked while r4
//                is kept in r30 and reloaded on every iteration.
//
// **The stride is 0x2C = 44 and that is the element size**, measured independently in
// `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c:43-50` from the two retail callers of the callee
// below (`fn_801FDA54` itself, and `fn_801FF96C`).  Nothing is asserted about the element's class:
// the receiver never appears in either body of this claim, so no struct is spelled and the element
// pointer is passed as `void*`.
//
// **Its callee is real, not a stand-in.**  `fn_801FDAA4` (0x801FDAA4, 0x20) is defined by
// `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`, a `Matching` unit whose `.text` starts exactly
// where this claim ends, so `bl 0x801FDA78` lands on a definition we wrote, in retail's bytes, and
// nothing in `src/MetroidPrime/PortLinkStubs.cpp` stands in for either symbol.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Here that is fn_801FDA54 first, then fn_801FDA1C.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh` - hence the explicit
// `void*` parameter on the callee, which is compile-time only and leaves the object byte-identical.
//
// Its own unit because a claim may not span an unclaimed gap, and it can join neither neighbour:
// in front of it `fn_801FD998` (0x801FD998, 0x84) is still unclaimed and left to dtk, behind it
// `fn_801FDAA4` is another unit's claim.  Both boundaries are function edges - 0x801FDA1C +
// 0x38 = 0x801FDA54, 0x801FDA54 + 0x50 = 0x801FDAA4.
//
// The directory is retail's own, taken from the nearest claimed ranges: this address sits between
// `ScriptObjects/Carve801FD924.cpp` (0x801FD924..0x801FD998) and `ScriptObjects/Carve801FDAA4.c`
// (0x801FDAA4..0x801FDAE8), so the code is the `MetroidPrime/ScriptObjects/` neighbourhood, which
// is where the item's seeder put it and where both functions' neighbours live.

/** 0x801FDAA4, `symbols.txt:8285`, 0x20 = 32 bytes: `rstl::destroy<T>` for the 0x2C-byte element,
 *  defined for real by `src/MetroidPrime/ScriptObjects/Carve801FDAA4.c`.  That file measures the
 *  element size from this function's two retail callers - one of which is `fn_801FDA54` above. */
extern void fn_801FDAA4(void* self);

/** The one pointer of `rstl::pointer_iterator`, which is what both functions' two parameters are.
 *  Retail names the iterator's class only inside mangled twin symbols elsewhere in the DOL; what
 *  these bytes need of it is exactly this - a struct of one pointer, passed by value. */
struct SCarve801FDA1CIterator {
  void* current;
};

void fn_801FDA54(struct SCarve801FDA1CIterator begin, struct SCarve801FDA1CIterator end);
void fn_801FDA1C(struct SCarve801FDA1CIterator begin, struct SCarve801FDA1CIterator end);

void fn_801FDA54(struct SCarve801FDA1CIterator begin, struct SCarve801FDA1CIterator end) {
  char* cur = (char*)begin.current;
  while (cur != (char*)end.current) {
    fn_801FDAA4(cur);
    cur += 0x2C;
  }
}

void fn_801FDA1C(struct SCarve801FDA1CIterator begin, struct SCarve801FDA1CIterator end) {
  fn_801FDA54(begin, end);
}
