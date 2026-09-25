struct SafeCrystalLoaders;
extern void SetLoader_SafeZone(SafeCrystalLoaders*);
extern "C" void fn_66_70();

extern "C" void RELMain() { fn_66_70(); }

extern "C" void RELExit() { SetLoader_SafeZone(0); }
