struct SafeCrystalLoaders;
extern void SetLoader_SafeZone(SafeCrystalLoaders*);
extern "C" void fn_66_70();

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names that platform/compiled_modules.cpp calls. The MWCC branch
// is the retail source token for token, so the matching build cannot see this change.
#ifdef __MWERKS__
extern "C" void RELMain() { fn_66_70(); }

extern "C" void RELExit() { SetLoader_SafeZone(0); }
#else
extern "C" void mp_relmain_safezone() { fn_66_70(); }

extern "C" void mp_relexit_safezone() { SetLoader_SafeZone(0); }
#endif
