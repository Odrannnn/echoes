
#include "MetroidPrime/ScriptObjects/CScriptCannonBall.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CMorphBall.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/ScriptLoader/SLdrCannonBall.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "MetroidPrime/ScriptObjects/CScriptEffect.hpp"
#include "MetroidPrime/TCastTo.hpp"

CScriptCannonBall::CScriptCannonBall(TUniqueId uid, const rstl::string& name,
                                     const CEntityInfo& info, const CTransform4f& xf,
                                     CAssetId effect)

: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(), CActorParameters(),
         kInvalidUniqueId)
, m_effect(effect) {}

CScriptCannonBall::~CScriptCannonBall() {}

// The header now only declares `~CGameSplineDesc`, and retail's own object defines it out of
// line here too (the local `spline` in `AcceptScriptMsg` is destroyed through it), so this unit
// carries its own copy. Declared after `~CScriptCannonBall` (retail offset 4180) and before
// `AcceptScriptMsg` (1816) because mwcceppc emits definitions in reverse source order.
CGameSplineDesc::~CGameSplineDesc() {}

void CScriptCannonBall::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {

  case kSM_Increment: {
    if (CPlayer* player =
            TCastToPtr< CPlayer >(mgr.GetObjectByIdFromListAll(msg.GetOriginator()))) {
      CMorphBall* morph = player->GetMorphBall();
      CTransform4f xf = morph->GetSurfaceToWorld();
      player->SetTransform(
          CTransform4f(xf.BuildMatrix3f(), player->GetTranslation())); // todo use position
      morph->SwitchToTire();
      m_fields[player->GetPlayerIndex()].OnIncrementMsg(mgr, 1);
    }
    break;
  }

  case kSM_XCRT: {
    for (int playerIndex = 0; playerIndex < mgr.GetNumPlayers(); ++playerIndex) {
      TUniqueId id = mgr.AllocateUniqueId();

      CLightParameters lParams;
      CGameSplineDesc spline(SLdrSpline(), CMotionSpline::kST_Bezier, 1.0f, false);

      CScriptEffect* newEffect =
          new CScriptEffect(id, rstl::string_l("CannonBall Effect"),
                            CEntityInfo(GetCurrentAreaId(), rstl::vector< SConnection >(), true),
                            CTransform4f::Identity(), CVector3f::One(), m_effect, 1, 0, 0, 0, 1.0f,
                            1.0f, 0.0f, 0.0f, false, 1.0f, 2.0f, 1.0f, true, true, true, lParams,
                            false, spline, false, false, false, CScriptEffect::kRO_Normal);
      newEffect->SetNextDrawNode(mgr.GetPlayer(playerIndex)->GetUniqueId());
      mgr.AddObject(newEffect);

      m_fields.push_back(TrackedShot(id, false));
    }
    break;
  }

  case kSM_XDelete: {
    for (int i = 0; i < m_fields.size(); ++i) {
      m_fields[i].FreeScriptObject(mgr);
    }
    m_fields.clear();
    break;
  }

  default:
    break;
  }
}

void CScriptCannonBall::Think(float dt, CStateManager& mgr) {
  for (int i = 0; i < m_fields.size(); ++i) {
    m_fields[i].Think(dt, mgr, i);
  }
}

CScriptCannonBall::TrackedShot::TrackedShot(TUniqueId id, bool b)
: m_scriptObject(id), m_f(1.0), m_updateFrameIdx(0), m_b(b) {}

