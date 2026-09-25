#include "MetroidPrime/ScriptObjects/CScriptAreaProperties.hpp"

#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/CGameArea.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/ScriptLoader/SLdrAreaAttributes.hpp"

// Retail defines exactly one of these, at 0x80239BD4, and its first parameter is a
// non-const CEntityInfo& - so the loader const_casts the const CEntityInfo& it is
// handed. Same declaration as CScriptCannonBall.cpp / CScriptForgottenObject.cpp /
// CScriptSkyRipple.cpp; the one in CEntityInfo.hpp is the const overload, which retail
// does not have.
const CEntityInfo& LdrToEntityInfo(CEntityInfo& info, const SLdrEditorProperties& props);

CScriptAreaProperties::CScriptAreaProperties(TUniqueId uid, const CEntityInfo& info, float density,
                                             float normalLightning, uint hasSkyBox,
                                             bool isDarkWorld, uint environmentEffects,
                                             CAssetId skyBoxAssetId, int phazonDamage, int unk1,
                                             float unk2, float unk3, const CColor& color)

: CEntity(uid, info, "AreaAttributes", false)
, m_hasSkybox(hasSkyBox)
, m_isDarkWorld(isDarkWorld)
, m_environmentEffects(environmentEffects)
, m_density(density)
, m_normalLightning(normalLightning)
, m_skyBoxAssetId(skyBoxAssetId)
, m_phazonDamage(phazonDamage)
, skyBoxModel(hasSkyBox ? rstl::optional_object< TLockedToken< CModel > >(
                              gpSimplePool->GetObj(SObjectTag('CMDL', skyBoxAssetId)))
                        : rstl::optional_object_null())
, x4c(unk1)
, x50(unk2)
, x54(unk3)
, m_color(color) {}

void CScriptAreaProperties::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message(msg.GetMessage());
  CEntity::AcceptScriptMsg(mgr, msg);
  if (GetCurrentAreaId() != kInvalidAreaId) {
    switch (message) {
    case kSM_XCRT:
      mgr.SetIsDarkWorld(m_isDarkWorld);
      break;
    case kSM_XALD:
      mgr.World()->Area(GetCurrentAreaId())->SetAreaAttributes(this);
      if (m_environmentEffects) {
        mgr.EnvFxManager()->SetDensity(m_density, 500);
      }
      break;
    case kSM_Play:
      mgr.EnvFxManager()->Play_801620A8();
      break;
    case kSM_Stop:
      mgr.EnvFxManager()->Stop_801620B4();
      break;
    case kSM_XDelete: {
      if (mgr.World()->Area(GetCurrentAreaId())->GetPhase() == 0x10) {
        mgr.World()->Area(GetCurrentAreaId())->SetAreaAttributes(nullptr);
      }
      break;
    }
    default:
      break;
    }
  }
}

CEntity* LoadAreaProperties(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrAreaAttributes sldrThis;
  // Retail's prologue (0x8013C2F8..) stores the fields in exactly this order, and mwcceppc
  // emits independent stores in source order, so the order is the source's.
  sldrThis.environmentGroupSound = -1;
  sldrThis.overrideSky = kInvalidAssetId;
  sldrThis.editorProperties.unknown_0x5d298a43 = 3;
  sldrThis.needSky = false;
  sldrThis.darkWorld = false;
  sldrThis.environmentEffects = 0;
  sldrThis.density = 0.f;
  sldrThis.normalLighting = 0.f;
  sldrThis.phazonDamage = 0;

  int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    uint propertyId = (uint)input.ReadInt32();
    u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(sldrThis.editorProperties, input);
      break;
    case 0x95d4bee7:
      sldrThis.needSky = input.ReadBool();
      break;
    case 0xb24fde1a:
      sldrThis.darkWorld = input.ReadBool();
      break;
    case 0x9d0006ab:
      sldrThis.environmentEffects = input.ReadInt32();
      break;
    case 0x56263e35:
      sldrThis.environmentGroupSound = input.ReadInt32();
      break;
    case 0x64e5fe9f:
      sldrThis.density = input.ReadFloat();
      break;
    case 0xba5f801e:
      sldrThis.normalLighting = input.ReadFloat();
      break;
    case 0xd208c9fa:
      sldrThis.overrideSky = input.ReadInt32();
      break;
    case 0xffeebc46:
      sldrThis.phazonDamage = input.ReadInt32();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  // Retail's tail (0x8013C584..): CColor::Black()'s four bytes are copied into a local at
  // r1+24 rather than the pointer being passed, mgr.AllocateUniqueId()'s result lands in a
  // local at r1+20, and the SLdrAreaAttributes is at r1+28 - with the two call temporaries at
  // r1+16. Only the CColor is a named local here: giving AllocateUniqueId and LdrToEntityInfo
  // named locals as well moves the whole frame 4 bytes the wrong way, because the `new`'s
  // argument slots are what retail's source has.
  CColor color = CColor::Black();
  return new CScriptAreaProperties(
      mgr.AllocateUniqueId(),
      LdrToEntityInfo(const_cast<CEntityInfo&>(info), sldrThis.editorProperties),
      sldrThis.density, sldrThis.normalLighting, sldrThis.needSky, sldrThis.darkWorld,
      sldrThis.environmentEffects, sldrThis.overrideSky, sldrThis.phazonDamage, 0, 0.0f, 0.0f,
      color);
}

CScriptAreaProperties::~CScriptAreaProperties() {}
