// `CGameCollision::RayWorldIntersection` - the static/dynamic combiner that
// `CStateManager::RayWorldIntersection` (src/MetroidPrime/CStateManager.cpp:539) forwards to.
// The body is the one already written in `src/MetroidPrime/CGameCollision.cpp`,
// which `configure.py` holds as a `NonMatching` unit (line 528) and `files.cmake` does not
// list, so nothing in the port compiled it.
//
// **Why a carve-out and not `CGameCollision.cpp`.**  That file is a whole `NonMatching`
// unit: the collider registry, the cached-AABB and moving/boolean collider set, the ray
// casters, the collision info list and the debug-model plumbing.  Listing it would add all
// of that and its callees, a net *rise* in the port's undefined count.
//
// **Net -1 with no new callees**: the body calls only `RayStaticIntersection` and
// `RayDynamicIntersection`, and both are already named in
// `docs/research/port_link_baseline.txt`; every `CRayCastResult` accessor it uses is
// inline in `include/Collision/CRayCastResult.hpp`.
//
// The rule the combiner implements is the one the name promises: take the dynamic hit if
// there is one and it is *at least as near* as the static hit, otherwise fall back to the
// static hit.  A tie goes to the dynamic one, which matters because `idOut` is written
// from the dynamic result - and on the static path `idOut` is reset to
// `kInvalidUniqueId`, so a caller that only reads `idOut` cannot mistake a world hit for
// an actor hit.
#include "MetroidPrime/CGameCollision.hpp"

#include "Collision/CRayCastResult.hpp"
#include "MetroidPrime/TGameTypes.hpp"

CRayCastResult
CGameCollision::RayWorldIntersection(const CStateManager& mgr, TUniqueId& idOut,
                                     const CVector3f& position, const CVector3f& direction,
                                     float length, const CMaterialFilter& filter,
                                     const rstl::reserved_vector< TUniqueId, 1024 >& nearList) {
  const CRayCastResult staticResult =
      RayStaticIntersection(mgr, position, direction, length, filter);
  const CRayCastResult dynamicResult =
      RayDynamicIntersection(mgr, idOut, position, direction, length, filter, nearList);
  if (dynamicResult.IsValid() &&
      (!staticResult.IsValid() || dynamicResult.GetTime() <= staticResult.GetTime())) {
    return dynamicResult;
  }
  idOut = kInvalidUniqueId;
  return staticResult;
}
