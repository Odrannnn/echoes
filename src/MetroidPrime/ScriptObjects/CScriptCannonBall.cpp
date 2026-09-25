
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

extern "C" CTransform4f fn_800CB764(const CMorphBall*);
extern "C" void fn_800E6AD0(CModelData*);
extern const float lbl_57_rodata_0;
extern const float lbl_57_rodata_4;
extern const float lbl_57_rodata_8;
extern const float lbl_57_rodata_C;
extern const float lbl_57_rodata_10;
extern const char lbl_57_rodata_14[];

CModelData::CModelData() { fn_800E6AD0(this); }

CScriptEffect::ParamStruct::ParamStruct(const SLdrSpline& spline, int unk1, float unk2, bool unk3)
: m_spline(spline), m_unk1(unk1), m_unk2(unk2), m_unk3(unk3) {}

SLdrCannonBall::~SLdrCannonBall() {}

CScriptCannonBall::CScriptCannonBall(TUniqueId uid, const rstl::string& name,
                                     const CEntityInfo& info, const CTransform4f& xf,
                                     CAssetId effect)

: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(), CActorParameters(),
         kInvalidUniqueId)
, m_effect(effect) {}

CScriptCannonBall::~CScriptCannonBall() {}

void CScriptCannonBall::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {

  case kSM_Increment: {
    if (CPlayer* player =
            TCastToPtr< CPlayer >(mgr.ObjectById(msg.GetOriginator()))) {
      CMorphBall* morph = player->GetMorphBall();
      CTransform4f xf = morph->xd28;
      player->SetTransformAlt(
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
      CScriptEffect::ParamStruct paramStruct(SLdrSpline(), 3, lbl_57_rodata_0, false);

      CScriptEffect* newEffect =
          new CScriptEffect(id, rstl::string_l(lbl_57_rodata_14 + 7),
                            CEntityInfo(GetCurrentAreaId(), rstl::vector< SConnection >(), true),
                            CTransform4f::Identity(), CVector3f::One(), m_effect, 1, 0, 0, 0,
                            lbl_57_rodata_0, lbl_57_rodata_0, lbl_57_rodata_4, lbl_57_rodata_4,
                            false, lbl_57_rodata_0, lbl_57_rodata_10, lbl_57_rodata_0, true, true,
                            true, lParams, false, paramStruct, false, false, false, 0);
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
: m_scriptObject(id), m_f(lbl_57_rodata_0), m_updateFrameIdx(0), m_b(b) {}

void CScriptCannonBall::TrackedShot::Think(float dt, CStateManager& mgr, int index) {
  if (!m_flag2) {
    return;
  }
  CScriptEffect* effect = TCastToPtr< CScriptEffect >(mgr.ObjectById(m_scriptObject));
  if (!effect) {
    return;
  }

  CPlayer* player = mgr.GetPlayer(index);
  if (m_f > lbl_57_rodata_4) {
    CTransform4f mat = fn_800CB764(player->GetMorphBall());
    effect->SetTransform(
        CTransform4f::LookAt(mat.GetTranslation(), mat.GetTranslation() + player->GetLookDir()));
  }
  if (m_b) {
    if (effect->GetFlagAt0x2c8()) {
      effect->AcceptScriptMsg(mgr, CScriptMsg(effect->GetUniqueId(), kInvalidUniqueId,
                                              kInvalidUniqueId, kSM_Activate, kSS_InvalidState));
      player->GetPlayerState()->SetItemAmount(CPlayerState::kIT_CannonBall, 1);
    }

    CMorphBall* morph = player->GetMorphBall();

    int uVar4 = (morph->xc78 - m_updateFrameIdx) < 0;
    int uVar3 = (morph->xc7c - m_updateFrameIdx) < 0;
    if (uVar3 < uVar4) {
      uVar3 = uVar4;
    }
    bool disable = true;
    if (uVar3 < 6 && morph->xc80_maybe_sa_state != 2) {
      CPlayer::EPlayerMorphBallState state = CPlayer::kMS_Unmorphed;
      if (player->GetSpawnedMorphballState() == CPlayer::kMS_Morphed) {
        state = player->GetMorphballTransitionState();
      }
      if (state == CPlayer::kMS_Morphed) {
        disable = false;
      }
    }
    if (disable) {

      effect->AcceptScriptMsg(mgr, CScriptMsg(effect->GetUniqueId(), kInvalidUniqueId,
                                              kInvalidUniqueId, kSM_Deactivate, kSS_InvalidState));
      player->GetPlayerState()->SetItemAmount(CPlayerState::kIT_CannonBall, 0);
      m_b = false;
    }

  } else {
    m_f -= dt / lbl_57_rodata_8;
    if (m_f < lbl_57_rodata_4) {
      m_f = lbl_57_rodata_4;
      effect->AcceptScriptMsg(mgr, CScriptMsg(effect->GetUniqueId(), kInvalidUniqueId,
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
  m_f = lbl_57_rodata_0;
}

void CScriptCannonBall::TrackedShot::FreeScriptObject(CStateManager& mgr) {
  mgr.DeleteObjectRequest(m_scriptObject);
}

CTransform4f LoadEditorTransform(const SLdrEditorProperties&);
const CEntityInfo& LdrToEntityInfo(CEntityInfo&, const SLdrEditorProperties&);

CEntity* REL_LoadCannonBall(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrEditorProperties editorProperties;
  CAssetId effect = static_cast<CAssetId>(-1);

  int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    uint propertyId = (uint)input.ReadInt32();
    u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(editorProperties, input);
      break;
    case 0xb68c6d96:
      effect = input.ReadInt32();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  return new CScriptCannonBall(mgr.AllocateUniqueId(), editorProperties.name,
                               LdrToEntityInfo(const_cast< CEntityInfo& >(info), editorProperties),
                               LoadEditorTransform(editorProperties), effect

  );
}

extern FScriptLoader REL_loader_CannonBall;

void SetRelLoaderFunctionToLoader() {
  REL_loader_CannonBall = REL_LoadCannonBall;
  SetLoader_CannonBall(&REL_loader_CannonBall);
}

extern "C" void RELMain() { SetRelLoaderFunctionToLoader(); }

extern "C" void RELExit() { SetLoader_CannonBall(nullptr); }

