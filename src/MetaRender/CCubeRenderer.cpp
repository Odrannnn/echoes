#include "MetaRender/CCubeRenderer.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Graphics/CCubeMaterial.hpp"
#include "Kyoto/Graphics/CCubeModel.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CDrawablePlaneObject.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector2i.hpp"
#include "rstl/math.hpp"

#include <dolphin/gx.h>
#include <string.h>

// NonMatching scaffold: unfinished rendering passes are explicitly marked below.
CCubeRenderer* CCubeRenderer::sRenderer = nullptr;
IWeaponRenderer* IWeaponRenderer::sWeaponRenderer = nullptr;

namespace Buckets {
typedef rstl::reserved_vector< CDrawable, 512 > DrawableList;
typedef rstl::reserved_vector< CDrawable*, 128 > Bucket;
typedef rstl::reserved_vector< Bucket, 50 > BucketList;
typedef rstl::reserved_vector< CDrawablePlaneObject, 8 > PlaneList;
typedef rstl::reserved_vector< ushort, 8 > PlaneBucketList;

static DrawableList* sData;
static BucketList* sBuckets;
static PlaneList* sPlaneObjectData;
static PlaneBucketList* sPlaneObjectBucket;
static rstl::reserved_vector< ushort, 50 > sBucketIndex;
static const rstl::pair< float, float > skWorstMinMaxDistance(99999.f, -99999.f);
static rstl::pair< float, float > sMinMaxDistance = skWorstMinMaxDistance;

void Shutdown();
void Init(void* workspace);
uint GetWorkspaceSize();
void Clear();
void Sort();
void Insert(const CVector3f& pos, const CAABox& bounds, EDrawableType type, const void* data,
            const CPlane& plane, ushort extraSort, bool alpha);
void InsertPlaneObject(float closeDistance, float farDistance, const CAABox& bounds,
                       bool invertTest, const CPlane& plane, bool zOnly, EDrawableType type,
                       const void* data);
} // namespace Buckets

template < bool Special, bool Alpha >
void CCubeRenderer::DrawGeometry(int areaId) {
  // TODO: traverse visible surfaces, selecting the material and alpha pass.
}

uint Buckets::GetWorkspaceSize() {
  return sizeof(DrawableList) + sizeof(BucketList) + sizeof(PlaneList) + sizeof(PlaneBucketList) +
         4;
}

void Buckets::Init(void* workspace) {
  // TODO: placement-construct the bucket containers in the aligned caller-owned workspace.
}

void Buckets::Shutdown() {
  sData = nullptr;
  sBuckets = nullptr;
  sPlaneObjectData = nullptr;
  sPlaneObjectBucket = nullptr;
}

void Buckets::Insert(const CVector3f& pos, const CAABox& bounds, EDrawableType type,
                     const void* data, const CPlane& plane, ushort extraSort, bool alpha) {
  DrawableList* list = sData;
  if (list->size() == list->capacity()) {
    return;
  }

  const float distance = plane.GetHeight(pos);
  list->push_back(CDrawable(type, extraSort, distance, bounds, data, alpha));
  sMinMaxDistance.first = rstl::min_val(distance, sMinMaxDistance.first);
  sMinMaxDistance.second = rstl::max_val(distance, sMinMaxDistance.second);
}

void Buckets::InsertPlaneObject(float closeDistance, float farDistance, const CAABox& bounds,
                                bool invertTest, const CPlane& plane, bool zOnly,
                                EDrawableType type, const void* data) {
  PlaneList* list = sPlaneObjectData;
  if (list->size() == list->capacity()) {
    return;
  }

  list->push_back(CDrawablePlaneObject(type, closeDistance, farDistance, bounds, invertTest, plane,
                                       zOnly, data));
}

void Buckets::Sort() {
  // TODO: depth buckets, plane intersections and per-bucket sorting.
}

void Buckets::Clear() {
  sData->clear();
  sBucketIndex.clear();
  sPlaneObjectData->clear();
  sPlaneObjectBucket->clear();
  for (Bucket* p = sBuckets->begin(); p != sBuckets->end(); ++p) {
    p->clear();
  }
  sMinMaxDistance = skWorstMinMaxDistance;
}

CCubeRenderer::SModelSurfaceOrder::SModelSurfaceOrder(const CCubeModel& model)
: mSurfaceIndices(), mOpaqueEnd(0), mSortedEnd(0), mTotalCount(0) {
  // TODO: partition the model's surface indices by material flags.
}

CCubeRenderer::CAreaListItem::CAreaListItem(
    const rstl::vector< CMetroidModelInstance >* geometry, const CAreaRenderOctTree* octTree,
    const rstl::vector< SAreaSurface >* surfaces, const rstl::vector< uint >* ambientLightIds,
    const rstl::vector< signed char >* ambientLightIndices,
    const rstl::auto_ptr< rstl::vector< TCachedToken< CTexture > > >& textures,
    const rstl::auto_ptr< rstl::vector< rstl::auto_ptr< CCubeModel > > >& models, int areaId)
