#ifndef _CBSLOCOMOTION
#define _CBSLOCOMOTION

#include "Kyoto/Math/CVector3f.hpp"
#include "MetroidPrime/BodyState/CBodyState.hpp"
#include "rstl/construct.hpp"
#include "rstl/pair.hpp"
#include "rstl/reserved_vector.hpp"

//! `fn_800F4FB4` - retail 0x800F4FB4, the out-of-line 0x40-byte chunk of a locomotion-table row
//! copy. Declared here, at global scope, because the `rstl::construct_impl` specialisation
//! below is its only caller and that is a namespace-scope template specialisation.
extern "C" void fn_800F4FB4(void* self, const void* src);

// The locomotion tables hold nothing that needs destroying: the outer vector's elements are
// reserved_vectors of a POD pair, and retail's 124-byte locomotion destructors inline both
// teardowns away. These must precede every use of reserved_vector's own members - mwceppc 2.7
// rejects an explicit specialisation of is_trivially_destructible that arrives after the
// primary template has already been instantiated for it ("tag ... redefined").
namespace rstl {
template <>
struct is_trivially_destructible< reserved_vector< pair< int, float >, 8 > > {
  enum { value = true };
};
template <>
struct is_trivially_destructible<
    reserved_vector< reserved_vector< pair< int, float >, 8 >, 15 > > {
  enum { value = true };
};

//! A row of `CBSBiPedLocomotion::mAnims` is copied as a flat 0x44-byte block, not a memberwise
//! loop, and this is the specialisation of `rstl::construct_impl` - the copy helper
//! `reserved_vector` itself uses - that the measured bytes call for. 0x44 is 8k + 4, so the
//! copy splits at the largest 8-byte boundary: the first 0x40 goes out of line (retail
//! 0x800F4FB4, `fn_800F4FB4`, defined in this unit's .cpp) and the trailing word stays inline.
//! Retail's constructor shows both halves at 0x800F444C..0x800F4460, the `lwz`/`stw` pair right
//! after the call, and loops it 15 times.
//!
//! The trailing word is part of the same block copy, so it moves as bits: retail's `lwz`/`stw`,
//! where a `float` assignment would be `lfs`/`stfs`. `mData[60]` is `mAnims[7].second`.
template <>
inline void construct_impl( void* dest, const reserved_vector< pair< int, float >, 8 >& src ) {
  reserved_vector< pair< int, float >, 8 >* self =
      static_cast< reserved_vector< pair< int, float >, 8 >* >(dest);
  fn_800F4FB4(self, &src);
  *reinterpret_cast< uint* >(&self->mData[60]) = *reinterpret_cast< const uint* >(&src.mData[60]);
}
} // namespace rstl

class CActor;

class CBSLocomotion : public CBodyState {
public:
  CBSLocomotion();

  // CBodyState
  ~CBSLocomotion() override {}
  bool IsMoving() const override = 0;
  bool CanShoot() const override;
  void Start(CBodyController& bc, CStateManager& mgr) override;
  pas::EAnimationState UpdateBody(float dt, CBodyController& bc, CStateManager& mgr) override;
  void Shutdown(CBodyController& bc) override;

  virtual bool IsPitchable() const;
  virtual float GetLocomotionSpeed(pas::ELocomotionType type, pas::ELocomotionAnim anim) const = 0;
  virtual float ApplyLocomotionPhysics(float dt, CBodyController& bc);
  virtual float UpdateLocomotionAnimation(float dt, float velMag, CBodyController& bc,
                                          bool init) = 0;
  virtual pas::EAnimationState GetBodyStateTransition(float dt, CBodyController& bc);
  virtual void ReStartBodyState(CBodyController& bc, bool maintainVel);

protected:
  float GetStartVelocityMagnitude(CBodyController& bc) const;
  float ComputeWeightPercentage(const rstl::pair< int, float >& a,
                                const rstl::pair< int, float >& b, float velocity) const;

  pas::ELocomotionType mLocomotionType;
};
CHECK_SIZEOF(CBSLocomotion, 0x8)

class CBSBiPedLocomotion : public CBSLocomotion {
public:
  explicit CBSBiPedLocomotion(CActor& actor);

  // CBodyState
  ~CBSBiPedLocomotion() override {}
  bool IsMoving() const override;
  void Start(CBodyController& bc, CStateManager& mgr) override;
  pas::EAnimationState UpdateBody(float dt, CBodyController& bc, CStateManager& mgr) override;

