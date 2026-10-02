// CIngBoostBallGuardian2094.cpp - IngBoostBallGuardian's (module 30) word reset, `.text`
// 0x2094..0x20A0: one leaf function, 0xC bytes.
//
//   0x2094 fn_30_2094 0xC  `li r0,0` / `stw r0,4(r3)` / `blr` - stores zero into the word at +0x04
//
// **Leaf**, measured on `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o`: its
// `.rela.text` holds nothing in 0x1F68..0x1F74, the range this function occupies, so the bytes are
// the whole of the claim and there is no callee to declare. `li r0,0` rather than a zero from
// memory is what makes it three instructions and not four, so the store is written as an ordinary
// `= 0` on an `int*` and not as a `memset` or a `CVector3f`-style clear.
//
// **No dead-strip hazard, and that is measured.** fn_30_2094 is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `auto_00_00000130_text.o`
// calls it at +0x1EAC from inside `fn_30_1FB4` (an unclaimed function), so dtk's own object holds
// the reference and this unit's `.text` survives the link. No `force_active:` entry is needed.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// .text 0x2094, 0xC bytes. `self` in r3.
void fn_30_2094(void* self) {
  *reinterpret_cast< int* >(static_cast< unsigned char* >(self) + 4) = 0;
}

#endif
}