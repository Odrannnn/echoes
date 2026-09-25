struct PlayerActorFunctions;
void SetLoader_PlayerActor(PlayerActorFunctions* loader);

extern "C" void RELExit() { SetLoader_PlayerActor(0); }
