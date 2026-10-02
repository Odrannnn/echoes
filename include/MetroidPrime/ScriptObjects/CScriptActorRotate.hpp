#ifndef _CSCRIPTACTORROTATE
#define _CSCRIPTACTORROTATE

#include "Kyoto/Math/CMayaSpline.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "MetroidPrime/CEntity.hpp"
#include "rstl/pair.hpp"

class CScriptActorRotate : public CEntity {
public:
  // Guessed flag names from constructor, message and Think behavior.
  enum EFlag {
    kF_AutoStart = 1,
    kF_Loop = 2,
    kF_DurationFromSplines = 8,
    kF_AdvanceTime = 0x10,
    kF_ExternalTime = 0x20,
    // Guessed names for the two flags `UpdateActorRotations` tests. Retail tests them with
    // `rlwinm. r0,r3,0,25,25` (0x8010ABB4) and `rlwinm. r0,r3,0,29,29` (0x8010ABBC), and `rlwinm`'s
    // MB/ME fields count from the MSB, so those are bits 31-25 = 6 and 31-29 = 2 - not 25 and 29.
    // Confirmed by measurement: a mask of 0x2000000 emits `0,6,6` here, and 0x40 emits `0,25,25`.
    kF_RateScale = 0x40,   // scale the sampled angles by `dt` and compose from the entity's own transform.
    kF_FlipOrder = 0x4,    // compose base * rotation instead of rotation * base.
  };

  CScriptActorRotate(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, uint flags,
                     const SLdrSpline& xRotation, const SLdrSpline& yRotation,
                     const SLdrSpline& zRotation, const SLdrSpline& xScale,
                     const SLdrSpline& yScale, const SLdrSpline& zScale, float duration);

  // CEntity
  ~CScriptActorRotate() override;
  CEntity* TypesMatch(int typeId) const override;
  void Think(float dt, CStateManager& mgr) override;
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) override;

  void StartRotation();            // Guessed name.
  void StopRotation();             // Guessed name.
  void SetCurrentTime(float time); // Guessed name.
  void UpdateActors(bool next, CStateManager& mgr);
  void UpdateTargetRotation(CStateManager& mgr);           // Guessed name.
  void SetActorTransforms(const CTransform4f& xf);         // Guessed name.
  void UpdateActorRotations(float dt, CStateManager& mgr); // Guessed name.
  void CheckEnd(CStateManager& mgr);                       // Guessed name.

private:
  float mDuration;
  CMayaSpline mXRotation;
  CMayaSpline mYRotation;
  CMayaSpline mZRotation;
  CMayaSpline mXScale;
  CMayaSpline mYScale;
  CMayaSpline mZScale;
  uint mFlags;
  float mCurrentTime;
  CTransform4f mCurrentTransform;
  rstl::vector< rstl::pair< TUniqueId, CTransform4f > > mActors;
  TUniqueId mTargetId;
  bool mPlaying : 1;
};
CHECK_SIZEOF(CScriptActorRotate, 0x20c)

#endif // _CSCRIPTACTORROTATE
