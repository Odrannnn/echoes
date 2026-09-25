#include "MetroidPrime/ScriptObjects/CScriptForgottenObject.hpp"

#include "MetaRender/CCubeRenderer.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"
#include "MetroidPrime/ScriptLoaderRel.hpp"
#include "MetroidPrime/ScriptObjects/CScriptActor.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "Kyoto/Alloc/CMemory.hpp"
#include "REL/REL_Setup.h"
#include "dolphin/gx.h"

static SScriptForgottenObject_FuncPtrs funcPtrs;

struct SScriptActorForgottenObjectFlags {
  uchar x0_0 : 1;
  uchar x0_1 : 1;
  uchar x0_2 : 1;
  uchar x0_3 : 1;
  uchar x0_4 : 1;
  uchar renderByForgottenObject : 1;
};

const CEntityInfo& LdrToEntityInfo(CEntityInfo&, const SLdrEditorProperties&);

CScriptForgottenObject::CScriptForgottenObject(TUniqueId uid, const CEntityInfo& info,
                                               const rstl::string& name)
: CEntity(uid, info, name, false)
, x24_(kInvalidUniqueId)
, x28_(kInvalidUniqueId) {}

extern "C" TUniqueId fn_24_478(CScriptForgottenObject* self, CStateManager& mgr,
                                EScriptObjectState state);

extern "C" TUniqueId fn_24_478(CScriptForgottenObject* self, CStateManager& mgr,
                                EScriptObjectState state) {
  const TUniqueId id = self->FindConnectedObject(mgr, state, kSM_None);
  CScriptActor* actor = TCastToPtr< CScriptActor >(mgr.ObjectById(id));
  if (actor != nullptr && actor->CheckActorRenderOnly()) {
    reinterpret_cast< SScriptActorForgottenObjectFlags* >(
        reinterpret_cast< uchar* >(actor) + 0x396)
        ->renderByForgottenObject = true;
    return id;
  }
  return kInvalidUniqueId;
}

void CScriptForgottenObject::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CEntity::AcceptScriptMsg(mgr, msg);
  if (msg.GetMessage() == kSM_XALD) {
    x24_ = fn_24_478(this, mgr, kSS_Zero);
    x28_ = fn_24_478(this, mgr, kSS_MaxReached);
  }
}

void CScriptForgottenObject::Render1(CStateManager& mgr) { RenderInternal(mgr, x24_, false); }

void CScriptForgottenObject::Render2(CStateManager& mgr) { RenderInternal(mgr, x28_, true); }

void CScriptForgottenObject::RenderInternal(CStateManager& mgr, TUniqueId uid, bool b) {
  const CEntity* self = this;
  if (!self->GetActive()) {
    return;
  }

  const CActor* actor = TCastToConstPtr< CActor >(mgr.GetObjectById(uid));
  GXSetColorUpdate(GX_FALSE);
  if (actor != nullptr && !actor->GetPreRenderClipped()) {
    const CModelData* data = actor->GetModelData();
    if (b) {
      gpRender->UnkH(0xff);
    } else {
      gpRender->UnkI();
    }

    const CModelFlags flags(CModelFlags::kT_Opaque, 1.f);
    data->Render(mgr, actor->GetTransform(), nullptr, flags);

    if (b) {
      gpRender->UnkI();
    }
  }
  GXSetColorUpdate(GX_TRUE);
}

extern "C" SLdrEditorProperties* fn_24_1E4(SLdrEditorProperties* props, short freeMemory) {
  if (props != nullptr) {
    props->~SLdrEditorProperties();
    if (freeMemory > 0) {
      CMemory::Free(props);
    }
  }
  return props;
}

CEntity* LoadForgottenObject(CStateManager& mgr, CInputStream& input, const CEntityInfo& info) {
  SLdrEditorProperties props;
  int propertyCount = input.ReadUint16();
  for (int i = 0; i < propertyCount; ++i) {
    int propertyId = input.ReadInt32();
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

  return new CScriptForgottenObject(mgr.AllocateUniqueId(),
                                    LdrToEntityInfo(const_cast< CEntityInfo& >(info), props),
                                    props.name);
}

void SetFuncPtrs() {
  funcPtrs.loader = &LoadForgottenObject;
  SetSScriptForgottenObject_FuncPtrs(&funcPtrs);
}

// See the note in Tweaks.cpp: Tweaks, CannonBall and ForgottenObject each define
// RELMain/RELExit because on the cube they are three separate modules, and a flat
// host link needs the three entry points to have distinct names. The MWCC branch
// is the retail form and is unchanged, so this Matching unit still matches.
#ifdef __MWERKS__
void RELMain() { SetFuncPtrs(); }

void RELExit() { SetSScriptForgottenObject_FuncPtrs(nullptr); }
#else
extern "C" void mp_relmain_forgottenobject() { SetFuncPtrs(); }

extern "C" void mp_relexit_forgottenobject() { SetSScriptForgottenObject_FuncPtrs(nullptr); }
#endif

CScriptForgottenObject::~CScriptForgottenObject() {}
