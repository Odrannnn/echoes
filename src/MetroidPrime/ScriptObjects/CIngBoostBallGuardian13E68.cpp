// CIngBoostBallGuardian13E68.cpp - IngBoostBallGuardian's (module 30) `AddToRenderer` override,
// `.text` 0x13E68..0x13ECC, 0x64 bytes. Vtable slot 8 of the module's second vtable (sec 5 +0xC44,
// the 36-slot one whose nearest DOL base is `CScriptDock`), which `tools/rel_class_map.py` names
// `AddToRenderer(const CStateManager&) const` off the DOL vtable's matching slot.
//
// The body is `CActor::AddToRenderer(mgr)`, then the flag bit at +0x5D8 and the portal-visibility
// test, then `EnsureRendered`: the three callees are exactly the three relocations retail's bytes
// name (`AddToRenderer__6CActorCFRC13CStateManager`,
// `IsActorVisibleInPortals__13CStateManagerCFRC6CActor`, `EnsureRendered__6CActorCFRC13CStateManager`
// - all three names are in `config/G2ME01/symbols.txt`), and they are declared `extern "C"` with
// those mangled names, the arrangement `CIngBoostBallGuardianRel.cpp` uses for `fn_8022FFC4`.
//
// **The flag test is the shift spelling and that is load-bearing.** Retail holds
// `rlwinm. r0,r0,27,31,31`, which is the *materialised* one-bit read MWCC emits for
// `(b >> k) & 1` - the shape `CIngBoostBallGuardianBits.cpp` documents for its `fn_30_B7AC` - and
// not the `rlwinm. r0,r0,0,MB,ME` shape a mask inside a branch compiles to. With `k` measured at 5
// the whole function is byte-identical to retail's (checked on a scratch file against
// `build/G2ME01/IngBoostBallGuardian/obj/auto_00_00011CD8_text.o`); `(b & 0x20) != 0` written
// directly, or hoisted into a `bool`, both compile to the other shape and are four bytes off.
//
// **No dead-strip hazard, and that is measured**: the function is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
// `auto_04_00000000_data.o` names `fn_30_13E68` as an undefined symbol (the module's own vtable at
// `.data`+0xC44 stores it at +0x20, slot 8), so dtk's object holds the reference. No
// `force_active:` entry and no `config/G2ME01/config.yml` change.
//
// This file is in `files.cmake` with an empty host branch, exactly as
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim).

extern "C" {

#ifdef __MWERKS__

void AddToRenderer__6CActorCFRC13CStateManager(const void* self, const void* mgr);
bool IsActorVisibleInPortals__13CStateManagerCFRC6CActor(const void* mgr, const void* actor);
void EnsureRendered__6CActorCFRC13CStateManager(const void* self, const void* mgr);

// .text 0x13E68, 0x64 bytes. The base call, then the flag bit at +0x5D8 (shift spelling, see
// above) and the portal test, then the unsorted render. `self` is saved in a callee-saved register
// because the two calls clobber r3, which is what retail does too.
void fn_30_13E68(const void* self, const void* mgr) {
  AddToRenderer__6CActorCFRC13CStateManager(self, mgr);
  if (((static_cast< const unsigned char* >(self)[0x5D8] >> 5) & 1) != 0 &&
      IsActorVisibleInPortals__13CStateManagerCFRC6CActor(mgr, self)) {
    EnsureRendered__6CActorCFRC13CStateManager(self, mgr);
  }
}

#endif
}
