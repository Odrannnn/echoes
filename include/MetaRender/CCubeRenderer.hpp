#ifndef _CCUBERENDERER
#define _CCUBERENDERER

#include "types.h"

#include <dolphin/gx/GXEnum.h>

#include "MetaRender/IRenderer.hpp"
#include "MetaRender/IWeaponRenderer.hpp"

#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CGraphicsPalette.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/Math/CFrustumPlanes.hpp"
#include "Kyoto/Math/CPlane.hpp"
#include "Kyoto/Text/CFont.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector2f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/list.hpp"
#include "rstl/pair.hpp"
#include "rstl/single_ptr.hpp"
#include "rstl/vector.hpp"

class CSkinnedModel;
class CModel;
class COsContext;
class CMemorySys;
class IFactory;
class IObjectStore;

// The element types of the three containers are not identified; the containers are. Each list
// destructor calls a per-node destructor (`fn_8027372C`, `fn_80273880`), so both elements have
// one; the vector's does not (`fn_80038F0C` frees the buffer and nothing else). The names are
// Metroid Prime's for the same members and are placeholders until a body that uses them lands.
struct SAreaListItem {
  ~SAreaListItem();
};
struct SFogVolumeListItem {
  ~SFogVolumeListItem();
};
struct SLightListItem {
  int x0_;
};

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
class CCubeRenderer : public IRenderer, public IWeaponRenderer {
public:
  // Retail's constructor is `fn_80271238` (0x80271238, 0x59C) - unnamed in `symbols.txt`, so
  // `src/MetaRender/Carve80271238.cpp` reaches it through an `extern "C"` wrapper. It reads `r4`
  // (the store) and `r7` (the factory) and nothing else; the middle two are dead in retail too.
  CCubeRenderer(IObjectStore& store, COsContext& osContext, CMemorySys& memorySys,
                IFactory& resFactory);
  ~CCubeRenderer() override;                                            //  0 0x80270848
  void AddStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry,
                         const CAreaOctTree* octTree, int areaIdx) override;  //  1 0x8026FE94
  void EnablePVS(const CPVSVisSet& set, int areaIdx) override;          //  2 0x802638FC
  void DisablePVS() override;                                           //  3 0x802638A8
  void UnkA() override;                                                 //  4
  void RemoveStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry) override; // 5
  void DrawUnsortedGeometry(int areaIdx, int mask, int targetMask) override;  //  6
  void DrawSortedGeometry(int areaIdx, int mask, int targetMask) override;    //  7
  void DrawStaticGeometry(int areaIdx, int mask, int targetMask) override;    //  8
  void DrawAreaGeometry(int areaIdx, int mask, int targetMask) override;      //  9
  void PostRenderFogs() override;                                       // 10
  void UnkB(int areaIdx, int mask, int targetMask) override;            // 11
  void UnkC() override;                                                 // 12
  void UnkD() override;                                                 // 13
  void SetModelMatrix(const CTransform4f& xf) override;                 // 14 0x8026FDEC
  // Overrides `IRenderer` slot 15 and `IWeaponRenderer` slot 1 at once; the second is reached
  // through retail's `@4@AddParticleGen__13CCubeRendererFRC12CParticleGen` thunk (0x80273F9C).
  void AddParticleGen(const CParticleGen& gen) override;                // 15 0x8026FAA4
#ifdef TARGET_PC
  void AddParticleGen(const CParticleGen& gen, const CVector3f&, const CAABox&) override;  // 16
#else
  void AddParticleGen2() override;                                      // 16 0x8026FA60
