#ifndef _CCUBERENDERER
#define _CCUBERENDERER

#include "types.h"

#include <dolphin/gx/GXEnum.h>

#include "MetaRender/IRenderer.hpp"
// #include "Weapons/IWeaponRenderer.hpp"

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector2f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/pair.hpp"

class CSkinnedModel;
class CModel;

// **`BeginPrimitive` stays `override`, and that costs this tree the five `Begin*` methods.**
// It is a measured trade, not an oversight. Retail's five `CCubeRenderer::Begin*` bodies reach
// `BeginPrimitive` with a direct `bl`, but `BeginPrimitive` *is* virtual: retail's
// `__vt__13CCubeRenderer` (`.data` 0x803B8C10, 0x150 bytes, 82 slots) holds it at slot 37,
// offset 0x9C. Three ways to get the direct call were tried and two are wrong:
//
//   * drop `virtual` from `IRenderer::BeginPrimitive` - the call becomes direct and all five
//     reach 100.00%, but it **removes a slot from the interface vtable** and four functions
//     that read a later slot by byte offset move: `CStateManager::fn_80039CCC` does
//     `lwz r12,316(r12)` (slot 77) and becomes `lwz r12,312(r12)`, and with it
//     `CActor::RenderInternal`, `CPlayerGun::fn_801D0CD0`, `fn_801D0D10` and
//     `CScriptForgottenObject::RenderInternal` all fall off 100.00%. Reverted.
//   * hide the base's `BeginPrimitive` with a non-virtual `CCubeRenderer::BeginPrimitive` of
//     the same name and a different signature - direct call, slot count preserved, but the
//     derived function then does **not** fill slot 37, which is wrong about retail.
//   * mark the class `final` so the call devirtualises - mwcceppc 2.7 with `-lang=c++` is
//     C++98 and rejects `final` as a syntax error, with or without `#pragma cpp_extensions on`.
//
// Retail had all of `CCubeRenderer`'s definitions in one translation unit, where mwcceppc
// devirtualises a call whose definition it can see; a carve cannot, because it would have to
// carry `BeginPrimitive`'s own 0x118 bytes. So the five `Begin*` methods at
// 0x8026ED44..0x8026EE0C are left for a lane that writes `BeginPrimitive` with them.
class CCubeRenderer : public IRenderer {
public:
  ~CCubeRenderer() override;
  // TODO types
  void AddStaticGeometry() override;
  void EnablePVS(const CPVSVisSet& set, int areaIdx) override;
  void DisablePVS() override;
  void UnkA() override;
  void RemoveStaticGeometry() override;
  void DrawUnsortedGeometry(int areaIdx, int mask, int targetMask) override;
  void DrawSortedGeometry(int areaIdx, int mask, int targetMask) override;
  void DrawStaticGeometry(int areaIdx, int mask, int targetMask) override;
  void DrawAreaGeometry(int areaIdx, int mask, int targetMask) override;
  void PostRenderFogs() override;
  void UnkB(int areaIdx, int mask, int targetMask) override;
  void UnkC() override;
  void UnkD() override;
  void SetModelMatrix(const CTransform4f& xf) override;
  void AddParticleGen(const CParticleGen& gen) override;
  void AddParticleGen(const CParticleGen& gen, const CVector3f&, const CAABox&) override;
  void AddPlaneObject(const void* obj, const CAABox& aabb, const CPlane& plane, int type) override;
  void AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds, int mode,
                   IRenderer::EDrawableSorting sorting) override;
  void SetDrawableCallback(TDrawableCallback cb, const void* ctx) override;
  void SetWorldViewpoint() override;
  void SetPerspective1(float, float, float, float, float) override;
  void SetPerspective2() override;
  rstl::pair< CVector2f, CVector2f > SetViewportOrtho(bool centered, float znear,
                                                      float zfar) override;
  void SetClippingPlanes(const CFrustumPlanes&) override;
  void SetViewport(int left, int right, int width, int height) override;
  void SetDepthReadWrite(bool read, bool update) override;
  void SetBlendMode_AdditiveAlpha() override;
  void SetBlendMode_AlphaBlended() override;
  void SetBlendMode_NoColorWrite() override;
  void SetBlendMode_ColorMultiply() override;
  void SetBlendMode_InvertDst() override;
  void SetBlendMode_InvertSrc() override;
  void SetBlendMode_Replace() override;
  void SetBlendMode_AdditiveDestColor() override;
  void SetDebugOption() override;
  void BeginScene() override;
  void EndScene() override;
  void BeginPrimitive(IRenderer::EPrimitiveType prim, int count) override;
  void BeginLines(int nverts) override;
  void BeginLineStrip(int nverts) override;
  void BeginTriangles(int nverts) override;
  void BeginTriangleStrip(int nverts) override;
  void BeginTriangleFan(int nverts) override;
  void PrimVertex(const CVector3f& vtx) override;
  void PrimNormal(const CVector3f& nrm) override;
  void PrimColor(float r, float g, float b, float a) override;
  void PrimColor(const CColor& color) override;
  void EndPrimitive() override;
  void SetAmbientColor(const CColor& color) override;
  void DrawString() override;
  void GetFPS() override;
  void CacheReflection() override;
  void DrawSpaceWarp() override;
  void DrawThermalModel() override;
  void DrawModelDisintegrate() override;
  void DrawModelFlat() override;
  void SetWireframeFlags() override;
  void SetWorldFog() override;
  void RenderFogVolume(const CColor&, const CAABox&, const TLockedToken< CModel >*,
                       const CSkinnedModel*) override;
  void SetThermal() override;
  void SetThermalColdScale() override;
  void DoThermalBlendCold() override;
  void DoThermalBlendHot() override;
  void GetStaticWorldDataSize() override;
  void SetGXRegister1Color() override;
  void SetWorldLightFadeLevel() override;
  void Something() override;
  void PrepareDynamicLights(const rstl::vector< CLight >& lights) override;
  virtual void UnkE();
  virtual void UnkF();
  virtual void UnkG();
  virtual void UnkH(int);
  virtual void UnkI();
  virtual void UnkJ();
  virtual void UnkK();
  virtual void UnkK2();
  virtual void UnkL(const CVector3f& pos, const CColor& color);

  void AllocatePhazonSuitMaskTexture();

  // The four members named here are the four that a body in this tree actually reads or
  // writes, and each is at retail's offset - measured with mwcceppc, not assumed:
  //
  //   x98_drawableCallback  +0x98   `SetDrawableCallback`   0x8026E7E4, 0xC
  //   x9c_drawableContext   +0x9C   `SetDrawableCallback`   0x8026E7E4, 0xC
  //   x34c_color            +0x34C  `PrimColor` x2           0x8026EF24 and 0x8026EF30
  //   x350_normal           +0x350  `PrimNormal`             0x8026ECDC, and `PrimVertex`
  //
  // **The pad starts at 0x04, not 0x08, and the `x8_` in the old name was wrong.** The first
  // member after the vtable is at 0x04: measured, `sizeof(CCubeRenderer)` is 0x35C and the four
  // offsets above come out where they should only with a 0x04 base - an `x8_pad` of 0x90
  // bytes lands the next member at 0x98, and from 0x08 it lands at 0x94. Every retail store in
  // the five carves in this directory writes +0x98 and +0x9C, so the header that says 0x08
  // was four bytes out on every one of them, and `PrimNormal` scored 99.57% because its
  // first `stfs` went to 844 instead of 848. Nothing in the tree reads the bitfields or the
  // pad, so the rename is a comment-level correction; the offsets are what moved, and they
  // moved to retail's.
  //
  // What is *not* claimed is that the rest of the padding is padding: the class is 0x35C
  // bytes as declared here, retail's `sizeof` is not measured, and the tail pad stops at the
  // last member any written body touches. The three bitfields keep the offset they had -
  // 0x314, which is also not what the old `x318_` names said.
  uchar x4_pad[0x94];
  TDrawableCallback x98_drawableCallback;
  const void* x9c_drawableContext;
  uchar xa0_pad[0x274];
  bool x314_24_ : 1;
  bool x314_25_ : 1;
  bool x314_26_ : 1;
  uchar x315_pad[0x37];
  CColor x34c_color;
  CVector3f x350_normal;
};

extern CCubeRenderer* gpRender;

#endif // _CCUBERENDERER
