#include "Kyoto/Math/CVector3f.hpp"

class CFlyerSwarm {
public:
  CVector3f GetBoidPosition(int index) const;
};

CVector3f CFlyerSwarm::GetBoidPosition(int index) const {
  const u8* self = reinterpret_cast<const u8*>(this);
  const u8* boids = *reinterpret_cast<const u8* const*>(self + 0x184);
  const float* transform = reinterpret_cast<const float*>(boids + index * 0xB8);
  return CVector3f(transform[3], transform[7], transform[11]);
}