: mGeometry(geometry)
, mOctTree(octTree)
, mSurfaces(surfaces)
, mAmbientLightIds(ambientLightIds)
, mAmbientLightIndices(ambientLightIndices)
, mTextures(textures)
, mModels(models)
, mAreaId(areaId) {
  mPVSAlpha.resize(surfaces->size() - 1, 0);
  mModelSurfaceOrders.reserve(mModels->size());
  for (int i = 0; i < mModels->size(); ++i) {
    mModelSurfaceOrders.push_back(SModelSurfaceOrder(*(*mModels)[i]));
  }
}

CCubeRenderer::CCubeRenderer(IObjectStore& store, COsContext& context, CMemorySys& memory,
                             IFactory& factory)
: mFactory(factory)
, mObjStore(store)
, mFont(1.f)
, mPrimVertCount(0)
, mFrustumPlanes(CTransform4f::Identity(), 1.5707964f, 1.f, 1.f, false, 100.f)
, mDrawableCallback(nullptr)
, mViewPlane(0.f, CUnitVector3f(CVector3f::Forward(), CUnitVector3f::kN_Yes))
, mPVSMode(0)
, mBlackTex(kTF_RGB565, 4, 4, 1)
, mReflectionRamp(kTF_IA8, 32, 32, 1)
, mFogVolumeRamp(kTF_I8, 256, 256, 1)
, mSphereRamp(kTF_I8, 32, 32, 1)
, mAlphaMaskRamp(kTF_I4, 16, 16, 1)
, mScanRamp(kTF_I4, 8, 8, 1)
, mRandom(20)
, mReflectionAge(2)
, mPrimColor(CColor::White())
, mPrimNormal(CVector3f::Forward())
, mWorldLightColor(static_cast< uchar >(255), static_cast< uchar >(0), static_cast< uchar >(255),
                   static_cast< uchar >(255))
, mSilhouetteMaskCountdown(0)
, mBigRing(store.GetObj("TXTR_BigRing"))
, mDarkWorldCloud(store.GetObj("TXTR_DarkWorldCloud"))
, mScanSweepBar(store.GetObj("TXTR_ScanSweepBar"))
, mFlatSphere(store.GetObj("CMDL_FlatSphere"))
, mFlatSphereLow(store.GetObj("CMDL_FlatSphereLow"))
, mFlatCylinder(store.GetObj("CMDL_FlatCylinder"))
, mFlatCylinderLow(store.GetObj("CMDL_FlatCylinderLow"))
, mDarkLightWorldPalette(ClonePalette(store.GetObj("TXTR_DarkLightworldPalette")))
, mReflectionDirty(false)
, mDrawWireframe(false)
, mRequestRGBA6(false)
, mCurrentRGBA6(false)
, mPreserveDestinationAlpha(false)
, mDisableFog(false)
, mPersistRGBA6(false)
, mRenderingSilhouette(false)
, mCurrentMaterialMode(0)
, mRequestedMaterialMode(0) {
  memset(mBlackTex.Lock(), 0, 32);
  mBlackTex.UnLock();
  GenerateFogVolumeRampTex();
  GenerateSphereRampTex();
  GenerateAlphaMaskRampTex();
  GenerateScanRampTex();
  GenerateReflectionTex();
  sRenderer = this;
  Buckets::Shutdown();
}

CGraphicsPalette* CCubeRenderer::ClonePalette(const TLockedToken< CTexture >& texture) {
  // TODO: copy the texture palette's format, entry count and entries.
  return nullptr;
}

void CCubeRenderer::GenerateReflectionTex() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::GenerateFogVolumeRampTex() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::GenerateSphereRampTex() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::GenerateAlphaMaskRampTex() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::GenerateScanRampTex() {
  // TODO: reconstruct this rendering pass.
}

CCubeRenderer::~CCubeRenderer() {
  sRenderer = nullptr;
  Buckets::Shutdown();
  if (mSilhouetteMask.get()) {
    mSilhouetteMask->ScheduleDeletion();
  }
}

void CCubeRenderer::AddStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry,
                                      const CAreaRenderOctTree* octTree,
                                      const rstl::vector< SAreaSurface >* surfaces,
                                      const rstl::vector< uint >* ambientLightIds,
                                      const rstl::vector< signed char >* ambientLightIndices,
                                      int areaId) {
  // TODO: reconstruct this rendering pass.
}

rstl::list< CCubeRenderer::CAreaListItem >::iterator
CCubeRenderer::FindStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry) {
  for (rstl::list< CAreaListItem >::iterator area = mAreaListItems.begin();
       area != mAreaListItems.end(); ++area) {
    if (area->mGeometry == geometry) {
      return area;
    }
  }
  return mAreaListItems.end();
}

void CCubeRenderer::RemoveStaticGeometry(const rstl::vector< CMetroidModelInstance >* geometry) {
  rstl::list< CAreaListItem >::iterator area = FindStaticGeometry(geometry);
  if (area != mAreaListItems.end()) {
    mAreaListItems.erase(area);
  }
}

