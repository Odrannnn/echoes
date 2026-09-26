// Retail 0x8001DAF4-0x8001DB54: `CMainFlow::~CMainFlow()`, 96 bytes - and the `.data` claim is the
// other half of this unit, `vtable for CMainFlow` at 0x803B1770.
//
// The destructor is CMainFlow's key function, so this is the unit that emits the vtable. The
// vtable's `OnMessage` slot relocates against `OnMessage__9CMainFlowFRC20CArchitectureMessage
// R18CArchitectureQueue`, and **that symbol is still retail's own bytes**: retail's `OnMessage`
// is unnamed in the DOL (`fn_8001DF54`) and no unit here reproduces it, so `symbols.txt` is
// renamed to give dtk's fill object the name the vtable needs. Without that rename this unit does
// not link - which is the honest state of the port: the vtable is defined and `OnMessage` is
// still a hole, and the port link says so by name instead of failing on a vtable that names
// nothing. See docs/research/port_link_stubs.md: this is not a stub, it is retail's own code.
//
//   8001daf4  94 21 ff f0   stwu   r1,-16(r1)
//   8001daf8  7c 08 02 a6   mflr   r0
//   8001df00 ...                      epilogue shape identical to CIOWin's
//   8001db14  3c a0 80 3b   lis    r5,0x803b       ; &vtable for CMainFlow
//   8001db18  38 80 00 00   li     r4,0             ; the base is destroyed, not deleted
//   8001db1c  38 05 17 70   addi   r0,r5,0x1770
//   8001db20  90 1e 00 00   stw    r0,0(r30)
//   8001db24  48 02 c3 0d   bl     0x80049e30      ; CIOWin::~CIOWin
//   8001db28  7f e0 07 35   extsh. r0,r31          ; the deleting flag
//   8001db2c  40 81 00 0c   ble    +0x44
//   8001db30  7f c3 f3 78   mr     r3,r30
//   8001db34  48 2b 08 55   bl     0x802ce388      ; CMemory::Free
//
// `x14_gameState` is a plain enum, so the body is `{}`: vptr, base destructor, and the deleting
// tail. Nothing in the source names the vtable - mwcceppc derives it, and the seven words it emits
// are retail's.
#include "MetroidPrime/CMainFlow.hpp"

CMainFlow::~CMainFlow() {}
