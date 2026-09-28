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
 *
 * ## The three callees are named now, and the third argument is upstream's `mModelInstance`
 *
 * All three `fn_` names in `config/G2ME01/symbols.txt` at exactly the addresses above:
 *
 *   fn_80310F38 = 0x80310F38, 0x30 bytes = `UpdateLastFrame__6CModelCFv`
 *   fn_803115F8 = 0x803115F8, 0x8C bytes = `VerifyCurrentShader__6CModelCFi`
 *   fn_802BBDB8 = 0x802BBDB8, 0x11C bytes = `TryLockTextures__10CCubeModelCFv`
 *
 * The third one is a `CCubeModel` member function, so the pointer at `+0x28` is a `CCubeModel*`
 * and it is `CModel::mModelInstance` - which is what upstream's own `Touch` passes
 * (`src/Kyoto/Graphics/DolphinCModel.cpp:280`: `UpdateLastFrame(); VerifyCurrentShader(shader);
 * mModelInstance->TryLockTextures();`). The pre-merge header spelled the member `x28_touchTarget`
 * and typed it `void*`; upstream types it `rstl::single_ptr<CCubeModel>`.
 *
 * **The offset is the same and it is not a guess.** On the GameCube `rstl::single_ptr` and
 * `rstl::vector` are 4 and 0x10 bytes, so the declaration order in upstream's `CModel.hpp` puts
 * `mData` at 0x00, `mDataLen` at 0x04, `mSurfaces` at 0x08, `mMatSets` at 0x18, **`mModelInstance`
 * at 0x28**, `mLastFrame` at 0x2C and the three bit-fields at 0x30, for the
 * `CHECK_SIZEOF(CModel, 0x34)` the header carries - 0x28 is the fifth member, unchanged, and the
 * old header's own `char x20_pad[8]; void* x28_touchTarget;` agreed. (On the 64-bit *host* build
 * the same members are at 0/8/16/40/64, which is why nothing here is asserted with
 * `CHECK_OFFSETOF` - the tree's macro is a no-op off `__MWERKS__` anyway, `include/static_assert.hpp:30`.)
 *
 * The port keeps the three `extern "C"` calls rather than the named ones: this unit's job is
 * retail's instruction schedule at 0x803112DC, and the named methods are the same three calls at
 * the same three addresses.
 */

#include "Kyoto/Graphics/CModel.hpp"

extern "C" void fn_80310F38(const CModel* self);
extern "C" void fn_803115F8(const CModel* self, int part);
extern "C" void fn_802BBDB8(void* target);

void CModel::Touch(int part) const {
  fn_80310F38(this);
  fn_803115F8(this, part);
  // `single_ptr::get() const` hands back `T*`, so this is the `CCubeModel*` the old `void*`
  // member held - no cast, and no `const` on the pointee, which is what `TryLockTextures` wants.
  fn_802BBDB8(mModelInstance.get());
}
