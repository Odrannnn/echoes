#ifndef _CCUBESURFACE
#define _CCUBESURFACE

#include "Kyoto/Math/CAABox.hpp"

class CCubeModel;
class CCubeSurface {
  struct SSurfaceData {
    CVector3f x0_center;
    uint x0c_materialIndex;
    uint x10_displayListSizeAndNormalHint;
    CCubeModel* x14_parent;
    CCubeSurface* x18_nextSurface;
    uint x1c_extraSize;
    CVector3f x20_normal;
    uint x2c_;
    CAABox x30_bounds;
  };

  static const CVector3f skDefaultNormal;
  const SSurfaceData* x0_data;

  CAABox GetBounds() const;
private:
};
#endif // _CCUBESURFACE
