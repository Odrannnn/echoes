// Ported from upstream PrimeDecomp/echoes @ d83da79: src/Kyoto/Graphics/CCubeSurface.cpp
#include "Kyoto/Graphics/CCubeSurface.hpp"

const CVector3f CCubeSurface::skDefaultNormal(1.f, 0.f, 0.f);

CAABox CCubeSurface::GetBounds() const {
  if (x0_data->x1c_extraSize != 0) {
    return x0_data->x30_bounds;
  }

  return CAABox(x0_data->x0_center, x0_data->x0_center);
}