void CCubeRenderer::SetModelMatrix(const CTransform4f& xf) { CGraphics::SetModelMatrix(xf); }

void CCubeRenderer::SetWorldViewpoint(const CTransform4f& xf) {
  CGraphics::SetViewPointMatrix(xf);
  mViewPlane = CPlane(xf.GetTranslation(), CUnitVector3f(xf.GetForward(), CUnitVector3f::kN_No));
}

/**
 * `CCubeRenderer::EndScene` - retail 0x8026FB80, 0x34 bytes, and `BeginScene` - retail 0x8026FBFC,
 * 0x180 = 384 bytes. Both were `src/MetaRender/Carve8026FB80.cpp` and `Carve8026FBFC.cpp` on
 * master; upstream's `config/G2ME01/splits.txt` gives both ranges to this unit, and the port build
 * compiles only the carve files, so the bodies move here and those keep just their notes.
 *
 * Both are vtable entries - `EndScene` is vtable slot 34 and `BeginScene` slot 35 (offsets 0x88 and
 * 0x94 of `CCubeRenderer`'s own vtable) - which is why the names are certain.
 *
 * The five members the bodies touch were `x120_`, `x348_`, `x4f4_`, `x4f8_` and the `0x554` bit
 * byte in the pre-merge header, which upstream renamed. The mapping is fixed by the layout the
 * matching build measures (`src/MetaRender/Carve80271238.cpp`): `mBigRing` is at 0x4FC and
 * `mDarkLightWorldPalette` at 0x550, so the eight `bool : 1` members between them are the byte at
 * 0x554 and the two trailing `int`s are 0x558/0x55C, which is the 0x560 the class claims; and
 * `mSilhouetteMaskCountdown`, `mSilhouetteMask`, `mBigRing` are three consecutive words, so the
 * pre-merge 0x4F4/0x4F8 are the first two.
 *
 *   x554_26_ -> mRequestRGBA6           x554_28_ -> mPreserveDestinationAlpha
 *   x554_27_ -> mCurrentRGBA6           x554_30_ -> mPersistRGBA6
 *   x120_    -> mReflectionTex          x348_    -> mReflectionAge
 *   x4f4_    -> mSilhouetteMaskCountdown  x4f8_   -> mSilhouetteMask
 */
extern "C" uchar lbl_80418AE4;
extern "C" {
void fn_802C1658();
void fn_802C1F5C(const CColor& color);
void fn_802C1608(GXCullMode mode);
void fn_802C162C(bool test, GXCompare comp, bool write);
void fn_802C15E8(GXBlendMode, GXBlendFactor, GXBlendFactor, GXLogicOp);
void fn_802C235C(float fovy, float aspect, float znear, float zfar);
void fn_802BF640();
void fn_802C420C(CTexture* tex);
void fn_802C1E60();
}

void CCubeRenderer::EndScene() {
  // 0x8026FB94-0x8026FBA4.  `cntlzw` + `rlwimi` is MWCC's spelling of this comparison; the shift
  // lands MWCC's bit-5 boolean on mask field 30, which is `mPersistRGBA6`.
  mPersistRGBA6 = (lbl_80418AE4 == 0);
  // 0x8026FBA8.  The EFB copy that closes the scene.
  fn_802C1658();
  // 0x8026FBAC-0x8026FBD4.  Retail branches with **`blt` to 0x8026FBD0**, which is the
  // *increment*, so the release is the fall-through and the condition is the **positive** `>=`:
  // written the other way round, MWCC emits `bge` over the release block and permutes the two
  // blocks.  `mReflectionTex = nullptr` is one statement, not two: `rstl::single_ptr`'s
  // `operator=` is `delete x0_ptr; x0_ptr = ptr;`, which is exactly the
  // `lwz / li r4,1 / bl dtor / stw 0` at 0x8026FBB8-0x8026FBC8.
  if (mReflectionAge >= 2) {
    mReflectionTex = nullptr;
  } else {
    ++mReflectionAge;
  }
  // 0x8026FBD8-0x8026FBE4.  A single `stw 0`, not four `stb` - so the `uint` spelling, which is
  // why this is not the same expression `BeginScene` uses for its clear colour.
  fn_802C1F5C(CColor(static_cast< uint >(0)));
}

