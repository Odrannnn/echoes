// Retail 0x8001DF54-0x8001E008: `CMainFlow::OnMessage(const CArchitectureMessage&,
// CArchitectureQueue&)`, 0xB4 = 180 bytes.
//
// This is the function `CMainFlow`'s vtable slot 2 points at, and the one that made the
// `vtable for CMainFlow` unit (MetroidPrime/CMainFlowDtor.cpp) possible while leaving
// `OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue` a hole in the port's link.
// See docs/research/boot_probe.md's closing section, and the three blockers named there: all
// three are now closed, and the way each was closed is written on the file that carries it.
//
//   #1 the 8-byte local is a class with two vtables at fixed addresses
//      -> include/MetroidPrime/CArchitectureMessageParm.hpp: CFrameMsgParm came out of
//         src/MetroidPrime/main.cpp's anonymous namespace, so its vtable symbol is nameable.
//   #2 retail calls the destructor out of line
//      -> the destructor is declared in that header and defined in
//         src/MetroidPrime/CFrameMsgParmDtor.cpp, so this unit *cannot* see its body and emits
//         `bl` where retail has one.
//   #3 CArchitectureMessage::GetParm() is out of line in retail
//      -> src/MetroidPrime/CArchitectureMessageGetParm.cpp, and the `inline_max_size(0)` in
//         CArchitectureMessage.hpp that keeps it from being folded in here.
//
// ## The body
//
//   8001df60  80 04 00 04   lwz  r0,4(r4)      ; msg.GetType() - the message is r4
//   8001df6c  2c 00 00 05   cmpwi r0,5
//   8001df70  93 c1 00 18   stw  r30,24(r1)
//   8001df74  7c 7e 1b 78   mr   r30,r3       ; `this`, spilled in the middle of the dispatch
//   8001df78  41 82 00 74   beq  0x8001dfec   ; == kAM_TimerTick (4)?  no, beq is 5
//   8001df7c  40 80 00 10   bge  0x8001df8c
//   8001df80  2c 00 00 04   cmpwi r0,4
//   8001df84  40 80 00 14   bge  0x8001df98   ; == kAM_TimerTick -> AdvanceGameState
//   8001df88  48 00 00 64   b    0x8001dfec   ; default: kMR_Normal
//   8001df8c  2c 00 00 07   cmpwi r0,7
//   8001df90  40 80 00 5c   bge  0x8001dfec   ; > kAM_ControllerStatus: default
//   8001df94  48 00 00 10   b    0x8001dfa4   ; falls into kAM_SetGameState
//   8001df98  7f e4 fb 78   mr   r4,r31       ; &queue
//   8001df9c  4b ff fe cd   bl   0x8001de68   ; AdvanceGameState(queue)
//   8001dfa0  48 00 00 4c   b    0x8001dfec
//   8001dfa4  7c 83 23 78   mr   r3,r4        ; &msg
//   8001dfa8  48 02 ad 3d   bl   0x80048ce4   ; msg.GetParm()
//   8001dfac  3c a0 80 3b   lis  r5,0x803b
//   8001dfb0  3c 80 80 3b   lis  r4,0x803b
//   8001dfb4  38 05 0d d0   addi r0,r5,0xdd0  ; 0x803B0DD0, IArchitectureMessageParm's vtable
//   8001dfb8  90 01 00 08   stw  r0,8(r1)     ; the local's base vptr
//   8001dfbc  38 04 1b 60   addi r0,r4,0x1b60  ; 0x803B1B60, CFrameMsgParm's vtable
//   8001dfc0  7f e5 fb 78   mr   r5,r31
//   8001dfc4  90 01 00 08   stw  r0,8(r1)     ; then its own, over the top
//   8001dfc8  80 83 00 04   lwz  r4,4(r3)     ; the parm's int at +4 ...
//   8001dfcc  7c c3 f3 78   mr   r3,r30       ; this
//   8001dfd0  90 81 00 0c   stw  r4,12(r1)    ; ... into the local's int at +4
//   8001dfd4  4b ff fb 81   bl   0x8001db54   ; SetGameState(state, queue)
//   8001dfd8  38 61 00 08   addi r3,r1,8
//   8001dfdc  38 80 ff ff   li   r4,-1
//   8001dfe0  48 02 a7 d9   bl   0x800487b8   ; ~CFrameMsgParm, the *deleting* destructor
//   8001dfe4  38 60 00 01   li   r3,1         ; kMR_Exit
//   8001dfe8  48 00 00 08   b    0x8001dff0
//   8001dfec  38 60 00 00   li   r3,0         ; kMR_Normal
//   8001dff0 ...                               epilogue
//
// **The dispatch is a five-way comparison, not a jump table**, and it is not a choice: mwcceppc
// turns a sparse `switch` over `EArchMsgType` into `cmpwi`/`bge` pairs with the two arms that
// matter - `kAM_TimerTick` (4) and `kAM_SetGameState` (6) - falling into a `default`. The `cmpwi 7`
// / `bge` is the range check for the 6 arm: the compiler is walking the sorted case values and
// bounds-checking. Writing the two cases out as a `switch` is what produces this; the previous
// lane's probe established that and this file only has to keep it.
//
// **The local at `r1+8` is a `CFrameMsgParm`, and the two `stw`s into `8(r1)` are the two
// constructors' vptr stores, in order**: the base's first, then the derived's over the top. That is
// what an inlined `CFrameMsgParm(int)` looks like, and it is why the unit must not see
// `CFrameMsgParm`'s destructor body: with the destructor complete here, the `bl 0x800487b8` at
// 0x8001dfe0 becomes an expansion and the function is not retail's shape (blocker #2).
//
// **`lwz r4,4(r3)` at 0x8001dfc8 is the copy of the parm's `int` member**, and it lands *after*
// the two vptr stores because the block is `CFrameMsgParm`'s **copy constructor expanded in
// place** - base vptr, then own vptr, then the member, in the class's declaration order. The
// obvious alternative, `CFrameMsgParm parm(src->GetFrameCount())`, puts the same load at the top
// of the block and reuses r3 for the second vtable address; 86.33% against 100.00%, and no
// combination of an int temporary and an explicit read fixes it. See the body.
#include "MetroidPrime/CMainFlow.hpp"

