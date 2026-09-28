// `CGameArea::HasPendingLayerLoads() const`.  Retail's symbol table names no function here -
// the name is a guess from the caller `CStateManager::fn_80036284`
// (src/MetroidPrime/CStateManager.cpp:631), which is the one place the game asks the
// question.  The body is the one already written in `src/MetroidPrime/CGameArea.cpp`, which
// `configure.py` holds as a `NonMatching` unit (line 427) and `files.cmake` does not list, so
// nothing in the port compiled it.
//
// **Why a carve-out and not `CGameArea.cpp`.**  That file is a whole `NonMatching` unit:
// `CGameArea` is one of the largest classes in the game, and listing it would pull in every
// other body it holds - `UpdateDynamicLayers`, `UpdateLayerLoading`,
// `DecompressAreaData`, `ClearDecompressionRequest`, the fog and lighting updaters and
// their own callees - which is a net *rise* in the port's undefined count, not a fall.
// One function per file is the arrangement `CGameAreaSetAreaAttributes.cpp` already uses
// for the same class.
//
// The body calls nothing, so it is net -1 on the port's link with no new callees at all.
//
// `mLayerPhases` is the per-layer phase array the header declares; the loop's bound is its
// own extent and the three phases that mean "not loading" are `kLP_Inactive`, `kLP_Ready`
// and `kLP_Active` - anything else (`kLP_Unloading`, `kLP_Loading`, and retail's remaining
// values) is a load in flight, which is the question the caller
// (`CStateManager::fn_80037xxx` at src/MetroidPrime/CStateManager.cpp:633) asks.
#include "MetroidPrime/CGameArea.hpp"

bool CGameArea::HasPendingLayerLoads() const {
  if (!mPostConstructed->mDecompressionRequests.empty()) {
    return true;
  }
  for (int i = 0; i < mPostConstructed->mLayerTokens.size(); ++i) {
    const ELayerPhase phase = mLayerPhases[i];
    if (phase != kLP_Inactive && phase != kLP_Ready && phase != kLP_Active) {
      return true;
    }
  }
  return false;
}
