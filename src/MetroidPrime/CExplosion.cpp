#include "MetroidPrime/CExplosion.hpp"

#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CGenDescription.hpp"
#include "Kyoto/Particles/CParticleElectric.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CGameLight.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "rstl/math.hpp"

// `.sbss` render flags, written by `CStateManager::fn_80036650` and defined outside this unit's
// split (`src/MetroidPrime/mainHead.cpp`). `ShouldDraw` below reads them: retail's is
// `lwz r31, lbl_80419AA0@sda21(r0)` / `lwz r30, lbl_80419A9C@sda21(r0)`, so `lbl_80419AA0` is the
// mask it ANDs with and `lbl_80419A9C` the value it compares against.
extern "C" uint lbl_80419A9C;
extern "C" uint lbl_80419AA0;

// The five `CParticleGen` methods retail defines out of line in this object, in the descending
// retail `.text` order mwcceppc needs (0x800534CC, 0x800534C4, 0x800534BC, 0x800534B4,
// 0x800534B0). `Kyoto/Particles/CParticleGen.hpp` only declares them; a body there would make
// every including object emit its own weak copy instead.

// .text:0xBCC | 0x800534CC | size: 0x54 - vtable index 23. The three locals are load-bearing:
// written as one expression, mwcc loads both flags *after* the virtual call into volatile
// registers and emits 20 bytes; it hoists them above the call into r31/r30 - and so spills and
// reloads them - only when they are read into locals first.
bool CParticleGen::ShouldDraw() const {
  const uint mask = lbl_80419AA0;
  const uint flags = lbl_80419A9C;
  const uint drawFlags = GetDrawFlags();
  return (drawFlags & mask) == flags;
}

// .text:0xBC4 | 0x800534C4 | size: 0x8 - vtable index 22.
uint CParticleGen::GetDrawFlags() const { return mDrawFlags; }

// .text:0xBBC | 0x800534BC | size: 0x8 - vtable index 20. Retail's `lfs f1,lbl_8041A920@sda21(r0)`
// is a real load, so this stays a literal rather than a folded `li f1,1.0`. The literal lands in
// this object's `.sdata2`, which is where the linked DOL's `.sdata2:0x8041A920` word comes from;
// `lbl_8041A920` is only defined on the PC side (`src/MetroidPrime/PortGlobals.cpp`), so naming it
// here would be an undefined symbol in the DOL link.
float CParticleGen::GetGeneratorRate() const { return 1.f; }

// .text:0xBB4 | 0x800534B4 | size: 0x8 - vtable index 12.
void CParticleGen::SetDrawFlags(uint flags) { mDrawFlags = flags; }

// .text:0xBB0 | 0x800534B0 | size: 0x4 - vtable index 11. Empty in retail: `blr` and nothing else.
void CParticleGen::SetGeneratorRate(float rate) {}

CExplosion::CExplosion(const TLockedToken< CGenDescription >& particle, TUniqueId uid,
                       const CEntityInfo& info, const rstl::string& name, const CTransform4f& xf,
                       const uint flags, const CVector3f& scale, const CColor& color,
                       int playerIndex)
: CEffect(uid, info, name, xf)
, mParticleGen(rs_new CElementGen(TToken< CGenDescription >(particle), CElementGen::kMOT_Normal,
                                  flags & 1 ? CElementGen::kOSF_Two : CElementGen::kOSF_One))
, mExplosionLight(kInvalidUniqueId)
, mSourceId(CToken(particle).GetTag().GetId())
, mScale(scale)
, mFlags(flags)
, mPlayerIndex(playerIndex)
, mHasRenderBounds(true)
, mUnknownFlag(flags & 2)
, mFixedTimeStep(flags & 4)
, mTime(0.f) {
  mParticleGen->SetGlobalTranslation(xf.GetTranslation());
  mParticleGen->SetOrientation(xf.GetRotation());
  mParticleGen->SetGlobalScale(scale);
  mParticleGen->SetModulationColor(color);
}

CExplosion::CExplosion(const TLockedToken< CElectricDescription >& electric, TUniqueId uid,
                       const CEntityInfo& info, const rstl::string& name, const CTransform4f& xf,
                       uint flags, const CVector3f& scale, const CColor& color, int playerIndex)
