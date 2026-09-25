#ifndef _CADDITIVEANIMPLAYBACK
#define _CADDITIVEANIMPLAYBACK

// TODO: check for Echoes

#include "types.h"

#include "rstl/rc_ptr.hpp"

class CAnimTreeNode;

class CAdditiveAnimationInfo {
private:
  float x0_fadeInDur;
  float x4_fadeOutDur;
};

class CAdditiveAnimPlayback {
public:
  enum EPlaybackPhase {
    kPP_None,
    kPP_FadingIn,
    kPP_FadingOut,
    kPP_FadedIn,
    kPP_FadedOut,
  };

private:
  CAdditiveAnimationInfo x0_info;
  rstl::ncrc_ptr< CAnimTreeNode > x8_anim;
  float x10_targetWeight;
  float x14_curWeight;
  bool x18_active;
  float x1c_weightTimer;
  EPlaybackPhase x20_phase;
  bool x24_needsFadeOut;
};
// 0x28, not 0x24: `x8_anim` is retail's 8-byte `rstl::rc_ptr` and every member after it moves up a
// word. Measured with mwcceppc's own flags into `.data` and read with `objdump -s`, not on the
// host, where `rstl` is 64-bit. See docs/research/rc_ptr.md.
CHECK_SIZEOF(CAdditiveAnimPlayback, 0x28)

#endif // _CADDITIVEANIMPLAYBACK
