#include "MetroidPrime/Enemies/CSwarmBasics.hpp"

bool CSwarmBasics::ShouldBuildAreaCollisionCacheForPartition(int) const { return true; }

void CSwarmBasics::BoidCollidedCallback(CStateManager&, CBoid&) {}
