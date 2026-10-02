// CIngBoostBallGuardian1464C.cpp - IngBoostBallGuardian's (module 30) `Touch` override, `.text`
// 0x1464C..0x1466C, 0x20 bytes. Vtable slot 17 of the module's second vtable (sec 5 +0xC44, the
// 36-slot one whose nearest DOL base is `CScriptDock`); `tools/rel_class_map.py` names that slot
// `Touch(CActor&, CStateManager&)` off the DOL vtable's matching slot, and the brief names it the
// same way.
//
// The body is one call to `Touch__6CActorFR6CActorR13CStateManager` with the same three arguments
// in the same registers (`this` in r3, the actor in r4, the state manager in r5 - no register
// moves before the `bl`, which is what a qualified base call produces), so this is
// `void Touch(CActor& actor, CStateManager& mgr) { CActor::Touch(actor, mgr); }`. The callee is an
// `extern "C"` declaration under the DOL's own mangled name, which is in
// `config/G2ME01/symbols.txt`; `CIngBoostBallGuardianRel.cpp` uses the same arrangement for
// `fn_8022FFC4`.
//
// **No dead-strip hazard, and that is measured**: the function is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
// `auto_04_00000000_data.o` names `fn_30_1464C` as an undefined symbol (the module's own vtable at
// `.data`+0xC44 stores it at slot 17), so dtk's object holds the reference. No `force_active:`
// entry and no `config/G2ME01/config.yml` change.
//
// This file is in `files.cmake` with an empty host branch, exactly as
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim).

extern "C" {

#ifdef __MWERKS__

void Touch__6CActorFR6CActorR13CStateManager(const void* self, void* actor, void* mgr);

// .text 0x1464C, 0x20 bytes. Vtable slot 17.
void fn_30_1464C(const void* self, void* actor, void* mgr) {
  Touch__6CActorFR6CActorR13CStateManager(self, actor, mgr);
}

#endif
}
