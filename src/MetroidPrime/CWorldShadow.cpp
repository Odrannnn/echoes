// `rs_new`'s file-name argument, named the way `Kyoto/Alloc/CMemory.hpp`'s `CMEMORY_NEW_FILE`
// branch is for (`src/MetroidPrime/Factories/CStateMachineFactory.cpp` does the same with
// `lbl_803AA230`). `__ct__12CWorldShadowFUiUib` opens with `lis r7,0x803B ; addi r0,r7,-30032`
// (`tools/dis.sh 0x800E23C8 0x108`) = 0x803A8AB0, which is retail's pooled `"\?\?(\?\?)"` - the
// literal `rs_new` would expand to. Leaving `rs_new` unexpanded makes this object emit its own
// `.rodata`, which the linker appends to the global string pool and which shifts every later pool
// entry - the trap `CMemory.hpp` documents. Must be set *before* any include, because `rs_new` is
// expanded inside the headers.
#define CMEMORY_NEW_FILE lbl_803A8AB0

#include "MetroidPrime/CWorldShadow.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Graphics/CCubeModel.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "WorldFormat/CPVSAreaSet.hpp"
#include "dolphin/gx/GXFrameBuffer.h"

// Retail calls the SDK's out-of-line *float* square root, `sqrt__Ff` at 0x8001D658, to fill the
// cached scale below, so the constant is a float and the call is not folded. `libc/math.h` only
// declares `double sqrt(double)` for MWCC, and both it and `sqrtf` are inlined there: expanding
// MSL's `frsqrte`-plus-Newton-steps sequence inline emits 22 instructions where retail has a
// `bl`, and needs three double constants in .sdata2 that retail does not have. So the float
// overload is named here, the way `MetroidPrime/CEulerAngles.cpp` names it too.
extern "C" float sqrt__Ff(float x);

// `CGraphics::GetDepthNear()` and `CGraphics::GetDepthFar()` read the two static floats
// `CGraphics::mDepthNear` (`.sbss:0x80419968`) and `CGraphics::mDepthFar` (`.sdata:0x80418AF0`).
// In retail both are *unnamed* (`lbl_80419968` / `lbl_80418AF0` in `config/G2ME01/symbols.txt`) and
// are defined by dtk's filler objects `auto_10_80419910_sbss.o` and `auto_09_80418AD4_sdata.o`,
// which no unit in `configure.py` compiles. So the C++ member names leave this object with two
// undefined symbols and the flip cannot link. `SetDepthRange` is what writes them - `stfs
// f5,-25624(r13); stfs f6,-29328(r13)` (`tools/dis.sh 0x802BFA38 0xA0`, resolved with
// `tools/sda.py s:-0x6418` / `s:-0x7290`) - so these are the same two words.
extern "C" float lbl_80419968; // CGraphics::mDepthNear
extern "C" float lbl_80418AF0; // CGraphics::mDepthFar

extern "C" const char lbl_803A8AB0[];

CWorldShadow::CWorldShadow(uint width, uint height, bool rgba8)
: mTexture(rs_new CTexture(rgba8 ? kTF_RGBA8 : kTF_RGB565, width, height, 1))
, mView(CTransform4f::Identity())
, mModel(CTransform4f::Identity())
, mObjectHalfExtent(1.f)
, mObjectPosition(0.f, 1.f, 0.f)
, mLightPosition(CVector3f::Zero())
, mArea(kInvalidAreaId)
, mLightIndex(-1)
, mBlurReset(true) {}

CWorldShadow::~CWorldShadow() {
  if (mTexture.get())
    mTexture->ScheduleDeletion();
}

// Guessed name
// The visor test has to be a `switch`, not `if (visor == kPV_Combat)`. Every if-shaped spelling -
// `if (!dark && visor == kPV) return true;`, `return !dark && visor == kPV`, the ternary, a plain
// `if (visor == kPV) return true; return false;` - makes MWCC materialise the comparison as a value
// (cntlzw/srwi or a saved register holding the result) instead of branching on it. Retail's bytes
// are `cmpwi r3,0; beq <true>; b <false>` with `li r3,1` / `li r3,0` tail-duplicated into each
// exit, and only the `switch` spells it that way: 27 instructions, byte-identical to retail.
bool CWorldShadow::CanRender(const CStateManager& mgr) {
  if (mgr.IsMultiplayer())
    return false;
  if (!mgr.GetIsDarkWorld()) {
    switch (mgr.GetPlayerState()->GetActiveVisor(mgr)) {
      case CPlayerState::kPV_Combat:
        return true;
      default:
        break;
    }
  }
  return false;
}

