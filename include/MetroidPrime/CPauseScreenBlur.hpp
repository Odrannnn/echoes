#ifndef _CPAUSESCREENBLUR
#define _CPAUSESCREENBLUR

#include "Kyoto/TToken.hpp"
#include "MetroidPrime/Cameras/CCameraBlurPass.hpp"
#include "MetroidPrime/CInGameGuiManagerCommon.hpp"

class CTexture;

class CPauseScreenBlur {
public:
  enum EState { kS_InGame, kS_MapScreen, kS_SaveGame, kS_HUDMessage, kS_Pause };

  virtual ~CPauseScreenBlur();

  bool IsGameDraw() const { return mGameDraw; }
  bool IsNotTransitioning() const { return mPrevState == mNextState; }

private:
  TLockedToken< CTexture > mMapLightQuarter;
  EState mPrevState;
  EState mNextState;
  float mBlurAmt;
  CCameraBlurPass mCamBlur;
  bool mBlurring : 1;
  bool mGameDraw : 1;
};
CHECK_SIZEOF(CPauseScreenBlur, 0x40)

#endif // _CPAUSESCREENBLUR