#ifndef _CPLAYERGUN
#define _CPLAYERGUN

#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CTriggerData.hpp"
#include "MetroidPrime/EStateMsg.hpp"

#include "Kyoto/TToken.hpp"

#include "Kyoto/Audio/CSfxHandle.hpp"
#include "Kyoto/Math/CTransform4f.hpp"

#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Weapons/CGunEffectUnk.hpp"

#include "rstl/reserved_vector.hpp"
#include "rstl/string.hpp"

class CWorldTransManager;
class CStateManager;
class CPlayer;
class CWorldShadow;
class CGunWeapon;
class CGrappleArm;
class CFinalInput;
class CModelData;
class CStateMachine;

class CPlayerGun;

// The gun's state machine tables: a name from the FSM resource and the member it runs.
struct SGunTriggerFunc {
  const char* m_name;
  bool (CPlayerGun::*m_func)(CStateManager&, const CTriggerData&);
};

struct SGunStateFunc {
  const char* m_name;
  void (CPlayerGun::*m_func)(CStateManager&, EStateMsg, float);
};

// Constructed by fn_801956BC; only the virtuals the gun calls are named.
class CGunStateMachine {
public:
  virtual ~CGunStateMachine();
  virtual void vfunc0C();
  virtual void vfunc10();
  virtual void SetStateFuncs(const SGunStateFunc* funcs, int count);
  virtual void SetTriggerFuncs(const SGunTriggerFunc* funcs, int count);
  virtual void vfunc1C();
  virtual void SetState(CStateManager& mgr, CPlayerGun* owner, const rstl::string& name);
  virtual void vfunc24();
  virtual void vfunc28();
  virtual const char* GetCurrentStateName() const;

  void SetStateMachine(const CStateMachine* machine);
  bool HasState() const { return x28_state != nullptr; }

private:
  char x4_pad[0x24];
  void* x28_state;
};

// Unnamed members; the method names are the retail addresses.
class CPlayerGunUnk570 {
public:
  void fn_801D6D8C();
  void fn_801D6ED0(int, CStateManager& mgr, float, bool);
  CGunEffectUnk& Unk7C() { return x7c; }

  // The three "touch the models" paths (0x800E5C78, 0x800E5D20, 0x800E5D80) each begin by testing
  // `rlwinm. r0,r0,26,31,31` - the byte at 0x14's one-bit run - and then branch on x10 being null:
  // non-null means the beams live behind x10 and are reached through 0x800E4E50, null means they
  // are inline and are reached through 0x800E4E9C. Nothing in the tree writes either bit.
  //
  // **This is the *second* field of the run, not the sixth.** MWCC 2.7 lays a run of one-bit
  // fields out so that a store to the k-th declared field is `rlwimi rA,rS,7-k,24+k,24+k` (so the
  // k-th field is bit k) but a *test* of the k-th field is `rlwinm. rX,rS,25+k,31,31` (which reads
  // bit 6-k). Measured on a standalone struct, 2026-09-25. Retail's shift 26 is therefore field
  // index 1 by the test rule and bit 5 by the store rule; the two cannot both be right, and only
  // the test side is reproduced here, so the field is named by its declaration index.
  char x0_pad[0x10];
  void* x10;
  bool x14_0 : 1;
  bool x14_1_modelsLoaded : 1;

private:
  char x15_pad[0x67];
  CGunEffectUnk x7c;
};

class CPlayerGunUnk578 {
public:
  void fn_801D5DD0(int beamId, CStateManager& mgr);
  void fn_801D6894(CStateManager& mgr, bool);
  TUniqueId fn_801D6924() const;
  void fn_801D6930(TUniqueId);
};

class CPlayerGunUnk624 {
public:
  void fn_80320A04();
  void fn_80320978();

private:
  char m_pad[0x20];
};

class CPlayerGun : public CEntity {
public:
  enum EChargePhase {
    kCP_NotCharging,
    kCP_Phase_1,
    kCP_Phase_2,
    kCP_Phase_3,
    kCP_AnimAndSfx,
    kCP_Phase_5,
  };
  enum ESeekerChargeState {
    kSCS_NotCharging,
    kSCS_State_1,
    kSCS_State_2,
    kSCS_State_3,
    kSCS_FullyCharged,
    // Ghidra has State 9 too
  };

