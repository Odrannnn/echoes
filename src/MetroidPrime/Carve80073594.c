// Carved out of an unclaimed dtk `auto_*` range by lane `carve2`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_*.s`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x80073594..0x8007359C, 0x8 = 8 bytes, 2 functions:
//
//   AddToRenderer__15CScriptWaypointCFRC13CStateManager  0x80073598  0x4  blr
//   Render__15CScriptWaypointCFRC13CStateManager         0x80073594  0x4  blr
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.
//
// **The two `fn_<addr>` names were placeholders, and are now `symbols.txt`'s real ones**
// (renamed 2026-09-30 with `match-cscriptcamerawaypoint`, which could not link until they
// were).  `CScriptCameraWaypoint`'s vtable is `__vt__21CScriptCameraWaypoint` at
// 0x803B32E0, and retail's copy of it has 0x80073598 at slot +0x28 and 0x80073594 at
// +0x2c; the C++ `CScriptWaypoint` in `include/MetroidPrime/ScriptObjects/CScriptWaypoint.hpp`
// puts `AddToRenderer` and `Render` at exactly those slots, because it derives from
// `CActor` and nothing between them is overridden.  Both bodies are one `blr`, i.e. empty.
// So the definitions below keep the C++-mangled symbol name *verbatim*, C-style, exactly
// as this file did for the placeholder: that is why the unit is a `.c` and not a `.cpp`,
// and the same arrangement `MetroidPrime/Weapons/Carve801D6930.c` uses for
// `SetTargetId__10CAuxWeaponF9TUniqueId`.  Writing them as real out-of-line C++ methods
// would mangle them a second time and objdiff would pair nothing.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial.
//
// The directory is retail own, taken from the nearest claimed range: this address is
// 0x1DE2C bytes into `MetroidPrime/CGameAreaSetAreaAttributes.cpp`, so the code is that unit
// neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
void AddToRenderer__15CScriptWaypointCFRC13CStateManager(void) {}

void Render__15CScriptWaypointCFRC13CStateManager(void) {}
