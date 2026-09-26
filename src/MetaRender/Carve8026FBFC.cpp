// `CCubeRenderer::BeginScene`, carved out of dtk's `auto_03_8026F628_text` range. Retail .text
// 0x8026FBFC..0x8026FD7C, 0x180 = 384 bytes, one function: `BeginScene__13CCubeRendererFv`.
//
// **This is the function the frame loop calls.** It is slot 35 of `__vt__13CCubeRenderer`, offset
// 0x94, and 0x800061C0 dispatches `lwz r12,148(r12)` on `gpRender` once per frame
// (`docs/research/cube_renderer_vtable.md`). It is not a draw: it puts the GX pipe into the state
// every draw of the frame assumes - viewport, clear colour, cull, depth, blend, a 75-degree
// projection, the identity model matrix, the EFB pixel format and alpha update - and then hands
// over to the `CGraphics` scene begin. Nothing is drawn by this function and nothing here claims so.
//
// Every callee, read out of retail rather than assumed:
//
//   SetUseVideoFilter__9CGraphicsFb     named; `Matching` in `src/Kyoto/Graphics/Carve802BEC1C.cpp`
//   SetViewport__9CGraphicsFiiii        named; (0, 0, mViewport.mWidth, mViewport.mHeight)
//   fn_802C1F5C  0x34  `CGraphics::SetClearColor`: stores the colour to `.sbss` and calls
//                      `GXSetCopyClear`. Retail passes a stack `CColor` built with four `stb`s.
//   fn_802C1608  0x24  `CGraphics::SetCullMode`: stores the mode and calls `GXSetCullMode`.
//   fn_802C162C  0x2C  `CGraphics::SetDepthWriteMode(test, comp, write)` - three arguments.
//   fn_802C15E8  0x20  `CGraphics::SetBlendMode`, the wrapper `Carve8026E7F0.cpp` already calls.
//   fn_802C235C  0xC4  `CGraphics::SetPerspective(fovy, aspect, znear, zfar)`.
//   SetModelMatrix__9CGraphicsFRC12CTransform4f   named; with `CTransform4f::sIdentity`.
//   fn_802BF640  0x68  a frame counter: `lbl_804199D4 = (n + 1) % 62192`, and a float of it.
//   fn_802C420C  0x3C  a `CTexture` release: `ScheduleDeletion(ForceSyncMRAM())` on its +0x44
//                      `CARAMToken` unless that is in state 6.
//   __dt__8CTextureFv                   named; with `r4 = 1`, i.e. `delete`.
//   GXSetPixelFmt, GXSetAlphaUpdate     the SDK, which on the port is Aurora.
//   SetDstAlpha__3CGXFbUc               named; `src/Kyoto/Graphics/CGX.cpp`.
//   fn_802C1E60  0x20  a one-call forwarder to `fn_802C1E80`, the `CGraphics` scene begin.
//
// `.sdata2` constants, by dtk's labels: `lbl_8041DFBC` 75.0f, `lbl_8041DFC0` 1.3333334f,
// `lbl_8041DEE4` 1.0f, `lbl_8041DEEC` 4096.0f.
//
// The eight bits at +0x554 are the header's `x554_24_`..`x554_31_`, named by `rlwimi` mask bit.
// What this function does with them is exactly what the source below says and no more; the
// Metroid Prime names for the same logic (`currentRGBA6`, `requestRGBA6`, ...) are not used,
// because Echoes clears a *different* bit from Prime's and a borrowed name would hide that.

#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/Math/CTransform4f.hpp"

#include <dolphin/gx.h>

extern "C" {
void fn_802C1F5C(const CColor& color);
void fn_802C1608(GXCullMode mode);
void fn_802C162C(bool test, GXCompare comp, bool write);
void fn_802C15E8(GXBlendMode, GXBlendFactor, GXBlendFactor, GXLogicOp);
void fn_802C235C(float fovy, float aspect, float znear, float zfar);
void fn_802BF640();
void fn_802C420C(CTexture* tex);
void fn_802C1E60();
}

void CCubeRenderer::BeginScene() {
  // 0x8026FC24-0x8026FC28: both viewport words are loaded *before* `SetUseVideoFilter`, so they
  // are read into locals first - a load cannot be moved across a call that might write it.
  int width = CGraphics::GetViewport().mWidth;
  int height = CGraphics::GetViewport().mHeight;
  CGraphics::SetUseVideoFilter(true);
  CGraphics::SetViewport(0, 0, width, height);
  fn_802C1F5C(CColor(static_cast< uchar >(0), 0, 0, 0));
  fn_802C1608(GX_CULL_FRONT);
  fn_802C162C(true, GX_LEQUAL, true);
  fn_802C15E8(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
  fn_802C235C(75.f, 1.3333334f, 1.f, 4096.f);
  CGraphics::SetModelMatrix(CTransform4f::Identity());
  fn_802BF640();

  // 0x8026FCB0-0x8026FCE8. The countdown is re-read after the store (`lwz r0,0x4f4`), which is
  // a decrement followed by a separate test, not a pre-decrement in the condition.
  if (x4f4_phazonSuitMaskCountdown != 0) {
    --x4f4_phazonSuitMaskCountdown;
    if (x4f4_phazonSuitMaskCountdown == 0) {
      fn_802C420C(x4f8_phazonSuitMask.get());
      x4f8_phazonSuitMask = nullptr;
    }
  }

  // 0x8026FCEC-0x8026FD4C.
  x554_27_ = x554_26_;
  if (!x554_30_) {
    x554_26_ = false;
  }
  GXSetPixelFmt(x554_27_ ? GX_PF_RGBA6_Z24 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  if (x554_28_) {
    x554_28_ = false;
  } else {
    GXSetAlphaUpdate(GX_TRUE);
  }
  CGX::SetDstAlpha(true, 0);
  fn_802C1E60();
}
