#include "MetroidPrime/CStateManager.hpp"

// Retail 0x8019E6BC .. 0x8019E714 (`fn_8019E6BC__Q213CStateManager14ScriptMsgArrayCFv`, 0x58 = 88
// bytes): pop the oldest script message off the ring. **Byte-exact, 22 instructions, no callee.**
//
// `CStateManager::ScriptMsgArray` is a fixed 192-entry **ring buffer**, not a vector, and this
// function is what says so. It reads the read cursor at +0xC04, indexes `mMessages[cursor]` with
// `slwi ...,4` (a 16-byte element), advances the cursor with the unsigned multiply-high sequence
// `lis r5,-21845` (0xAAAAAAAA) / `mulhwu` / `srwi 7` / `mulli 192` / `subf` - which is `% 192` and
// needs `uint`, not `int` - and builds the popped message **into the return slot in `r3`**
// (`sth`/`sth`/`sth`/`stw`/`stw` at `0(r3)`..`12(r3)`), so the return type is by value and a
// `const CScriptMsg&` cannot compile to it at all. The header's own comment on
// `CStateManager::fn_8003BE54`'s `CScriptMsg msg = mScriptMsgs.fn_8019E6BC();` still reads the
// same either way.
//
// The sizes behind that are measured with mwcceppc, not assumed: `sizeof(CScriptMsg)` is 0x10 with
// `m_unk`/`m_originator`/`m_id` as two-byte `TUniqueId`s at 0/2/4 and `m_msg`/`m_state` as
// four-byte enums at 8/0xC, so `mMessages[192]` is 0xC00 and the two cursors are at retail's 0xC00 and
// 0xC04.
//
CScriptMsg CStateManager::ScriptMsgArray::fn_8019E6BC() const {
  const uint idx = mReadIndex;
  mReadIndex = (idx + 1) % 192;
  return mMessages[idx];
}