void CWorldShadow::BuildLightShadowTexture(const CStateManager& mgr, TAreaId areaId,
                                           uint lightIndex, const CAABox& bounds, bool motionBlur,
                                           bool lighten) {
  if (mArea != areaId || mLightIndex != lightIndex) {
    mBlurReset = true;
    mArea = areaId;
    mLightIndex = lightIndex;
  }
  if (areaId == kInvalidAreaId)
    return;

  const CGameArea& area = mgr.GetWorld()->GetAreaAlways(areaId);
  if (!area.IsLoaded())
    return;

  // `area` is the only thing bound here, as upstream has it. Retail re-derives the
  // post-constructed block from it after the `GetCenterPoint` call rather than holding it live
  // across the call, and naming it in a local is what pins it in a saved register instead - one
  // load fewer, and a different register for the area pointer.
  const CWorldLight& light = area.GetPostConstructed()->mLightsA[lightIndex];
  const CVector3f center = bounds.GetCenterPoint();
  const CPVSAreaSet* pvs = area.GetPostConstructed()->mPvs.get();
  CPVSVisSet lightSet(kVSS_OutOfBounds);
  if (pvs && pvs->GetLightIndexCount() > 0 && gkPVSEnabled == 1)
    lightSet = pvs->GetLightSet(lightIndex + pvs->GetNum2ndLights());
  const rstl::pair< int, const CPVSVisSet* > areaSet(areaId.Value(), &lightSet);

  CVector3f lightToPoint = center - light.GetPosition();
  mObjectHalfExtent = (bounds.GetMaxPoint() - center).Magnitude();
  const float distance = lightToPoint.Magnitude();
  const float fov = CMath::Rad2Deg(atan2f(mObjectHalfExtent, distance)) * 2.f;
  if (fov < 0.00001f)
    return;

  lightToPoint.Normalize();
  mView = CTransform4f::LookAt(light.GetPosition(), center, CVector3f(0.f, 0.f, -1.f));
  mObjectPosition = center;
  mLightPosition = light.GetPosition();
  CGraphics::SetViewPointMatrix(mView);
  const CFrustumPlanes frustum(mView, CRelAngle::FromDegrees(fov).AsRadians(), 1.f, 0.1f, true,
                               distance + mObjectHalfExtent);
  gpRender->SetPerspective(fov, mTexture->GetWidth(), mTexture->GetHeight(), 0.1f, 1000.f);
  gpRender->PrepareWorldRendering(&areaSet, 1, frustum, nullptr, rstl::vector< CLight >(), nullptr,
                                  0);

  const float depthNear = lbl_80419968;
  const float depthFar = lbl_80418AF0;
  CGraphics::SetDepthRange(0.f, 1.f);
  // Upstream's four separate reads, not one `const CViewport` copy: retail loads only the four
  // integer members, so a whole-struct copy both loads the two float members retail never
  // touches and needs a 24-byte stack slot this function does not have.
  const int backupVpLeft = CGraphics::GetViewport().mLeft;
  const int backupVpTop = CGraphics::GetViewport().mTop;
  const int backupVpWidth = CGraphics::GetViewport().mWidth;
  const int backupVpHeight = CGraphics::GetViewport().mHeight;
  gpRender->SetViewport(0, 0, mTexture->GetWidth() * 2, mTexture->GetHeight() * 2);
  const float extent = 1.4142f * mObjectHalfExtent;
  mModel = CTransform4f::LookAt(center - CVector3f(0.f, 0.f, 0.1f), light.GetPosition());
  gpRender->SetModelMatrix(mModel);
  gpRender->PrimColor(CColor::White());
  CGraphics::SetAlphaCompare(kAF_Always, 0, kAO_And, kAF_Always, 0);
  CGraphics::SetDepthWriteMode(true, kE_LEqual, true);
  CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, kBF_InvSrcAlpha, kLO_Clear);
  CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvPassthru);
  CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
  gpRender->BeginTriangleStrip(4);
  gpRender->PrimVertex(CVector3f(-extent, 0.f, extent));
  gpRender->PrimVertex(CVector3f(extent, 0.f, extent));
  gpRender->PrimVertex(CVector3f(-extent, 0.f, -extent));
  gpRender->PrimVertex(CVector3f(extent, 0.f, -extent));
  gpRender->EndPrimitive();

  gpRender->SetModelMatrix(CTransform4f::Identity());
  CCubeModel::SetRenderModelBlack(true);
  CCubeModel::SetDrawingOccluders(true);
  gpRender->DrawUnsortedGeometry(areaId.Value(), 0, 0);
  CCubeModel::SetRenderModelBlack(false);
  CCubeModel::SetDrawingOccluders(false);

  if (lighten) {
    gpRender->SetModelMatrix(mModel);
    CGraphics::SetAlphaCompare(kAF_Always, 0, kAO_And, kAF_Always, 0);
    CGraphics::SetDepthWriteMode(false, kE_LEqual, false);
    CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, kBF_InvSrcAlpha, kLO_Clear);
    CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvPassthru);
    CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
    CGraphics::StreamBegin(kP_TriangleStrip);
    CGraphics::StreamColor(1.f, 1.f, 1.f, 0.25f);
    CGraphics::StreamVertex(CVector3f(-extent, 0.f, extent));
    CGraphics::StreamVertex(CVector3f(extent, 0.f, extent));
    CGraphics::StreamVertex(CVector3f(-extent, 0.f, -extent));
    CGraphics::StreamVertex(CVector3f(extent, 0.f, -extent));
    CGraphics::StreamEnd();
    CGraphics::SetDepthWriteMode(true, kE_LEqual, true);
  }
  // Upstream's spelling, kept: the same test as `!mBlurReset`, but retail compares the byte
  // against 1 and branches when it is not, where `!` makes the compiler compare against 0 and
  // branch the other way.
  if (motionBlur && mBlurReset != true) {
    CGraphics::SetDepthWriteMode(false, kE_LEqual, false);
    CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, kBF_InvSrcAlpha, kLO_Clear);
    CGraphics::SetAlphaCompare(kAF_Always, 0, kAO_And, kAF_Always, 0);
    CGraphics::SetTevOp(kTS_Stage0, CGraphics::kEnvModulate);
    CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
    CGraphics::Render2D(*mTexture, 0, mTexture->GetWidth() * 2, mTexture->GetHeight() * 2,
                        -mTexture->GetWidth() * 2, CColor(1.f, 1.f, 1.f, 0.85f));
    CGraphics::SetDepthWriteMode(true, kE_LEqual, true);
  }
  mBlurReset = false;

  GXSetTexCopySrc(0, CGraphics::GetRenderMode().xfbHeight - mTexture->GetHeight() * 2,
                  mTexture->GetWidth() * 2, mTexture->GetHeight() * 2);
  GXSetTexCopyDst(mTexture->GetWidth(), mTexture->GetHeight(),
                  mTexture->GetTexelFormat() == kTF_RGB565 ? GX_TF_RGB565 : GX_TF_RGBA8, GX_TRUE);
  // Upstream's function-local static, which nothing reads. Its lazy initialiser - the guard byte
  // load, the zero store and the flag store - is seven instructions of this function's bytes.
  static int unkInt = 0;
  mTexture->SetFlag1(true);
  GXCopyTex(mTexture->GetBitMapData(0), GX_TRUE);
  mTexture->UnLock();
  gpRender->SetViewport(backupVpLeft, backupVpTop, backupVpWidth, backupVpHeight);
  CGraphics::SetDepthRange(depthNear, depthFar);
}

void CWorldShadow::EnableModelProjectedShadow(const CTransform4f& transform, uint lightIndex,
                                              float scale) const {
  static float sqrt2 = sqrt__Ff(2.0f);
  CTransform4f textureTransform = CTransform4f::LookAt(
      CVector3f::Zero(), mLightPosition - mObjectPosition, CVector3f(0.f, 0.f, 1.f));
  CTransform4f rotation = transform;
  rotation.SetTranslation(CVector3f::Zero());
  textureTransform = rotation.GetInverse() * textureTransform;
  textureTransform *= CTransform4f::Scale(sqrt2 * mObjectHalfExtent * scale);
  textureTransform = textureTransform.GetInverse();
  textureTransform = CTransform4f::Translate(0.5f, 0.f, 0.5f) * textureTransform;
  const uchar lightMask = 1 << lightIndex;
  CCubeModel::EnableShadowMaps(mTexture.get(), textureTransform, lightMask, lightMask);
}

void CWorldShadow::DisableModelProjectedShadow() const { CCubeModel::DisableShadowMaps(); }

void CWorldShadow::ResetBlur() { mBlurReset = true; }
