// `CRumbleManager::StopRumble(short)`.  The body is the one already written in
// `src/MetroidPrime/CRumbleManager.cpp` (line 41), which `configure.py` holds as a
// `MatchingFor("G2ME01")` unit (line 502) and `files.cmake` does not list.
//
// **It is its own file for two reasons.**  `CRumbleManager.cpp` is a `MatchingFor` unit, so it
// must not be edited: a definition added to it would put a symbol in an object dtk links for
// the matching build, and that is the arrangement `src/MetroidPrime/PortGlobals.cpp`'s header
// warns about.  A file `configure.py` never claims is invisible to the matching build, so the
// DOL is byte-identical with or without it.  And listing the whole unit in `files.cmake` would
// bring `Rumble`'s two overloads, the positional overload's `CPlayer::GetTranslation`, the
// `skRumbleFxTable` global and `CGameState`/`CGameOptions` reach with it - a net *rise*.
//
// **Net -1 with no new callees**: `CRumbleGenerator::Stop` is retail's
// `src/Kyoto/Input/CRumbleGenerator.cpp`, which `files.cmake` does list.
//
// The `-1` is retail's sentinel, not an index: `CPlayerGun` and the AI both hold a rumble id
// and pass it back to cancel, and the id they get back from `Rumble()` when the effect is
// refused is `-1`.  Stopping id `-1` would index past the end of the generator's effect table,
// so the early return is what makes "the rumble never started" a safe thing to cancel.
#include "MetroidPrime/CRumbleManager.hpp"

void CRumbleManager::StopRumble(short id) {
  if (id == -1) {
    return;
  }
  mRumbleGenerator.Stop(id);
}
