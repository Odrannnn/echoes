// Functions for CScriptSkyRipple.

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"
#include "Kyoto/Alloc/CMemory.hpp"

class CScriptSkyRipple : public CActor {
public:
  CScriptSkyRipple(TUniqueId uid, const CEntityInfo& info,
                   const SLdrEditorProperties& props);
  ~CScriptSkyRipple() override;

private:
  TUniqueId x158_;
  TUniqueId x15a_;
  uint padding_;
};

extern "C" void fn_80232334(void* loader);
extern "C" void fn_800E6AD0(CModelData* modelData);
extern "C" CEntity* REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo(
    CStateManager& mgr, CInputStream& input, const CEntityInfo& info);
extern "C" void* __nw__FUlPCcPCc(uint size, const char* file, const char* function);
extern "C" const char lbl_70_rodata_C[];
const CEntityInfo& LdrToEntityInfo(CEntityInfo& info, const SLdrEditorProperties& props);

extern "C" {
FScriptLoader REL_loader_SkyRipple;
}

extern "C" void __ct__10CModelDataFv(CModelData* modelData) {
  fn_800E6AD0(modelData);
}

CScriptSkyRipple::CScriptSkyRipple(TUniqueId uid, const CEntityInfo& info,
                                   const SLdrEditorProperties& props)
: CActor(uid, props.name, info, 0, CTransform4f::Identity(), CModelData(), CMaterialList(),
         CActorParameters(), kInvalidUniqueId)
, x158_(kInvalidUniqueId), x15a_(kInvalidUniqueId) {
}

CScriptSkyRipple::~CScriptSkyRipple() {}

extern "C" SLdrEditorProperties* fn_70_210(SLdrEditorProperties* props, int deleteFlag) {
  if (props != nullptr) {
    props->~SLdrEditorProperties();
    if (static_cast<short>(deleteFlag) > 0) {
      CMemory::Free(props);
    }
  }
  return props;
}

extern "C" CHealthInfo* fn_70_60(CActor* self, CStateManager& mgr) {
  return self->HealthInfo(mgr);
}

extern "C" void RELExit() { fn_80232334(nullptr); }

extern "C" CEntity* REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo(
    CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrEditorProperties props;
  u16 propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    uint propertyId = static_cast<uint>(input.ReadInt32());
    u16 propertySize = input.ReadUint16();
    switch (propertyId) {
    case 0x255a4580:
      LoadTypedefEditorProperties(props, input);
      break;
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  CScriptSkyRipple* object = static_cast< CScriptSkyRipple* >(
      __nw__FUlPCcPCc(0x160, lbl_70_rodata_C, nullptr));
  if (object != nullptr) {
    TUniqueId uid = mgr.AllocateUniqueId();
    object = new (object) CScriptSkyRipple(
        uid, LdrToEntityInfo(const_cast< CEntityInfo& >(info), props), props);
  }
  return object;
}

extern "C" void SetRelLoaderFunctionToLoader__Fv() {
  REL_loader_SkyRipple = &REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo;
  fn_80232334(&REL_loader_SkyRipple);
}

extern "C" void RELMain() { SetRelLoaderFunctionToLoader__Fv(); }

// 0x00000000  __dt__16CScriptSkyRippleFv  size 0x60
// 0x00000060  fn_70_60  size 0x2C
// 0x0000008C  RELExit  size 0x24
// 0x000000B0  RELMain  size 0x20
// 0x000000D0  SetRelLoaderFunctionToLoader__Fv  size 0x30
// 0x00000100  REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo  size 0x110
// 0x00000210  fn_70_210  size 0x54
// 0x00000264  fn_70_264  size 0xCC
// 0x00000330  fn_70_330  size 0x1A0
// 0x000004D0  fn_70_4D0  size 0x188
// 0x00000658  fn_70_658  size 0x98
// 0x000006F0  fn_70_6F0  size 0x9C
// 0x0000078C  fn_70_78C  size 0x3C
// 0x000007C8  __ct__16CScriptSkyRippleF9TUniqueIdRC11CEntityInfoRC20SLdrEditorProperties  size 0xE4
// 0x000008AC  __ct__10CModelDataFv  size 0x20

// REL/REL_Setup.cpp functions.
// 0x000008CC  _unresolved  size 0xC4
// 0x00000990  _epilog  size 0x24
// 0x000009B4  _prolog  size 0x24
// 0x000009D8  fn_70_9D8  size 0x4C
// 0x00000A24  fn_70_A24  size 0x4C
