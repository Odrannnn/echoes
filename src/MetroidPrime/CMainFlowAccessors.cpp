// Retail 0x8001DF48-0x8001DF54: `CMainFlow::Draw` and `CMainFlow::GetIsContinueDraw`, 12 bytes
// of `.text` in two functions.
//
// Same mechanism as MetroidPrime/CIOWinAccessors.cpp, and the same proof: `vtable for CMainFlow`
// is 0x1C bytes at 0x803B1770 and its contents are 0, 0, 0x8001DAF4, 0x8001DF54, 0x8001DF4C,
// 0x8001DF48, 0x80049E10. Two zero words of header, then one slot per virtual in declaration
// order: `~CMainFlow`, `OnMessage`, `GetIsContinueDraw`, `Draw`, and then `PreDraw` - which is
// **not** overridden here, so the last slot is `CIOWin::PreDraw` at 0x80049E10, the same
// address CIOWin's own vtable holds. That is the check that the slot order read here is right,
// and it is also why the port's header must not give CMainFlow a `PreDraw`.
//
//   8001df48  4e 80 00 20   blr     Draw
//   8001df4c  38 60 00 00   li r3,0  GetIsContinueDraw
//   8001df50  4e 80 00 20   blr
//
// `GetIsContinueDraw` returns **false** here and **true** in CIOWin: the two vtables disagree, and
// that is what makes it a measurement rather than a guess. (What it *means* - this IOWin declining
// to keep drawing and letting the flow it drives do it - is inference, not something the bytes say.)
//
// Descending retail offset, as the lane briefing requires: mwcceppc emits definitions in reverse
// source order and the linker keeps the object's `.text` order verbatim.
#include "MetroidPrime/CMainFlow.hpp"

bool CMainFlow::GetIsContinueDraw() const { return false; }
void CMainFlow::Draw() const {}
