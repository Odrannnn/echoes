// Retail `GetBounds__13CSimpleShadowCFv` = `_ZNK13CSimpleShadow9GetBoundsEv`,
// .text 0x800DF3DC..0x800DF458, 0x7C = 124 bytes. Carved out of dtk's
// `auto_03_800DC2F4_text` range; `SetAlwaysCalculateRadius` (0x800DF458) starts immediately
// above and `CSimpleShadowAccessors.cpp` claims the 4-byte `GetTransform` at 0x800DF478 above
// that. A unit may not claim two discontiguous ranges in one section, so this is its own file.
//
//     stwu r1,-48(r1) / mflr r0
//     lfs  f1,52(r4)          ; x34_radius
//     lfs  f0,48(r4)          ; x30_scale
//     addi r5,r1,8
//     lfs  f6,44(r4)          ; posZ
//     fmuls f7,f1,f0          ; radius * scale
//     lfs  f0,12(r4)          ; posX
//     lfs  f1,28(r4)          ; posY
//     addi r4,r1,20            ; &max
//     stfs f0,32(r1) / f1,36(r1) / f6,40(r1)   ; the translation, spilled
//     fadds f5,f1,f7 / f4,f6,f7 / f3,f0,f7
//     fsubs f2,f0,f7 / f1,f1,f7 / f0,f6,f7
//     stfs f3,8(r1) / f5,12(r1) / f4,16(r1)     ; first corner
//     stfs f2,20(r1) / f1,24(r1) / f0,28(r1)    ; second corner
//     bl   __ct__6CAABoxFRC9CVector3fRC9CVector3f
//
// **NonMatching at 76.65%, and the 24% is one frame decision, not logic.** Every store, every
// `fadds`/`fsubs`, the dead `addi r5,r1,8`, the register assignment and the two `addi rX,r1,N`
// corner addresses are byte-identical to retail. What is left is the frame: retail allocates 48
// bytes and spills the translation to sp+32/36/40, so its frame holds **three** `CVector3f` (nine
// floats) where this compiles to two and a 32-byte frame.
//
// So the translation really is a third stack object in retail that is written and then read
// straight back out of `this` - the loads at +0x0C/+0x1C/+0x2C are CSE'd against the spill, not
// against the spill's absence. Nine source shapes were tried (three-argument `CAABox`, named
// temporaries, `operator+`/`operator-` with a `CVector3f` and with a scalar, `+=`/`-=` on locals,
// default-construct-then-assign, and a static helper taking `const CVector3f&`); the ones that
// keep the stores at sp+8 and sp+20 all produce the 32-byte frame, and the ones that materialise
// a third `CVector3f` lose the store addresses instead (25.97%, 49.16%, 60.65%, 6.0%). The
// arrangement that scores highest is the last one below.
//
// The min/max order is measured rather than assumed: mwcceppc evaluates the two `CAABox`
// arguments right to left, so the **second** argument's `CVector3f` is the one that lands at
// sp+8. Retail's sp+8 holds `pos + r` and sp+20 holds `pos - r`, which is what
// `CAABox(pos - r, pos + r)` produces - the intuitive order, with the store addresses coming out
// the other way round. `CAABox(pos + r, pos - r)` scores 60.84% and puts the `fadds` results at
// sp+20.
//
// `docs/research/port_link_gap_list.md` lists `_ZNK13CSimpleShadow9GetBoundsEv`; this file is the
// groundwork for it (the `splits.txt` range and dtk's retail object are in place, so
// `tools/try_batch.py src/MetroidPrime/CSimpleShadowGetBounds.cpp
// MetroidPrime/CSimpleShadowGetBounds GetBounds <variants.py>` iterates on it directly).
#include "MetroidPrime/CSimpleShadow.hpp"

#include "Kyoto/Math/CAABox.hpp"

CAABox CSimpleShadow::GetBounds() const {
  const CVector3f t = x0_xf.GetTranslation();
  const float r = x34_radius * x30_scale;
  const CVector3f d(r, r, r);
  return CAABox(t - d, t + d);
}
