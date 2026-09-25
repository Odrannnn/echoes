#ifndef _CSWARMBASICS
#define _CSWARMBASICS

#include "Kyoto/Math/CVector3f.hpp"

class CStateManager;

// Partial ABI declaration for the independently matched CSwarmBasics methods.
// The actor base and the rest of the swarm layout are not modeled yet.
class CSwarmBasics {
public:
  class CBoid;

  bool CanRenderUnsorted(const CStateManager&) const;
  CVector3f GetOrbitPosition(const CStateManager&) const;
  bool ShouldBuildAreaCollisionCacheForPartition(int) const;
  void BoidCollidedCallback(CStateManager&, CBoid&);
};

#endif
