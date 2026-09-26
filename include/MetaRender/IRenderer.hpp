#ifndef _IRENDERER
#define _IRENDERER

#include "types.h"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

#include "Kyoto/TToken.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"

class CTransform4f;
class CParticleGen;
class IObjectStore;
class COsContext;
class CMemorySys;
class IFactory;
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
class CMetroidModelInstance;
class CAreaOctTree;

class IRenderer {
public:
  typedef void (*TDrawableCallback)(const void*, const void*, int);
  // `CacheReflection__13CCubeRendererFPFPvRC9CVector3f_vPvb`.
  typedef void (*TReflectionCallback)(void*, const CVector3f&);

  // `SetDebugOption__13CCubeRendererFQ29IRenderer12EDebugOptioni` names the type and nothing
  // in the tree reads a value of it, so no enumerator is claimed.
  enum EDebugOption {
    kDO_Unknown
  };
  
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

  // **The slot order below is retail's, read out of `__vt__13CCubeRenderer`** (`.data`
  // 0x803B8C10; the full table is `docs/research/cube_renderer_vtable.md`). It used to be Metroid
  // Prime's order, which has a `SetClippingPlanes` at slot 24 that Echoes does not, so every
  // slot from 24 on was one late: **`BeginScene` was slot 36 in this header and is slot 35 in
  // retail**, which is the `lwz r12,148(r12)` the frame loop dispatches at 0x800061C0. The total
  // still came to 78 only because `CCubeRenderer` padded the tail with nine invented virtuals.
  //
  // **Every slot is pure**, because retail's own `IRenderer` table - `lbl_803B0C1C`, `.data`
  // 0x803B0C1C, 0x140 bytes, stored at +0x00 by `CCubeRenderer`'s constructor (0x8027124C) and
  // restored by its destructor (0x80270A2C) - is two header words and **78 zero slots**. mwcceppc
  // writes 0 for a pure virtual, so a zero table is an all-pure interface, destructor included.
  //
  // Where retail's mangled name is known the signature is retail's. The slots retail names
  // `fn_<address>` are named `UnkNN` after their slot, with the address beside them; the four
  // older names a caller already uses (`UnkA`/`UnkB`/`UnkH`/`UnkI`/`UnkL`, and `UnkC`/`UnkD`) are
  // kept at the slot the caller reads, which is what the caller's bytes pin.
  virtual ~IRenderer() = 0;                                            //  0 0x80270848
  virtual void AddStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry,
                                 const CAreaOctTree* octTree, int areaIdx) = 0;  //  1
  virtual void EnablePVS(const CPVSVisSet& set, int areaIdx) = 0;      //  2
  virtual void DisablePVS() = 0;                                       //  3
  virtual void UnkA() = 0;                                             //  4 0x80264194
  virtual void RemoveStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry) = 0; // 5
  virtual void DrawUnsortedGeometry(int areaIdx, int mask, int targetMask) = 0;  //  6 0x8026411C
  virtual void DrawSortedGeometry(int areaIdx, int mask, int targetMask) = 0;    //  7 0x8026F214
  virtual void DrawStaticGeometry(int areaIdx, int mask, int targetMask) = 0;    //  8 0x8026402C
  virtual void DrawAreaGeometry(int areaIdx, int mask, int targetMask) = 0;      //  9 0x8026A368
  virtual void PostRenderFogs() = 0;                                   // 10 0x802640A4
  virtual void UnkB(int areaIdx, int mask, int targetMask) = 0;        // 11 0x80263FB8
  virtual void UnkC() = 0;                                             // 12 0x80263EAC
  virtual void UnkD() = 0;                                             // 13 0x8026B9FC
  virtual void SetModelMatrix(const CTransform4f& xf) = 0;             // 14
  virtual void AddParticleGen(const CParticleGen& gen) = 0;            // 15
