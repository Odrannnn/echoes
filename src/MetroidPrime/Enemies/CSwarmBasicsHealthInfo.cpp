#include "MetroidPrime/Enemies/CSwarmBasics.hpp"

CHealthInfo* CSwarmBasics::HealthInfo() {
  return reinterpret_cast<CHealthInfo*>(reinterpret_cast<char*>(this) + 0x428);
}
