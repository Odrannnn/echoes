// Retail 0x800487B8-0x80048814: `CFrameMsgParm::~CFrameMsgParm()`, 0x5C = 92 bytes - and the
// `.data` claim is the other half of this unit, `lbl_803B1B60`, the class's vtable.
//
// **The destructor is the class's key function, so this is the unit that emits the vtable**, and
// that is why the out-of-line definition the header declares and the `.data` claim belong together.
// The accessors do not: nothing here needs them beside the vtable. This is the same arrangement
// `MetroidPrime/CIOWinDtor.cpp` and `MetroidPrime/CMainFlowDtor.cpp` use, and the same reason.
//
//   800487b8  94 21 ff f0   stwu  r1,-16(r1)
//   800487bc  7c 08 02 a6   mflr  r0
//   800487c0  90 01 00 14   stw   r0,20(r1)
//   800487c4  93 e1 00 0c   stw   r31,12(r1)      ; ONE saved register - see below
//   800487c8  7c 7f 2b 79   mr.   r31,r3         ; `this`, and the flag stays live in r4
//   800487cc  41 82 00 30   beq   +0x44          ; if (this == 0) return
//   800487d0  3c 60 80 3b   lis   r3,0x803b
//   800487d4  38 03 1b 60   addi  r0,r3,0x1b60    ; 0x803B1B60 - this class's vtable
//   800487d8  90 1f 00 00   stw   r0,0(r31)
//   800487dc  41 82 00 10   beq   +0x10          ; DEAD: same (this == 0) test, r31 == r3 != 0
//   800487e0  3c 60 80 3b   lis   r3,0x803b
//   800487e4  38 03 0d d0   addi  r0,r3,0xdd0     ; 0x803B0DD0 - the base's vtable
//   800487e8  90 1f 00 00   stw   r0,0(r31)
//   800487ec  7f e0 07 35   extsh. r0,r4          ; the deleting flag, sign-extended
//   800487f0  40 81 00 0c   ble   +0xc
//   800487f4  7f e3 fb 78   mr    r3,r31
//   800487f8  48 28 5b 91   bl    0x802ce388     ; CMemory::Free
//   800487fc ...                                  epilogue
//
// Two things in there are worth writing down because they are the whole reason this body is
// reproducible at all, and both are measurements, not readings of the source:
//
// **There is no call to the base destructor.** The base's vptr is restored and nothing is called,
// because `IArchitectureMessageParm`'s destructor is *inline and empty*, and MWCC expands it - all
// that survives is the `stw 0x803B0DD0,0(r31)`. The alternative, a **pure** base destructor, costs
// 0x800487b8's exactness: mwcceppc then emits `li r4,0 ; bl __dt__24IArchitectureMessageParmFv`
// where retail has the vptr store, which is 4 bytes of `lis/addi/stw` against 2 instructions and
// makes the function 0x60 = 96 bytes instead of 0x5C. Measured both ways with mwcceppc's flags; see
// this file's entry in the report below and docs/research/boot_probe.md.
//
// **Only r31 is saved.** Because the inlined base destructor needs no call, nothing clobbers r4, so
// the deleting flag never has to be spilled and `this` gets r31 on its own. A destructor whose
// "rest of destruction" contains a call keeps the flag in r31 and `this` in r30 and therefore saves
// both - which is what retail's `~CIOWin` (0x80049E30, 104 bytes, `bl internal_dereference` for the
// `rstl::string` member) and `~CMainFlow` (0x8001DAF4, 96 bytes, `bl ~CIOWin`) do. The two shapes are
// not interchangeable, so the shape is the check that the base really is inline-empty here.
//
// The `.data` claim is 0xC, not 0x10: MWCC's vtable is two zero words of header (offset-to-top, then
// typeinfo, which is zero because the build is `-RTTI off`) and then **one slot for the
// destructor** - not the Itanium two, and no padding. Retail's own block at 0x803B1B60 reads
// 0, 0, 0x800487B8, 0 and the trailing zero at 0x803B1B6C belongs to the next symbol
// (`lbl_803B1B70`), so claiming 0x10 would be claiming a word this object does not have.
#include "MetroidPrime/CArchitectureMessageParm.hpp"

// `x4_frameCount` is a plain int with no destructor, so the body is `{}`. mwcceppc derives the
// vptr restore, the inlined base destructor and the deleting tail from the class alone; the seven
// words it emits are retail's.
CFrameMsgParm::~CFrameMsgParm() {}
