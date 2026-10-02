// CIngBoostBallGuardianPredicates.cpp - IngBoostBallGuardian's (module 30) state/flag predicate
// run, `.text` 0xC1A4..0xC240: seven functions, 0x9C bytes, contiguous in retail
// (`symbols.txt`: 0xC1A4 0xC, 0xC1B0 0x28, 0xC1D8 0x18, 0xC1F0 0x14, 0xC204 0x14, 0xC218 0x14,
// 0xC22C 0x14) and a claim may not span an unclaimed gap.
//
//   0xC1A4 fn_30_C1A4 0xC  one bit of the byte at +0x11ED
//   0xC1B0 fn_30_C1B0 0x28 one bit of +0x11EC, or the word at +0x115C positive
//   0xC1D8 fn_30_C1D8 0x18 the float at +0x11C0 greater than the float at +0x083C
//   0xC1F0 fn_30_C1F0 0x14 the word at +0x0AE4 equal to 4
//   0xC204 fn_30_C204 0x14 ... equal to 3
//   0xC218 fn_30_C218 0x14 ... equal to 2
//   0xC22C fn_30_C22C 0x14 the word at +0x1130 non-zero
//
// **Every one of the seven is leaf**, measured on
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o`: `.rela.text` holds nothing in
// 0xC0C8..0xC1E0, the range these seven occupy. So the bytes are the whole of the claim, and there
// is no callee to declare.
//
// **The three `== const` predicates and the `!= 0` one are three different MWCC idioms and all
// three were measured on a scratch file rather than assumed:**
//   `w == K`  ->  `subfic r0, r0, K` / `cntlzw r0, r0` / `srwi r3, r0, 5` / `blr`
//   `w != 0`  ->  `neg r0, r3` / `or r0, r0, r3` / `srwi r3, r0, 31` / `blr`
//   `f1 > f0` ->  `fcmpo cr0, f1, f0` / `mfcr r0` / `extrwi r3, r0, 1, 1` / `blr`
// and none of them is reachable from the others: `w == 0` compiles to `neg`/`andc`/`srwi 31`
// instead, so `fn_30_C22C` has to be spelled `!= 0` and not `!(w == 0)`. `cntlzw`+`srwi 5` is
// `r0 != 0` for a word, which is why the three equality predicates share a body apart from the
// `subfic` immediate, and `mfcr`+`extrwi` is how MWCC returns a comparison result as a `bool`.
//
// **`fn_30_C1B0` is `||` and the join point is the `li r4,1`.** Retail loads `li r4,0` before the
// first test, branches *over* it when either operand already decides, and puts `li r4,1` at the
// single join, which is the shape `a || b` gets; the `&&` spelling would need the accumulator to
// be reset on the second path and does not produce these ten instructions.
//
// **No dead-strip hazard, and that is measured.** None of the seven is in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but all seven are named by
// `auto_04_00000000_data.o` - the module's own vtable stores them at `.data`+0xA8, +0xB4, +0xC0,
// +0xCC, +0xD8, +0xE4 and +0xF0 - so dtk's object holds the reference and this unit's `.text`
// survives the link. No `force_active:` entry and no `config/G2ME01/config.yml` change is needed.
//
// This file is in `files.cmake` with an empty host branch, exactly as the header of
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order; `python3 tools/check_decl_order.py --unit
// MetroidPrime/ScriptObjects/CIngBoostBallGuardianPredicates.cpp`.

extern "C" {

#ifdef __MWERKS__

// .text 0xC22C, 0x14 bytes. `w` in r3.
bool fn_30_C22C(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  return *reinterpret_cast< const int* >(base + 0x1130) != 0;
}

// .text 0xC218, 0x14 bytes. `w` in r3.
bool fn_30_C218(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  return *reinterpret_cast< const int* >(base + 0xAE4) == 2;
}

// .text 0xC204, 0x14 bytes. `w` in r3.
bool fn_30_C204(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  return *reinterpret_cast< const int* >(base + 0xAE4) == 3;
}

// .text 0xC1F0, 0x14 bytes. `w` in r3.
bool fn_30_C1F0(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  return *reinterpret_cast< const int* >(base + 0xAE4) == 4;
}

// .text 0xC1D8, 0x18 bytes. `self` in r3; both operands are members of it.
bool fn_30_C1D8(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  const float* a = reinterpret_cast< const float* >(base + 0x11C0);
  const float* b = reinterpret_cast< const float* >(base + 0x083C);
  return *a > *b;
}

// .text 0xC1B0, 0x28 bytes. `self` in r3.
bool fn_30_C1B0(const void* self) {
  const unsigned char* base = static_cast< const unsigned char* >(self);
  const unsigned int* count = reinterpret_cast< const unsigned int* >(base + 0x115C);
  return ((base[0x11EC] >> 5) & 1) != 0 || static_cast< int >(*count) > 0;
}

// .text 0xC1A4, 0xC bytes. `self` in r3.
bool fn_30_C1A4(const void* self) {
  return (static_cast< const unsigned char* >(self)[0x11ED] & 2) != 0;
}

#endif
}