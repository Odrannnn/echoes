// CScriptCoin - the multiplayer coin-race projectile (module ScriptCoin, REL id 58).
//
// Retail's own vtable for this class is `lbl_58_data_40` in the module's .data. It names
// `fn_58_1350` in the `Render` slot, and dtk named the symbol locally, so the definition below is
// what the module's vtable relocation resolves against. CPhysicsActor's declaration is in the tree
// (CHECK_SIZEOF 0x2d0) and its Render is the symbol called here.
//
// MWCC emits definitions in reverse source order, so a file's members must be listed in descending
// retail text-address order or the unit's .text will not line up with its claimed range.

// CPhysicsActor::Render(const CStateManager&), imported from the DOL.
extern "C" void Render__13CPhysicsActorCFRC13CStateManager(const void* self, const void* mgr);

extern "C" void fn_58_1350(const void* self, const void* mgr) {
  Render__13CPhysicsActorCFRC13CStateManager(self, mgr);
}