void CScriptCannonBall::TrackedShot::Think(float dt, CStateManager& mgr, int index) {
  if (!m_flag2) {
    return;
  }
  CScriptEffect* effect = TCastToPtr< CScriptEffect >(mgr.GetObjectByIdFromListAll(m_scriptObject));
  if (!effect) {
    return;
  }

  CPlayer* player = mgr.GetPlayer(index);
  if (m_f > 0.0f) {
    CTransform4f mat = player->GetMorphBall()->GetBallToWorld();
    effect->SetTransform(
        CTransform4f::LookAt(mat.GetTranslation(), mat.GetTranslation() + player->GetLookDir()));
  }
  if (m_b) {
    if (!effect->IsEmitting()) {
      effect->AcceptScriptMsg(mgr, CScriptMsg(kInvalidUniqueId, effect->GetUniqueId(),
                                              kInvalidUniqueId, kSM_Activate, kSS_InvalidState));
      player->GetPlayerState()->SetItemAmount(CPlayerState::kIT_CannonBall, 1);
    }

    CMorphBall* morph = player->GetMorphBall();

    int uVar4 = (morph->GetLastWallCollisionFrame() - m_updateFrameIdx) < 0;
    int uVar3 = (morph->GetLastFloorCollisionFrame() - m_updateFrameIdx) < 0;
    if (uVar3 < uVar4) {
      uVar3 = uVar4;
    }
    bool disable = true;
    if (uVar3 < 6 && morph->GetBallState() != CMorphBall::kBS_Spider) {
      CPlayer::EPlayerMorphBallState state = CPlayer::kMS_Unmorphed;
      if (player->GetSpawnedMorphballState() == CPlayer::kMS_Morphed) {
        state = player->GetMorphballTransitionState();
      }
      if (state == CPlayer::kMS_Morphed) {
        disable = false;
      }
    }
    if (disable) {

      effect->AcceptScriptMsg(mgr, CScriptMsg(kInvalidUniqueId, effect->GetUniqueId(),
                                              kInvalidUniqueId, kSM_Deactivate, kSS_InvalidState));
      player->GetPlayerState()->SetItemAmount(CPlayerState::kIT_CannonBall, 0);
      m_b = false;
    }

  } else {
    m_f -= dt / 0.25f;
    if (m_f < 0.0f) {
      m_f = 0.0f;
      effect->AcceptScriptMsg(mgr, CScriptMsg(kInvalidUniqueId, effect->GetUniqueId(),
                                              kInvalidUniqueId, kSM_Deactivate, kSS_InvalidState));
      m_flag2 = false;
    }
  }

  CColor color = CColor::White().WithAlphaModulatedBy(m_f * player->fn_8000BE98());
  effect->SetModelFlags(CModelFlags(CModelFlags::kT_One, color));
}

void CScriptCannonBall::TrackedShot::OnIncrementMsg(CStateManager& mgr, int param) {
  uchar flags = param;
  m_b = flags;
  m_flag2 = flags;
  if (flags == 0) {
    return;
  }
  m_updateFrameIdx = mgr.GetUpdateFrameIdx();
  m_f = 1.0f;
}

void CScriptCannonBall::TrackedShot::FreeScriptObject(CStateManager& mgr) {
  mgr.DeleteObjectRequest(m_scriptObject);
}

CTransform4f LoadEditorTransform(const SLdrEditorProperties&);

// Retail has this one out of line, in this REL (__dt__14SLdrCannonBallFv at .text 0x1F8).
SLdrCannonBall::~SLdrCannonBall() {}

CEntity* REL_LoadCannonBall(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrCannonBall sldrThis;

  int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    uint propertyId = (uint)input.ReadInt32();
    u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefSLdrEditorProperties(sldrThis.editorProperties, input);
      break;
    case 0xb68c6d96:
      sldrThis.effect = input.ReadInt32();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  return new CScriptCannonBall(mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
                               LdrToEntityInfo(info, sldrThis.editorProperties),
                               LoadEditorTransform(sldrThis.editorProperties), sldrThis.effect

  );
}

FScriptLoader REL_loader_CannonBall;

void SetRelLoaderFunctionToLoader() {
  REL_loader_CannonBall = REL_LoadCannonBall;
  SetLoader_CannonBall(&REL_loader_CannonBall);
}

// On the cube mwldeppc's linker script calls RELMain/RELExit to build this
// module's prolog and epilog. Tweaks, CannonBall and ForgottenObject each define
// them, which is correct there: they are three separate modules. A flat host link
// cannot hold three symbols with one name, so on the host each gets a distinct
// name and platform/compiled_modules.cpp registers them by module name and runs
// them before the game's entry. MWCC still compiles RELMain/RELExit, so the
// GameCube object is unchanged.
#ifdef __MWERKS__
#define MP_CANNONBALL_MAIN RELMain
#define MP_CANNONBALL_EXIT RELExit
#else
#define MP_CANNONBALL_MAIN mp_relmain_cannonball
#define MP_CANNONBALL_EXIT mp_relexit_cannonball
#endif

extern "C" void MP_CANNONBALL_MAIN() { SetRelLoaderFunctionToLoader(); }

extern "C" void MP_CANNONBALL_EXIT() { SetLoader_CannonBall(nullptr); }
