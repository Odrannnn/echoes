/**
 * `CGameGlobalObjects::CGameGlobalObjects(COsContext&, CMemorySys&)` - retail
 * `__ct__18CGameGlobalObjectsFR10COsContextR10CMemorySys`, `.text:0x8000848C`, `size:0xE4`.
 *
 * Host only: the DOL's copy is in `src/MetroidPrime/main.cpp`, which `files.cmake` cannot list
 * (`tools/check_files_cmake.py`), so this file carries upstream's spelling of the same body.
 * **It is the only writer of `gpGameState`**; `CGameArchitectureSupport`'s constructor reads it
 * with no null test. Retail never reads its two parameters.
 */

#include "MetroidPrime/CGameGlobalObjects.hpp"

#include "MetroidPrime/CInGameTweakManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

extern CCharacterFactoryBuilder* gpCharacterFactoryBuilder;

CGameGlobalObjects::CGameGlobalObjects(COsContext& context, CMemorySys& memorySys)
: mSimplePool(mResFactory)
, mGameState(rs_new CGameState())
, mInGameTweakManager(rs_new CInGameTweakManager()) {
  gpResourceFactory = &mResFactory;
  gpSimplePool = &mSimplePool;
  gpCharacterFactoryBuilder = &mCharacterFactoryBuilder;
  gpGameState = mGameState.get();
  gpTweakManager = mInGameTweakManager.get();
  gpRelFileManager = &mRelFileManager;
}
