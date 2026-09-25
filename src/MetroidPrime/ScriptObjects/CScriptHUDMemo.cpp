#include "MetroidPrime/ScriptObjects/CScriptHUDMemo.hpp"

#include "MetroidPrime/HUD/CSamusHud.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/TCastTo.hpp"

#include "MetroidPrime/ScriptLoader/SLdrHUDMemo.hpp"

#include "Kyoto/Text/CStringTable.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

namespace {
struct SHUDMemoData {
  SLdrEditorProperties editorProperties;
  float displayTime;
  bool clearWindow;
  bool player1;
  bool player2;
  bool player3;
  bool player4;
  bool typeOut;
  bool useOriginator;
  int displayType;
  CAssetId string;
};
union SHUDMemoStorage {
  uint alignment;
  SHUDMemoData data;
};
} // namespace

CScriptHUDMemo::CScriptHUDMemo(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                               const CHUDMemoParms& parms, bool useOriginator,
                               CScriptHUDMemo::EDisplayType disp, CAssetId msg)
: CEntity(uid, info, name, false)
, m_parms(parms)
, m_useOriginator(useOriginator)
, m_dispType(disp)
, m_stringTableId(msg)
, m_stringTable(msg == kInvalidAssetId ? rstl::optional_object_null()
                                       : rstl::optional_object< TLockedToken< CStringTable > >(
                                             gpSimplePool->GetObj(SObjectTag('STRG', msg)))) {}

CScriptHUDMemo::~CScriptHUDMemo() {}

void CScriptHUDMemo::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CHUDMemoParms parms = m_parms;
  if (m_useOriginator) {
    if (TCastToConstPtr< CPlayer >(mgr.GetObjectById(msg.GetOriginator()))) {
      uint mask = mgr.MaskUIdNumPlayers(msg.GetOriginator());
      parms = CHUDMemoParms(m_parms.GetDisplayTime(), m_parms.IsClearMemoWindow(),
                            m_parms.IsFadeOutOnly(), m_parms.IsHintMemo(), 1 << mask, true);
    }
  }

  switch (msg.GetMessage()) {
  case kSM_SetToZero:
    if (GetActive()) {
      if (m_dispType == kDT_MessageBox) {
        mgr.ShowPausedHUDMemo(m_stringTableId, m_parms.GetDisplayTime());
      } else if (m_stringTable) {
        CSamusHud::DisplayHudMemo((*m_stringTable)->GetString(0), parms);
      } else {
        CSamusHud::DisplayHudMemo(rstl::wstring_l(L""), parms);
      }
    }
    break;
  case kSM_Deactivate:
    if (GetActive() && m_dispType == kDT_StatusMessage) {
      CSamusHud::DisplayHudMemo(rstl::wstring_l(L""),
                                CHUDMemoParms(0.f, false, true, false, 0xf, true));
    }
    break;
  }

  CEntity::AcceptScriptMsg(mgr, msg);
}

CScriptHUDMemo* LoadHUDMemo(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SHUDMemoStorage storage;
  new (&storage.data) SHUDMemoData;
  storage.data.displayTime = 0.f;
  storage.data.clearWindow = true;
  storage.data.player1 = true;
  storage.data.player2 = true;
  storage.data.player3 = true;
  storage.data.player4 = true;
  storage.data.typeOut = true;
  storage.data.useOriginator = false;
  storage.data.displayType = 0;
  storage.data.string = kInvalidAssetId;

  int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    uint propertyId = (uint)input.ReadInt32();
    u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(storage.data.editorProperties, input);
      break;
    case 0x1a26c1cc: //('display_time', _decode_display_time),
      storage.data.displayTime = input.ReadFloat();
      break;
    case 0x84e2496f:
      storage.data.clearWindow = input.ReadBool();
      break;
    case 0xa8fadfa5:
      storage.data.player1 = input.ReadBool();
      break;
    case 0xef5aa575:
      storage.data.player2 = input.ReadBool();
      break;
    case 0xd23a8cc5:
      storage.data.player3 = input.ReadBool();
      break;
    case 0x601a50d5:
      storage.data.player4 = input.ReadBool();
      break;
    case 0xafd0158e:
      storage.data.typeOut = input.ReadBool();
      break;
    case 0xbd6f7b11:
      storage.data.useOriginator = input.ReadBool();
      break;
    case 0x4ab3b95b:
      storage.data.displayType = input.ReadInt32();
      break;
    case 0x9182250c:
      storage.data.string = input.ReadInt32();
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  int mask = 0;
  if (storage.data.player1) {
    mask |= 1;
  }
  if (storage.data.player2) {
    mask |= 2;
  }
  if (storage.data.player3) {
    mask |= 4;
  }
  if (storage.data.player4) {
    mask |= 8;
  }

  CScriptHUDMemo* result = new CScriptHUDMemo(
      mgr.AllocateUniqueId(), storage.data.editorProperties.name,
      LdrToEntityInfo(info, storage.data.editorProperties),
      CHUDMemoParms(storage.data.displayTime, storage.data.clearWindow, false, false, mask,
                    storage.data.typeOut),
      storage.data.useOriginator, CScriptHUDMemo::EDisplayType(storage.data.displayType),
      storage.data.string);
  storage.data.editorProperties.~SLdrEditorProperties();
  return result;
}
