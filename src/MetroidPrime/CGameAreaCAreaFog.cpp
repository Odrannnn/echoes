// `CGameArea::CAreaFog`'s constructor and its one predicate.  Both bodies are the ones already
// written in `src/MetroidPrime/CGameArea.cpp` (lines 548 and 559), which `configure.py` holds as
// a `NonMatching` unit (line 427) and `files.cmake` does not list.
//
// **Why a carve-out and not `CGameArea.cpp`.**  That file is a whole `NonMatching` unit - the
// area's layer tables, the dynamic-layer loop, the fog and lighting updaters, the static
// geometry build - and listing it would add every one of those bodies and their callees, which
// is a net *rise* in the port's undefined count.  One function per file is the arrangement
// `CGameAreaSetAreaAttributes.cpp` already uses for the same class.
//
// **Net -2 with no new callees**: the constructor only stores retail's own starting fog state
// (no fog, range 0..1024, half-grey, zero rate of change) and `IsFogDisabled` compares one
// enum.  Every type it names - `CVector2f`, `CVector3f` - is constructed inline.
//
// The starting state is retail's, not a guess: `mFogMode = kRFM_None` is what
// `CAreaFog`'s own zero-initialised `.data` image holds before an area's property load runs, and
// `kRFM_None` is also the value `DisableFog()` writes, so a fog block that never appears in a
// level's properties leaves the area unfogged rather than inheriting the previous area's fog.
#include "MetroidPrime/CGameArea.hpp"

CGameArea::CAreaFog::CAreaFog()
: mFogMode(kRFM_None)
, mRangeCur(0.f, 1024.f)
, mRangeTarget(mRangeCur)
, mRangeDelta(0.f, 0.f)
, mColorCur(0.5f, 0.5f, 0.5f)
, mColorTarget(mColorCur)
, mColorDelta(0.f) {}

bool CGameArea::CAreaFog::IsFogDisabled() const { return mFogMode == kRFM_None; }
