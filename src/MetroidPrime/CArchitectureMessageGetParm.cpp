// Retail 0x80048CEC-0x80048CF4: `CArchitectureMessage::GetParm()` and
// `CArchitectureMessage::GetParm() const`, 8 bytes each.
//
//   80048cec  80 63 00 08   lwz  r3,8(r3)      ; the rc_ptr's data pointer
//   80048cf0  4e 80 00 20   blr
//   80048ce4  80 63 00 08   lwz  r3,8(r3)      ; identical - the two overloads
//   80048ce8  4e 80 00 20   blr
//
// Both were **unnamed** in the retail DOL (`fn_80048CEC`, `fn_80048CE4`) and nothing in the port
// called them, so this file and the two renames in `config/G2ME01/symbols.txt` are what make
// `CMainFlow::OnMessage`'s `mr r3,r4 ; bl 0x80048ce4` expressible at all. Retail's is the const
// overload because `OnMessage` takes `CArchitectureMessage const&` and MWCC passes a reference as
// the bare address - so `lwz r0,4(r4)` at 0x8001df60 is reading through the const one.
//
// The order here is the reverse of the addresses on purpose: mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim, so the **non-const** one,
// which retail has at the higher address 0x80048CEC, is written first. Ascending, the two 8-byte
// bodies would be permuted - the module's bytes would come out swapped with objdiff still at 100%
// (it pairs by name). `tools/check_decl_order.py` is what says so before a flip.
//
// `inline_max_size(0)` is on the declarations in the header, not here: it is what stops mwcceppc
// folding two instructions into a caller. Without it `OnMessage` carries an expansion where retail
// has a `bl`.
//
// The body calls `rc_ptr::GetPtr()`, which is `const_cast<T*>(mPtr)` on the word at +0x08 - the
// same load, and the same 8 bytes. The lib is built `-inline deferred,noauto` (configure.py's
// `cflags_retro`), so a member function defined in a class body is **not** expanded: mwcceppc
// emitted a weak out-of-line `GetPtr__Q24rstl34rc_ptr<24IArchitectureMessageParm>CFv` and a `bl` to
// it, which is 36 bytes where retail has 8 and two names retail has no name for.
#include "MetroidPrime/CArchitectureMessage.hpp"

#pragma inline_max_size(0)
IArchitectureMessageParm* CArchitectureMessage::GetParm() {
  return mParm.GetPtr();
}

// PORT NOTE: retail's *const* `GetParm` (0x80048CE4, `GetParm__20CArchitectureMessageCFv`) is the
// one `CMainFlow::OnMessage` calls, and it used to be defined here next to the non-const overload
// with the same `inline_max_size(0)` treatment. Upstream `CArchitectureMessage.hpp` declares it
// `const` **and defines it in the class body** (`return mParm.GetPtr();`), which is a definition
// this translation unit cannot repeat, and deleting an upstream inline body is not this lane's to
// do. The overload is therefore not duplicated here; `GetParm() const` resolves to upstream's
// inline. Merge decision needed: either the header's inline body moves out of line, or
// `CMainFlow::OnMessage`'s `bl 0x80048ce4` becomes an expansion and the DOL hash moves.

