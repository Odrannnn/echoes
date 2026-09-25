#ifndef _CSORTEDLISTMANAGER
#define _CSORTEDLISTMANAGER

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/reserved_vector.hpp"

class CAABox;
class CActor;
class CMaterialFilter;
class CVector3f;

typedef rstl::reserved_vector< TUniqueId, 1024 > TEntityList;

class CSortedListManager {
public:
  void BuildNearList(TEntityList& out, const CVector3f& pos, const CVector3f& dir, float mag,
                     const CMaterialFilter& filter, const CActor* actor) const;
  void BuildColliderList(TEntityList& out, const CActor& actor, const CAABox& aabb) const;
  void BuildNearList(TEntityList& out, const CAABox& aabb, const CMaterialFilter& filter,
                     const CActor* actor) const;
};

#endif // _CSORTEDLISTMANAGER
