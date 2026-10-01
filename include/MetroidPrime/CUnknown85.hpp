#ifndef _CUNKNOWN85
#define _CUNKNOWN85

#include "MetroidPrime/CActor.hpp"

// Guessed name, already used by src/MetroidPrime/TypesMatch.cpp
// (`TYPES_MATCH_CLASS(CUnknown85, CActor)` and `CAST_TO_IMPL(CUnknown85, 85)`, retail entity type
// 85). Retail names the class nothing, so the placeholder is what this tree calls it.
//
// The declaration exists only so `CCameraManager::SetSurfaceCamera` can spell the cast retail
// makes: at 0x801AB5A0 that function calls `TCastToPtr<10CUnknown85>__FP7CEntity` (0x80098E9C).
// TypesMatch.cpp holds that template specialisation but is deliberately out of the port build, so
// PortGlobals.cpp carries the same `PORT_CAST_TO_PTR` specialisation the other five casts use. No
// recovered layout - nothing in the port reads a CUnknown85.
class CUnknown85 : public CActor {
};

#endif // _CUNKNOWN85