#endif
  void AddPlaneObject(const void* obj, const CAABox& aabb, const CPlane& plane, int type) override;
  void AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds, int mode,
                   IRenderer::EDrawableSorting sorting) override;       // 18 0x8026F7FC
  void SetDrawableCallback(TDrawableCallback cb, const void* ctx) override;  // 19 0x8026E7E4
  void SetWorldViewpoint(const CTransform4f& xf) override;              // 20 0x8026FD7C
  void SetPerspective(float, float, float, float, float) override;      // 21 0x8026EC20
  void SetPerspective(float, float, float, float) override;             // 22 0x8026EC00
  rstl::pair< CVector2f, CVector2f > SetViewportOrtho(bool centered, float znear,
                                                      float zfar) override;  // 23
  void SetViewport(int left, int right, int width, int height) override;     // 24
  void SetDepthReadWrite(bool read, bool update) override;              // 25
  void SetBlendMode_AdditiveAlpha() override;                           // 26
  void SetBlendMode_AlphaBlended() override;                            // 27
  void SetBlendMode_NoColorWrite() override;                            // 28
  void SetBlendMode_ColorMultiply() override;                           // 29
  void SetBlendMode_InvertDst() override;                               // 30
  void SetBlendMode_InvertSrc() override;                               // 31
  void SetBlendMode_Replace() override;                                 // 32
  void SetBlendMode_AdditiveDestColor() override;                       // 33
  void SetDebugOption(EDebugOption option, int value) override;         // 34 0x8026E78C
  void BeginScene() override;                                           // 35 0x8026FBFC
  void EndScene() override;                                             // 36 0x8026FB80
  void BeginPrimitive(IRenderer::EPrimitiveType prim, int count) override;  // 37
  void BeginLines(int nverts) override;                                 // 38
  void BeginLineStrip(int nverts) override;                             // 39
  void BeginTriangles(int nverts) override;                             // 40
  void BeginTriangleStrip(int nverts) override;                         // 41
  void BeginTriangleFan(int nverts) override;                           // 42
  void PrimVertex(const CVector3f& vtx) override;                       // 43
  void PrimNormal(const CVector3f& nrm) override;                       // 44
  void PrimColor(float r, float g, float b, float a) override;          // 45
  void PrimColor(const CColor& color) override;                         // 46
  void EndPrimitive() override;                                         // 47
  void SetAmbientColor(const CColor& color) override;                   // 48
  void DrawString(const char* str, int x, int y) override;              // 49
  void GetFPS() override;                                               // 50
  void CacheReflection(TReflectionCallback cb, void* ctx, bool clearAfter) override;  // 51
  void DrawSpaceWarp(const CVector3f& pt, float strength) override;     // 52
  void Unk53() override;
  void Unk54() override;
  void Unk55() override;
  void Unk56() override;
  void Unk57() override;
  void Unk58() override;
  void Unk59() override;
  void SetWireframeFlags(int flags) override;                           // 60
  void SetWorldFog(ERglFogMode mode, float startz, float endz, const CColor& color) override;
  void Unk62() override;
  void Unk63() override;
  void Unk64() override;
  void Unk65() override;
  void Unk66() override;
  void Unk67() override;
  void GetStaticWorldDataSize() override;                               // 68
  void Unk69() override;
  void Unk70() override;
  void Unk71() override;
  void UnkH(int) override;                                              // 72
  void UnkI() override;                                                 // 73
  void Unk74() override;
  void Unk75() override;
  void Unk76() override;
  void UnkL(const CVector3f& pos, const CColor& color) override;        // 77

  void AllocatePhazonSuitMaskTexture();

  // **`sizeof` is 0x560 = 1376**, measured three ways that agree: retail's `AllocateRenderer`
  // asks its pool for `li r3,1376` (0x8026EF70); the constructor's highest store is
  // `stw r6,1372(r30)` (0x8027175C); and mwcceppc reports `sizeof(CCubeRenderer)` as 0x560 for
  // this declaration (`lbl_sizeof_CCubeRenderer` in `src/MetaRender/Carve80271238.cpp`). This
  // header used to end at `x350_normal`, 0x35C = 860 bytes - **516 bytes short**, so every
  // `new`-free use of the class was fine and anything that sized or constructed it was wrong.
  //
  // Every member is pinned by an instruction in retail's constructor (`ctor`, 0x80271238) or
  // destructor (`dtor`, 0x80270848). The two self-pointer runs the first reading took for
  // anonymous words are `rstl::list`s: the destructor calls a list destructor on 0x1C and 0x330,
  // and `list`'s constructor writes exactly `start = end = prev = next = &prev, count = 0`.
  IFactory& x8_factory;                  // ctor 0x8027128C `stw r7,8(r30)` - the 4th argument
  IObjectStore& xc_store;                // ctor 0x80271290 `stw r31,12(r30)` - the 1st
  CFont x10_font;                        // ctor `bl fn_802BAD6C` f1=1.0; dtor `bl fn_802BAD30`
  int x18_;                              // ctor 0x802712A0 `stw r6,24(r30)` = 0
  rstl::list< SAreaListItem > x1c_areaListItems;  // ctor 0x802712B0-D4; dtor `bl fn_80273770`
  CFrustumPlanes x34_frustumPlanes;      // ctor `bl __ct__14CFrustumPlanes...` on r30+0x34
  TDrawableCallback x98_drawableCallback;  // ctor 0x802712E4 = 0; `SetDrawableCallback`
  const void* x9c_drawableContext;       // `SetDrawableCallback`; the ctor leaves it alone
  CPlane xa0_viewPlane;                  // ctor 0x80271310-34: normalised (0,1,0), then 0.f
  bool xb0_;                             // ctor 0x80271338 `stb r0,176(r30)` = 0
  // 0xB4. Never written by the constructor, and not padding: without it mwcceppc places the
  // first texture at 0xB4, and retail constructs it at `addi r3,r30,0xb8` (0x8027130C). The
  // measurement that found it put `sizeof` at 0x55C and `x4fc_bigRing` at 0x4F8.
  int xb4_;
  CTexture xb8_blackTex;                 // ctor `bl __ct__8CTexture...` (7,4,4,1); dtor 0x802709F4
  rstl::single_ptr< CTexture > x120_;    // ctor 0x80271348 = 0; dtor deletes it (`li r4,1`)
  CTexture x124_tex;                     // (3, 32, 32, 1)
  CTexture x18c_tex;                     // (1, 256, 256, 1)
  CTexture x1f4_tex;                     // (1, 32, 32, 1)
  CTexture x25c_tex;                     // (0, 16, 16, 1)
  CTexture x2c4_tex;                     // (0, 8, 8, 1)
  CRandom16 x32c_random;                 // ctor `bl __ct__9CRandom16FUi`, seed 20
  rstl::list< SFogVolumeListItem > x330_fogVolumes;  // ctor 0x802713D4-E8; dtor `bl fn_802738F0`
  int x348_;                             // ctor 0x802713EC = 2
  CColor x34c_color;                     // ctor = `CColor::White()`; `PrimColor`
  CVector3f x350_normal;                 // ctor = `CVector3f::sForwardVector`; `PrimNormal`
  CColor x35c_color;                     // ctor 0x80271438-44 `stb` ff,00,ff,ff: `CColor()`
  rstl::vector< SLightListItem > x360_;  // ctor 0x80271448-50 zeroes +4/+8/+C; dtor `bl fn_80038F0C`
  // 0x370. The constructor zeroes this word and never touches the 0x180 bytes after it, and the
  // destructor calls nothing on either - the shape of a `reserved_vector` of a trivially
  // destructible element with its count first. The element type is not identified, so it is
  // not claimed; the bytes are.
  int x370_count;                        // ctor 0x80271454 = 0
  uchar x374_items[0x180];
  int x4f4_phazonSuitMaskCountdown;      // ctor 0x80271458 = 0; `BeginScene` counts it down
  rstl::single_ptr< CTexture > x4f8_phazonSuitMask;  // ctor 0x8027145C; `BeginScene` frees it at 0
  TLockedToken< CTexture > x4fc_bigRing;           // "TXTR_BigRing"
  TLockedToken< CTexture > x508_darkWorldCloud;    // "TXTR_DarkWorldCloud"
  TLockedToken< CTexture > x514_scanSweepBar;      // "TXTR_ScanSweepBar"
  TLockedToken< CModel > x520_flatSphere;          // "CMDL_FlatSphere"
  TLockedToken< CModel > x52c_flatSphereLow;       // "CMDL_FlatSphereLow"
  TLockedToken< CModel > x538_flatCylinder;        // "CMDL_FlatCylinder"
  TLockedToken< CModel > x544_flatCylinderLow;     // "CMDL_FlatCylinderLow"
  // ctor 0x802716CC `stw r3,1360(r30)`, from `TXTR_DarkLightworldPalette` through
  // `fn_802711A4`; dtor `bl fn_802C3F50`, which is `CGraphicsPalette`'s deleting destructor
  // (it tests +0x4 against `sCurrentFrameCount` and frees the `single_ptr` at +0xC).
  rstl::single_ptr< CGraphicsPalette > x550_darkLightworldPalette;
  // 0x554. Eight bits, all cleared by the constructor (`rlwimi ...,r6,...` with r6 = 0 at
  // 0x802716F0-50). mwcceppc allocates the first-declared bit to the byte's top bit, so
  // `x554_24_` is mask 0x80. `BeginScene` is the only reader in the tree: it copies bit 26 into
  // bit 27, clears bit 26 unless bit 30 is set, picks the pixel format from bit 27, and either
  // clears bit 28 or enables alpha update.
  bool x554_24_ : 1;
  bool x554_25_ : 1;
  bool x554_26_ : 1;
  bool x554_27_ : 1;
  bool x554_28_ : 1;
  bool x554_29_ : 1;
  bool x554_30_ : 1;
  bool x554_31_ : 1;
  int x558_;                             // ctor 0x80271758 = 0
  int x55c_;                             // ctor 0x8027175C = 0 - the highest store, 0x55C + 4
};

extern CCubeRenderer* gpRender;

#endif // _CCUBERENDERER
