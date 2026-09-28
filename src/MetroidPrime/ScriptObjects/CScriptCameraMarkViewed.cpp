// `CScriptCamera::MarkViewed(const CStateManager&) const`.  The body is the one already written
// in `src/MetroidPrime/ScriptObjects/CScriptCamera.cpp` (line 93), which `configure.py` holds
// as a `NonMatching` unit (line 444) and `files.cmake` does not list.
//
// **Why a carve-out and not `CScriptCamera.cpp`.**  That file is a whole `NonMatching` unit -
// the load and property cases, the spline translation, the camera-control script message
// handling and the dtor - and listing it would add all of it, a net *rise* in the port's
// undefined count.
//
// What the body does: record that this camera has been seen, in the *system* options rather
// than the save slot, as the `(world asset id, editor id)` pair the camera is identified by
// inside a world.  That is the pair the "seen cameras" wipe list is keyed on, so the cinematic
// a camera belongs to does not replay once the player has watched it.
#include "MetroidPrime/ScriptObjects/CScriptCamera.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

void CScriptCamera::MarkViewed(const CStateManager& mgr) const {
  gpGameState->SystemOptions().SetCinematicState(
      rstl::pair< CAssetId, TEditorId >(mgr.GetWorld()->GetWorldAssetId(), GetEditorId()), true);
}
