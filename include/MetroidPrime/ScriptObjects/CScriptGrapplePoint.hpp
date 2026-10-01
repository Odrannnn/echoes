#ifndef _CSCRIPTGRAPPLEPOINT
#define _CSCRIPTGRAPPLEPOINT

#include "MetroidPrime/CActor.hpp"

// Partial declaration: retail's CScriptGrapplePoint derives from CActor and carries no members of
// its own that anything in this repo reaches (tools/dis.sh 0x8009D580 is its destructor, and
// src/MetroidPrime/TypesMatch.cpp's TYPES_MATCH_CLASS entry declares the same base). Nothing here
// declares a key function, so including this emits no vtable.
class CScriptGrapplePoint : public CActor {};

#endif // _CSCRIPTGRAPPLEPOINT