#ifndef _CDEPENDENCYGROUPTOKEN
#define _CDEPENDENCYGROUPTOKEN

#include "Kyoto/TToken.hpp"
#include "rstl/vector.hpp"

class CDependencyGroup;
class IObjectStore;

class CDependencyGroupToken {
public:
  CDependencyGroupToken(const TToken< CDependencyGroup >& group, IObjectStore& store);

  void Lock();
  void Unlock();
  bool IsLocked() const;
  bool IsLoaded();

private:
  TToken< CDependencyGroup > x0_group;
  rstl::vector< CToken > x8_dependencies;
  uint x18_lockCount;
  bool x1c_loaded : 1;
};
CHECK_SIZEOF(CDependencyGroupToken, 0x20)

#endif // _CDEPENDENCYGROUPTOKEN
