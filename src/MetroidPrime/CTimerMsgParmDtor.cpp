// Retail 0x80048834-0x80048890: `CTimerMsgParm::~CTimerMsgParm()`, 0x5C = 92 bytes, and the
// `.data` claim is `lbl_803B1B70`, the class's vtable.
//
// The twin of `MetroidPrime/CFrameMsgParmDtor.cpp` and byte-for-byte the same body: retail's four
// parm destructors - `fn_800487B8` (0x800487B8), `fn_80048834` (0x80048834), `fn_800488B8`
// (0x800488B8) and `fn_80048930` (0x80048930) - are all 0x5C and differ only in which of the four
// vtables they store, at `lis`/`addi`:
//
//   80048850  3c 60 80 3b   lis   r3,0x803b
//   80048854  38 03 1b 70   addi  r0,r3,0x1b70    ; 0x803B1B70 - this class's vtable
//   ...
//   80048860  38 03 0d d0   addi  r0,r3,0xdd0     ; 0x803B0DD0 - the base's, as always
//
// A destructor is four bytes of code longer than its own vtable is wide apart: 0x5C here, and the
// header's comment on `CFrameMsgParm` explains why the class shape (inline, empty base destructor)
// is what makes the register allocation come out with `this` in r31 and the flag left in r4.
//
// The vtable is 0xC: two zero header words and **one** slot, the destructor. Retail's block at
// 0x803B1B70 reads 0, 0, 0x80048834, 0 and the trailing zero is `lbl_803B1B80`'s, so 0x10 would be
// claiming a word this object does not carry.
#include "MetroidPrime/CArchitectureMessageParm.hpp"

CTimerMsgParm::~CTimerMsgParm() {}