// 0x8026FC24-0x8026FD4C.  The viewport words are both read *before* `SetUseVideoFilter`, so they go
// into locals first - a load cannot be moved across a call that might write it.  The
// `mSilhouetteMaskCountdown` is re-read after the store (`lwz r0,0x4f4`), which is a decrement
// followed by a separate test and not a pre-decrement in the condition, and the two
// destination-alpha flags are each cleared by an `if` whose *else* branch runs
// `GXSetAlphaUpdate`, not by an assignment.
void CCubeRenderer::BeginScene() {
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
  // 0x8026FCB0-0x8026FCE8.
  if (mSilhouetteMaskCountdown != 0) {
    --mSilhouetteMaskCountdown;
    if (mSilhouetteMaskCountdown == 0) {
      fn_802C420C(mSilhouetteMask.get());
      mSilhouetteMask = nullptr;
    }
  }
  // 0x8026FCEC-0x8026FD4C.
  mCurrentRGBA6 = mRequestRGBA6;
  if (!mPersistRGBA6) {
    mRequestRGBA6 = false;
  }
  GXSetPixelFmt(mCurrentRGBA6 ? GX_PF_RGBA6_Z24 : GX_PF_RGB8_Z24, GX_ZC_LINEAR);
  if (mPreserveDestinationAlpha) {
    mPreserveDestinationAlpha = false;
  } else {
    GXSetAlphaUpdate(GX_TRUE);
  }
  CGX::SetDstAlpha(true, 0);
  fn_802C1E60();
}

void CCubeRenderer::AddParticleGen(const CParticleGen& gen) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::AddParticleGen(const CParticleGen& gen, const CVector3f& pos,
                                   const CAABox& bounds) {
  Buckets::Insert(pos, bounds, kDT_Particle, &gen, mViewPlane, 0, true);
}

void CCubeRenderer::AddPlaneObject(const void* obj, const CAABox& bounds, const CPlane& plane,
                                   int type) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::AddDrawable(const void* obj, const CVector3f& pos, const CAABox& bounds,
                                int mode, EDrawableSorting sorting) {
  if (sorting == kDS_UnsortedCallback) {
    mDrawableCallback(obj, mDrawableCallbackUserData, mode);
  } else {
    Buckets::Insert(pos, bounds, static_cast< EDrawableType >(mode + kDT_Actor), obj, mViewPlane, 0,
                    sorting == kDS_AlphaSortedCallback);
  }
}

void CCubeRenderer::SetupRendererStates(bool depthWrite) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetupCGraphicsStates() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::AddWorldSurface(short modelIndex, ushort surfaceIndex, uint blend,
                                    const CAABox& bounds) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawRenderBucketsDebug() {}

void CCubeRenderer::RenderBucketItems(const CAreaListItem* area, bool alpha) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawSortedGeometry(int mode, int areaId) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::EvaluateModelLights(uchar* lights, const CAABox& bounds, const uint* overlaps,
                                        int wordCount, uint modelIndex) {
  // TODO: reconstruct this rendering pass.
}

/**
 * `fn_80272958` - the allocator `AllocateRenderer` takes `li r3,1376` from. Retail `.text`
 * 0x80272958..0x80272988, 0x30 = 48 bytes. It was `src/MetaRender/Carve80272958.c` on master;
 * upstream's `config/G2ME01/splits.txt` gives the range to this unit, so the body moves here and
 * the `.c` keeps only its note. It stays `extern "C"` - a C++ one mangles to
 * `_Z12fn_80272958iPKvPv` and objdiff pairs nothing with retail's unmangled `fn_80272958`.
 *
 * It ignores all three arguments: `r3`, `r4` and `r5` are never read. `fn_802729C0` is a lazy
 * initialiser for a `.sbss` byte and a `.sbss` counter word and returns the address of that word,
 * so this is a refcount bump, and `fn_802729B4` is `return (void*)0x803DEF28;` - one fixed arena,
 * not a heap block. Writing this does not give the port a heap; `src/MetaRender/PortCCubeRenderer.cpp`
 * is the host's translation of it.
 */
extern void* fn_802729C0(void);
extern void* fn_802729B4(void);
extern "C" void* fn_80272958(int size, const char* name, void* mem) {
  int* counter = static_cast< int* >(fn_802729C0());
  *counter = *counter + 1;
  return fn_802729B4();
}

/**
 * `AllocateRenderer`, retail 0x8026EF54, 0x9C = 156 bytes. It was
 * `src/MetaRender/Carve8026EF54.cpp` on master; upstream's `splits.txt` gives the range to this
 * unit, and the port build compiles only the carve file, so the body moves here and that one
 * keeps its note.
 *
 * The pre-merge body was 100.00% from objdiff with `&lbl_803AE3BC[86]` spelled as a folded
 * constant, and it stayed that way in the port because dtk links the unit's *retail* object for
 * the DOL (`build/G2ME01/obj/...`) rather than this one. The literal is not repeated here: the
 * `.rodata` word at 0x803AE3BC is a 252-byte tail-merged string pool this unit cannot own, and
 * `lbl_803AE3BC` is declared, not defined, exactly as the carve file had it.
 *
 * **`gpRender` stops being null and that is all this buys.** The `1376` is retail's own pool block
 * size - the only evidence of `sizeof(CCubeRenderer)` outside the constructor - but the allocator
 * never reads it, and the pointer it returns has no vtable in it until `fn_80271238`
 * (`CCubeRenderer`'s constructor, 0x80271238) runs. No frame has been rendered by this and none
 * is claimed.
 */
