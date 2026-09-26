// Retail 0x80049E30-0x80049E98: `CIOWin::~CIOWin()`, 104 bytes - and the `.data` claim is the
// other half of this unit, `vtable for CIOWin` at 0x803B1BA0.
//
// The destructor is the class's **key function** (the first non-pure, non-inline virtual), so this
// is the translation unit that emits the vtable, and the vtable's three accessors' slots relocate
// against MetroidPrime/CIOWinAccessors.cpp. Retail's vtable is 28 bytes here and 0x20 in
// `config/G2ME01/symbols.txt` - dtk rounds an object's size up, and the next symbol
// `__vt__6CActor` starts at 0x803B1BC0, so the claim is exactly 0x803B1BA0-0x803B1BC0 either way.
//
// The vtable is not written out in the source and must not be: mwcceppc derives it from the
// class, and it comes out as 0, 0, &~CIOWin, 0 (OnMessage is pure), &GetIsContinueDraw, &Draw,
// &PreDraw - which is retail's, word for word. Claiming the `.data` range is what stops `dtk dol
// split` also filling it with retail bytes, which would be a second owner for one symbol.
//
//   80049e30  94 21 ff f0   stwu   r1,-16(r1)
//   80049e34  7c 08 02 a6   mflr   r0
//   80049e38  90 01 00 14   stw    r0,20(r1)
//   80049e3c  93 e1 00 0c   stw    r31,12(r1)
//   80049e40  7c 9f 23 78   mr     r31,r4          ; the deleting flag
//   80049e44  93 c1 00 08   stw    r30,8(r1)
//   80049e48  7c 7e 1b 79   mr.    r30,r3
//   80049e4c  41 82 00 30   beq    +0x4c
//   80049e50  3c 60 80 3b   lis    r3,0x803b       ; &vtable for CIOWin
//   80049e54  34 1e 00 04   addic. r0,r30,4
//   80049e58  38 03 1b a0   addi   r0,r3,0x1ba0
//   80049e5c  90 1e 00 00   stw    r0,0(r30)
//   80049e60  41 82 00 0c   beq    +0x3c
//   80049e64  38 7e 00 04   addi   r3,r30,4        ; &name
//   80049e68  48 2b 4b 51   bl     0x802fe9b8      ; ~rstl::basic_string
//   80049e6c  7f e0 07 35   extsh. r0,r31
//   80049e70  40 81 00 0c   ble    +0x4c
//   80049e74  7f c3 f3 78   mr     r3,r30
//   80049e78  48 28 45 11   bl     0x802ce388      ; CMemory::Free
//   ...                       epilogue
//
// ~rstl::basic_string is `{ internal_dereference(); }` and `name` is the only member, so the body
// is `{}` - the vptr store, the member's destructor and the `delete this` tail are all the
// compiler's. `this == nullptr` returning early and the flag in r4 are the deleting-destructor
// convention the whole port uses, and `CIOWin::CIOWin` (0x80049E98) and
// `CMainFlow::~CMainFlow` (0x8001DAF4) are the same shape.
#include "MetroidPrime/CIOWin.hpp"

#include "rstl/string.hpp"

CIOWin::~CIOWin() {}