  class CGunMorph {
  public:
    enum EGunState {
      kGS_InWipeDone,
      kGS_OutWipeDone,
      kGS_InWipe,
      kGS_OutWipe,
    };
    enum EMorphEvent {
      kME_None,
      kME_InWipeDone,
      kME_OutWipeDone,
    };
    enum EDir {
      kD_In,
      kD_Out,
      kD_Done,
    };

    CGunMorph(float gunTransformTime, float holoHoldTime);
    EMorphEvent Update(float inY, float outY, float dt, const CPlayer& player);
    void StartWipe(EDir dir);

    float GetYLerp() const { return x0_yLerp; }
    float GetTransitionFactor() const { return x18_transitionFactor; }
    EGunState GetGunState() const { return x20_gunState; }
    void SetWeaponChanged() { x24_25_weaponChanged = true; }

  private:
    float x0_yLerp;
    float x4_gunTransformTime;
    float x8_remTime;
    float xc_speed;
    float x10_holoHoldTime;
    float x14_remHoldTime;
    float x18_transitionFactor;
    EDir x1c_dir;
    EGunState x20_gunState;
    bool x24_24_morphing : 1;
    bool x24_25_weaponChanged : 1;
  };

  CPlayerGun(TUniqueId, int);
  ~CPlayerGun();

  TUniqueId GetPlayerUniqueId() const { return m_playerUniqueId; }
  CPlayer* GetPlayer(CStateManager& mgr) const;
  CPlayer* GetPlayerFromAll(CStateManager& mgr) const;

  void PlayAnim(CStateManager&, int animType, int loop);
  void fn_801cdca0(CStateManager&, CPlayer*, bool);
  TUniqueId GetTargetId(CStateManager&);

  bool IsOutOfAmmoToShoot(CStateManager&) const;
  bool GetBeamAmmoTypeAndCosts(bool chargeCombo, CStateManager&,
                               CPlayerState::EItemType& beamAmmoTypeA,
                               CPlayerState::EItemType& beamAmmoTypeB, int& outBeamAmmoCost) const;

  void ResetCharge(CStateManager&, bool);
  void StopChargeSound(CStateManager&, bool);
  void EnableChargeFx(CStateManager&, bool);

  void UpdateNormalShotCycle(float dt, CStateManager& mgr);
  void UpdateChargeState(float dt, CStateManager& mgr);

  // State machine triggers, named by the retail name table.
  bool ShouldHolster(CStateManager&, const CTriggerData&);
  bool IsHolstered(CStateManager&, const CTriggerData&);
  bool IsNotHolstered(CStateManager&, const CTriggerData&);
  bool StartCharge(CStateManager&, const CTriggerData&);
  bool InitiateCombo(CStateManager&, const CTriggerData&);
  bool Discharge(CStateManager&, const CTriggerData&);
  bool TransitionToMorphball(CStateManager&, const CTriggerData&);
  bool TransitionToPlayer(CStateManager&, const CTriggerData&);
  bool AnimOver(CStateManager&, const CTriggerData&);
  bool ActivateMissile(CStateManager&, const CTriggerData&);
  bool CloseMissile(CStateManager&, const CTriggerData&);
  bool ChargeDone(CStateManager&, const CTriggerData&);
  bool ButtonRelease(CStateManager&, const CTriggerData&);
  bool ComboOver(CStateManager&, const CTriggerData&);
  bool InterruptEvent(CStateManager&, const CTriggerData&);
  bool GunLoaded(CStateManager&, const CTriggerData&);
  bool Scanning(CStateManager&, const CTriggerData&);
  bool InCinematic(CStateManager&, const CTriggerData&);
  bool StartFidget(CStateManager&, const CTriggerData&);
  bool FidgetOver(CStateManager&, const CTriggerData&);
  bool Grappling(CStateManager&, const CTriggerData&);
  bool IsAlive(CStateManager&, const CTriggerData&);
  bool InPhazon(CStateManager&, const CTriggerData&);

