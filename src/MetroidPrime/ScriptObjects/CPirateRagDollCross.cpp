// CPirateRagDollCross.cpp - PirateRagDoll's (module 50) `fn_50_D80`, .text 0xD80..0xDC0: a
// three-float cross product.
//
// **This is not part of the head claim**, which is `CPirateRagDollRel.cpp` at .text 0x0..0x10C.
// It is its own unit because the range 0xD80..0xDC0 is not contiguous with the head: `fn_50_10C`
// through `fn_50_D4C` sit between them and are left unclaimed (see that file). A carve is
// contiguous per source, so the two are two units and two `Object(Matching, ...)` lines.
//
// Retail (`build/G2ME01/PirateRagDoll/asm/auto_00_0000010C_text.s`, and the leftover
// `build/G2ME01/PirateRagDoll/asm/MetroidPrime/ScriptObjects/CPirateRagDollCross.s` from an
// earlier run of this same item):
//
//     lfs f5,0x8(r4)  / lfs f7,0x4(r5) / lfs f3,0x0(r4) / lfs f2,0x8(r5)
//     fmuls f0,f7,f5 / fmuls f1,f2,f3 / fmsubs f2,f4,f2,f0 / fmuls f0,f6,f4
//     fmsubs f1,f5,f6,f1 / fmsubs f0,f3,f7,f0 / stfs x3 / blr
//
// **Two things about that listing are load-bearing and neither is visible in the arithmetic.**
//
// 1. `-fp_contract on` (which the REL cflags carry, `cflags_rel` in `configure.py`) folds each
//    `x*y - z*w` into one `fmsubs`. Write the difference with a plain `-` and the compiler does
//    it; there is no source-level annotation to add.
// 2. **The operand order inside two of the three `fmuls` is what distinguishes retail from the
//    textbook form, and it is invisible to every percentage this repo can print.** Retail emits
//    `fmuls f0,f7,f5`, `fmuls f1,f2,f3`, `fmuls f0,f6,f4` - the first and third products are
//    written **b-component first**, only the second a-component first. All three textbook
//    spellings (`ay*bz - az*by`, `az*bx - ax*bz`, `ax*by - ay*bx`) give the same 0x40 bytes, the
//    same arithmetic, objdiff at 100%, `unit_fit.sh` "fits", and **still break the module's
//    sha1 on four bytes**. Multiplication is commutative, so only the encoder's operand order
//    separates them. The landed source below is `ay*bz - by*az`, `az*bx - bz*ax`,
//    `ax*by - bx*ay`, which is the b-first spelling of all three.
//
// The three operands are hoisted into locals before the arithmetic. Written as
// `av[1]*bv[2] - av[2]*bv[1]` and so on straight off the two pointers, the compiler re-loads
// each operand per component and emits **0x58 bytes** with three separate `fmuls`+`fmsubs`
// pairs; the `const float` locals give exactly 0x40 with retail's register assignment
// (f3/f4/f5 = a, f6/f7/f2 = b). This is the same finding as `CPirateRagDollCross.cpp`'s
// sibling spelling notes in `docs/RUNNING_THE_DECOMP.md` for other REL float helpers.
//
// The argument order is fixed by which operand retail loads into f3/f4/f5 (r4, `a`) and
// f6/f7/f2 (r5, `b`): `out = a.Cross(b)`, not `b.Cross(a)`. The result is a hidden return
// pointer in r3, so the function is written as one, exactly as `fn_45_2BBC` and `fn_22_5D4`
// are in their files.
//
// **No dead-strip hazard**: `fn_50_D80` is called by direct `bl` from `fn_50_4F8`
// (`bl fn_50_D80` at .text 0x6F8 and 0x72C), which *is* in the module's own
// `build/G2ME01/PirateRagDoll/ldscript.lcf` FORCEACTIVE block, so it survives the `.plf` link's
// `-strip_partial` without a `force_active:` entry in `config/G2ME01/config.yml`.
//
// Not in the head's `#ifdef __MWERKS__` pattern: this unit defines neither `RELMain` nor
// `RELExit`, so `tools/check_files_cmake.py` requires it in `files.cmake`. That is safe and
// measured - `powerpc-eabi-nm -u` on its object prints nothing (one self-contained float
// function, zero externals), so the port's undefined count does not move.

#include "Kyoto/Math/CVector3f.hpp"

extern "C" {
// .text 0xD80, 0x40 bytes: the module's cross-product helper.
void fn_50_D80(CVector3f* out, const CVector3f& a, const CVector3f& b) {
  const float ax = a.GetX(), ay = a.GetY(), az = a.GetZ();
  const float bx = b.GetX(), by = b.GetY(), bz = b.GetZ();
  out->SetX(ay * bz - by * az);
  out->SetY(az * bx - bz * ax);
  out->SetZ(ax * by - bx * ay);
}
} // extern "C"