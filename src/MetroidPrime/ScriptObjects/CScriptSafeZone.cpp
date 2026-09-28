// The setter is upstream's name for DOL 0x8021BE8C (ScriptLoaderRel.cpp).
struct SSafeZone_FuncPtrs;
void SetSSafeZone_FuncPtrs(SSafeZone_FuncPtrs*);
extern "C" void fn_66_70();

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
extern "C" void RELMain() { fn_66_70(); }

extern "C" void RELExit() { SetSSafeZone_FuncPtrs(0); }
#else
extern "C" void mp_relmain_safezone() { fn_66_70(); }

extern "C" void mp_relexit_safezone() { SetSSafeZone_FuncPtrs(0); }
#endif
