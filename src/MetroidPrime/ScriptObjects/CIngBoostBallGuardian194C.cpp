// CIngBoostBallGuardian194C.cpp - IngBoostBallGuardian's (module 30) structure initialiser,
// `.text` 0x194C..0x1988: one leaf function, 0x3C bytes.
//
//   0x194C fn_30_194C 0x3C  stores six floats from f1..f6, six words from r4..r9, then two floats
//                           from f7/f8 - fifteen `stfs`/`stw` and a `blr`, nothing else
//
// **The signature is read off the ABI, and it is what makes the body fifteen straight stores.**
// r3 is the object being built, so the first floating argument lands in f1 and the first integer
// argument in r4; retail stores f1..f6 at +0x00..+0x14, r4..r9 at +0x18..+0x2C and f7/f8 at
// +0x30/+0x34, which is exactly "six floats, then six ints, then two floats" as the parameter
// list - six floats take f1..f6, the six ints take r4..r9 with r3 already the object, and the last
// two floats take f7/f8. The layout of the object is therefore six floats, six ints, two floats.
//
// **Leaf**, measured on `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o`: its
// `.rela.text` holds nothing in 0x1818..0x1858, the range this function occupies, so the bytes are
// the whole of the claim and there is no callee to declare.
//
// **No dead-strip hazard, and that is measured.** fn_30_194C is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `auto_00_00000130_text.o`
// calls it at +0x1808 from an unclaimed function, so dtk's own object holds the reference and this
// unit's `.text` survives the link. No `force_active:` entry is needed.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// .text 0x194C, 0x3C bytes. `self` in r3; see the note above on the parameter order.
void fn_30_194C(void* self, float f0, float f1, float f2, float f3, float f4, float f5, int i0,
                int i1, int i2, int i3, int i4, int i5, float g0, float g1) {
  unsigned char* base = static_cast< unsigned char* >(self);
  float* head = reinterpret_cast< float* >(base);
  int* words = reinterpret_cast< int* >(base + 0x18);
  head[0] = f0;
  head[1] = f1;
  head[2] = f2;
  head[3] = f3;
  head[4] = f4;
  head[5] = f5;
  words[0] = i0;
  words[1] = i1;
  words[2] = i2;
  words[3] = i3;
  words[4] = i4;
  words[5] = i5;
  head[12] = g0;
  head[13] = g1;
}

#endif
}