/**
 * `CModel::Touch(int) const` - retail `Touch__6CModelCFi`, 0x803112DC, 0x4C = 76 bytes, 19
 * instructions: three calls and a standard two-register prologue.
 *
 * ```
 * 803112dc  stwu    r1,-16(r1)
 * 803112e0  mflr    r0
 * 803112e4  stw     r0,20(r1)
 * 803112e8  stw     r31,12(r1)
 * 803112ec  mr      r31,r4        ; the part index
 * 803112f0  stw     r30,8(r1)
 * 803112f4  mr      r30,r3        ; this
 * 803112f8  bl      fn_80310F38   ; (this)
 * 803112fc  mr      r3,r30
 * 80311300  mr      r4,r31
 * 80311304  bl      fn_803115F8   ; (this, part)
 * 80311308  lwz     r3,40(r30)    ; this->x28
 * 8031130c  bl      fn_802BBDB8   ; (this->x28)
 * 80311310  lwz     r0,20(r1) ; lwz r31,12(r1) ; lwz r30,8(r1)
 * 8031131c  mtlr    r0 ; addi r1,r1,16 ; blr
 * ```
 *
 * The first two callees are unnamed in `config/G2ME01/symbols.txt` (`fn_80310F38`, 0x30 bytes, and
 * `fn_803115F8`, 0x8C) and are called through `extern "C"`, so the port-side spelling stays the
 * map's. The third is `fn_802BBDB8`, which reads a byte flag at +0x40 of its argument - that is
 * the only thing measured about the object at `this+0x28`, and nothing in the tree names its type,
 * so `CModel::x28` is an opaque pointer.
 *
 * This is the function `src/MetroidPrime/CModelTouchParts.cpp` (a `Matching` unit) calls from
 * `fn_80027B44` and `fn_80027AE8`, which `CGunEffectTouch.cpp` and `CGunEffectTouchAll.cpp` reach.
 * Those two units compiled and linked without it because they only *call* it; the port's link
 * asked for `_ZNK6CModel5TouchEi` and nothing defined it.
 */

#include "Kyoto/Graphics/CModel.hpp"

extern "C" void fn_80310F38(const CModel* self);
extern "C" void fn_803115F8(const CModel* self, int part);
extern "C" void fn_802BBDB8(void* target);

void CModel::Touch(int part) const {
  fn_80310F38(this);
  fn_803115F8(this, part);
  fn_802BBDB8(x28_touchTarget);
}