  // CBSLocomotion
  float GetLocomotionSpeed(pas::ELocomotionType type, pas::ELocomotionAnim anim) const override;
  float UpdateLocomotionAnimation(float dt, float velMag, CBodyController& bc, bool init) override;

  virtual bool IsStrafing(CBodyController& bc) const;

protected:
  float UpdateRun(float velocity, CBodyController& bc, pas::ELocomotionAnim anim);
  float UpdateWalk(float velocity, CBodyController& bc, pas::ELocomotionAnim anim);
  float UpdateStrafe(float velocity, CBodyController& bc, pas::ELocomotionAnim anim);
  const rstl::pair< int, float >& GetLocoAnimation(pas::ELocomotionType type,
                                                   pas::ELocomotionAnim anim) const;

  static const float skMinWalkPercent;

  rstl::reserved_vector< rstl::reserved_vector< rstl::pair< int, float >, 8 >, 15 > mAnims;
  pas::ELocomotionAnim mAnim;
  float mPrimeTime;
};
CHECK_SIZEOF(CBSBiPedLocomotion, 0x410)

class CBSRestrictedLocomotion : public CBSLocomotion {
public:
  explicit CBSRestrictedLocomotion(CActor& actor);

  // CBodyState
  ~CBSRestrictedLocomotion() override {}
  bool IsMoving() const override;

  // CBSLocomotion
  float GetLocomotionSpeed(pas::ELocomotionType type, pas::ELocomotionAnim anim) const override;
  float UpdateLocomotionAnimation(float dt, float velMag, CBodyController& bc, bool init) override;

private:
  rstl::reserved_vector< int, 15 > mAnims;
  pas::ELocomotionAnim mAnim;
};
CHECK_SIZEOF(CBSRestrictedLocomotion, 0x4c)

class CBSFlyerLocomotion : public CBSBiPedLocomotion {
public:
  CBSFlyerLocomotion(CActor& actor, bool pitchable);

  // CBodyState
  ~CBSFlyerLocomotion() override {}

  // CBSLocomotion
  bool IsPitchable() const override;
  float ApplyLocomotionPhysics(float dt, CBodyController& bc) override;

private:
  bool mPitchable;
};
CHECK_SIZEOF(CBSFlyerLocomotion, 0x414)

class CBSWallWalkerLocomotion : public CBSBiPedLocomotion {
public:
  explicit CBSWallWalkerLocomotion(CActor& actor);

  // CBodyState
  ~CBSWallWalkerLocomotion() override {}

  // CBSLocomotion
  float ApplyLocomotionPhysics(float dt, CBodyController& bc) override;
};
CHECK_SIZEOF(CBSWallWalkerLocomotion, 0x410)

class CBSAiMovedFlyerLocomotion : public CBSBiPedLocomotion {
public:
  explicit CBSAiMovedFlyerLocomotion(CActor& actor);

  // CBodyState
  ~CBSAiMovedFlyerLocomotion() override {}

  // CBSLocomotion
  float ApplyLocomotionPhysics(float dt, CBodyController& bc) override;
  float UpdateLocomotionAnimation(float dt, float velMag, CBodyController& bc, bool init) override;
};
CHECK_SIZEOF(CBSAiMovedFlyerLocomotion, 0x410)

class CBSFloaterLocomotion : public CBSRestrictedLocomotion {
public:
  explicit CBSFloaterLocomotion(CActor& actor);

  // CBodyState
  ~CBSFloaterLocomotion() override {}

  // CBSLocomotion
  float ApplyLocomotionPhysics(float dt, CBodyController& bc) override;
};
CHECK_SIZEOF(CBSFloaterLocomotion, 0x4c)

// Guessed name: Echoes-specific directional animation-blending locomotion.
class CBSBlendedLocomotion : public CBSBiPedLocomotion {
public:
  CBSBlendedLocomotion(CActor& actor, float turnSpeed);

  // CBodyState
  ~CBSBlendedLocomotion() override {}

  // CBSLocomotion
  float ApplyLocomotionPhysics(float dt, CBodyController& bc) override;
  float UpdateLocomotionAnimation(float dt, float velMag, CBodyController& bc, bool init) override;
  void ReStartBodyState(CBodyController& bc, bool maintainVel) override;

private:
  CVector3f mDirection;
  float mTurnSpeed;
  float mTimeMoving;
};
CHECK_SIZEOF(CBSBlendedLocomotion, 0x424)

#endif // _CBSLOCOMOTION