: CEffect(uid, info, name, xf)
, mParticleGen(rs_new CParticleElectric(TToken< CElectricDescription >(electric)))
, mExplosionLight(kInvalidUniqueId)
, mSourceId(CToken(electric).GetTag().GetId())
, mScale(scale)
, mFlags(flags)
, mPlayerIndex(playerIndex)
, mHasRenderBounds(true)
, mUnknownFlag(flags & 2)
, mFixedTimeStep(flags & 4) {
  // The native electric constructor does not initialize mTime.
  mParticleGen->SetGlobalTranslation(xf.GetTranslation());
  mParticleGen->SetOrientation(xf.GetRotation());
  mParticleGen->SetGlobalScale(scale);
  mParticleGen->SetModulationColor(color);
}

CExplosion::~CExplosion() {}

void CExplosion::AddToRenderer(const CStateManager& mgr) const {
  if (!GetPreRenderClipped()) {
    EnsureRendered(mgr);
  }
}

void CExplosion::Render(const CStateManager&) const { mParticleGen->Render(); }

void CExplosion::PreRender(CStateManager& mgr) {
  SetPreRenderClipped(!mHasRenderBounds);
  if (GetPreRenderClipped()) {
    return;
  }

  CActor::PreRender(mgr);
  if (GetPreRenderClipped()) {
    return;
  }

  if (mPlayerIndex == mgr.GetCurrentRenderPlayerIndex() &&
      mgr.GetCurrentRenderCameraManager()->IsInFPCamera()) {
    SetPreRenderClipped(true);
  } else if (mFlags & 8) {
    const CGameCamera* camera =
        mgr.CameraManager(mgr.GetCurrentRenderPlayerIndex())->CurrentCamera(mgr, true);
    const float distance = (GetTranslation() - camera->GetTranslation()).Magnitude();
    float scale = rstl::min_val(4.f, rstl::max_val(0.2f, distance));
    scale *= 0.25f;
    mParticleGen->SetGlobalScale(mScale * scale);
  }
}

void CExplosion::Think(float dt, CStateManager& mgr) {
  if (GetTransformDirtySpare()) {
    mParticleGen->SetGlobalTranslation(GetTranslation());
    mParticleGen->SetOrientation(GetTransform().GetRotation());
    SetTransformDirtySpare(false);
  }
  mParticleGen->Update(mFixedTimeStep ? 1.0 / 60.0 : dt);

  if (mExplosionLight != kInvalidUniqueId) {
    CGameLight* light = TCastToPtr< CGameLight >(mgr.ObjectById(mExplosionLight));
    if (light && GetActive()) {
      light->SetLight(mParticleGen->GetLight());
    }
  }

  mTime += dt;
  if (mTime > 15.f) {
    mgr.DeleteObjectRequest(GetUniqueId());
  } else {
    if (mParticleGen->IsSystemDeletable()) {
      mgr.DeleteObjectRequest(GetUniqueId());
    }
    CActor::Think(dt, mgr);
  }
}

void CExplosion::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  const TUniqueId sender = msg.GetUnk();
  switch (message) {
  case kSM_XCRT:
    if (mgr.GetNumPlayers() < 3u && mParticleGen->SystemHasLight()) {
      mExplosionLight = mgr.AllocateUniqueId();
      const uint sourceId = mSourceId;
      mgr.AddObject(rs_new CGameLight(mExplosionLight, GetCurrentAreaId(), GetActive(),
                                      rstl::string_l(""), GetTransform(), GetUniqueId(),
                                      mParticleGen->GetLight(), sourceId, 1, 0.f));
    }
    break;
  case kSM_XDelete:
    if (mExplosionLight != kInvalidUniqueId) {
      mgr.DeleteObjectRequest(mExplosionLight);
      mExplosionLight = kInvalidUniqueId;
    }
    break;
  default:
    break;
  }

  CActor::AcceptScriptMsg(mgr, msg);
  if (mExplosionLight != kInvalidUniqueId) {
    mgr.SendScriptMsg(mExplosionLight, sender, message, kInvalidUniqueId);
  }
}

void CExplosion::PreRenderAllViewports(CStateManager& mgr) {
  rstl::optional_object< CAABox > bounds = mParticleGen->GetBounds();
  if (bounds) {
    SetOtherBounds(*bounds);
    SetRenderBounds(*bounds);
    mHasRenderBounds = true;
  } else {
    mHasRenderBounds = false;
    const CVector3f pos = GetTranslation();
    const CAABox pointBounds(pos, pos);
    SetOtherBounds(pointBounds);
    SetRenderBounds(pointBounds);
  }
  UpdatePortalSystemState(mgr);
}
