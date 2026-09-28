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

class CArchitectureMessage {

public:
  CArchitectureMessage(EArchMsgTarget target, int type,
                       const rstl::rc_ptr< IArchitectureMessageParm >& parm)
  : mTarget(target), mType(static_cast< EArchMsgType >(type)), mParm(parm) {}

  EArchMsgType GetType() const { return mType; }
  const IArchitectureMessageParm* GetParm() const { return mParm.GetPtr(); }
  EArchMsgTarget GetTarget() const { return mTarget; }

  // Port: retail carries a **non-const** `GetParm` at 0x80048CEC (8 bytes, `lwz r3,8(r3)` -
  // the rc_ptr's data pointer) next to the const one at 0x80048CE4, and both were unnamed in
  // the DOL. Upstream only models the const overload and defines it in the class body, so this
  // declaration is the missing half. The definition is in
  // `src/MetroidPrime/CArchitectureMessageGetParm.cpp` and is `inline_max_size(0)` there, which
  // is what stops mwcceppc folding the two instructions into a caller.
  IArchitectureMessageParm* GetParm();

private:
  EArchMsgTarget mTarget;
  EArchMsgType mType;
  rstl::rc_ptr< IArchitectureMessageParm > mParm;
};

#endif // _CARCHITECTUREMESSAGE