extern "C" void* lbl_80418998[2];
extern "C" const char lbl_803AE3BC[];
extern "C" void* fn_80271238(void* self, IObjectStore&, COsContext&, CMemorySys&, IFactory&);
IRenderer* AllocateRenderer(IObjectStore& store, COsContext& context, CMemorySys& memory,
                            IFactory& factory) {
  // `lbl_803AE3BC + 86` is 0x803AE412, retail's own unused debug name; written as an add because
  // the `+86` is a separate `addi` in retail and a string literal would compile to `lis ; addi ; mr`.
  void* p = fn_80272958(1376, lbl_803AE3BC + 86, 0);
  if (p) {
    p = fn_80271238(p, store, context, memory, factory);
  }
  void* q = p;
  if (p) {
    q = static_cast< char* >(p) + 4;
  }
  lbl_80418998[0] = q;
  return static_cast< IRenderer* >(p);
}

void CCubeRenderer::PrimColor(float r, float g, float b, float a) { mPrimColor.Set(r, g, b, a); }

void CCubeRenderer::PrimColor(const CColor& color) { mPrimColor = color; }

void CCubeRenderer::BeginPrimitive(EPrimitiveType primitive, int count) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::BeginLines(int count) { CCubeRenderer::BeginPrimitive(kPT_Lines, count); }

void CCubeRenderer::BeginLineStrip(int count) {
  CCubeRenderer::BeginPrimitive(kPT_LineStrip, count);
}

void CCubeRenderer::BeginTriangles(int count) {
  CCubeRenderer::BeginPrimitive(kPT_Triangles, count);
}

void CCubeRenderer::BeginTriangleStrip(int count) {
  CCubeRenderer::BeginPrimitive(kPT_TriangleStrip, count);
}

void CCubeRenderer::BeginTriangleFan(int count) {
  CCubeRenderer::BeginPrimitive(kPT_TriangleFan, count);
}

void CCubeRenderer::PrimVertex(const CVector3f& vertex) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::PrimNormal(const CVector3f& normal) { mPrimNormal = normal; }

void CCubeRenderer::EndPrimitive() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetAmbientColor(const CColor& color) { CGraphics::SetAmbientColor(color); }

void CCubeRenderer::SetPerspective(float fovy, float width, float height, float znear, float zfar) {
  CGraphics::SetPerspective(fovy, width / height * CGraphics::GetPixelAspectRatio(), znear, zfar);
}

void CCubeRenderer::SetPerspective(float fovy, float aspect, float znear, float zfar) {
  CGraphics::SetPerspective(fovy, aspect, znear, zfar);
}

rstl::pair< CVector2f, CVector2f > CCubeRenderer::SetViewportOrtho(bool centered, float znear,
                                                                   float zfar) {
  // TODO: configure the viewport-derived orthographic projection.
  return rstl::pair< CVector2f, CVector2f >(CVector2f(0.f, 0.f), CVector2f(0.f, 0.f));
}

void CCubeRenderer::SetViewport(int left, int top, int width, int height) {
  CGraphics::SetViewport(left, top, width, height);
  CGraphics::SetScissor(left, top, width, height);
}

void CCubeRenderer::SetDepthReadWrite(bool read, bool update) {
  CGraphics::SetDepthWriteMode(read, kE_LEqual, update);
}

void CCubeRenderer::SetBlendMode_AdditiveAlpha() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, kBF_One, kLO_Clear);
}

void CCubeRenderer::SetBlendMode_AlphaBlended() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, kBF_InvSrcAlpha, kLO_Clear);
}

void CCubeRenderer::SetBlendMode_NoColorWrite() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_Zero, kBF_One, kLO_Clear);
}

void CCubeRenderer::SetBlendMode_ColorMultiply() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_Zero, kBF_SrcColor, kLO_Clear);
}

void CCubeRenderer::SetBlendMode_InvertDst() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_InvDstColor, kBF_Zero, kLO_Clear);
}

void CCubeRenderer::SetBlendMode_InvertSrc() {
  CGraphics::SetBlendMode(kBM_Logic, kBF_One, kBF_Zero, kLO_InvCopy);
}

void CCubeRenderer::SetBlendMode_Replace() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_One, kBF_Zero, kLO_Clear);
}

void CCubeRenderer::SetBlendMode_AdditiveDestColor() {
  CGraphics::SetBlendMode(kBM_Blend, kBF_DstColor, kBF_One, kLO_Clear);
}

float CCubeRenderer::GetFPS() { return CGraphics::GetFPS(); }

void CCubeRenderer::SetDrawableCallback(TDrawableCallback callback, const void* context) {
  mDrawableCallback = callback;
  mDrawableCallbackUserData = context;
}

void CCubeRenderer::SetDebugOption(EDebugOption option, int value) {
  switch (option) {
  case kDO_PVSMode:
    mPVSMode = value != 0;
    break;
  case kDO_PVSState:
    mPVSState = value;
    break;
  case kDO_FogDisabled:
    mDisableFog = value != 0;
    break;
  }
}

