#ifndef _CGUNEFFECTUNK
#define _CGUNEFFECTUNK

#include "types.h"

// Embedded in the gun's aux weapon (+0x7c) and the grapple arm's effect object (+0x30);
// the method names are the retail addresses.
class CGunEffectUnk {
public:
  bool fn_801DCFF0();
  void fn_801DD010();
};

#endif // _CGUNEFFECTUNK
