// CInGameTweakManager::ReadFromMemoryCard - retail 0x8016BDE4, 8 bytes: `li r3,0 ; blr`.
//
// Its own unit because the functions on either side of it are not written. It was the C carve
// `Player/Carve8016BDE4.c` (`fn_8016BDE4`) until the eighth upstream sync named the symbol.
#include "MetroidPrime/CInGameTweakManager.hpp"

bool CInGameTweakManager::ReadFromMemoryCard(const rstl::string& name) { return false; }
