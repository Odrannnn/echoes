#ifndef _CGAMECOLLISION
#define _CGAMECOLLISION

#include "types.h"

#include "Collision/CRayCastResult.hpp"
#include "MetroidPrime/CSortedListManager.hpp"

class CMaterialFilter;
class CStateManager;

class CGameCollision {
public:
  static CRayCastResult RayStaticIntersection(const CStateManager& mgr, const CVector3f& pos,
                                              const CVector3f& dir, float length,
                                              const CMaterialFilter& filter);
  static CRayCastResult RayWorldIntersection(const CStateManager& mgr, TUniqueId& idOut,
                                             const CVector3f& pos, const CVector3f& dir,
                                             float length, const CMaterialFilter& filter,
                                             const TEntityList& nearList);
};

#endif // _CGAMECOLLISION
