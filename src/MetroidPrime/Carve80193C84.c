// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x80193C84..0x80193C8C, 0x8 = 8 bytes, 1 function:
//
//   fn_80193C84    0x80193C84  0x8    lwz       r3, 0x8(r3)
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  (One function, so the rule is vacuous
// here; it is stated because a second function would make it matter.)
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit
// is a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial.
//
// **What the twin settles.**  `goal_seed.py` paired this with the matched
// `MakeMsg::GetParmDeleteIOWin` (`src/MetroidPrime/Decode.cpp:4`), whose body is
//
//     return *static_cast< const CArchMsgParmString* >(msg.GetParm());
//
// i.e. an inline accessor that loads a pointer member at `+0x8` and hands it back.  Retail
// `fn_80193C84` is the same load **without** the outer dereference - it returns the stored
// pointer itself - so this is the shape of `msg.GetParm()` on its own.
//
// **The neighbour confirms it.**  `fn_80193C8C`, the very next function in this same
// `auto_03_80193C84_text` range, does `stw r3, 0x8(r29)` at 0x80193CBC with `r29` its own
// `this`, i.e. it writes the member this one reads; it then reads it back at 0x80193CD4.
// `fn_80193C84` has no caller anywhere in the DOL (`build/binutils/powerpc-eabi-objdump -d
// build/G2ME01/main.elf` shows the single definition and no `bl`), so the byte shape and
// the neighbour are the whole of the evidence - which is exactly what a carve is allowed
// to rest on, because the bytes are the result.
//
// The directory is retail's own, taken from the nearest claimed range.  The claim
// immediately below this one, `MetroidPrime/Carve80193C7C.c`, ends **exactly** at
// 0x80193C84 - the "carve placed where a neighbour's `.text` ends" shape of
// `MetroidPrime/Carve80003858.c` - and the nearest non-carve claim below that is
// `MetroidPrime/Player/CGMDeathMatch.cpp` (`.text` 0x801931F4..0x80193BD4), so this
// address is 0xB0 bytes past the end of that unit.  For an anonymous function that is the
// only evidence there is, and it beats a lane picking the directory it happened to own.
//
// The member is at `+0x8` by retail's load displacement, so this reads exactly what retail
// reads and adds nothing: no assumption beyond the pointer the caller passed in r3.
void* fn_80193C84(void* self) { return *(void**)((unsigned char*)self + 8); }