#include "MetroidPrime/CArchitectureMessageParm.hpp"

CIOWin::EMessageReturn CMainFlow::OnMessage(const CArchitectureMessage& msg,
                                            CArchitectureQueue& queue) {
  // **The two `kMR_Normal` returns are one return statement, and that is what produces retail's
  // shape.** Written as `case kAM_TimerTick: AdvanceGameState(queue); return kMR_Normal;` plus a
  // `default: return kMR_Normal;`, mwcceppc emits `li r3,0` at the end of *each* arm and then
  // branches to a common epilogue - 180 bytes with one instruction too many. Retail has
  // `b 0x8001dfa0` after `bl AdvanceGameState`, and 0x8001dfec is the single `li r3,0` that both
  // that arm and the `default` reach, so the `switch` falls out of the `kAM_SetGameState` arm
  // instead of returning from it. Breaks, not returns.
  switch (msg.GetType()) {
  case kAM_TimerTick:
    AdvanceGameState(queue);
    break;
  case kAM_SetGameState: {
    // **The local is a *copy* of the message's parm, and that is what fixes the instruction
    // order.** Written as an int - `CFrameMsgParm parm(src->GetFrameCount())` - mwcceppc hoists the
    // `lwz r4,4(r3)` to the top of the block, reuses r3 for the second vtable address and emits
    // 180 bytes in which six instructions sit somewhere else. Written as a copy from the parm, the
    // generated code *is* the copy constructor expanded in place, and the order follows the class:
    // the base's vptr, then the derived's, then the member - which is retail's order, byte for
    // byte. Measured with tools/try_batch.py over five spellings; this was the only one at zero
    // differing instructions, and it is the only one that is also the obvious thing to write.
    CFrameMsgParm parm(*static_cast< const CFrameMsgParm* >(msg.GetParm()));
    SetGameState(static_cast< EClientFlowStates >(parm.GetFrameCount()), queue);
    return kMR_Exit;
  }
  default:
    break;
  }
  return kMR_Normal;
}
