// `CStateManager::ScriptMsgArray`'s other two methods, for the **port's** build only - this file
// is in `files.cmake` and deliberately **not** in `configure.py`.
//
// It cannot be in `configure.py`. `src/MetroidPrime/CStateManagerScriptMsgArray.cpp` claims
// `.text 0x8019E6BC..0x8019E714` for retail's `fn_8019E6BC` alone, and the two functions here sit
// in the ranges **either side** of that, which `dtk` fills with retail's own bytes as
// `auto_03_8019B988_text.o` and `auto_03_8019E714_text.o`. A `Matching` unit that also defined
// them would be multiply-defined against those, and the link fails with:
//
//   multiply-defined: 'CStateManager::ScriptMsgArray::fn_8019E69C()' in
//   CStateManagerScriptMsgArray.o
//   Previously defined in auto_03_8019B988_text.o
//
// So: written, correct, and not claimed. Both are retail's -
// `Append__Q213CStateManager14ScriptMsgArrayFRC10CScriptMsg` at 0x8019E714, 0x58 = 88 bytes, and
// `fn_8019E69C__Q213CStateManager14ScriptMsgArrayFv` at 0x8019E69C, 0x20 = 32 bytes - and both are
// two of the undefined symbols the port's link asks for on this class. Together with
// `fn_8019E6BC` this is net -3 on the port's link, and no callee is introduced.
//
// Neither is byte-exact yet, and the residuals are register allocation, not logic:
//
//   `Append`, 0x58 vs 0x58, 22 instructions each. Every value is right; the registers and the
//   schedule are not. Retail keeps `lastIndex` in `r6`, forms `r8 = lastIndex + 1` before the
//   copy, and does the multiply-high into `r7` between the field loads; MWCC emits
//   `mulhwu r0,r0,r5` and an indexed `sthx r6,r3,r8` where retail has `add r9,r3,r6` and a plain
//   `sth`. Two spellings were measured: `msgs[lastIndex] = msg` gives 22 instructions and a second
//   `lwz` of the cursor, a `CScriptMsg&` bound to the slot gives 23. The array-index form is kept.
//
//   `fn_8019E69C`, 0x20 vs 0x20, 8 instructions each. Retail computes the wrapped value as
//   `subfic r0,r0,192` then `add r3,r0,r4`, i.e. `(otherIndex - 192) + lastIndex`; MWCC
//   reassociates to `add r3,r4,r0` then `addi r3,r3,-192` and allocates `r0`/`r4` the other way
//   round. The compare is `cmplw` - unsigned - in both, which is one reason the two cursors are
//   `uint` in `include/MetroidPrime/CStateManager.hpp`.
//
// What a later lane has to do: get both exact, then give them a `configure.py` unit and their own
// `splits.txt` ranges. `tools/try_batch.py` with `VARIANTS` is the tool for the register work; the
// logic is already right, so the variant count should be small.
#include "MetroidPrime/CStateManager.hpp"

void CStateManager::ScriptMsgArray::Append(const CScriptMsg& msg) {
  const uint idx = lastIndex;
  msgs[idx] = msg;
  lastIndex = (idx + 1) % 192;
}

int CStateManager::ScriptMsgArray::fn_8019E69C() {
  if (lastIndex < otherIndex) {
    return static_cast< int >(lastIndex + otherIndex - 192);
  }
  return static_cast< int >(lastIndex - otherIndex);
}
