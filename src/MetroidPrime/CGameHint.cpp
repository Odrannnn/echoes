#include "MetroidPrime/CGameHint.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/TCastTo.hpp"

// Retail's base-class call reads its `CMaterialList` argument out of a DOL global, not out of the
// constant `kMT_NoStepLogic`: `.sdata:0x804188D0` (`lbl_804188D0`, eight bytes) is loaded by the
// `lwz r5` of the material window. Spelled as the literal, MW pools a private copy of the zero
// into this object (`.sdata` `@374`) instead of relocating, so the unit gains a section the
// retail object does not have. `config/G2ME01/splits.txt` claims no `.sdata` here, so the read
// goes through the extern and the DOL's own bytes answer it. The value is unchanged: the global
// is zero, so `Add` still contributes `u64(1) << 0`. CScriptSpindleCamera.cpp does the same for
// `lbl_804186F0` and explains the pool at greater length.
extern "C" const EMaterialTypes lbl_804188D0;

CGameHint::CGameHint(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                     const CTransform4f& xf, int priority, float timer, int acrossAreas,
                     int breakType, uint deleteOnRemoval, uint requiredPresses, float unknown16c,
                     const SCallback& onExpire, const SCallback& onBreak, float breakDelay)
// The comma in the eighth argument is load-bearing and changes no behaviour: reading the const
// TUniqueId has no side effect and the value is discarded, so the expression is exactly
// CActorParameters(). Retail materialises the kInvalidUniqueId copy (`lhz` + `sth r0,16(r1)`)
// *before* the CActorParameters temporary's constructor call; written plainly, MW evaluates the
// arguments the other way round, sinks the `sth` into the CMaterialList window, and those 7
// instructions come out permuted - the same 7 instructions, 412 bytes, but the ctor stops at
// 94.16667%. The comma sequences the copy first, which is retail's order.
: CActor(uid, name, info, 0, xf, CModelData::CModelDataNull(), CMaterialList(lbl_804188D0),
         (static_cast< void >(kInvalidUniqueId), CActorParameters()), kInvalidUniqueId)
, mPriority(priority)
, mTimer(timer)
, mBreakType(breakType)
, mDeleteOnRemoval(deleteOnRemoval)
, mRequiredPresses(requiredPresses)
, x16c_(unknown16c)
, mOnExpire(onExpire)
, mOnBreak(onBreak)
, mBreakDelay(breakDelay)
, mAcrossAreas(acrossAreas) {}

CGameHint::~CGameHint() {}

void CGameHint::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  if (!mgr.IsMultiplayer()) {
    CActor::AcceptScriptMsg(mgr, msg);
    return;
  }

  CScriptMsg forwarded = msg;
  switch (msg.GetMessage()) {
  case kSM_Deactivate:
  case kSM_Decrement:
  case kSM_Increment:
    if (CGameCamera* camera = TCastToPtr< CGameCamera >(mgr.ObjectById(msg.GetOriginator()))) {
      forwarded = CScriptMsg(msg.GetUnk(), msg.GetId(), camera->Player(mgr).GetUniqueId(),
                             msg.GetMessage(), msg.GetState());
    }
    break;
  default:
    break;
  }
  CActor::AcceptScriptMsg(mgr, forwarded);
}
