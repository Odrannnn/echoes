#ifndef _CTWEAKPLAYER
#define _CTWEAKPLAYER

struct SLdrTweakPlayer;

class CTweakPlayer {
public:
  /**
   * Word 0 of retail's four-byte tweak-player cell. `Tweaks.rel`'s
   * `REL_CreateTweakGlobals` does `gpTweakPlayerA = new[4]; cell[0] =
   * &gpTweakContents->TweakPlayer;`, and all five of `CTweakPlayer`'s accessors are
   * `lwz r3,0(r3)` followed by a float load out of the `SLdrTweakPlayer` that pointer
   * names - so the receiver is the cell, not a class with members of its own. Upstream
   * has no member to put the pointer in; the port needs one, so it is added here and
   * only here, where the host build can see it and the matching GameCube build cannot.
   * See src/MetroidPrime/PortTweakGlobals.cpp and the CTweakPlayer accessor units.
   */
  SLdrTweakPlayer* mTweak;

  float GetBallRadius();
  float GetEyeOffset() const;
  float GetLeftAnalogMax();
  float GetRightAnalogMax();
  float GetVariaSuitDamageReduction();
  float GetDarkSuitDamageReduction();
  float GetLightSuitDamageReduction();
  float GetGrappleBeamSpeed() const;
  float GetGrappleBeamXWaveAmplitude() const;
  float GetGrappleBeamZWaveAmplitude() const;
  float GetGrappleBeamAnglePhaseDelta() const;
};

extern CTweakPlayer* gpTweakPlayerA;
extern CTweakPlayer* gpTweakPlayerB;

#endif // _CTWEAKPLAYER
