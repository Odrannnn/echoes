#ifndef _CSWARMBASICS
#define _CSWARMBASICS

#include "Kyoto/Math/CVector3f.hpp"

class CStateManager;
class CHealthInfo;

// Partial ABI declaration only: CSwarmBasics derives from CActor in retail, but
// modeling that base here would emit an incomplete duplicate vtable.
class CSwarmBasics {
public:
  class CBoid;

  bool CanRenderUnsorted(const CStateManager&) const;
  CVector3f GetOrbitPosition(const CStateManager&) const;
  CHealthInfo* HealthInfo(CStateManager&);
  bool ShouldBuildAreaCollisionCacheForPartition(int) const;
  void BoidCollidedCallback(CStateManager&, CBoid&);
};

#endif
