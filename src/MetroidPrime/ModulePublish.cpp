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
// The other thirteen already resolve, because the modules that call them are not
// compiled into the port.
//
// **Scope, stated honestly:** this file is a port-side definition, deliberately
// absent from `configure.py`, so it closes three link symbols and makes the
// publish real. It is **not** a Matching unit: each store here targets this
// file's own static, not the retail global at `_SDA_BASE_ - 27080`, so the
// displacement will not match until a unit claims those ranges with the right
// small-data layout. What is reproduced is retail's *behaviour* and its code
// shape - MWCC emits `stw r3,off(r13)` for exactly this - not its bytes.

#include "MetroidPrime/ScriptLoaderRel.hpp"

namespace {

// The globals retail's `stw` writes. Named for what they hold rather than for
// their offsets, and deliberately file-static: see the scope note above.
STweaks_FuncPtrs* sTweaksFuncPtrs = nullptr;
FScriptLoader* sCannonBallLoader = nullptr;
SScriptForgottenObject_FuncPtrs* sForgottenObjectFuncPtrs = nullptr;

} // namespace

// Accessors, so the port can read a published table. Retail reads the globals
// directly from the DOL; nothing needs that here.
const STweaks_FuncPtrs* GetTweaksFuncPtrs() { return sTweaksFuncPtrs; }
const FScriptLoader* GetCannonBallLoader() { return sCannonBallLoader; }
const SScriptForgottenObject_FuncPtrs* GetForgottenObjectFuncPtrs() {
  return sForgottenObjectFuncPtrs;
}

void SetTweaks_FuncPtrs(STweaks_FuncPtrs* funcPtrs) { sTweaksFuncPtrs = funcPtrs; }

void SetLoader_CannonBall(FScriptLoader* loader) { sCannonBallLoader = loader; }

void SetSScriptForgottenObject_FuncPtrs(SScriptForgottenObject_FuncPtrs* funcPtrs) {
  sForgottenObjectFuncPtrs = funcPtrs;
}
