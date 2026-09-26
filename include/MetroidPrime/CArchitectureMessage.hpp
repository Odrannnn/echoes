#ifndef _CARCHITECTUREMESSAGE
#define _CARCHITECTUREMESSAGE

#include "types.h"

#include "rstl/rc_ptr.hpp"

enum EArchMsgTarget {
  kAMT_IOWinManager,
  kAMT_Game,
};

enum EArchMsgType {
  kAM_RemoveIOWin = 0,
  kAM_CreateIOWin = 1,
  kAM_ChangeIOWinPriority = 2,
  kAM_RemoveAllIOWins = 3,
  kAM_TimerTick = 4,
  kAM_UserInput = 5,
  kAM_SetGameState = 6,
  kAM_ControllerStatus = 7,
  kAM_QuitGameplay = 8,
  kAM_FrameBegin = 10,
  kAM_FrameEnd = 11,
};

struct IArchitectureMessageParm {
  virtual ~IArchitectureMessageParm() {}
};

// Retail's is 16 bytes and holds *three* things, not four, which is worth writing down because
// every factory in MakeMsg writes four words and the fourth is easy to misread as a second
// parameter. `CreateFrameEnd` (0x800489AC), `CreateFrameBegin` (0x80048A80) and `CreateTimerTick`
// (0x80048DC8) all end with the same closing sequence:
//
//   stw r30,0(r29)      ; +0x00 target
//   stw r0,4(r29)       ; +0x04 type   (11, 10 and 4 respectively)
//   lwz r0,8(r1)        ; the rc_ptr they built on the stack
//   stw r0,8(r29)       ; +0x08 ... the rc_ptr's *data pointer*
//   lwz r0,12(r1)
//   stw r0,12(r29)      ; +0x0c ... the rc_ptr's *refcount pointer*
//   lwz r5,12(r29) ; lwz r4,0(r5) ; +1 ; stw  -> AddRef
//   bl ReleaseData__Q24rstl34rc_ptr<24IArchitectureMessageParm>Fv
//
// +0x08/+0x0c are the two halves of retail's `rstl::rc_ptr` ({ T* x0_ptr; u32* x4_refCount; }),
// not two parameters: the same `new(4)`-with-`*refCount = 1` that appears at 0x80144140 when
// CGameState's constructor builds its own rc_ptr, and the same `*(refCount) += 1` / Release pair.
// The three-argument constructor here is therefore retail's shape, and the fourth word is this
// port's rc_ptr being a single word wide - see include/rstl/rc_ptr.hpp.
class CArchitectureMessage {

public:
  CArchitectureMessage(EArchMsgTarget target, int type,
                       const rstl::rc_ptr< IArchitectureMessageParm >& parm)
  : x0_target(target), x4_type(static_cast< EArchMsgType >(type)), x8_parm(parm) {}

  EArchMsgType GetType() const { return x4_type; }
  EArchMsgTarget GetTarget() const { return x0_target; }

  // **Both overloads are out of line on purpose, and they are two retail functions.** Retail
  // carries them at 0x80048CEC and 0x80048CE4, 8 bytes each and identical - `lwz r3,8(r3) ; blr`,
  // the rc_ptr's data pointer - and they were unnamed in the DOL, so nothing in the port could
  // call them until `config/G2ME01/symbols.txt` was renamed to the two mangled names.
  //
  // `inline_max_size(0)` is what keeps mwcceppc from folding either one into its caller: the body
  // is two instructions and would otherwise be expanded, and `CMainFlow::OnMessage` needs the
  // `bl`. The definitions are in src/MetroidPrime/CArchitectureMessageGetParm.cpp, **not** here -
  // a definition in this header is emitted by every translation unit that includes it, which would
  // be several owners of one symbol. The same rule as `CArchitectureQueue::Pop` and
  // `CGameState::GetWorldState`.
  //
  // MWCC's mangling does encode constness, which is what lets the two coexist at all:
  // `GetParm__20CArchitectureMessageFv` and `GetParm__20CArchitectureMessageCFv` (measured with
  // mwcceppc, not assumed). Retail emits definitions in reverse source order, so the **non-const**
  // overload is written first and lands at the higher address, 0x80048CEC.
  IArchitectureMessageParm* GetParm();
  const IArchitectureMessageParm* GetParm() const;

private:
  EArchMsgTarget x0_target;
  EArchMsgType x4_type;
  rstl::rc_ptr< IArchitectureMessageParm > x8_parm;
};

namespace MakeMsg {
// Not `static`: at namespace scope that is internal linkage, so no other translation unit can
// define or call these, and the build warns "used but never defined" for both of them.
// Retail's three factories, all 0xCC bytes and identical bar the type constant and the parm:
//   CreateFrameEnd  (0x800489AC)  type 11, parm 8 bytes, vtable 0x803B1B60, holds an int
//   CreateFrameBegin(0x80048A80)  type 10, parm 8 bytes, vtable 0x803B1B60, holds an int
//   CreateTimerTick (0x80048DC8)  type  4, parm 8 bytes, vtable 0x803B1B70, holds a float
// The parm classes are defined in an anonymous namespace in src/MetroidPrime/main.cpp, so
// `GetParm()` hands back an `IArchitectureMessageParm` nothing can read yet: whoever needs the
// frame count or the tick has to promote them out of the anonymous namespace, or go through
// these factories. No consumer does today - `GetParm()` has no callers in the port.
CArchitectureMessage CreateFrameEnd(EArchMsgTarget target, const int& frameCount);
CArchitectureMessage CreateFrameBegin(EArchMsgTarget target, int frameCount);
CArchitectureMessage CreateTimerTick(EArchMsgTarget target, const float& deltaTime);
}

#endif // _CARCHITECTUREMESSAGE
