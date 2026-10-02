#include "MetroidPrime/ScriptObjects/CScriptWaypoint.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/SLdrWaypoint.hpp"
#include "MetroidPrime/TCastTo.hpp"

// Guessed name
class CValidWaypointPredicate : public CValidEntityPredicate {
public:
  // CValidEntityPredicate
  ~CValidWaypointPredicate() override {}
  bool IsValid(const CStateManager& mgr, TUniqueId id) const override;
};

bool CValidWaypointPredicate::IsValid(const CStateManager& mgr, TUniqueId id) const {
  return TCastToConstPtr< CScriptWaypoint >(mgr.GetObjectById(id)) != nullptr;
}

CScriptWaypoint::CScriptWaypoint(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                                 const CTransform4f& xf)
: CActor(uid, name, info, 0, xf, CModelData::CModelDataNull(), CMaterialList(kMT_NoStepLogic),
         CActorParameters::None(), kInvalidUniqueId) {
  SetUseInSortedLists(false);
  SetCallTouch(false);
}

CScriptWaypoint::~CScriptWaypoint() {}

void CScriptWaypoint::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  const EScriptObjectMessage message = msg.GetMessage();
  CActor::AcceptScriptMsg(mgr, msg);

  if (GetActive()) {
    switch (message) {
    case static_cast< EScriptObjectMessage >('ARRV'):
      SendScriptMsgs(kSS_Arrived, mgr, msg.GetOriginator(), kSM_None);
      break;
    default:
      break;
    }
  }
}

TUniqueId CScriptWaypoint::NextWaypoint(CStateManager& mgr) const {
  rstl::vector< TUniqueId > ids =
      FindConnectedObjects_if(mgr, kSS_Arrived, kSM_Next, CValidWaypointPredicate());
  if (ids.empty()) {
    return kInvalidUniqueId;
  }
  return ids[int(mgr.Random()->Float() * ids.size() * 0.99f)];
}

TUniqueId CScriptWaypoint::FollowWaypoint(CStateManager& mgr) const {
  return CheckConnectedObject(mgr, kSS_Arrived, kSM_Follow);
}

void CScriptWaypoint::AddToRenderer(const CStateManager& mgr) const {}

void CScriptWaypoint::Render(const CStateManager& mgr) const {}


CEntity* LoadWaypoint(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SLdrWaypoint sldrThis;
  // `SLdrWaypoint.inc` written out by hand, the way `CScriptRelay.cpp` and `CUnknown90.cpp`
  // do it: the generated file is produced by `scripts/generate_script_loaders.py`, and the
  // one thing that has to differ is the type of the count below.
  //
  // The explicit `u16` conversion is redundant - `ReadUint16()` already returns `u16` - and
  // it is the one thing in this function that decides where the `operator new` result lives.
  // Without it mwcceppc hands that result the register the loop-invariant `0x255a4580` case
  // constant held (r29); retail's is r28, the one the property count held. Declaring the type
  // `u16` *without* the cast does not move it - only the extra conversion node does. Measured,
  // not guessed: `static_cast< u16 >` on this read reproduces retail byte for byte, while
  // `const int`/`const uint`/`const u16` alone all leave the four `mr`/`mr.` operands on r29.
  // Same fix and the same measurement as `CUnknown90.cpp`; see `docs/research/missing_classes.md`.
  const u16 propertyCount = static_cast< u16 >(input.ReadUint16());
  for (int i = 0; i < propertyCount; ++i) {
    const uint propertyId = input.Get< uint >();
    const u16 propertySize = input.ReadUint16();

    switch (propertyId) {
    case 0x255a4580: {
      LoadTypedefEditorProperties(sldrThis.editorProperties, input);
      break;
    }
    default:
      input.ReadBytes(nullptr, propertySize);
      break;
    }
  }

  return rs_new CScriptWaypoint(mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
                                LdrToEntityInfo(info, sldrThis.editorProperties),
                                LdrToTransform4f(sldrThis.editorProperties));
}
