/**
 * `fn_80004A4C` - retail `.text:0x80004A4C`, `size:0x54` = 84 bytes, 0x80004A4C..0x80004AA0. The
 * next symbol, `fn_80004AA0`, is the block's own copy and starts at 0x80004AA0, so that is the
 * exact end of the range this unit claims.
 *
 *     80004a4c  stwu r1,-16(r1) / mflr r0 / stw r0,20(r1) / stw r31,12(r1) / stw r30,8(r1)
 *     80004a5c  mr   r31,r4          ; the flag, live from entry
 *     80004a64  mr.  r30,r3          ; `this`
 *     80004a68  beq  0x80004a84      ; if (this == 0) return
 *     80004a6c  lwz  r3,12(r30)      ; the data pointer
 *     80004a70  bl   0x802ce388      ; CMemory::Free
 *     80004a74  extsh. r0,r31         ; the flag, sign-extended to 16
 *     80004a78  ble  0x80004a84      ; if (flag <= 0) return
 *     80004a7c  mr   r3,r30
 *     80004a80  bl   0x802ce388      ; CMemory::Free(this)
 *     80004a84  epilogue, `mr r3,r30` ; returns `this`
 *
 * So it is a **deleting destructor** for the 16-byte `{u32, u32, u32, void*}` block that
 * `include/MetroidPrime/Player/CGameStateBlocks.hpp` calls `SGameStateBlock`:
 *
 *     if (this) { CMemory::Free(this->x0c_data); if ((short)flag > 0) CMemory::Free(this); }
 *     return this;
 *
 * `x0c_data` is the third word of `SGameStateBlock`, which is the only reading of the shape that
 * fits: 22 of retail's callers pass the address of a 16-byte stack temporary whose `+0x04` and
 * `+0x08` they have just zeroed and whose `+0x0C` they have just set to null, e.g.
 * `CGameState::CGameState()` at 0x80144AC8 (`addi r3,r29,272 ; li r4,-1 ; bl fn_80004A4C`) and
 * `ResetGameState__5CMainFv` at 0x80003BAC (`addi r3,r1,8 ; li r4,-1`). `CMemory::Free` is
 * null-safe (0x802CE3A4 is the `cmplwi r31,0` test), which is why the null `x0c_data` needs no test
 * of its own and why `fn_80004A4C` is only ten instructions.
 *
 * Three details decide the register allocation and all three are measurements.
 *
 * - **`(short)flag`**, not `flag`. Retail sign-extends the halfword itself (`extsh. r0,r31`) and
 *   tests the result, so the source compares a 16-bit value. **All 22 callers in the DOL pass
 *   -1**, so the free-through-to-self is dead for every one of them - it is not dead in the
 *   source, and the proof is the twin: `__dt__80004B9C` (0x80004B9C, 0x50) is this function
 *   instruction for instruction - same frame, same `mr. r30,r3 ; beq`, same `extsh. r0,r31 ; ble`,
 *   same `mr r3,r30 ; bl CMemory::Free` tail - and its *first* call is `fn_80004BEC` instead, which
 *   is what makes the pair a destructor family rather than a helper.
 * - **The flag stays in `r4`.** Nothing between entry and the test clobbers it - the only call is
 *   `CMemory::Free`, which takes its argument in `r3` - so `r4` never has to be spilled, and the
 *   frame saves `r30` and `r31` only. `r30` is `this` and `r31` is the flag, in that order, which
 *   is the reverse of `~CIOWin` (0x80049E30), whose "rest of destruction" contains a call and so
 *   spills both. See `src/MetroidPrime/CFrameMsgParmDtor.cpp`, which documents the pair.
 * - **`this` is tested once, not twice.** `mr. r30,r3 ; beq` at 0x80004A64 is the null test; the
 *   epilogue's `mr r3,r30` is the return, and there is no second test on the way out. So the
 *   function returns `this` and the body is guarded by one `if`.
 *
 * **Not in `files.cmake`, measured.** Listing it closes `_ZN7CMemory5FreeEPv` and `fn_80004A4C`
 * is *not* something the port asks for yet, so the net is +1 undefined symbol and 0 closed. It
 * becomes worth listing with `CGameStateCtor.cpp` and the other `CGameState*` units, which is the
 * change that gets the port past `gpGameState is null`. See `tools/check_files_cmake.py`.
 */
#include "types.h"

#include "Kyoto/Alloc/CMemory.hpp"

#include "MetroidPrime/Player/CGameStateBlocks.hpp"

// C linkage: retail's symbol table names this `fn_80004A4C`, and a C++ spelling would mangle to a
// name objdiff has nothing to pair with. The other 23 callers in the DOL - including
// `CGameStateStreamCtor.cpp` and `CGameStateCtor.cpp` - declare it with this same signature, so
// this definition is what they link against.
extern "C" SGameStateBlock* fn_80004A4C(SGameStateBlock* self, int flag) {
  if (self != nullptr) {
    CMemory::Free(self->x0c_data);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
