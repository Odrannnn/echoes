#ifndef _CSTATEMANAGERCONTAINER
#define _CSTATEMANAGERCONTAINER

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

// Only the members the decompiled code touches are laid out.
class CStateManagerContainerUnk13EC0 {
public:
  bool GetUnk14_24() const { return x14_24; }

private:
  char x0_pad[0x14];
  bool x14_24 : 1;
};

class CStateManagerContainer {
public:
  typedef rstl::reserved_vector< TUniqueId, 20 > TIdList;

  ~CStateManagerContainer();

  CStateManagerContainerUnk13EC0& Unk13EC0() { return x13ec0; }
  const CStateManagerContainerUnk13EC0& GetUnk13EC0() const { return x13ec0; }
  TIdList& IdList13ED8() { return x13ed8; }
  TIdList& IdList13F04() { return x13f04; }
  TIdList& IdList13F30() { return x13f30; }
  TIdList& IdList13F5C() { return x13f5c; }
  TIdList& IdList13F88() { return x13f88; }

private:
  char x0_pad[0x13ec0];
  CStateManagerContainerUnk13EC0 x13ec0;
  TIdList x13ed8;
  TIdList x13f04;
  TIdList x13f30;
  TIdList x13f5c;
  TIdList x13f88;
};

#endif // _CSTATEMANAGERCONTAINER
