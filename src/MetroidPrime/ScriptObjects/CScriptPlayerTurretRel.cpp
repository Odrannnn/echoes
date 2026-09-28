struct SPlayerTurret_FuncPtrs;

void SetLoader_PlayerTurret(SPlayerTurret_FuncPtrs*);

extern "C" void RELExit() { SetLoader_PlayerTurret(0); }
