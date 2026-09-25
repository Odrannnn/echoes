struct PlayerTurretFunctions;

void SetLoader_PlayerTurret(PlayerTurretFunctions*);

extern "C" void RELExit() { SetLoader_PlayerTurret(0); }
