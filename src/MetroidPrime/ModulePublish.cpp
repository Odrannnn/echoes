// The module-publish thunks: how a REL module's function-pointer table reaches
// the game.
//
// Retail has sixteen of these, one per module, named Set*__F... in
// config/G2ME01/symbols.txt. Sixteen of the seventeen `Set*` symbols there are
// **eight bytes**, and every one of them is the same two instructions:
//
//   802187e4 <SetTweaks_FuncPtrs__FP16STweaks_FuncPtrs>:
//   802187e4:  stw  r3,-27080(r13)      ; the table pointer into a .sdata2 global
//   802187e8:  blr
//
// A module's init calls its own `Set*` to publish its table, and the DOL reads
// the global afterwards. That is the whole of the module system's wiring, and it
// is one store per module. `platform/compiled_modules.cpp` is the host's half of
// the same idea: it decides *when* a module's init runs, and these decide *where
// its table lands*.
//
// Three of the sixteen are undefined in the port link and are the ones the
// modules we have reimplemented call:
//
//   0x802187E4  SetTweaks_FuncPtrs                    stw r3,-27080(r13)
//   0x8021FAB4  SetLoader_CannonBall                  stw r3,-26696(r13)
//   0x8022D574  SetSScriptForgottenObject_FuncPtrs    stw r3,-26536(r13)
//
// **`SetLoader_CannonBall` is no longer one of them.** It is a `Matching` unit now -
// `src/MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp`, retail 0x8021FAB4, 8 bytes, 100% -
// so the definition here was a *duplicate definition* in the host link, which `tools/gate.sh`
// reports as `DUP SetLoader_CannonBall(...)` and refuses. It has been deleted, and
// `GetCannonBallLoader` below now reads the retail global rather than this file's own static,
// which is what makes the port's publish reach the same place the DOL's does. The other two
// still resolve to this file's statics.
//
// The other thirteen already resolve, because the modules that call them are not
// compiled into the port.
//
// **Scope, stated honestly:** the two that remain here are port-side definitions,
// deliberately absent from `configure.py`, so they close two link symbols and make the
// publish real. They are **not** Matching units: each store here targets this
// file's own static, not the retail global at `_SDA_BASE_ - 27080` / `- 26536`, so the
// displacement will not match until a unit claims those ranges with the right
// small-data layout. What is reproduced is retail's *behaviour* and its code
// shape - MWCC emits `stw r3,off(r13)` for exactly this - not its bytes.

#include "MetroidPrime/ScriptLoaderRel.hpp"

// The 2026-09-28 upstream merge briefly took `SetTweaks_FuncPtrs` and `gLoader_Tweaks` from
// upstream's `ScriptLoaderRel.cpp`. Retail's 0x802187E4 lies outside that unit's split
// (0x8021BA60..0x8021BFE0), so with the unit `Matching` the auto-split asm and our object both
// defined the setter and mwldeppc failed on a multiply-defined symbol. They live here again.
// `gLoader_CannonBall` stays the retail 8-byte slot owned by the `Matching` unit
// `ScriptLoader/CannonBall.cpp` (upstream's plain-pointer definition in ScriptLoaderRel.cpp left
// main.elf with an undefined symbol once that unit was compiled).

namespace {

SScriptForgottenObject_FuncPtrs* sForgottenObjectFuncPtrs = nullptr;

} // namespace

// At file scope, not in the unnamed namespace: Tweaks.cpp calls the setter by its external name.
STweaks_FuncPtrs* gLoader_Tweaks = nullptr;
void SetTweaks_FuncPtrs(STweaks_FuncPtrs* loaders) { gLoader_Tweaks = loaders; }
// The two-field layout is repeated from CannonBall.cpp, where it is file-local; moving it into a
// header would edit a `Matching` unit.
struct SLoaderSlotCannonBall {
  FScriptLoader* value;
  unsigned int padding;
};
extern SLoaderSlotCannonBall gLoader_CannonBall;

// Accessors, so the port can read a published table. Retail reads the globals
// directly from the DOL; nothing needs that here.
const STweaks_FuncPtrs* GetTweaksFuncPtrs() { return gLoader_Tweaks; }
const FScriptLoader* GetCannonBallLoader() { return gLoader_CannonBall.value; }
const SScriptForgottenObject_FuncPtrs* GetForgottenObjectFuncPtrs() {
  return sForgottenObjectFuncPtrs;
}

void SetSScriptForgottenObject_FuncPtrs(SScriptForgottenObject_FuncPtrs* funcPtrs) {
  sForgottenObjectFuncPtrs = funcPtrs;
}
