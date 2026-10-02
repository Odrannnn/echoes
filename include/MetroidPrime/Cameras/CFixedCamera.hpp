#ifndef _CFIXEDCAMERA
#define _CFIXEDCAMERA

#include "MetroidPrime/Cameras/CGameCamera.hpp"

// Guessed name. Echoes' CCameraManager owns a fixed camera at +0x38 that retail only ever touches
// through CGameCamera's virtual SetActive; there is no CFixedCamera unit in
// config/G2ME01/splits.txt, so this is the declaration needed to call it and deliberately carries no
// recovered layout. SetFixedCamera also reaches an out-of-line id setter at retail 0x80228910, which
// is not written here (see the comment above SetFixedCamera in CCameraManager.cpp).
class CFixedCamera : public CGameCamera {
public:
  // Retail reads this id at +0x20C (`lhz r4,524(r3)` in SetFixedCamera at 0x801AB6B4). The setter
  // is declared and not defined here on purpose, like CSurfaceCamera's: retail's caller is a real
  // `bl` to 0x80228910, which an inline setter would turn into a store in the caller. The port's
  // definition is in src/MetroidPrime/PortGlobals.cpp.
  TUniqueId GetScriptCameraId() const { return mScriptCameraId; }
  void SetScriptCameraId(TUniqueId id);

private:
  uchar mUnknown200[0xC]; // +0x200..0x20C, not read by anything recovered
  TUniqueId mScriptCameraId;
};

#endif // _CFIXEDCAMERA
