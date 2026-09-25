#include "MetroidPrime/Enemies/CSwarmBasics.hpp"

CHealthInfo* CSwarmBasics::HealthInfo(CStateManager&) {
  return reinterpret_cast<CHealthInfo*>(reinterpret_cast<char*>(this) + 0x428);
}