CTexture* CCubeRenderer::GetRealReflection() {
  mReflectionAge = 0;
  CTexture* tex = mReflectionTex.get();
  if (tex == nullptr) {
    return &mBlackTex;
  }
  return tex;
}

void CCubeRenderer::CacheReflection(void (*callback)(void*, const CVector3f&), void* context,
                                    bool clear) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawSpaceWarp(const CVector3f& point, float strength) {
  if (point.GetZ() >= 1.f) {
    return;
  }
  _DrawSpaceWarp(point, strength);
}

void CCubeRenderer::_DrawSpaceWarp(const CVector3f& point, float strength) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetWireframeFlags(int flags) {
  CCubeModel::SetDrawingOccluders((flags & 1) != 0);
  mDrawWireframe = (flags & 2) != 0;
}

void CCubeRenderer::SetWorldFog(ERglFogMode mode, float start, float end, const CColor& color) {
  CGraphics::SetFog(mode, start, end, color);
}

int CCubeRenderer::GetStaticWorldDataSize() {
  int size = 0;
  for (rstl::list< CAreaListItem >::const_iterator area = mAreaListItems.begin();
       area != mAreaListItems.end(); ++area) {
    if (area->mTextures.get() != nullptr) {
      size += area->mTextures->size() * sizeof(TCachedToken< CTexture >);
    }
  }
  return size;
}

void CCubeRenderer::DrawFogFan(const CVector3f* vertices, int count) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawFogFans(const CPlane* planes, int planeCount, const CVector3f* vertices,
                                int vertexCount, int front, int back) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawFogSlices(const CPlane* planes, int planeCount, int planeIndex,
                                  const CVector3f& center, float extent) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::RenderFogVolumeModel(const CAABox& bounds, const CModel* model,
                                         const CTransform4f& modelView, CTransform4f view,
                                         const CSkinnedModel* skinnedModel) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::RenderFogVolume(const CColor& color, const CAABox& bounds,
                                    const TLockedToken< CModel >* model,
                                    const CSkinnedModel* skinnedModel) {
  if (!mDisableFog) {
    mFogVolumes.push_back(
        CFogVolumeListItem(CGraphics::GetModelMatrix(), color, bounds, model, skinnedModel));
  }
}

void CCubeRenderer::ReallyRenderFogVolume(const CColor& color, const CAABox& bounds,
                                          const CModel* model, const CSkinnedModel* skinnedModel) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetRequestedMaterialMode(int mode) {
  mRequestedMaterialMode = mode;
  if (mode == 0) {
    SetMaterialMode(0);
  }
}

void CCubeRenderer::PostRenderFogs() {
  // TODO: reconstruct this rendering pass.
}

CCubeRenderer::CFogVolumeListItem::CFogVolumeListItem(const CTransform4f& xf, const CColor& color,
                                                      const CAABox& bounds,
                                                      const TLockedToken< CModel >* model,
                                                      const CSkinnedModel* skinnedModel)
: mTransform(xf)
, mColor(color)
, mBounds(bounds)
, mModel(model ? rstl::optional_object< TLockedToken< CModel > >(*model)
               : rstl::optional_object_null())
, mSkinnedModel(skinnedModel) {}

void CCubeRenderer::DrawModelDisintegrate(const SModelRenderData& data, const CTexture& texture,
                                          const CColor& color, float amount) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawModelFlat(const SModelRenderData& data, const CModelFlags& flags,
                                  uchar unsortedOnly) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawScreenFilter(const CColor& color0, const CColor& color1,
                                     const CColor& color2) {
  // TODO: reconstruct this rendering pass.
}

bool CCubeRenderer::DrawScanSurface(const CAreaListItem& area, const CCubeModel& model,
                                    const CCubeSurface& surface, uint lightSet, bool alpha) {
  // TODO: select the surface material and scan-pass drawing state.
  return false;
}

void CCubeRenderer::DrawScanRing(float radius, float thickness, float alpha, float fade,
                                 float scanTime, int areaId) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetGXRegister1Color(const CColor& color) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetWorldLightFadeLevel(float level) {
  const uchar value = static_cast< uchar >(level * 255.f);
  mWorldLightColor = CColor(value, value, value, static_cast< uchar >(255));
}

uchar CCubeRenderer::FindOrAddLightSet(uint lightSet) {
  for (int i = 0; i < mLightSets.size(); ++i) {
    if (mLightSets[i] == lightSet) {
      return static_cast< uchar >(i);
    }
  }
  if (mLightSets.size() < mLightSets.capacity()) {
    mLightSets.push_back(lightSet);
    return static_cast< uchar >(mLightSets.size() - 1);
  }
  return 0;
}

void CCubeRenderer::FindOverlappingWorldModels(rstl::vector< uint >& models, const CAABox& bounds) {
  // TODO: reconstruct this rendering pass.
}