  // State machine states.
  void Start(CStateManager&, EStateMsg, float);
  void Main(CStateManager&, EStateMsg, float);
  void InMorphball(CStateManager&, EStateMsg, float);
  void Charging(CStateManager&, EStateMsg, float);
  void Recoil(CStateManager&, EStateMsg, float);
  void ComboActive(CStateManager&, EStateMsg, float);
  void Holstered(CStateManager&, EStateMsg, float);
  void Fidgeting(CStateManager&, EStateMsg, float);
  void MissileActive(CStateManager&, EStateMsg, float);
  void MissileClosing(CStateManager&, EStateMsg, float);
  void EventHandler(CStateManager&, EStateMsg, float);

  CStateMachine* GetStateMachine();
  void ResetStateMachine(CStateManager& mgr);

  CVector3f GetCurrentBeamUnkVector() const;
  int GetNumBombsAvailable(CStateManager&) const;
  void fn_801CA734(CStateManager&);
  void InitBeamData();
  CTransform4f GetLctrTransform(const CModelData& modelData, const rstl::string& name,
                                bool dynamic) const;
  void fn_801C9E9C(CStateManager& mgr);
  void fn_801C71F8(CStateManager& mgr);
  void fn_801C72B4(CStateManager& mgr, float dt);
  void fn_801CA8C8(CStateManager& mgr, bool);
  void fn_801CB344(int beamId, CStateManager& mgr);
  void fn_801CD55C(CStateManager& mgr, bool);
  void fn_801CA958(CStateManager& mgr, bool);
  void fn_801CA9A8(CStateManager& mgr, bool);
  void fn_801CDD28(CStateManager& mgr);
  bool fn_801CE0DC(CStateManager& mgr);
  void fn_801CE4FC(CStateManager& mgr);
  void fn_801CE5C0(const CFinalInput& input, CStateManager& mgr);
  void fn_801CEA58(int type, bool);
  bool fn_801CEAEC();
  void fn_801CEBB0();
  void fn_801DE430(CStateManager& mgr);
  void fn_801CE800(const CFinalInput& input, CStateManager& mgr);
  void fn_801D0CD0(CStateManager& mgr);
  void fn_801D0D10(CStateManager& mgr);

