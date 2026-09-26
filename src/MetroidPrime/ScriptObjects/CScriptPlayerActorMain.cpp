extern "C" void fn_61_70();

// Every REL module defines RELMain, which a flat host link cannot hold, so on the host this
// takes a distinct name that platform/compiled_modules.cpp calls. The MWCC branch is the
// retail source token for token, so the matching build cannot see this change.
//
// There is deliberately no exit point: retail's module has a prolog and no epilog, and the
// module's RELExit is CScriptPlayerActor.cpp's. The registry holds a null shutdown for it.
#ifdef __MWERKS__
extern "C" void RELMain() { fn_61_70(); }
#else
extern "C" void mp_relmain_playeractormain() { fn_61_70(); }
#endif
