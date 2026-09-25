#ifndef _CSCRIPTWATER
#define _CSCRIPTWATER

#include "types.h"

#include "Kyoto/Math/CAABox.hpp"

// Partial ABI declarations only: in retail CScriptWater derives from CScriptTrigger, which derives
// from CActor. Modelling those bases here would emit incomplete vtables, so only the non-virtual
// call CAi::FluidFXThink makes is declared.
class CScriptTrigger {
public:
  CAABox GetTriggerBoundsWR() const; // GetTriggerBoundsWR__14CScriptTriggerCFv per Trilogy
};

class CScriptWater : public CScriptTrigger {};

#endif // _CSCRIPTWATER
