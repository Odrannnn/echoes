// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from the dtk disassembly, the instructions are the
// ones dtk itself emitted into `build/G2ME01/asm/auto_03_800F15CC_text.s`, and the body below
// is the C those bytes are the compilation of.
//
// .text 0x800F1A10..0x800F1A18, 0x8 = 8 bytes, 1 function:
//
//   fn_800F1A10    0x800F1A10  0x4    lbz       r3, 0x8(r3)
//                  0x800F1A14  0x4    blr
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  (One function, so the rule is vacuous here;
// it is stated because a second function would make it matter.)
//
// Retail names none of these.  The DOL map carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the functions on
// either side of this run are not trivial: the claim above ends at 0x800F1A10 with
// `__ct__9CBSAttackFv`, and the claim below starts at 0x800F1A18 with `fn_800F1A18`.
//
// **What the body is.**  `fn_800F1A10` is an accessor: one unsigned-byte load out of the
// object at `+0x8` and an immediate return, with no caller, no callee and no data
// reference.  `build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` shows the
// symbol exactly once, as its own definition, and no `bl` to it anywhere in the DOL - so the
// load displacement is the whole of the evidence for the member's offset, and the offset is
// also the whole of the body.  That is enough for a carve precisely because the bytes
// *are* the result.
//
// **Why a byte and not a word.**  Retail's instruction is `lbz`, so the returned member is a
// byte at `+0x8`, and the neighbours in this same neighbourhood agree: `fn_800F1A24` writes
// it with `stb r0, 0x8(r31)` where `r31` is its own `this` and `r0` is `1` (0x800F1A68),
// `fn_800F1A84` writes `0` the same way (0x800F1C00), and `__ct__6CBSDieFv` zeroes it in its
// constructor (0x800F1C3C) in an object that is only 0xC bytes long.  A `bool` or an `int`
// return would compile to the same two instructions here - `lbz` already zero-extends into
// all of r3 - so the C below says `unsigned char` to match the load retail actually performs
// rather than to pin down a type the bytes cannot distinguish.
//
// The directory is retail's own, taken from the nearest claimed range.  The claim immediately
// below this one is `MetroidPrime/Carve800F15C8.c` (`.text` 0x800F15C8..0x800F15CC), and the
// nearest non-carve claim below that is `MetroidPrime/BodyState/CBodyStateInfo.cpp` (`.text`
// 0x800EF908..0x800F128C), so this address is 0x784 bytes past the end of that unit.  For an
// anonymous function that is the only evidence there is, and it beats a lane picking the
// directory it happened to own.
//
// This unit claims `.text` and nothing else: the function references no `.data`, no
// `.rodata` and no `.sdata2`, so there is no second section for the carve to take.
unsigned char fn_800F1A10(void* self) { return *(unsigned char*)((unsigned char*)self + 8); }
