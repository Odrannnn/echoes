#ifndef _CSTATEMANAGERCONTAINER
#define _CSTATEMANAGERCONTAINER

#include "types.h"

#include "MetroidPrime/CScriptObjectLoaderHelper.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

class CStateManagerContainer {
public:
  typedef rstl::reserved_vector< TUniqueId, 20 > TIdList;

  ~CStateManagerContainer();

  CScriptObjectLoaderHelper& ScriptObjectLoaderHelper() { return x13ec0; }
  const CScriptObjectLoaderHelper& GetScriptObjectLoaderHelper() const { return x13ec0; }
  TIdList& IdList13ED8() { return x13ed8; }
  TIdList& IdList13F04() { return x13f04; }
  TIdList& IdList13F30() { return x13f30; }
  TIdList& IdList13F5C() { return x13f5c; }
  TIdList& IdList13F88() { return x13f88; }

private:
  char x0_pad[0x13ec0];
  CScriptObjectLoaderHelper x13ec0;
  TIdList x13ed8;
  TIdList x13f04;
  TIdList x13f30;
  TIdList x13f5c;
  TIdList x13f88;
};

#endif // _CSTATEMANAGERCONTAINER
