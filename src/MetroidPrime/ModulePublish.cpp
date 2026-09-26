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

namespace {

// The globals retail's `stw` writes. Named for what they hold rather than for
// their offsets, and deliberately file-static: see the scope note above.
STweaks_FuncPtrs* sTweaksFuncPtrs = nullptr;
SScriptForgottenObject_FuncPtrs* sForgottenObjectFuncPtrs = nullptr;

} // namespace

// `SetLoader_CannonBall` now writes the retail global, in a `Matching` unit, so the port reads
// *that* rather than a private copy - otherwise publishing a loader would have no effect on the
// consumer. The slot's type is a file-local struct in both
// `src/MetroidPrime/ScriptLoader/CannonBall.cpp` and `.../CannonBallLoaderSet.cpp`; repeating
// the two-field layout here is the same thing those two files already do, and moving it into a
// header would edit a `Matching` unit.
//
// **At file scope, and that is load-bearing.** Declared inside the anonymous namespace above,
// the `extern` picked up internal linkage, GCC synthesised the name
// `_ZN12_GLOBAL__N_1L18gLoader_CannonBallE` for it, and the port's link gained a *new*
// undefined symbol instead of losing one. An `extern` declaration of an object that is defined
// elsewhere must not be in an unnamed namespace.
struct SLoaderSlotCannonBall {
  FScriptLoader* value;
  unsigned int padding;
};

extern SLoaderSlotCannonBall gLoader_CannonBall;

// Accessors, so the port can read a published table. Retail reads the globals
// directly from the DOL; nothing needs that here.
const STweaks_FuncPtrs* GetTweaksFuncPtrs() { return sTweaksFuncPtrs; }
const FScriptLoader* GetCannonBallLoader() { return gLoader_CannonBall.value; }
const SScriptForgottenObject_FuncPtrs* GetForgottenObjectFuncPtrs() {
  return sForgottenObjectFuncPtrs;
}

void SetTweaks_FuncPtrs(STweaks_FuncPtrs* funcPtrs) { sTweaksFuncPtrs = funcPtrs; }

void SetSScriptForgottenObject_FuncPtrs(SScriptForgottenObject_FuncPtrs* funcPtrs) {
  sForgottenObjectFuncPtrs = funcPtrs;
}
