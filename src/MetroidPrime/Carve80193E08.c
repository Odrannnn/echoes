// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `symbols.txt`, the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80193E08_text.s`, and the body below is the C those bytes are the
// compilation of.
//
// .text 0x80193E08..0x80193E34, 0x2C = 44 bytes, 1 function:
//
//   fn_80193E08    0x80193E08  0x2C    11 instructions
//
// **What it is.** `fn_80193E08` is the *constructor* of the 12-byte object that
// `CGameState`'s stream constructor allocates at `+0x19C`
// (`src/MetroidPrime/Player/CGameStateStreamCtor.cpp`: `li r3,12 ; bl new ; mr. r4,r3 ; beq ;
// bl fn_80193E08`, with a null test, so it is only reached on a non-null allocation). The
// identification is measured, not guessed:
//
//   * Both vtable addresses are in the DOL's `.data`, and the layout says what they are.
//     `lbl_803B0D68` is 0x68 bytes of which the first two words are zero and the third is
//     `0x80004798` - a **one-slot** vtable, whose occupant `fn_80004798` (0x48 bytes) is
//     exactly a *deleting destructor*: `mr. r31,r3 ; beq ; lis r5 ; addi r0,r5,3432 ; stw
//     r0,0(r31) ; ble ; bl Free__7CMemoryFPCv`.  So `lbl_803B0D68` is the vtable of a class
//     whose **only** virtual function is its own destructor.
//   * `lbl_803B5CB0` is also 0x68 bytes, also two zero header words, and then **24** slots -
//     all of them inside 0x80193C30..0x80193E04, i.e. the class's own virtuals sit either side
//     of this constructor.
//   * Retail's complete-object destructor for it is `fn_80193BD4` (0x54 bytes): it stores
//     `0x803B5CB0`, then stores `0x803B0D68` over it - the base subobject's destructor inlined,
//     with the dead `beq` mwcceppc emits in front of an implicit member destructor - and then
//     `Free`s the object when the destructor flag casts to a positive short.
//   * `fn_80193E08` is that constructor: base vtable store, own vtable store, then the object's
//     own three members zeroed - a byte at +4, a byte at +5 and a word at +8.  The base store
//     is dead, exactly as in the destructor: the second store overwrites it.
//
// The class is still unnamed, and the tree's own guess for it is
// `SGameStateMarker` (`include/.../CGameStateStreamCtor.cpp:289`, three `u32`s), which is
// **wrong in two of the three members**: +4 and +5 are written with `stb`, so they are bytes -
// two `bool`s - not words.  A 24-virtual, 12-byte class with a base whose only virtual is its
// destructor is a small interface implementation, and `fn_80193C8C` (one of the 24) calls
// `GetItemPercentageRatio__12CPlayerStateCFv` and stores 1/2/3 at `+8` by how that compares to
// 75 and 100, so `+8` is a difficulty tier and `+5` a "set" flag.  What names it is still open.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol
// split` fails with "Cyclic dependency ... link order"), and because the functions on either
// side of this run are not trivial.  `0x80193E04` immediately below is already
// `MetroidPrime/Carve80193E04.c`.
//
// The two vtables are **not** declared here and **not** defined here: `lbl_803B0D68` is
// defined by dtk's own `auto_07_803B0C00_data.o` and `lbl_803B5CB0` by
// `auto_07_803B5450_data.o`, both of which are in the link because the rest of the DOL needs
// them.  So the two `lis`/`addi` pairs below resolve to retail's own addresses, which is why
// this unit claims `.text` and nothing else.

// The two words at `+0` are vtable pointers, written with plain `lis`/`addi` against retail's
// own `.data`; `+4` and `+5` are bytes and `+8` is a word.
extern const int lbl_803B0D68[];
extern const int lbl_803B5CB0[];

void fn_80193E08(void* self) {
  *(unsigned int*)self = (unsigned int)&lbl_803B0D68[0];
  *(unsigned int*)self = (unsigned int)&lbl_803B5CB0[0];
  *(unsigned char*)((unsigned char*)self + 4) = 0;
  *(unsigned char*)((unsigned char*)self + 5) = 0;
  *(unsigned int*)((unsigned char*)self + 8) = 0;
}