#ifdef TARGET_PC
  // Port: `AddParticleGen2` was a placeholder for this overload. It occupies the
  // same vtable slot, so naming it recovers the interface the derived classes
  // already override without changing the object layout. Metroid Prime's
  // recovered IRenderer declares the same two overloads.
  virtual void AddParticleGen(const CParticleGen& gen, const CVector3f&, const CAABox&) = 0; // 16
#else
  virtual void AddParticleGen2() = 0;                                  // 16 0x8026FA60
#endif
  virtual void AddPlaneObject(const void* obj, const CAABox& aabb, const CPlane& plane,
                              int type) = 0;                           // 17
  virtual void AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds, int mode,
                           IRenderer::EDrawableSorting sorting) = 0;   // 18
  // The second parameter is `const void*`, not `void*`: retail's mangled name is
  // `SetDrawableCallback__13CCubeRendererFPFPCvPCvi_vPCv`, and the trailing `C` is exactly
  // that difference - `void*` spells `Pv` and gives `...PCvi_vPv`, which is a *different
  // symbol* and leaves retail's vtable (which is real, at `.data` 0x803B7AE0) pointing at a
  // name nothing defines. The function-pointer parameter needs no change: the typedef
  // `void (*)(const void*, const void*, int)` already mangles to `PFPCvPCvi_v`.
  virtual void SetDrawableCallback(TDrawableCallback cb, const void* ctx) = 0;  // 19
  virtual void SetWorldViewpoint(const CTransform4f& xf) = 0;          // 20
  // Retail overloads the name: `SetPerspective__13CCubeRendererFfffff` (five floats, slot 21)
  // and `SetPerspective__13CCubeRendererFffff` (four, slot 22).
  virtual void SetPerspective(float, float, float, float, float) = 0;  // 21
  virtual void SetPerspective(float, float, float, float) = 0;         // 22
  virtual rstl::pair< CVector2f, CVector2f > SetViewportOrtho(bool centered, float znear,
                                                              float zfar) = 0;  // 23
  virtual void SetViewport(int left, int right, int width, int height) = 0;     // 24
  virtual void SetDepthReadWrite(bool read, bool update) = 0;          // 25
  virtual void SetBlendMode_AdditiveAlpha() = 0;                       // 26
  virtual void SetBlendMode_AlphaBlended() = 0;                        // 27
  virtual void SetBlendMode_NoColorWrite() = 0;                        // 28
  virtual void SetBlendMode_ColorMultiply() = 0;                       // 29
  virtual void SetBlendMode_InvertDst() = 0;                           // 30
  virtual void SetBlendMode_InvertSrc() = 0;                           // 31
  virtual void SetBlendMode_Replace() = 0;                             // 32
  virtual void SetBlendMode_AdditiveDestColor() = 0;                   // 33
  virtual void SetDebugOption(EDebugOption option, int value) = 0;     // 34
  virtual void BeginScene() = 0;                                       // 35 0x8026FBFC
  virtual void EndScene() = 0;                                         // 36
  // This one **is** virtual, and it has to stay that way: retail's `__vt__13CCubeRenderer`
  // puts `CCubeRenderer::BeginPrimitive` at slot 37 (offset 0x9C, read out of
  // `build/G2ME01/main.elf`), and four functions in `NonMatching` units read a later slot by
  // byte offset - `CStateManager::fn_80039CCC` does `lwz r12,316(r12)`, which is slot 77.
  // Removing this virtual to make the five `Begin*` callers emit a direct call moved that to
  // 312 and cost four functions their 100.00%.
  virtual void BeginPrimitive(EPrimitiveType prim, int count) = 0;     // 37
  virtual void BeginLines(int nverts) = 0;                             // 38
  virtual void BeginLineStrip(int nverts) = 0;                         // 39
  virtual void BeginTriangles(int nverts) = 0;                         // 40
  virtual void BeginTriangleStrip(int nverts) = 0;                     // 41
  virtual void BeginTriangleFan(int nverts) = 0;                       // 42
  virtual void PrimVertex(const CVector3f& vtx) = 0;                   // 43
  virtual void PrimNormal(const CVector3f& nrm) = 0;                   // 44
  virtual void PrimColor(float r, float g, float b, float a) = 0;      // 45
  virtual void PrimColor(const CColor& color) = 0;                     // 46
  virtual void EndPrimitive() = 0;                                     // 47
  virtual void SetAmbientColor(const CColor& color) = 0;               // 48
  virtual void DrawString(const char* str, int x, int y) = 0;          // 49
  virtual void GetFPS() = 0;                                           // 50
  virtual void CacheReflection(TReflectionCallback cb, void* ctx, bool clearAfter) = 0;  // 51
  virtual void DrawSpaceWarp(const CVector3f& pt, float strength) = 0; // 52
  virtual void Unk53() = 0;                                            // 53 0x8026B45C
  virtual void Unk54() = 0;                                            // 54 0x8026B2B8
  virtual void Unk55() = 0;                                            // 55 0x8026364C
  virtual void Unk56() = 0;                                            // 56 0x80262E04
  virtual void Unk57() = 0;                                            // 57 0x80267ED8
  virtual void Unk58() = 0;                                            // 58 0x802679DC
  virtual void Unk59() = 0;                                            // 59 0x80267A44
  virtual void SetWireframeFlags(int flags) = 0;                       // 60
  virtual void SetWorldFog(ERglFogMode mode, float startz, float endz,
                           const CColor& color) = 0;                   // 61
  virtual void Unk62() = 0;                                            // 62 0x8026CED8
  virtual void Unk63() = 0;                                            // 63 0x8026BE18
  virtual void Unk64() = 0;                                            // 64 0x80267214
  virtual void Unk65() = 0;                                            // 65 0x80264978
  virtual void Unk66() = 0;                                            // 66 0x80265D70
  virtual void Unk67() = 0;                                            // 67 0x8026A9C0
  virtual void GetStaticWorldDataSize() = 0;                           // 68
  virtual void Unk69() = 0;                                            // 69 0x8026A338
  virtual void Unk70() = 0;                                            // 70 0x8026A300
  virtual void Unk71() = 0;                                            // 71 0x80263D14
  virtual void UnkH(int) = 0;                                          // 72 0x80266B94
  virtual void UnkI() = 0;                                             // 73 0x80266B64
  virtual void Unk74() = 0;                                            // 74 0x8018B9D4
  virtual void Unk75() = 0;                                            // 75 0x80264F8C
  virtual void Unk76() = 0;                                            // 76 0x80263550
  virtual void UnkL(const CVector3f& pos, const CColor& color) = 0;    // 77 0x80262FE4
};

// Retail's symbol for this is `AllocateRenderer__FR12IObjectStoreR10COsContextR10CMemorySysR8IFactory`
// (`config/G2ME01/symbols.txt` line 10822), which says two things this declaration got wrong and
// which nothing in the tree can see while the function has no body: it is at **global scope**, not
// in a namespace, and the fourth argument is **`IFactory&`**, not `IResFactory&`. Both matter now
// that the function is written - `src/MetaRender/Carve8026EF54.cpp` defines it, and a
// `Renderer::`-scoped or `IResFactory&` declaration would mangle to a name objdiff cannot pair
// with retail's. `src/MetroidPrime/main.cpp:57` already redeclared it correctly at global scope,
// which is why the port linked before: it was using its own declaration, not this one.
IRenderer* AllocateRenderer(IObjectStore&, COsContext&, CMemorySys&, IFactory&);

// A pure destructor still needs a body, because `~CCubeRenderer` calls it. Retail's calls nothing
// here - it restores `lbl_803B0C1C` into +0x00 and moves on - so the body is empty and inline.
inline IRenderer::~IRenderer() {}

#endif // _IRENDERER
