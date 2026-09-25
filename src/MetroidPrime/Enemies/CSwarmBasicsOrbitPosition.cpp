#include "MetroidPrime/Enemies/CSwarmBasics.hpp"

CVector3f CSwarmBasics::GetOrbitPosition(const CStateManager&) const {
  const float* pos = reinterpret_cast<const float*>(reinterpret_cast<const char*>(this) + 0x194);
  return CVector3f(pos[0], pos[1], pos[2]);
}
