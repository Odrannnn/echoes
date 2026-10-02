#ifndef _CCAMERASHAKEMANAGER
#define _CCAMERASHAKEMANAGER

#include "MetroidPrime/TGameTypes.hpp"

class CCameraShakerData;
class CCameraShakeManager;
class CStateManager;

// Retail's unnamed fn_801E7EC0; the trailing two ints are both 0 at the one known call site.
extern "C" void fn_801E7EC0(CCameraShakeManager* self, const CCameraShakerData& data,
                            CStateManager& mgr, int arg0, int arg1);

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
