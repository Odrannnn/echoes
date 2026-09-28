struct SPlayerActor_FuncPtrs;
void SetLoader_PlayerActor(SPlayerActor_FuncPtrs* loader);

extern "C" void RELExit() { SetLoader_PlayerActor(0); }
