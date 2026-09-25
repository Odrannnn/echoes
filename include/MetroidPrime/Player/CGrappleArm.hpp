#ifndef _CGRAPPLEARM
#define _CGRAPPLEARM

#include "types.h"

#include "MetroidPrime/Weapons/CGunEffectUnk.hpp"

class CStateManager;

class CGrappleArmUnk21C {
public:
  CGunEffectUnk& Unk30() { return x30; }

private:
  char x0_pad[0x30];
  CGunEffectUnk x30;
};

class CGrappleArm {
public:
  bool IsGrappling() const { return (x298_flags >> 4) & 1; }
  CGrappleArmUnk21C* GetUnk21C() const { return x21c; }
  void fn_801C37BC(int);
  void fn_801C3824(CStateManager& mgr, float, bool);

private:
  char x0_pad[0x21c];
  CGrappleArmUnk21C* x21c;
  char x220_pad[0x78];
  uint x298_flags;
};

#endif // _CGRAPPLEARM
