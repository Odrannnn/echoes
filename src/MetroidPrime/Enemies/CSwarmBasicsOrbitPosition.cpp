#include "MetroidPrime/Enemies/CSwarmBasics.hpp"

CVector3f CSwarmBasics::GetOrbitPosition(const CStateManager&) const {
  const CVector3f* pos =
      reinterpret_cast<const CVector3f*>(reinterpret_cast<const char*>(this) + 0x194);
  return *pos;
}
