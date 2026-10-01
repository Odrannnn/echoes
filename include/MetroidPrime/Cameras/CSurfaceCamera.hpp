#ifndef _CSURFACECAMERA
#define _CSURFACECAMERA

#include "MetroidPrime/Cameras/CGameCamera.hpp"

// Guessed name. Echoes' CCameraManager owns a surface camera at +0x34 that retail reaches only
// through CGameCamera's virtual SetActive and one **out-of-line** script-id setter at 0x801E95A8 -
// unlike CPathCamera's and CSpindleCamera's, whose id stores are inline. There is no
// CSurfaceCamera unit in config/G2ME01/splits.txt, so this carries no recovered layout, exactly as
// CFixedCamera.hpp does; the difference is that the setter must NOT be inline here, so it is
// declared and left undefined. Declared-but-undefined is deliberate: mwcceppc turns an inline setter
// into `sth r3,X(r3)` in the caller, and retail's caller is a real `bl`.
//
// The out-of-line setter's own body belongs to a range no unit in this tree claims; the port needs
// a definition, which src/MetroidPrime/PortGlobals.cpp supplies. See the comment above
// ClearSurfaceCamera in CCameraManager.cpp.
class CSurfaceCamera : public CGameCamera {
public:
  // Retail reads this id at +0x200 (`lhz r3,512(r3)` in SetSurfaceCamera at 0x801AB57C) and clears
  // it through the out-of-line setter below, which is the same word CPathCamera's and
  // CSpindleCamera's inline setters store.
  TUniqueId GetScriptCameraId() const { return mScriptCameraId; }
  void SetScriptCameraId(TUniqueId id);

private:
  TUniqueId mScriptCameraId;
};

#endif // _CSURFACECAMERA