int CCubeRenderer::DrawOverlappingWorldModelShadows(int areaId, rstl::vector< uint >& models,
                                                    const CAABox& bounds) {
  // TODO: draw the selected model surfaces and return their count.
  return 0;
}

void CCubeRenderer::DrawWorldModelShadow(const CAABox& bounds) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawOverlappingWorldModelIDs(int areaId, rstl::vector< uint >& models,
                                                 const CAABox& bounds) {
  // TODO: reconstruct this rendering pass.
}

void* CCubeRenderer::GetRenderToTexBuffer(int index) {
  // TODO: recover ownership of the shared render-to-texture workspace.
  return nullptr;
}

void CCubeRenderer::CopyScreenTex(uint divisor, bool half, void* dest, GXTexFmt format,
                                  bool clear) const {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DoPhazonSuitIndirectAlphaBlur(float scale, float amount) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::ReallyDrawPhazonSuitEffect(const CColor& color, const CTexture& texture) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::ReallyDrawPhazonSuitIndirectEffect(const CColor& color, const CTexture& texture,
                                                       const CTexture& indirectTexture, float scale,
                                                       float offset, float alpha,
                                                       const CColor& additiveColor) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::RenderSilhouette(
    float blur, const CColor& color,
    const rstl::optional_object< TCachedToken< CTexture > >& texture, float scale, float offset,
    float alpha, const CColor& additiveColor) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::AllocatePhazonSuitMaskTexture() {
  // TODO: reconstruct this rendering pass.
}

float CCubeRenderer::GetRandomInterpolation(float time, float period, int seed) {
  // TODO: interpolate between deterministic random values.
  return 0.f;
}

void CCubeRenderer::PopulateNoiseTexCoords(float time,
                                           rstl::reserved_vector< CVector2f, 9 >& coords) {
  // TODO: reconstruct this rendering pass.
}

bool CCubeRenderer::EnableSilhouetteRender() {
  // TODO: allocate/copy the silhouette mask and update render state.
  return false;
}

void CCubeRenderer::DrawSilhouetteNoise(const SSilhouetteNoise& noise) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::fn_802679DC(const void* unused, const CModel& model, const CModelFlags& flags) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::LoadEnvironmentTextureMatrix(uint matrix, uint postMatrix,
                                                 const CTransform4f& xf, bool alternate) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::LoadScrollingTextureMatrix(uint matrix, const CVector2f& scroll,
                                               const CVector2f& scale) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawDarkWorldVolume(const CVector3f& pos, const CVector3f& scale, uchar mix,
                                        uchar alpha, bool inside, float lod,
                                        const CVector2f& scroll1, const CVector2f& scroll2,
                                        const CVector2f& texScale1, const CVector2f& texScale2,
                                        const CTexture& environment, const CTexture& cloud1,
                                        const CTexture& cloud2, CColor color, CColor additiveColor,
                                        bool cylinder, bool additive) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::GetScreenMipInfo(int width, int height, int mipCount, GXTexFmt format,
                                     int* size, int* mipWidth, int* mipHeight) {
  int total = 0;
  for (int i = 0; i < mipCount; ++i) {
    width >>= 1;
    height >>= 1;
    total += GXGetTexBufferSize(width, height, format, GX_FALSE, 0);
  }
  if (size) {
    *size = total;
  }
  if (mipWidth) {
    *mipWidth = width;
  }
  if (mipHeight) {
    *mipHeight = height;
  }
}

void CCubeRenderer::SetupScreenCopyStates() {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawTexturedScreenQuad(int left, int top, int width, int height) {
  // TODO: reconstruct this rendering pass.
}

void* CCubeRenderer::GenerateScreenMipmaps(int mipCount, GXTexFmt copyFormat, GXTexFmt loadFormat,
                                           int left, int top, int width, int height) {
  // TODO: copy successive framebuffer mip levels into the render workspace.
  return nullptr;
}

void CCubeRenderer::GenerateScreenMipmaps(int mipCount, bool depth) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::SetMaterialMode(int mode) {
  mCurrentMaterialMode = mode;
  if (mode == 1) {
    CCubeMaterial::UseThermalTevs();
  } else if (mode == 0) {
    CCubeMaterial::UseNormalTevs();
  }
}

void CCubeRenderer::SetDestinationAlpha(int alpha) {
  mRequestRGBA6 = true;
  GXSetAlphaUpdate(GX_TRUE);
  CGX::SetDstAlpha(GX_TRUE, alpha);
}

void CCubeRenderer::DisableDestinationAlpha() {
  GXSetAlphaUpdate(GX_FALSE);
  CGX::SetDstAlpha(GX_TRUE, 0);
}

void CCubeRenderer::DrawScanVisor(float scanTime, float width, float height, const CColor& color,
                                  const CColor& scanColor, const CColor& maskColor,
                                  const CColor* palette, int paletteSize,
                                  const CVector3f& scanRange) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawDarkWorldTransition(const CColor& color0, const CColor& color1,
                                            const CColor& color2, const CColor& color3,
                                            const CVector2i& offset, const CVector2i& sourceSize,
                                            const CVector2i& targetSize) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawDarkWorldFilter(float amount) {
  // TODO: reconstruct this rendering pass.
}

uint CCubeRenderer::PackLightSet(const uchar* lights, float ambient) {
  const uint level = CMath::ClampI(0, static_cast< int >(63.f * ambient), 63);
  return (lights[0] & 63) | ((lights[1] & 63) << 6) | ((lights[2] & 63) << 12) |
         ((lights[3] & 63) << 18) | (level << 24);
}

void CCubeRenderer::UnpackLightSet(uint lightSet, uchar* lights, float* ambient,
                                   uchar* quantizedAmbient) {
  for (int i = 0; i < 4; ++i) {
    lights[i] = (lightSet >> (6 * i)) & 63;
  }

  const uchar level = (lightSet >> 24) & 63;
  if (ambient) {
    *ambient = level * (1.f / 63.f);
  }
  if (quantizedAmbient) {
    *quantizedAmbient = level;
  }
}

void CCubeRenderer::PrepareWorldRendering(
    const rstl::pair< int, const CPVSVisSet* >* pvsSets, int pvsCount,
    const CFrustumPlanes& frustum,
    const rstl::reserved_vector< rstl::pair< int, CFrustumPlanes >, 10 >* areaFrusta,
    const rstl::vector< CLight >& lights, const rstl::pair< int, float >* ambientLights,
    int ambientLightCount) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawUnsortedGeometry(int areaId, int, int) { DrawGeometry< false, false >(areaId); }

void CCubeRenderer::DrawUnsortedGeometryAlpha(int areaId) { DrawGeometry< false, true >(areaId); }

void CCubeRenderer::DrawSpecialGeometry(int areaId, int, int) { DrawGeometry< true, false >(areaId); }

void CCubeRenderer::DrawSpecialGeometryAlpha(int areaId, int, int) { DrawGeometry< true, true >(areaId); }

void CCubeRenderer::DrawAreaModel(int areaId, int modelId, const CModelFlags& flags) {
  // TODO: reconstruct this rendering pass.
}

CAABox CCubeRenderer::GetAreaModelBounds(int areaId, int modelId) {
  // TODO: find the area's model and return its bounds.
  return CAABox(CVector3f::Zero(), CVector3f::Zero());
}

void CCubeRenderer::DrawVisibleAreaGeometry(int areaId, const CPVSVisSet& pvs,
                                            const CFrustumPlanes& frustum, const CAABox& bounds) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::ActivateLightsForModel(uint lightSet) {
  // TODO: reconstruct this rendering pass.
}

rstl::list< CCubeRenderer::CAreaListItem >::iterator CCubeRenderer::FindArea(int areaId) {
  for (rstl::list< CAreaListItem >::iterator area = mAreaListItems.begin();
       area != mAreaListItems.end(); ++area) {
    if (area->mAreaId == areaId) {
      return area;
    }
  }
  return mAreaListItems.end();
}

rstl::list< CCubeRenderer::CAreaListItem >::const_iterator
CCubeRenderer::FindArea(int areaId) const {
  return const_cast< CCubeRenderer* >(this)->FindArea(areaId);
}

void CCubeRenderer::EnablePVS(int areaId, const rstl::vector< rstl::pair< int, int > >& visible) {
  DisablePVS(areaId);
  rstl::list< CAreaListItem >::iterator area = FindArea(areaId);
  if (area != mAreaListItems.end()) {
    for (int i = 0; i < visible.size(); ++i) {
      area->mPVSAlpha[visible[i].first] = visible[i].second << 2;
    }
  }
}

void CCubeRenderer::DisablePVS(int areaId) {
  rstl::list< CAreaListItem >::iterator area = FindArea(areaId);
  if (area != mAreaListItems.end()) {
    CBasics::ZeroMemory(area->mPVSAlpha.data(), area->mPVSAlpha.size());
  }
}

void CCubeRenderer::DrawModelWithTextureMask(const SModelRenderData& model, const CTexture& texture,
                                             const CVector3f& origin, const CColor& color,
                                             float scale) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::CopyTextureRegion(void* dest, int format, int left, int top, int width,
                                      int height) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawDarkWorldCloud(float time, const CVector3f& scale, const CColor& color) {
  // TODO: reconstruct this rendering pass.
}

void CCubeRenderer::DrawModelNoise(const SModelRenderData& data, const CColor& color,
                                   bool additive) {
  // TODO: reconstruct this rendering pass.
}

uint GetRendererWorkspaceSize() { return Buckets::GetWorkspaceSize(); }

void SetRendererWorkspace(void* workspace) { Buckets::Init(workspace); }

void ReleaseRendererWorkspace() { Buckets::Shutdown(); }

void CCubeRenderer::DrawString(const char* text, int x, int y) {
  mFont.DrawString(text, x, y, CColor::White());
}
