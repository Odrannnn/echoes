#ifndef _CARCHITECTUREMESSAGEPARM
#define _CARCHITECTUREMESSAGEPARM

#include "types.h"

#include "MetroidPrime/CArchitectureMessage.hpp"

// ---------------------------------------------------------------------------
// Retail's message-parm classes, promoted out of `src/MetroidPrime/main.cpp`'s anonymous
// namespace on 2026-09-26 (lane k3). See docs/research/boot_probe.md's closing section, blocker 1.
//
// **Why they had to move.** `CMainFlow::OnMessage` (retail `fn_8001DF54`, 0x8001DF54, 180 bytes)
// constructs an 8-byte parm on its stack and stores **two vtable addresses into it**:
//
//   8001dfb4  3c a0 80 3b  lis   r5,0x803b
//   8001dfb8  38 05 0d d0  addi  r0,r5,0xdd0     ; 0x803B0DD0, the base's vtable
//   8001dfbc  3c 80 80 3b  lis   r4,0x803b
//   8001dfc0  38 04 1b 60  addi  r0,r4,0x1b60     ; 0x803B1B60, the derived's vtable
//   8001dfc4  90 01 00 08  stw   r0,8(r1)
//   8001dfc8  90 01 00 08  stw   r0,8(r1)
//
// so a `Matching` unit for `OnMessage` has to place both exactly. The only classes in the tree with
// that shape were `CFrameMsgParm` and `CTimerMsgParm` in main.cpp's anonymous namespace, and a
// class in an anonymous namespace gets **local** vtable symbols - so the vtable could not be named,
// claimed or placed at all. `0x803B1B60` is the *same* vtable `MakeMsg::CreateFrameEnd` uses (its
// constructor is retail's `fn_80048814`, which writes `0x803B0DD0` then `0x803B1B60` then the int at
// +4), so the parm `OnMessage` builds is exactly `CFrameMsgParm`.
//
// **The destructors are declared here and defined in their own translation units** - that is blocker
// 2, and it is the same requirement as the vtable: retail calls the destructor *out of line*
// (`addi r3,r1,8 ; li r4,-1 ; bl 0x800487B8`), while a class that is complete in the translation unit
// that constructs it gets its destructor expanded instead, and `OnMessage` would then carry a dtor
// body where retail has one `bl`. Being out of line is also what makes the destructor the class's
// **key function**, and the key function's unit is the one that emits the vtable - so the
// `.data` claim lives with the destructor (see `MetroidPrime/CFrameMsgParmDtor.cpp`).
//
// The accessors are non-virtual and uncalled, so they cost nothing: a class's vtable has one slot per
// *virtual*, and MWCC does not emit an un-odr-used inline member at all.
// ---------------------------------------------------------------------------

/// Retail's parm for `kAM_FrameEnd` and `kAM_FrameBegin` (and for the `kAM_SetGameState` message
/// `CMainFlow::OnMessage` handles): 8 bytes, a vptr and one `int` at +4. Three retail symbols:
/// the vtable `lbl_803B1B60`, the constructor `fn_80048814` (0x80048814, 28 bytes) and the
/// out-of-line deleting destructor `fn_800487B8` (0x800487B8, 0x5C = 92 bytes).
class CFrameMsgParm : public IArchitectureMessageParm {
public:
  explicit CFrameMsgParm(int frameCount) : x4_frameCount(frameCount) {}
  ~CFrameMsgParm() override;

  int GetFrameCount() const { return x4_frameCount; }

private:
  int x4_frameCount;
};

/// Retail's parm for `kAM_TimerTick`: 8 bytes, a vptr and one `float` at +4, written by
/// `fn_8004898C` (0x8004898C) as a bare `stfs 1,4(r3)`. Vtable `lbl_803B1B70`, destructor
/// `fn_80048834` (0x80048834, 92 bytes).
class CTimerMsgParm : public IArchitectureMessageParm {
public:
  explicit CTimerMsgParm(float deltaTime) : x4_deltaTime(deltaTime) {}
  ~CTimerMsgParm() override;

  float GetDeltaTime() const { return x4_deltaTime; }

private:
  float x4_deltaTime;
};

#endif // _CARCHITECTUREMESSAGEPARM