  // Virtuals after CEntity's, in retail vtable order.
  virtual void fn_801D18D0(CStateManager& mgr);
  virtual void fn_801D1558();
  virtual void RenderBeamParticles(const CStateManager& mgr);
  virtual void fn_801D0864();
  virtual void SetUnk470(float f, const CVector3f& v);
  virtual void fn_801D19CC(CStateManager& mgr);
  virtual void fn_801CF528();
  virtual float GetBeamVelocity() const;
  virtual void SetUnk578Id(TUniqueId id);
  virtual TUniqueId GetUnk578Id();
  virtual void fn_801C97DC();
  virtual void fn_801D0700();
  virtual void fn_801CE82C();
  virtual void UpdateStateMachine(CStateManager& mgr);
  virtual void InitStateMachine(CStateManager& mgr);

private:
  CTransform4f m_0x24;
  CTransform4f m_0x54;
  float m_0x84;
  float m_0x88;
  float m_0x8c;
  CWorldTransManager* m_worldTransManager;
  CActorLights m_lights;
  TUniqueId m_playerUniqueId;
  TUniqueId m_lightId;
  CWorldShadow* m_worldShadow;
  float m_cooldown;
  float m_0x384;
  float m_gunHolsterRemTime;
  uint m_0x38c;               // 0x38c
  char m_pad0[4];             // 0x390
  uint m_0x394;               // 0x394
  uint m_0x398;               // 0x398
  char m_pad0b[8];            // 0x39c
  CPlayerState::EChargeStage m_chargeState; // 0x3a4
  int m_gunHolsterState;      // 0x3a8 EGunHolsterState
  short m_0x3ac;
  bool m_isUnderwater : 1;
  bool m_0x3ae_b1 : 1;
  bool m_0x3ae_b2 : 1;
  bool m_0x3ae_b3 : 1;
  bool m_0x3ae_b4 : 1;
  bool m_0x3ae_b5 : 1;
  bool m_0x3ae_b6 : 1;
  bool m_0x3ae_b7 : 1;
  CTransform4f m_gunWorldXf;              // 0x3b0
  char m_pad1[0x90];                      // 0x3e0
  CVector3f m_0x470;                      // 0x470
  TCachedToken< CStateMachine > m_stateMachineToken; // 0x47c
  CGunStateMachine m_stateMachine;        // 0x488
  char m_pad1b[0x14];                     // 0x4b4
  CGunMorph m_morph;                      // 0x4c8
  char m_pad1g[0x70];                     // 0x4f0
  int m_0x560;                            // 0x560
  int m_0x564;                            // 0x564
  char m_pad1c[4];                        // 0x568
  bool m_0x56c_b0 : 1;                    // 0x56c
  CPlayerGunUnk570* m_0x570;              // 0x570
  CGrappleArm* m_grappleArm;              // 0x574
  CPlayerGunUnk578* m_0x578;              // 0x578
  CGunWeapon* m_powerBeam;                // 0x57c
  CGunWeapon* m_darkBeam;                 // 0x580
  CGunWeapon* m_lightBeam;                // 0x584
  CGunWeapon* m_annihilatorBeam;          // 0x588
  rstl::reserved_vector< CGunWeapon*, 4 > m_beams; // 0x58c; elements start at 0x590
  char m_pad1h[0x84];                     // 0x5a0
  CPlayerGunUnk624 m_0x624;               // 0x624
  CGunWeapon* m_currentBeam;              // 0x644
  CGunWeapon* m_outgoingBeam;             // 0x648
  CGunWeapon* m_loadingBeam;              // 0x64c
  CGunWeapon* m_nextBeam;                 // 0x650
  char m_pad1f[12];                       // 0x654
  float m_0x660[10];                      // 0x660
  float m_0x688;                          // 0x688
  float m_0x68c[2];                       // 0x68c
  EChargePhase m_chargePhase;             // 0x694
  ESeekerChargeState m_seekerChargeState; // 0x698
  float m_timerRelatedToSeekers;          // 0x69c
  int m_0x6a0;                            // 0x6a0
  char m_pad2[0xCC];                      // 0x6a4
  int m_0x770;                            // 0x770
  CPlayerState::EBeamId m_currentBeamId;  // 0x774
  CPlayerState::EBeamId m_nextBeamId;     // 0x778
  int m_0x77c;                            // 0x77c
  int m_0x780;                            // 0x780
  char m_pad3[0x10];                      // 0x784
  int m_0x794;                            // 0x794
  char m_pad3b[0x8];                      // 0x798
  CSfxHandle m_chargeSfx;                 // 0x7a0
  CSfxHandle m_sfxForShoot;               // 0x7a4
  short m_chargeRumbleHandle;             // 0x7a8
  float m_maybeChargeAnim;                // 0x7ac
  char m_pad4[0x10];                      // 0x7b0
  rstl::reserved_vector< TUniqueId, 20 > m_0x7c0; // 0x7c0
  TUniqueId m_0x7ec;                      // 0x7ec
  float m_0x7f0;                          // 0x7f0
  float m_0x7f4;                          // 0x7f4
  char m_pad4b[0x4];                      // 0x7f8
  int m_absorbedPhazonShots;              // 0x7fc
  char m_pad5[0x10];                      // 0x800
  bool m_0x810_b0 : 1;
  bool m_0x810_b1 : 1;
  bool m_0x810_b2 : 1;
  bool m_0x810_b3 : 1;
  bool m_0x810_b4 : 1;
  bool m_0x810_b5 : 1;
  bool m_0x810_b6 : 1;
  bool m_0x810_b7 : 1;
  bool m_0x810_b8 : 1;
  bool m_0x810_b9 : 1;
  bool m_0x810_b10 : 1;
  bool m_0x810_b11 : 1;
  bool m_0x810_b12 : 1;
  bool m_0x810_b13 : 1;
};
CHECK_SIZEOF(CPlayerGun, 0x814)

#endif // _CPLAYERGUN
