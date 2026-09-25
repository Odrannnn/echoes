#include "MetroidPrime/ScriptObjects/CScanTreeInventory.hpp"

#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/Streams/CInputStream.hpp"

#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrScannableParameters.hpp"
#include "rstl/string.hpp"

#ifdef TARGET_PC
static const CPlayerState::EItemType kInventorySlotToItemType[] = {
  CPlayerState::kIT_PowerBeam, 
  CPlayerState::kIT_DarkBeam, 
  CPlayerState::kIT_LightBeam,
  CPlayerState::kIT_AnnihilatorBeam,
  CPlayerState::kIT_SuperMissile,
  CPlayerState::kIT_Darkburst,
  CPlayerState::kIT_Sunburst,
  CPlayerState::kIT_SonicBoom,
  CPlayerState::kIT_CombatVisor,
  CPlayerState::kIT_ScanVisor,
  CPlayerState::kIT_DarkVisor,
  CPlayerState::kIT_EchoVisor,
  CPlayerState::kIT_VariaSuit,
  CPlayerState::kIT_DarkSuit,
  CPlayerState::kIT_LightSuit,
  CPlayerState::kIT_MorphBall,
  CPlayerState::kIT_BoostBall,
  CPlayerState::kIT_SpiderBall,
  CPlayerState::kIT_MorphBallBombs,
  CPlayerState::kIT_DarkBomb,
  CPlayerState::kIT_LightBomb,
  CPlayerState::kIT_AnnihilatorBomb,
  CPlayerState::kIT_ChargeBeam,
  CPlayerState::kIT_GrappleBeam,
  CPlayerState::kIT_SpaceJumpBoots,
  CPlayerState::kIT_GravityBoost,
  CPlayerState::kIT_SeekerLauncher,
  CPlayerState::kIT_ScrewAttack,
  CPlayerState::kIT_Powerbomb,
  CPlayerState::kIT_Missile,
  CPlayerState::kIT_DarkAmmo,
  CPlayerState::kIT_LightAmmo,
  CPlayerState::kIT_EnergyTanks,
  CPlayerState::kIT_TempleKey1,
  CPlayerState::kIT_TempleKey2,
  CPlayerState::kIT_TempleKey3,
  CPlayerState::kIT_TempleKey4,
  CPlayerState::kIT_TempleKey5,
  CPlayerState::kIT_TempleKey6,
  CPlayerState::kIT_TempleKey7,
  CPlayerState::kIT_TempleKey8,
  CPlayerState::kIT_TempleKey9,
  CPlayerState::kIT_AgonKey1,
  CPlayerState::kIT_AgonKey2,
  CPlayerState::kIT_AgonKey3,
  CPlayerState::kIT_TorvusKey1,
  CPlayerState::kIT_TorvusKey2,
  CPlayerState::kIT_TorvusKey3,
  CPlayerState::kIT_HiveKey1,
  CPlayerState::kIT_HiveKey2,
  CPlayerState::kIT_HiveKey3,
  CPlayerState::kIT_EnergyTransferModule,
  CPlayerState::kIT_ChargeCombo
};
#else
// Retail's copy of the slot table sits in rodata outside this unit's split.
extern "C" const CPlayerState::EItemType lbl_803ACAE0[];
#endif

#ifndef TARGET_PC
extern "C" CScanTreeInventory* fn_8021294C(void* self, int id, const SLdrTransform& transform,
                                           CAssetId nameStringTable, CAssetId scannableInfo,
                                           CPlayerState::EItemType itemType,
                                           const rstl::string& nameStringName);
#endif

struct SLdrScanTreeInventory {
  SLdrScanTreeInventory() : nameStringTable(kInvalidAssetId) { inventorySlotId = 10; }

  SLdrEditorProperties editorProperties;
  CAssetId nameStringTable;
  rstl::string nameStringName;
  uint inventorySlotId;
  SLdrScannableParameters scannableParams;
};

CScanTreeInventory* LoadScanTreeInventory(int* id, CInputStream& input) {
  SLdrScanTreeInventory sldrThis;
  
  const u16 propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    const uint propertyId = input.Get< uint >();
    const u16 propertySize = input.ReadUint16();
    
    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(sldrThis.editorProperties, input);
      break;
    case 0x46219bac:
      sldrThis.nameStringTable = input.ReadInt32();
      break;
    case 0x32698bd6:
      sldrThis.nameStringName = rstl::string(input);
      break;
    case 0x3d326f90:
      sldrThis.inventorySlotId = input.ReadInt32();
      break;
    case 0x2da1ec33:
      LoadTypedefSLdrScannableParameters(sldrThis.scannableParams, input);
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

#ifdef TARGET_PC
  return new CScanTreeInventory(
      *id & 0xffff, sldrThis.editorProperties.transform, sldrThis.nameStringTable,
      sldrThis.scannableParams.scannableInfo0,
      sldrThis.inventorySlotId < 0x35 ? kInventorySlotToItemType[sldrThis.inventorySlotId]
                                      : CPlayerState::kIT_PowerBeam,
      sldrThis.nameStringName);
#else
  // The retail map has no name for CScanTreeInventory's constructor.
  void* result = operator new(0x6c, "??(??)", nullptr);
  if (result != nullptr) {
    result = fn_8021294C(result, *id & 0xffff, sldrThis.editorProperties.transform,
                         sldrThis.nameStringTable, sldrThis.scannableParams.scannableInfo0,
                         sldrThis.inventorySlotId < 0x35
                             ? lbl_803ACAE0[sldrThis.inventorySlotId]
                             : CPlayerState::kIT_PowerBeam,
                         sldrThis.nameStringName);
  }
  return static_cast< CScanTreeInventory* >(result);
#endif
}

#ifndef TARGET_PC
// Retail's string pool for this unit holds "Logbook" and "Samus Gear" ahead of the loader's
// "??(??)". MWCC pools strings in reverse order of appearance, so they came from functions after
// the loader that the linker dead-stripped. These stand-ins keep "??(??)" at the same pool offset.
const char* CScanTreeInventory_UnusedString0() { return "Samus Gear"; }
const char* CScanTreeInventory_UnusedString1() { return "Logbook"; }
#endif
