// CIngBoostBallGuardian11E44.cpp - IngBoostBallGuardian's (module 30) override of
// `CPhysicsActor::GetCollisionPrimitive`, `.text` 0x11E44..0x11E4C, 8 bytes.
//
// `tools/rel_class_map.py IngBoostBallGuardian` puts the module's second vtable (sec 5 +0xC44, the
// 36-slot one whose nearest DOL base is `CScriptDock`) at slot 29 = `fn_30_11E44`, and slot 29 of
// the DOL's `__vt__13CPhysicsActor` is `GetCollisionPrimitive__13CPhysicsActorCFv` - so this is
// `const CCollisionPrimitive* GetCollisionPrimitive() const` and its body is the address of the
// class's own primitive at +0x4D0, not the base's at +0x258.
//
// This item's brief calls it `GetCollisionPrimitive__13CPhysicsActorCFv` at slot 29 of the vtable
// whose nearest DOL base is `11CScriptDock`, which is the same measurement.
//
// **The name is the module's `fn_30_11E44`, not the base's mangle, and that is deliberate.** The
// vtable at `.data`+0xC44 is dtk's data object (`auto_04_00000000_data.o`), and its slot 29
// relocation names `fn_30_11E44`; the module's `symbols.txt` names the address that way too. A
// source that declared the class and defined the member would emit
// `GetCollisionPrimitive__18CBoostBallGuardianCFv` *and* a second `__vt__18CBoostBallGuardian`
// vtable in our own object's `.data` (measured on a scratch file: `nm` shows both), which the
// split does not claim and which this unit cannot carry. So the signature is used for the shape
// and the symbol stays the module's - the same arrangement as every other unit in this module.
//
// **No dead-strip hazard, and that is measured**: the function is not in
// `build/G2ME01/IngBoostBallGuardian/ldscript.lcf`'s FORCEACTIVE list, but
// `auto_04_00000000_data.o` holds it as an undefined symbol, so dtk's own object keeps the
// reference and the `.text` survives the link. No `force_active:` entry and no
// `config/G2ME01/config.yml` change.
//
// This file is in `files.cmake` with an empty host branch, exactly as
// `CIngBoostBallGuardianBits.cpp` explains.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim).

extern "C" {

#ifdef __MWERKS__

// .text 0x11E44, 0x8 bytes. `addi r3,r3,0x4D0` / `blr`: the member at +0x4D0, returned as a
// pointer. Vtable slot 29 of `.data`+0xC44.
const void* fn_30_11E44(const void* self) {
  return static_cast< const char* >(self) + 0x4D0;
}

#endif
}
