// CIngBoostBallGuardianC6AC.cpp - IngBoostBallGuardian's (module 30) flag/level pair, `.text`
// 0xC6AC..0xC6CC: two contiguous leaf functions, 0x20 bytes.
//
//   0xC6AC fn_30_C6AC 0xC  one bit of the byte at +0x11ED
//   0xC6B8 fn_30_C6B8 0x14 the word at +0x06B4 equal to 3
//
// Both are leaf - `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o`'s `.rela.text`
// holds nothing in 0xC5B0..0xC5E8, the range these two occupy - so the bytes are the whole of the
// claim and there is no callee to declare. They are contiguous in retail (`symbols.txt`: 0xC6AC 0xC,
// 0xC6B8 0x14) and a claim may not span an unclaimed gap.
//
// `fn_30_C6B8` is the `subfic`/`cntlzw`/`srwi` idiom MWCC emits for `w == K`, the same one
// `CIngBoostBallGuardianPredicates.cpp` measures for its three equality predicates; `fn_30_C6AC` is
// the one-bit mask family, whose encoding is tabulated in `CIngBoostBallGuardianBits.cpp`'s header.
//
// **No dead-strip hazard, and that is measured.** Neither function is in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but both are named by
// `auto_04_00000000_data.o` - the module's vtable stores them at `.data`+0x18 and +0x0C - so dtk's
// object holds the reference and this unit's `.text` survives the link.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order.

extern "C" {

#ifdef __MWERKS__

// .text 0xC6B8, 0x14 bytes.
bool fn_30_C6B8(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  return *reinterpret_cast< const int* >(base + 0x6B4) == 3;
}

// .text 0xC6AC, 0xC bytes.
bool fn_30_C6AC(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x11ED] & 8) != 0;
}

#endif
}