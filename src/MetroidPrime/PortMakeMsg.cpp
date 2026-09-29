/**
 * Port-only: the `MakeMsg` factories and accessors that `CInputGenerator.cpp` and
 * `CIOWinManager.cpp` call, `GetParmTimerTick`, which the IOWins' `OnMessage` reads once
 * `DistributeOneMessage` reaches them, and `CreateCreateIOWin`, which `CMainFlow::SetGameState`
 * calls on the first frame.
 *
 *   GetParmDeleteIOWin          0x80048F70, 0x8
 *   CreateCreateIOWin           0x80048EA4, 0xCC
 *   GetParmCreateIOWin          0x80048E9C, 0x8
 *   GetParmChangeIOWinPriority  0x80048E94, 0x8
 *   GetParmTimerTick            0x80048DC0, 0x8
 *   CreateUserInput             0x80048CF4, 0xCC
 *   CreateControllerStatus      0x80048C08, 0xDC
 *
 * They are in upstream's `src/MetroidPrime/Decode.cpp`, a `MatchingFor` unit, and the bodies
 * below are that file's, verbatim. Decode.cpp itself cannot be in files.cmake: it also defines
 * `CreateFrameEnd`/`CreateFrameBegin`/`CreateTimerTick`, which `mainMid.cpp` already defines for
 * the port (tools/check_files_cmake.py's entry for it has the measurement). This file is not in
 * configure.py, so it cannot affect main.dol.
 *
 * `CMainFlowDtor.cpp` calls `CreateCreateIOWin` by its old placeholder name, `fn_80048EA4`,
 * through `extern "C"` with pointer parameters - the spelling that reproduces retail's call site
 * under mwcceppc (point 1 of that file's notes). `fn_80048EA4` below forwards that call: the first
 * argument is the target (the call site passes 0, `kAMT_IOWinManager`), the next two point at the
 * priority pair, and the last points at the window pointer - retail's `const int&, const int&,
 * CIOWin* const&` passed as the addresses they are.
 *
 * Descending by retail address, as mwcceppc emits in reverse source order.
 */
#include "MetroidPrime/Decode.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

const CArchMsgParmString& MakeMsg::GetParmDeleteIOWin(const CArchitectureMessage& msg) {
  return *static_cast< const CArchMsgParmString* >(msg.GetParm());
}

CArchitectureMessage MakeMsg::CreateCreateIOWin(EArchMsgTarget target, const int& pmin,
                                                const int& pmax, CIOWin* const& iowin) {
  return CArchitectureMessage(
      target, kAM_CreateIOWin,
      rs_new CArchMsgParmInt32Int32VoidPtr(pmin, pmax, reinterpret_cast< const void* >(iowin)));
}

extern "C" CArchitectureMessage fn_80048EA4(const char* target, const int* pmin, const int* pmax,
                                            void* iowin) {
  return MakeMsg::CreateCreateIOWin(static_cast< EArchMsgTarget >(reinterpret_cast< uintptr_t >(target)),
                                    *pmin, *pmax, *static_cast< CIOWin* const* >(iowin));
}

const CArchMsgParmInt32Int32VoidPtr& MakeMsg::GetParmCreateIOWin(const CArchitectureMessage& msg) {
  return *static_cast< const CArchMsgParmInt32Int32VoidPtr* >(msg.GetParm());
}

const CArchMsgParmInt32Int32String&
MakeMsg::GetParmChangeIOWinPriority(const CArchitectureMessage& msg) {
  return *static_cast< const CArchMsgParmInt32Int32String* >(msg.GetParm());
}

const CArchMsgParmReal32& MakeMsg::GetParmTimerTick(const CArchitectureMessage& msg) {
  return *static_cast< const CArchMsgParmReal32* >(msg.GetParm());
}

CArchitectureMessage MakeMsg::CreateUserInput(EArchMsgTarget target, const CFinalInput& input) {
  return CArchitectureMessage(target, kAM_UserInput, rs_new CArchMsgParmUserInput(input));
}

CArchitectureMessage MakeMsg::CreateControllerStatus(const EArchMsgTarget target, const short& chan,
                                                     const bool& connected) {
  return CArchitectureMessage(target, kAM_ControllerStatus,
                              rs_new CArchMsgParmControllerStatus(chan, connected));
}
