/**
 * Host only: `CMemoryCardSys`'s constructor and destructor (retail 0x803096C4 / 0x80309660) and
 * its two flags, `mIsInitialized` (.sbss 0x80419B88) and `mIsCardSysExists` (0x80419B89).
 *
 * The DOL's copies are in `src/Kyoto/DolphinCMemoryCardSys.cpp`, which is the whole memory-card
 * system and is out of `files.cmake` (`tools/check_files_cmake.py` has the measurement). Delete
 * this file when that unit is listed. `CGameGlobalObjects`+0x00 is the one instance.
 */
#include "Kyoto/CMemoryCardSys.hpp"

bool CMemoryCardSys::mIsInitialized = false;
bool CMemoryCardSys::mIsCardSysExists = false;

CMemoryCardSys::CMemoryCardSys() {
  if (!mIsInitialized) {
    // Retail calls `CARDInit()` here. There is no memory card on the host: the only `CARDInit`
    // the platform's <dolphin/card.h> declares is Aurora's `CARDInit(const char* game, const char*
    // maker)`, which dereferences both, so the call is dropped and the one-shot guard still flips.
    mIsInitialized = true;
  }
  mIsCardSysExists = true;
}

CMemoryCardSys::~CMemoryCardSys() { mIsCardSysExists = false; }
