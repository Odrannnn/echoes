#ifndef _CTRIGGERDATA
#define _CTRIGGERDATA

#include "types.h"

// The argument an FSM trigger gets from its state machine resource.
class CTriggerData {
public:
  CTriggerData(float arg) : x0_arg(arg) {}
  float GetArg() const { return x0_arg; }

private:
  float x0_arg;
};

#endif // _CTRIGGERDATA
