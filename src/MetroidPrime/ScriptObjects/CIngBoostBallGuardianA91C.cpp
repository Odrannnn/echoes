// CIngBoostBallGuardianA91C.cpp - IngBoostBallGuardian's (module 30) vector cross product, `.text`
// 0xA91C..0xA95C: one leaf function, 0x40 bytes.
//
//   0xA91C fn_30_A91C 0x40  six `lfs` out of two 12-byte operands, three `fmuls`, three `fmsubs`
//                           and three `stfs`
//
// **The body is a cross product and the operand order in the source is load-bearing.** Retail is
//
//   fmuls  f0, f7, f5        f7 = b.y, f5 = a.z   ->  b.y * a.z
//   fmuls  f1, f2, f3        f2 = b.z, f3 = a.x   ->  b.z * a.x
//   fmuls  f0, f6, f4        f6 = b.x, f4 = a.y   ->  b.x * a.y
//
// so each subtrahend is written `b_component * a_component`, **not** the commutative
// `a_component * b_component`. Swapping them back (the natural `az * by`) compiles to
// `fmuls f0, f5, f7` and is two bytes out on two of the three products; that was measured, and the
// same operand order is what MWCC's register assignment needs to keep all six loads live in f3..f7
// and produce retail's one-block schedule.
//
// **All six operands have to be named locals.** Writing the three expressions directly over
// `av[i]`/`bv[i]` makes MWCC reload the vectors once per component and produces three separate
// five-instruction blocks instead of retail's single sixteen-instruction one - measured, and the
// reason the six `const float` locals below are load-bearing rather than decorative.
//
// **Leaf**, measured on `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00000130_text.o`: its
// `.rela.text` holds nothing in 0xA820..0xA860, the range this function occupies, so the bytes are
// the whole of the claim and there is no callee to declare.
//
// **No dead-strip hazard, and that is measured.** fn_30_A91C is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but `auto_00_00011CD8_text.o`
// calls it five times (+0x1310, +0x14F8, +0x1734, +0x17B8, +0x42B0, each inside an unclaimed
// function), so dtk's own object holds the reference and this unit's `.text` survives the link.
//
// In `files.cmake` with an empty host branch, as `CIngBoostBallGuardianBits.cpp` explains.
// Definitions are in descending retail text order (one function).

extern "C" {

#ifdef __MWERKS__

// .text 0xA91C, 0x40 bytes. `out` in r3, the two operands in r4 and r5; see the note above.
void fn_30_A91C(void* out, const void* a, const void* b) {
  const float* av = static_cast< const float* >(a);
  const float* bv = static_cast< const float* >(b);
  float* r = static_cast< float* >(out);
  const float ax = av[0];
  const float ay = av[1];
  const float az = av[2];
  const float bx = bv[0];
  const float by = bv[1];
  const float bz = bv[2];
  r[0] = ay * bz - by * az;
  r[1] = az * bx - bz * ax;
  r[2] = ax * by - bx * ay;
}

#endif
}