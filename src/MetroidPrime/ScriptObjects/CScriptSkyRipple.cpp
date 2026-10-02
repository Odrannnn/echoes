// Functions for CScriptSkyRipple.

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptActor.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"
#include "Kyoto/Alloc/CMemory.hpp"

class CScriptSkyRipple : public CActor {
public:
  CScriptSkyRipple(TUniqueId uid, const CEntityInfo& info,
                   const SLdrEditorProperties& props);
  ~CScriptSkyRipple() override;

  // Retail connects the ripple to the two objects it mirrors, at 0x158 and 0x15a.
  TUniqueId x158_;
  TUniqueId x15a_;

private:
  uint padding_;
};

extern "C" void fn_80232334(void* loader);
#ifndef __MWERKS__
extern "C" void fn_800E6AD0(CModelData* modelData);
#endif
extern "C" CEntity* REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo(
    CStateManager& mgr, CInputStream& input, const CEntityInfo& info);
extern "C" void* __nw__FUlPCcPCc(uint size, const char* file, const char* function);
extern "C" const char lbl_70_rodata_C[];
// The two connection states fn_70_658 passes to fn_70_6F0: retail holds them in .data as
// the four bytes "IS00" and "IS01" and loads them with `lis`/`lwz`, so they are `EScriptObjectState`
// objects read by value, not immediates. Defined here because the port links this file and an
// extern with no definition is one more undefined symbol against the gap baseline.
extern "C" EScriptObjectState lbl_70_data_0 = static_cast< EScriptObjectState >(0x49533030);
extern "C" EScriptObjectState lbl_70_data_4 = static_cast< EScriptObjectState >(0x49533031);

extern "C" {
// Host-only initialiser; see CScriptPufferRel.cpp. MWCC keeps the retail common symbol.
#ifdef __MWERKS__
FScriptLoader REL_loader_SkyRipple;
#else
FScriptLoader REL_loader_SkyRipple = 0;
#endif
}

// The REL's local forwarder to CModelData's DOL default constructor (0x800E6AD0, named
// `__ct__10CModelDataFv` in upstream's symbols.txt); see CScriptScriptStreamedMovie.cpp.
// On the host, that file already defines the extern-C forwarder name, and a flat host link
// cannot hold it twice.
#ifdef __MWERKS__
extern "C" void __ct__10CModelDataFv(CModelData* modelData);

extern "C" void fn_70_8AC(CModelData* modelData) { __ct__10CModelDataFv(modelData); }
#else
extern "C" void mp_skyripple_ct__10CModelDataFv(CModelData* modelData) {
  fn_800E6AD0(modelData);
}
#endif

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

// Retail 0x264. The vtable slot after Think, which the REL's symbol table leaves unnamed;
// it moves the two connected objects to the current camera before CActor::Think runs.
extern "C" void fn_70_264(CScriptSkyRipple* self, float dt, CStateManager& mgr) {
  if (self->GetActive()) {
    CActor* first = TCastToPtr< CActor >(mgr.ObjectById(self->x158_));
    CActor* second = TCastToPtr< CActor >(mgr.ObjectById(self->x15a_));
    const CVector3f& cameraPosition =
        mgr.GetCameraManager(0)->GetCurrentCamera(mgr, true)->GetTranslation();
    if (first != nullptr) {
      first->SetTranslation(cameraPosition);
    }
    if (second != nullptr) {
      second->SetTranslation(cameraPosition);
    }
  }
  self->CActor::Think(dt, mgr);
}

// Retail 0x78C. The vtable entry between ClearFluidList and AddToRenderer.
extern "C" void fn_70_78C(CScriptSkyRipple* self, CStateManager& mgr) {
  if (self->GetActive()) {
    // Read through a reference, not through GetUniqueId(): the by-value argument is one
    // stack temporary, and retail fills it with a single `lhz` from the member at 0x8.
    mgr.fn_80037A04(*reinterpret_cast< TUniqueId* >(
        reinterpret_cast< uint* >(self) + 2));
  }
}

// Retail 0x6F0. Called from AcceptScriptMsg twice, with lbl_70_data_0 and lbl_70_data_4
// ("IS00" and "IS01") as the connection state. Returns the connected actor's id, or
// kInvalidUniqueId when there is none or it is not render-only.
extern "C" TUniqueId fn_70_6F0(CScriptSkyRipple* self, CStateManager& mgr,
                              EScriptObjectState state) {
  TUniqueId id = self->FindConnectedObject(mgr, state, kSM_Attach);
  if (CScriptActor* actor = TCastToPtr< CScriptActor >(mgr.ObjectById(id))) {
    if (actor->CheckActorRenderOnly()) {
      actor->SetSkipRendering(true);
      return id;
    }
  }
  return kInvalidUniqueId;
}

// Retail 0x658. The vtable entry after PreThink.
extern "C" void fn_70_658(CScriptSkyRipple* self, CStateManager& mgr, const CScriptMsg& msg) {
  self->CActor::AcceptScriptMsg(mgr, msg);
  // Only kSM_XALD, which retail spells as one shifted subtract of 0x5841 plus an equality
  // against the low half 0x4c44, so the body runs for that one message and not for the whole
  // 0x58410000..0x58414C44 range.
  if (msg.GetMessage() == kSM_XALD) {
    self->x158_ = fn_70_6F0(self, mgr, lbl_70_data_0);
    self->x15a_ = fn_70_6F0(self, mgr, lbl_70_data_4);
  }
}

extern "C" CHealthInfo* fn_70_60(CActor* self, CStateManager& mgr) {
  return self->HealthInfo();
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
extern "C" void RELExit() { fn_80232334(nullptr); }
#else
extern "C" void mp_relexit_skyripple() { fn_80232334(nullptr); }
#endif

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
        uid, LdrToEntityInfo(info, props), props);
  }
  return object;
}

extern "C" void SetRelLoaderFunctionToLoader__Fv() {
  REL_loader_SkyRipple = &REL_LoadSkyRipple__FR13CStateManagerR12CInputStreamRC11CEntityInfo;
  fn_80232334(&REL_loader_SkyRipple);
}

#ifdef __MWERKS__
extern "C" void RELMain() { SetRelLoaderFunctionToLoader__Fv(); }
#else
extern "C" void mp_relmain_skyripple() { SetRelLoaderFunctionToLoader__Fv(); }
#endif

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
// 0x000008AC  fn_70_8AC  size 0x20 (forwarder to the DOL CModelData ctor)

// REL/REL_Setup.cpp functions.
// 0x000008CC  _unresolved  size 0xC4
// 0x00000990  _epilog  size 0x24
// 0x000009B4  _prolog  size 0x24
// 0x000009D8  fn_70_9D8  size 0x4C
// 0x00000A24  fn_70_A24  size 0x4C
