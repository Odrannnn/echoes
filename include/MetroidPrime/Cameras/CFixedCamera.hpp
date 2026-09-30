#ifndef _CFIXEDCAMERA
#define _CFIXEDCAMERA

#include "MetroidPrime/Cameras/CGameCamera.hpp"

// Guessed name. Echoes' CCameraManager owns a fixed camera at +0x38 that retail only ever touches
// through CGameCamera's virtual SetActive; there is no CFixedCamera unit in
// config/G2ME01/splits.txt, so this is the declaration needed to call it and deliberately carries no
// recovered layout. SetFixedCamera also reaches an out-of-line id setter at retail 0x80228910, which
// is not written here (see the comment above SetFixedCamera in CCameraManager.cpp).
class CFixedCamera : public CGameCamera {
};

#endif // _CFIXEDCAMERA
