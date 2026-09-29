#ifndef _CCAMERASHAKEMANAGER
#define _CCAMERASHAKEMANAGER

#include "MetroidPrime/TGameTypes.hpp"

class CStateManager;

// Guessed name. Echoes moved the camera-shake list out of CCameraManager into a separate manager
// object; this is the declaration needed to call it, and deliberately carries no recovered layout.
class CCameraShakeManager {
public:
  virtual ~CCameraShakeManager();

  // Guessed names; the bodies live in the retail-derived CCameraShakeManager unit.
  void Update(float dt, CStateManager& mgr);
  CVector3f GetShakeOffset(const CStateManager& mgr) const;
};

#endif // _CCAMERASHAKEMANAGER
