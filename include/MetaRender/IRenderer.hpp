#ifndef _IRENDERER
#define _IRENDERER

#include "types.h"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

#include "Kyoto/TToken.hpp"

class CTransform4f;
class CParticleGen;
class IObjectStore;
class COsContext;
class CMemorySys;
class CResFactory;
class CAABox;
class CPlane;
class CVector2f;
class CVector3f;
class CModel;
class CFrustumPlanes;
class CSkinnedModel;
class CColor;
class CLight;
class CPVSVisSet;

class IRenderer {
public:
  typedef void (*TDrawableCallback)(const void*, const void*, int);
  
  enum EDrawableSorting {
    kDS_SortedCallback,
    kDS_UnsortedCallback,
  };

  // Retail's `BeginPrimitive` takes this, not `GXPrimitive`: the mangled name of the one
  // CCubeRenderer definition of it is
  // `BeginPrimitive__13CCubeRendererFQ29IRenderer14EPrimitiveTypei`, i.e.
  // `IRenderer::EPrimitiveType`. The five callers in
  // `src/MetaRender/Carve8026ED44.cpp` pass 144/152/160/168/176, a stride of 8, so retail's
  // enum has more members than the five this tree names and the gaps are not guessed at
  // here - only the five values that are measured are. Naming the rest is not needed to
  // reproduce any body and would be invention.
  enum EPrimitiveType {
    kPT_Triangles = 144,
    kPT_TriangleStrip = 152,
    kPT_TriangleFan = 160,
    kPT_Lines = 168,
    kPT_LineStrip = 176,
  };

  virtual ~IRenderer();
  // TODO vtable
  
  virtual void AddStaticGeometry();
  virtual void EnablePVS(const CPVSVisSet& set, int areaIdx);
  virtual void DisablePVS();
  virtual void UnkA();
  virtual void RemoveStaticGeometry();
  virtual void DrawUnsortedGeometry(int areaIdx, int mask, int targetMask);
  virtual void DrawSortedGeometry(int areaIdx, int mask, int targetMask);
  virtual void DrawStaticGeometry(int areaIdx, int mask, int targetMask);
  virtual void DrawAreaGeometry(int areaIdx, int mask, int targetMask);
  virtual void PostRenderFogs();
  virtual void UnkB(int areaIdx, int mask, int targetMask);
  virtual void UnkC();
  virtual void UnkD();
  virtual void SetModelMatrix(const CTransform4f& xf);
  virtual void AddParticleGen(const CParticleGen& gen);
#ifdef TARGET_PC
  // Port: `AddParticleGen2` was a placeholder for this overload. It occupies the
  // same vtable slot, so naming it recovers the interface the derived classes
  // already override without changing the object layout. Metroid Prime's
  // recovered IRenderer declares the same two overloads.
  virtual void AddParticleGen(const CParticleGen& gen, const CVector3f&, const CAABox&);
#else
  virtual void AddParticleGen2();
#endif
  virtual void AddPlaneObject(const void* obj, const CAABox& aabb, const CPlane& plane, int type);
  virtual void AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds, int mode,
                           IRenderer::EDrawableSorting sorting);
  // The second parameter is `const void*`, not `void*`: retail's mangled name is
  // `SetDrawableCallback__13CCubeRendererFPFPCvPCvi_vPCv`, and the trailing `C` is exactly
  // that difference - `void*` spells `Pv` and gives `...PCvi_vPv`, which is a *different
  // symbol* and leaves retail's vtable (which is real, at `.data` 0x803B7AE0) pointing at a
  // name nothing defines. The function-pointer parameter needs no change: the typedef
  // `void (*)(const void*, const void*, int)` already mangles to `PFPCvPCvi_v`.
  virtual void SetDrawableCallback(TDrawableCallback cb, const void* ctx);
  virtual void SetWorldViewpoint();
  virtual void SetPerspective1(float, float, float, float, float);
  virtual void SetPerspective2();
  virtual rstl::pair< CVector2f, CVector2f > SetViewportOrtho(bool centered, float znear,
                                                              float zfar);
  virtual void SetClippingPlanes(const CFrustumPlanes&);
  virtual void SetViewport(int left, int right, int width, int height);
  virtual void SetDepthReadWrite(bool read, bool update);
  virtual void SetBlendMode_AdditiveAlpha();
  virtual void SetBlendMode_AlphaBlended();
  virtual void SetBlendMode_NoColorWrite();
  virtual void SetBlendMode_ColorMultiply();
  virtual void SetBlendMode_InvertDst();
  virtual void SetBlendMode_InvertSrc();
  virtual void SetBlendMode_Replace();
  virtual void SetBlendMode_AdditiveDestColor();

  virtual void SetDebugOption();
  virtual void BeginScene();
  virtual void EndScene();
  // This one **is** virtual, and it has to stay that way: retail's `__vt__13CCubeRenderer`
  // puts `CCubeRenderer::BeginPrimitive` at slot 37 (offset 0x9C, read out of
  // `build/G2ME01/main.elf`), and four functions in `NonMatching` units read a later slot by
  // byte offset - `CStateManager::fn_80039CCC` does `lwz r12,316(r12)`, which is slot 77.
  // Removing this virtual to make the five `Begin*` callers emit a direct call moved that to
  // 312 and cost four functions their 100.00%. `CCubeRenderer` is `final` instead, which
  // devirtualises the five calls without touching the slot count.
  virtual void BeginPrimitive(EPrimitiveType prim, int count);
  virtual void BeginLines(int nverts);
  virtual void BeginLineStrip(int nverts);
  virtual void BeginTriangles(int nverts);
  virtual void BeginTriangleStrip(int nverts);
  virtual void BeginTriangleFan(int nverts);
  virtual void PrimVertex(const CVector3f& vtx);
  virtual void PrimNormal(const CVector3f& nrm);
  virtual void PrimColor(float r, float g, float b, float a);
  virtual void PrimColor(const CColor& color);
  virtual void EndPrimitive();
  virtual void SetAmbientColor(const CColor& color);
  virtual void DrawString();
  virtual void GetFPS();
  virtual void CacheReflection();
  virtual void DrawSpaceWarp();
  virtual void DrawThermalModel();
  virtual void DrawModelDisintegrate();
  virtual void DrawModelFlat();
  virtual void SetWireframeFlags();
  virtual void SetWorldFog();
  virtual void RenderFogVolume(const CColor&, const CAABox&, const TLockedToken< CModel >*,
                       const CSkinnedModel*);
  virtual void SetThermal();
  virtual void SetThermalColdScale();
  virtual void DoThermalBlendCold();
  virtual void DoThermalBlendHot();
  virtual void GetStaticWorldDataSize();
  virtual void SetGXRegister1Color();
  virtual void SetWorldLightFadeLevel();
  virtual void Something();
  virtual void PrepareDynamicLights(const rstl::vector<CLight>& lights);
};

namespace Renderer {
IRenderer* AllocateRenderer(IObjectStore&, COsContext&, CMemorySys&, CResFactory&);
}; // namespace Renderer

#endif // _IRENDERER
