#include "MetroidPrime/Player/CMorphBall.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Particles/CDeferredParticleEffect.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleSwoosh.hpp"
#include "MetroidPrime/CActorLights.hpp"
#include "MetroidPrime/CAnimRes.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CRainSplashGenerator.hpp"
#include "MetroidPrime/CWorldShadow.hpp"
#include "MetroidPrime/Player/CMorphBallShadow.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakBall.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

#include "rstl/math.hpp"

// Structure-first reconstruction. TODO bodies below are scaffolds, not equivalent implementations.

// Guessed names for TU-local state.
static float sBallCloseToCollisionDistance;
static rstl::reserved_vector< int, 64 > sWakeEffectForMaterial;

// The pair at retail 0x800D0490..0x800D0584 is `rstl::vector< TUniqueId, float >::reserve(int)`
// and the `rstl::uninitialized_copy` helper it calls, written out under `extern "C"` names for the
// same reason `rstl::reserved_vector`'s `operator=` is (see `include/rstl/reserved_vector.hpp`:
// retail's object emits a template instantiation under an unmangled name, so objdiff never pairs
// it with the mangled symbol the compiler writes for us). Retail's pair is byte-identical to the
// weak `reserve__Q24rstl62vector<Q24rstl18pair<9TUniqueId,f>,...>Fi` and the local
// `uninitialized_copy<...pointer_iterator<pair<TUniqueId, float>>...>` this file already emits -
// verified instruction by instruction, 46/46 with only branch-target addresses differing, and
// 15/15 for the helper. The helper's `lhz`/`sth` at +0 and `lfs`/`stfs` at +4 are `TUniqueId`'s
// 2-byte value and the float.
//
// **Both bodies are written out rather than delegated to the template, and the helper's loop is
// written in `pointer_iterator` terms.** `rstl::vector::reserve` and
// `rstl::uninitialized_copy` are both out-of-line under this unit's rule flags
// (`-inline deferred,noauto`), so calling either emits a forwarder - measured 17.09% for the
// wrapper and a 12-instruction stub for the helper. And `fn_800D0548`'s loop is
// `uninitialized_copy`'s own `It`/`It` form, not its `S*`/`S*` overload: retail opens
// `lwz r6,0(r3)` / `lwz r0,0(r4)`, i.e. it reads each argument's `current` field, and the same
// loop written over raw pointers parks the bound in `r3` instead of `r0` (98.67%, that one
// instruction).
typedef rstl::vector< rstl::pair< TUniqueId, float > > SUniqueIdFloats;

extern "C" SUniqueIdFloats::value_type* fn_800D0548(SUniqueIdFloats::iterator begin,
                                                    SUniqueIdFloats::iterator end,
                                                    SUniqueIdFloats::value_type* out) {
  SUniqueIdFloats::value_type* tmp = out;
  SUniqueIdFloats::iterator cur = begin;
  for (; cur != end; ++cur, ++tmp) {
    rstl::construct(tmp, *cur);
  }
  return tmp;
}

// Retail 0x800D0490, 0xB8 = 46 insns: `reserve` growing to `slwi r3,size,3` bytes, copying
// `[begin(), end())` through `fn_800D0548` into it with the two iterators built on the stack
// (`stw r6,12(r1)` .. `stw r0,20(r1)`), freeing the old buffer, then storing the new pointer at
// +12 and the new capacity at +8. The pointer walk between the copy and the `Free` is
// `destroy(mItems, mItems + mCount)` over a `pair<TUniqueId, float>` that is not registered as
// trivially destructible, which is what keeps that loop in retail's object.
extern "C" void fn_800D0490(SUniqueIdFloats* self, int size) {
  if (size <= self->mCapacity) {
    return;
  }
  SUniqueIdFloats::value_type* newData;
  self->mAllocator.allocate(newData, size);
  fn_800D0548(self->begin(), self->end(), newData);
  for (SUniqueIdFloats::iterator it = self->begin(); it != self->end(); ++it) {
    rstl::destroy(&*it);
  }
  self->mAllocator.deallocate(self->mItems);
  self->mItems = newData;
  self->mCapacity = size;
}

// `fn_800D042C` (0x800D042C, 0x64 = 25 insns) is retail's out-of-line copy of one
// `CCollisionInfo`, and this unit's split is what claims 0x800D042C for it - so it is written
// out here, not in the `Collision/` unit that first needed it. See
// `include/Collision/CCollisionInfo.hpp` for why it lives outside `src/Collision/CCollisionInfo.cpp`.
//
// **Retail's copy is a flat 96-byte block move, and the memberwise spelling does not produce
// it.** Measured, not assumed: retail is 12 `lfd`/`stfd` pairs over `0x60` bytes with no
// register reuse across pairs, while `*self = other` on this layout lowers to 24 `lwz`/`stw`
// moves plus `lhz`/`lbz` for the `TUniqueId` and the two bit-fields (0xB0 bytes, and
// `CCollisionInfo`'s own copy constructor is the same shape with `lfs`/`stfs` for the six
// `CVector3f`s). The 8-byte granularity is the whole difference, and it is what MWCC emits when
// the copy is written through a **`double`-typed** 8-byte view: a `double[12]` of this size
// copies as exactly these 12 instructions, and so do a `double[2]` x 6 and 12 `double` members
// (measured on scratch files compiled with this unit's own rule flags from `build.ninja`).
// The element type has to be the 8-byte *floating* one - a `u64[12]` of the same size is also
// 0x64 bytes but comes out as `lwz` pairs - and `lfd`/`stfd` are the only load/store in retail's
// 0x64 bytes, with no `lfdu`/`stfdu`, so retail's source loaded through a `double` lvalue too.
//
// So the copy is written as the block move it is, through a 12-`double` view of the object
// rather than through the members. This is retail's own lowering, not a shortcut around the
// work: it copies all 96 bytes, `CHECK_SIZEOF(CCollisionInfo, 0x60)` in the header holds the
// view's size to the object's, and the call sites are `bl`s to an undefined symbol in retail
// too (`Collision/CCollidableSphere.o` lists `U fn_800D042C` in the retail object dump), so
// nothing but this function's own bytes depends on the spelling.
union SCCollisionInfoBlock {
  CCollisionInfo mInfo;
  double mQuads[sizeof(CCollisionInfo) / sizeof(double)];
};

// Retail 0x800D042C, 0x64 = 25 insns, 100.00% - byte-identical to retail's 0x800D042C..0x800D0490.
extern "C" void fn_800D042C(CCollisionInfo* self, const CCollisionInfo& other) {
  SCCollisionInfoBlock* dst = reinterpret_cast< SCCollisionInfoBlock* >(self);
  const SCCollisionInfoBlock* src = reinterpret_cast< const SCCollisionInfoBlock* >(&other);
  for (int i = 0; i < sizeof(CCollisionInfo) / sizeof(double); ++i) {
    dst->mQuads[i] = src->mQuads[i];
  }
}

// The four pairs at retail 0x800D0024..0x800D0438 are `rstl::reserved_vector<T, N>`'s
// `resize(count, value)` and a "fill all N from empty" wrapper around it, one pair per
// instantiation, emitted out of line into this TU. The element stride is the measurement that
// separates the four: `slwi ...,2` for `fn_800D0170` (4-byte `float`), `mulli ...,12` for
// `fn_800D0064` and `fn_800D022C` (12-byte `CVector3f`), `slwi ...,4` for `fn_800D0338`
// (16-byte), and the wrapper's literal is the instantiation's `N` (`li r4,15` at 0x800D0024 and
// 0x800D0130, `li r4,5` at 0x800D01EC and 0x800D02F8). Retail's object does not resolve these
// names, so the `extern "C"` names below are the placeholders.
//
// **`resize` is spelled out here rather than called, and the fill is
// `rstl::uninitialized_fill_n` rather than a memberwise loop** - both measured on
// `fn_800D0170`. Calling `self->resize(n, *value)` emits a weak outlined
// `resize__Q24rstl21reserved_vector<f,15>FiRCf` and leaves the `extern "C"` function as a
// 0x20-byte forwarder, which objdiff cannot pair with retail at all (`---`). And a hand-written
// `self->mBuffer[i] = *value` over a `struct { int mCount; float mBuffer[15]; }` scored 81.77%:
// it folds the `+4` of `&mBuffer[i]` into the store offset and emits `stfsu`, where retail's
// fill base is `slwi r0,count,2` / `add r5,r3,r0` / `addi r5,r5,4` - `(self + count*stride) + 4`,
// which is what `data() + mCount` on a `uchar mData[]` starting at +4 lowers to - and its body
// is `stfs`+`addi` on a walked pointer, which is `uninitialized_fill_n`'s `++cur`. With both,
// `fn_800D0170` and all three sibling fills are byte-identical to retail.
typedef rstl::reserved_vector< float, 15 > SFillFloats15;
typedef rstl::reserved_vector< CVector3f, 15 > SFillVec3s15;
typedef rstl::reserved_vector< CVector3f, 5 > SFillVec3s5;

// Retail 0x800D0338, 0xF4 = 61 insns: the same fill with a 16-byte element, one `lfs` and three
// `lwz` per element (`lfs f0,0(r5)` / `lwz r7,4(r5)` / `lwz r6,8(r5)` / `lwz r5,12(r5)`). The
// element type is this file's choice, not a measurement - the retail object does not resolve the
// symbol. `CQuaternion` (a `float` then three more words) is what fits; a placeholder struct of
// one `float` and three `uint` scored identically.
typedef rstl::reserved_vector< CQuaternion, 5 > SFillQuats5;

// Retail 0x800D0338, 0xF4 = 61 insns: `reserved_vector< CQuaternion, 5 >::resize`.
extern "C" void fn_800D0338(SFillQuats5* self, int n, const CQuaternion* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D02F8, 0x40 = 16 insns: the `N = 5` wrapper over `fn_800D0338` -
// `mr r5,r4` / `li r4,5` pass the literal count and the caller's pointer straight through,
// `li r0,0` / `stw r0,0(r3)` empties the vector first, and the epilogue's `mr r3,r31` returns
// the object, so the return type is a pointer, not `void`.
extern "C" SFillQuats5* fn_800D02F8(SFillQuats5* self, const CQuaternion* value) {
  self->mCount = 0;
  fn_800D0338(self, 5, value);
  return self;
}

// Retail 0x800D022C, 0xCC = 51 insns: `reserved_vector< CVector3f, 5 >::resize`.
extern "C" void fn_800D022C(SFillVec3s5* self, int n, const CVector3f* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D01EC, 0x40 = 16 insns: the `N = 5` wrapper over `fn_800D022C`.
extern "C" SFillVec3s5* fn_800D01EC(SFillVec3s5* self, const CVector3f* value) {
  self->mCount = 0;
  fn_800D022C(self, 5, value);
  return self;
}

// Retail 0x800D0170, 0x7C = 31 insns: `reserved_vector< float, 15 >::resize`. `lwz r0,0(r3)` /
// `cmpw` / `beqlr` is `mCount == n` returning immediately (the count is written at the very end,
// `stw r4,0(r3)`, so the early exit must skip it), `bgt` skips the fill when the vector is
// shrinking, and `subf.` + `ble` is `resize`'s `mCount <= count` test. MWCC unrolls
// `uninitialized_fill_n` eight wide (`srwi. r0,r7,3` / `bdnz`, then the `andi. r6,r6,7` remainder).
//
// **The value arrives by pointer, not in a register** - measured, not assumed: retail's
// `lfs f0,0(r5)` dereferences `r5`, and `uninitialized_fill_n`'s `const S& value` keeps `r5` as
// that address through the loop. A by-value parameter instead leaves the value already live in
// `f1` and shifts every subsequent allocation.
extern "C" void fn_800D0170(SFillFloats15* self, int n, const float* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D0130, 0x40 = 16 insns: the `N = 15` wrapper over `fn_800D0170`.
extern "C" SFillFloats15* fn_800D0130(SFillFloats15* self, const float* value) {
  self->mCount = 0;
  fn_800D0170(self, 15, value);
  return self;
}

// Retail 0x800D0064, 0xCC = 51 insns: `reserved_vector< CVector3f, 15 >::resize` -
// `mulli r0,r6,12` is the 12-byte stride, and the three `lfs f2,f1,f0` of the value are hoisted
// above the trip-count guard.
extern "C" void fn_800D0064(SFillVec3s15* self, int n, const CVector3f* value) {
  const int count = self->mCount;
  if (count == n) {
    return;
  }
  if (count <= n) {
    rstl::uninitialized_fill_n(self->data() + count, n - count, *value);
  }
  self->mCount = n;
}

// Retail 0x800D0024, 0x40 = 16 insns: the `N = 15` wrapper over `fn_800D0064`.
extern "C" SFillVec3s15* fn_800D0024(SFillVec3s15* self, const CVector3f* value) {
  self->mCount = 0;
  fn_800D0064(self, 15, value);
  return self;
}

// Retail 0x800CF02C, 0x84 = 33 insns: the fourth link of the teardown chain, and the only one
// that takes a **`rstl::vector`** rather than a raw object. `lwz r0,4(r30)` / `lwz r3,12(r30)` /
// `slwi r0,r0,3` / `add r0,r3,r0` is the empty element walk `rstl::destroy(begin(), end())` over
// 8-byte elements, and `rstl::destroy`'s two `pointer_iterator` arguments are what the four
// `stw r3,20(r1)`..`stw r3,8(r1)` stores are - the same stack-built iterator pair
// `fn_800D0490` passes to `fn_800D0548`. **Calling `rstl::destroy(begin(), end())` is what reaches
// it**: an explicit `for (it = begin(); it != end(); ++it) rstl::destroy(&*it);` loop emits the
// same walk but no stack temporaries, a 16-byte frame instead of 32, and scores 81.58%. Adding
// `mItems = nullptr` / `mCount = 0` / `mCapacity = 0` after the free - retail has no such stores -
// costs 87.88%.
extern "C" void* fn_800CF02C(void* self, short deleting) {
  if (self != nullptr) {
    SUniqueIdFloats* v = static_cast< SUniqueIdFloats* >(self);
    rstl::destroy(v->begin(), v->end());
    CMemory::Free(v->mItems);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// Retail 0x800CEF2C..0x800CF02C, a four-link out-of-line teardown chain declared descending
// by retail offset (`fn_800CF02C`, `fn_800CEFD8`, `fn_800CEF84`, `fn_800CEF2C`). They are global
// symbols (`config/G2ME01/symbols.txt:3681-3684`, no `scope:local`) and `fn_800CEF2C`/
// `fn_800CEF84` are also called from `CGrappleArm` and `CPlayerGunBase`, so they are member
// teardown helpers promoted to extern linkage, not statics.
//
// All four share one shape, measured instruction by instruction from the disassembly:
// `stwu/mflr/stw/stw r31/stw r30`, `mr r31,r4` (the flag), `mr. r30,r3` (the object, which sets
// CR0 so the opening `beq` is the null test), the body, then `extsh. r0,r31` + `ble` and a single
// exit. Three consequences, each measured on the identical chain in `CPhysicsActor.cpp`
// (`fn_800EB944` there, 25/25 bytes):
//   - the flag is a **sign-extended halfword** branched on **sign**, so the parameter is `short`
//     and the test is `deleting > 0`; a `bool` emits `clrlwi.`/`beq` instead of the `extsh.`;
//   - the return is `void*` and there is **one exit** returning the object (retail's epilogue is
//     a single `mr r3,r30` before the reloads), so `void` drops that instruction and an early
//     `return self` adds one back;
//   - each link passes a **literal** -1 or 1 - for the next link's flag, not a stored value.
//
// `fn_800CEFD8` (0x800CEFD8, 0x54 = 21 insns) frees the pointer at +12 unconditionally and the
// object only when `deleting > 0`.
extern "C" void* fn_800CEFD8(void* self, short deleting) {
  if (self != nullptr) {
    CMemory::Free(*reinterpret_cast< void** >(reinterpret_cast< char* >(self) + 12));
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800CEF84` (0x800CEF84, 0x54 = 21 insns): `lwz`-free, so the first `Free` is the whole call -
// the chain link is `fn_800CEFD8(self, -1)`.
extern "C" void* fn_800CEF84(void* self, short deleting) {
  if (self != nullptr) {
    fn_800CEFD8(self, -1);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

// `fn_800CEF2C` (0x800CEF2C, 0x58 = 22 insns): dereferences +0 before delegating, so it is the
// outermost link and takes the pointer *stored in* its object.
extern "C" void* fn_800CEF2C(void* self, short deleting) {
  if (self != nullptr) {
    fn_800CEF84(*reinterpret_cast< void** >(self), 1);
    if (deleting > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}

void CMorphBall::DeleteBallShadow() { mShadow = nullptr; }

void CMorphBall::CreateBallShadow() {
  if (!mShadow.get()) {
    mShadow = rs_new CMorphBallShadow(64, 64, gpSimplePool->GetObj("TXTR_BallFade"));
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::RenderToShadowTex(CStateManager& mgr) {
  // TODO: Gather shadow receivers and render CMorphBallShadow with the player texture.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::DrawBallShadow(CStateManager& mgr) {
  // TODO: Draw the projected ball shadow against the gathered world/actor receivers.
}

// Guessed name.
void CMorphBall::InitializeWakeEffects() {
  sWakeEffectForMaterial.resize(64, -1);
  sWakeEffectForMaterial[kMT_Phazon] = 0;
  sWakeEffectForMaterial[kMT_Dirt] = 2;
  sWakeEffectForMaterial[kMT_Organic] = 3;
  sWakeEffectForMaterial[kMT_Sand] = 4;
  const char* effects[] = {"PhazonWake",  "PhazonWakeOrange", "DirtWake",
                           "OrganicWake", "SandWake",         "RainWake"};
  const char* groups[] = {"PhazonWake_DGRP",  "PhazonWakeOrange_DGRP", "DirtWake_DGRP",
                          "OrganicWake_DGRP", "SandWake_DGRP",         "RainWake_DGRP"};
  for (int i = 0; i < 6; ++i) {
    const CDependencyGroupToken group(gpSimplePool->GetObj(groups[i]), *gpSimplePool);
    mWakeEffects.push_back(rstl::auto_ptr< CDeferredParticleEffect >(
        rs_new CDeferredParticleEffect(gpSimplePool->GetObj(effects[i]), group)));
  }
}

// Guessed name.
void CMorphBall::ResetScrewAttackExitAnimationTimer() { mScrewAttackExitAnimationFrames = 5; }

// Guessed name.
int CMorphBall::GetScrewAttackGroundedFrames() const { return mScrewAttackGroundedFrames; }

bool CMorphBall::InScrewAttackMode() const {
  return mBallState == kBS_ScrewAttack || mBallState == kBS_ScrewAttackWallJump ||
         mBallState == kBS_ScrewAttackRecovery;
}

bool CMorphBall::IsProjectile() const { return mBallState == kBS_Projectile; }

bool CMorphBall::IsBoostShieldActive() const { return mTimeNotInBoost < 1.f; }

float CMorphBall::GetBoostChargeTimer() const { return mBoostChargeTime; }

float CMorphBall::GetTimeNotInBoost() const { return mTimeNotInBoost; }

bool CMorphBall::IsBoosting() const {
  return mBallState == kBS_Boost || mBallState == kBS_SpiderBoost;
}

// Guessed name; workspace type name is a cross-game hypothesis.
void CMorphBall::PointGenerator(const CSkinnedModel& model, const SSkinningWorkspace& workspace,
                                void* context) {
  if (context) {
    static_cast< CRainSplashGenerator* >(context)->GeneratePoints(model, workspace);
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::StartLandingSfx() {
  // TODO: Choose and play the landing sound from velocity and selected surface material.
}

void CMorphBall::StopSounds() {
  if (mRollSfx) {
    CSfxManager::SfxStop(mRollSfx);
    mRollSfx.Clear();
  }
  if (mSpiderSfx) {
    CSfxManager::SfxStop(mSpiderSfx);
    mSpiderSfx.Clear();
  }
  if (mDeathBallSfx) {
    CSfxManager::SfxStop(mDeathBallSfx);
    mDeathBallSfx.Clear();
  }
  if (mScrewAttackSfx) {
    CSfxManager::SfxStop(mScrewAttackSfx);
    mScrewAttackSfx.Clear();
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::StartScrewAttackSfx() {
  // TODO: Start the Screw Attack sound with the player's sound-channel settings.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateMorphBallSound(float dt, CStateManager& mgr) {
  // TODO: Maintain the roll, Spider, death-ball and Screw Attack emitters.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::SelectMorphBallSounds(const CMaterialList& material) {
  // TODO: Select roll/landing sound IDs for collision material and multiplayer mode.
}

void CMorphBall::TakeDamage(float damage) {
  if (damage <= 0.f) {
    mDamageEffect = 0.f;
    mDamageEffectDecaySpeed = 0.f;
    return;
  }

  if (damage >= 20.f) {
    mDamageEffectDecaySpeed = 0.25f;
  } else if (damage > 5.f) {
    mDamageEffectDecaySpeed = 1.f - 0.75f * ((damage - 5.f) / 15.f);
  } else {
    mDamageEffectDecaySpeed = 1.f;
  }
  mDamageEffect = 1.f;
}

CMorphBall::EBombJumpState CMorphBall::GetBombJumpState() const { return mBombJumpState; }

void CMorphBall::SetBallBoostState(EBallBoostState state) { mBoostState = state; }

CMorphBall::EBallBoostState CMorphBall::GetBallBoostState() const { return mBoostState; }

void CMorphBall::SetAsProjectile(bool projectile) {
  if (projectile) {
    mBallState = kBS_Projectile;
  } else if (mBallState == kBS_Projectile) {
    mBallState = kBS_Normal;
  }
}

void CMorphBall::TouchModel(const CStateManager& mgr) const {
  mBallModel->Touch(mgr, mBallModelShader);
  if (mPlayer.GetPlayerState()->HasPowerUp(CPlayerState::kIT_SpiderBall) &&
      mSpiderBallGlassModel.get()) {
    mSpiderBallGlassModel->Touch(mgr, mSpiderBallGlassModelShader);
  }
  mLowPolyBallModel->Touch(mgr, mLowPolyBallModelShader);
}

CModelData* CMorphBall::GetMorphBallModel(const rstl::string& name, float radius) {
  if (name == rstl::string("")) {
    return nullptr;
  }
  const SObjectTag* tag = gpResourceFactory->GetResourceIdByName(name.data());
  const CVector3f scale(2.f * radius, 2.f * radius, 2.f * radius);
  if (tag->type == 'CMDL') {
    return rs_new CModelData(CStaticRes(tag->id, scale));
  }
  return rs_new CModelData(CAnimRes(tag->id, CAnimRes::kDefaultCharIdx, scale, 0, false));
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::LoadMorphBallModel() {
  // TODO: Select normal/spider/boost resources and glow colors for the three Echoes suits.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::FluidFXThink(CActor::EFluidState state, CScriptWater& water, CStateManager& mgr) {
  // TODO: Rate-limit water splashes using speed, fluid state and the fluid-plane manager.
}

bool CMorphBall::IsClimbable(const CCollisionInfo& collision) const {
  if (CMath::AbsF(collision.GetNormalLeft().GetZ()) < 0.7f) {
    const float height = GetBallPosition().GetZ() - collision.GetPoint().GetZ();
    return height > 0.1f && height < GetBallRadius() - 0.05f;
  }
  return false;
}

// The original body is genuinely empty.
void CMorphBall::Touch(CActor& actor, CStateManager& mgr) {}

// Retail 0x800C190C, 0x98 = 38 insns.
// `bl GetIsInHalfPipeMode__10CMorphBallCFv`; in half-pipe mode it takes
// `GetVelocityWR().Magnitude() * 1.5f` (r2-28852 = 1.5), then `rstl::max_val` against
// 0.01f (r2-28764) **first** and then `rstl::min_val` against 95.f (r2-28836). Retail
// loads both constants before the multiply (f0 = 1.5, f2 = 0.01, then `fmuls f0,f0,f1`),
// and its first `fcmpo cr0,f0,f2` / `bge` takes f0 (the product) when the product is the
// larger - a max. Otherwise it returns
// `gpTweakBall->GetBallTranslationMaxSpeed(mPlayer.GetSurfaceRestraint())`.
float CMorphBall::ComputeMaxSpeed() const {
  if (GetIsInHalfPipeMode()) {
    float maxSpeed = mPlayer.GetVelocityWR().Magnitude() * 1.5f;
    maxSpeed = rstl::max_val(maxSpeed, 0.01f);
    return rstl::min_val(maxSpeed, 95.f);
  }
  return gpTweakBall->GetBallTranslationMaxSpeed(mPlayer.GetSurfaceRestraint());
}

void CMorphBall::SpinToSpeed(float speed, const CVector3f& direction, float dt) {
  const float angularSpeed = mPlayer.GetAngularVelocityWR().GetVector().Magnitude();
  mPlayer.ApplyTorqueWR(dt * (speed - angularSpeed) * direction);
}

void CMorphBall::ApplyGravity() {
  mPlayer.SetMomentumWR(CVector3f(0.f, 0.f, mPlayer.GetMass() * GetGravityAcceleration()));
}

// Retail 0x800C1AC0, 0x90 = 36 insns.
// `lwz r3,0(r3)` / `bl CheckSubmerged__7CPlayerCFv`; if submerged *and* not
// `li r4,25` (0x19 = kIT_GravityBoost, checked through `lwz r3,4884(r3)` =
// CPlayer+0x1314 = mPlayerState) it returns `GetBallWaterGravity`. Otherwise it tests
// `lwz r0,3200(r31)` (this+0xC80 = mBallState) against 4 then 5: 4 (kBS_ScrewAttack) ->
// GetScrewAttackGravity, 5 (kBS_ScrewAttackWallJump) -> GetScrewAttackWallJumpGravity,
// else GetBallGravity. It is an if/else chain, **not** a switch - written as a switch
// mwcceppc emits a `cmpwi 5` / `bge` / `cmpwi 4` tree in the opposite order and the
// function drops to 83%.
float CMorphBall::GetGravityAcceleration() const {
  if (mPlayer.CheckSubmerged() && !mPlayer.GetPlayerState()->HasPowerUp(CPlayerState::kIT_GravityBoost)) {
    return gpTweakBall->GetBallWaterGravity();
  }
  if (mBallState == kBS_ScrewAttack) {
    return gpTweakBall->GetScrewAttackGravity();
  }
  if (mBallState == kBS_ScrewAttackWallJump) {
    return gpTweakBall->GetScrewAttackWallJumpGravity();
  }
  return gpTweakBall->GetBallGravity();
}

// Retail 0x800C1B50, 0x98 = 38 insns.
// `lwz r3,0(r30)` / `bl GetSurfaceRestraint__7CPlayerCFv` / `bl GetBallTranslationFriction__10CTweakBallCFi`
// then, while `lhz r4,740(r3)` (CPlayer+0x2e4 = mAttachedActor) differs from the 2-byte SDA
// constant at -27740(r13) (kInvalidUniqueId), `fmuls f1,f1,2.f` (r2-28784 = 2.0). Then
// `lwz r0,752(r3)` (CPlayer+0x2f0 = mEnergyDrain.mSources.mCount); if it is > 0 the
// count is converted with the 2^52 + 2^31 trick and scaled by 1.5 (r2-28852).
float CMorphBall::CalculateSurfaceFriction() const {
  float friction = gpTweakBall->GetBallTranslationFriction(mPlayer.GetSurfaceRestraint());
  if (mPlayer.GetAttachedActor() != kInvalidUniqueId) {
    friction *= 2.f;
  }
  const int count = mPlayer.GetEnergyDrainSourceCount();
  if (count > 0) {
    friction *= count * 1.5f;
  }
  return friction;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeLiftForces(const CVector3f& controlForce, const CVector3f& velocity,
                                   const CStateManager& mgr) {
  // TODO: Update the lift averages and apply the contact-dependent upward force.
}

CAABox CMorphBall::GetRenderBounds(const CStateManager& mgr) const {
  const CVector3f center = GetBallPosition();
  const CVector3f extent(2.f * mRadius, 2.f * mRadius, 2.f * mRadius);
  CAABox bounds(center - extent, center + extent);
  if (mSlowBlueTailSwooshGen->GetModulationColor().GetAlpha() != 0.f) {
    const rstl::optional_object< CAABox > trailBounds = mSlowBlueTailSwooshGen->GetBounds();
    if (trailBounds.valid()) {
      bounds.AccumulateBounds(trailBounds->GetMinPoint());
      bounds.AccumulateBounds(trailBounds->GetMaxPoint());
    }
  }
  return bounds;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::CollidedWith(const TUniqueId& id, const CCollisionInfoList& collisions,
                              CStateManager& mgr) {
  // TODO: Process contact materials/normals, boost damage, half-pipe and Screw Attack collisions.
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::BallCloseToCollision(const CStateManager& mgr, float distance,
                                      const CMaterialFilter& filter) const {
  // TODO: Test a swept sphere against the world's filtered collision geometry.
  return false;
}

void CMorphBall::DisableHalfPipeStatus() {
  SetIsInHalfPipeMode(false);
  SetIsInHalfPipeModeInAir(false);
  SetTouchedHalfPipeRecently(false);
  mTouchHalfPipeCooldown = 0.f;
  mDisableControlCooldown = 0.f;
  mPlayer.SetCollisionAccuracyModifier(5.f);
  mPrevHalfPipeNormal = CVector3f::Zero();
  mHalfPipeNormal = CVector3f::Zero();
}

void CMorphBall::SetTouchedHalfPipeRecently(bool touched) {
  mTouchedHalfPipeRecently = touched;
}

bool CMorphBall::GetTouchedHalfPipeRecently() const { return mTouchedHalfPipeRecently; }

void CMorphBall::SetIsInHalfPipeModeInAir(bool active) { mInHalfPipeModeInAir = active; }

bool CMorphBall::GetIsInHalfPipeModeInAir() const { return mInHalfPipeModeInAir; }

void CMorphBall::SetIsInHalfPipeMode(bool active) { mInHalfPipeMode = active; }

bool CMorphBall::GetIsInHalfPipeMode() const { return mInHalfPipeMode; }

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateHalfPipeStatus(CStateManager& mgr, float dt) {
  // TODO: Expire half-pipe/contact cooldowns and adjust collision accuracy.
}

// Guessed name.
void CMorphBall::RenderScrewAttackJumpEffects() const {
  if (mScrewAttackJumpFlashGen.get()) {
    mScrewAttackJumpFlashGen->Render();
  }
  if (mScrewAttackWallJumpFlashGen.get()) {
    mScrewAttackWallJumpFlashGen->Render();
  }
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::RenderDamageEffects(const CStateManager& mgr,
                                     const CTransform4f& transform) const {
  // TODO: Render the damage overlay using the ball's damage timer and glow color.
}

void CMorphBall::RenderIceBreakEffect(const CStateManager& mgr) const {
  if (mMorphBallIceBreakGen.get()) {
    mMorphBallIceBreakGen->Render();
  }
}

void CMorphBall::UpdateIceBreakEffect(float dt) {
  if (!mMorphBallIceBreakGen.get() && mMorphBallIceBreak.HasLock() &&
      mMorphBallIceBreak.IsLoaded()) {
    mMorphBallIceBreakGen = rs_new CElementGen(mMorphBallIceBreak);
    mMorphBallIceBreakGen->SetOrientation(mPlayer.GetTransform().GetRotation());
  }
  if (mMorphBallIceBreakGen.get()) {
    if (mMorphBallIceBreakGen->IsSystemDeletable()) {
      mMorphBallIceBreakGen = nullptr;
      mMorphBallIceBreak.Unlock();
    } else {
      mMorphBallIceBreakGen->SetGlobalTranslation(GetBallPosition());
      mMorphBallIceBreakGen->Update(dt);
    }
  }
}

void CMorphBall::ResetMorphBallIceBreak() {
  mMorphBallIceBreak.Lock();
  mMorphBallIceBreakGen = nullptr;
}

bool CMorphBall::IsMorphBallTransitionFlashValid() const {
  return mMorphBallTransitionFlashGen.get() != nullptr;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::RenderMorphBallTransitionFlash(const CStateManager& mgr) const {
  // TODO: Apply the suit-dependent transition color before rendering the generator.
}

void CMorphBall::UpdateMorphBallTransitionFlash(float dt) {
  if (!mMorphBallTransitionFlashGen.get() && mMorphBallTransitionFlash.HasLock() &&
      mMorphBallTransitionFlash.IsLoaded()) {
    mMorphBallTransitionFlashGen = rs_new CElementGen(mMorphBallTransitionFlash);
    mMorphBallTransitionFlashGen->SetOrientation(mPlayer.GetTransform().GetRotation());
  }
  if (mMorphBallTransitionFlashGen.get()) {
    if (mMorphBallTransitionFlashGen->IsSystemDeletable()) {
      mMorphBallTransitionFlashGen = nullptr;
      mMorphBallTransitionFlash.Unlock();
    } else {
      mMorphBallTransitionFlashGen->SetGlobalTranslation(GetBallPosition());
      mMorphBallTransitionFlashGen->Update(dt);
    }
  }
}

void CMorphBall::ResetMorphBallTransitionFlash() {
  mMorphBallTransitionFlash.Lock();
  mMorphBallTransitionFlashGen = nullptr;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::Render(const CStateManager& mgr, const CActorLights* lights) const {
  // TODO: Render the ball, glass, trails and Echoes Screw Attack/death-ball effects.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::PreRender(CStateManager& mgr, const CFrustumPlanes& frustum) {
  // TODO: Prepare model animation, rain-splash point generation, actor lights and the world shadow.
}

// Scaffold, not a reconstructed implementation.
float CMorphBall::GetMinimumAlignmentSpeed() const {
  // TODO: Return zero in Spider mode; otherwise use the alignment-speed tweak.
  return 0.f;
}

void CMorphBall::DampLinearAndAngularVelocities(float linearDamping, float angularDamping,
                                                float dt) {
  const float frames = 60.f * dt;
  const float linearScale = pow(1.f - linearDamping, frames);
  mPlayer.SetVelocityWR(linearScale * mPlayer.GetVelocityWR());
  const float angularScale = pow(1.f - angularDamping, frames);
  mPlayer.SetAngularVelocityWR(mPlayer.GetAngularVelocityWR() * angularScale);
}

void CMorphBall::ApplyFriction(float friction) {
  CVector3f velocity = mPlayer.GetVelocityWR();
  if (velocity.Magnitude() <= friction) {
    velocity = CVector3f::Zero();
  } else {
    velocity = (velocity.Magnitude() - friction) * velocity.AsNormalized();
  }
  mPlayer.SetVelocityWR(velocity);
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::UpdateMarbleDynamics(CStateManager& mgr, float dt, const CVector3f& point) {
  // TODO: Apply marble alignment, rolling torque and contact-force response.
  return false;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplyBoostBallDamage(CStateManager& mgr, TUniqueId id, const CDamageInfo& damage,
                                      float dt) {
  // TODO: Filter already-hit actors, scale damage and update the cooldown/history.
}

void CMorphBall::CancelBoosting() {
  mBoostChargeTime = 0.f;
  mBoostDrainTime = 0.f;
  if (mBallAnimationIndex == 1) {
    mBallAnimationIndex = 0;
    CSfxManager::SfxStop(mBoostChargeSfx);
    mBoostChargeSfx.Clear();
  }
}

void CMorphBall::LeaveBoosting() {
  if (IsBoosting()) {
    mBoostChargeTime = 0.f;
    mBallState = kBS_Normal;
  }
  mBoostDrainTime = 0.f;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::EnterBoosting(CStateManager& mgr, bool skipImpulse) {
  // TODO: Enter normal/spider boost, optionally apply the impulse, and reset damage history.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeBoostBallMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Handle charge, release, draining, Spider Boost direction and damage.
}

void CMorphBall::SetScrewAttackActive(bool active) { mForcedScrewJumpInput = active; }

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplyScrewAttackDamage(float dt, CStateManager& mgr) {
  // TODO: Build the swept contact list and apply Screw Attack damage.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateScrewAttackRecovery(float dt) {
  // TODO: Recover from recoil/collisions and request the player's exit animation.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeScrewAttackMovement(const CFinalInput& input, CStateManager& mgr,
                                            float dt) {
  // TODO: Handle jump/wall-jump input, speed/height limits and Screw Attack recovery.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateDeathBall(float dt, CStateManager& mgr) {
  // TODO: Update the multiplayer death-ball effects and per-object damage cooldowns.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateBallLight(float dt, CStateManager& mgr) {
  // TODO: Update the inner-glow light from the generator and ball lighting state.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateEffects(float dt, CStateManager& mgr) {
  // TODO: Update particle transforms, trail history, wake selection and light intensities.
}

void CMorphBall::StopParticleWakes() {
  mWallSparkGen->SetParticleEmission(false);
  for (int i = 0; i < 6; ++i) {
    mWakeEffects[i]->SetParticleEmission(false);
  }
}

void CMorphBall::LeaveMorphBallState(CStateManager& mgr) {
  LeaveBoosting();
  CancelBoosting();
  CSfxManager::SfxStop(mBoostChargeSfx);
  mBoostReleaseSfx.Clear();
  StopParticleWakes();
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::EnterMorphBallState(CStateManager& mgr, EBallState state) {
  // TODO: Reset ball/spider/boost state, sounds, averages and lighting for the requested mode.
}

void CMorphBall::SetBallLightActive(CStateManager& mgr, bool active) {
  mBallLightActive = active;
}

void CMorphBall::DeleteLight(CStateManager& mgr) {
  if (mBallInnerGlowLight != kInvalidUniqueId) {
    mgr.DeleteObjectRequest(mBallInnerGlowLight);
    mBallInnerGlowLight = kInvalidUniqueId;
  }
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::DoUserAnimEvent(CStateManager& mgr, const CInt32POINode& node,
                                 EUserEventType type) {
  // TODO: Handle event 0x1b during morphed Screw Attack recovery through CPlayer.
  return false;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: Handle Echoes CScriptMsg creation/deletion of the inner-glow light.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::Update(float dt, CStateManager& mgr) {
  // TODO: Update effects, light, death-ball state, tire interpolation, damage decay, rain and
  // sound.
}

void CMorphBall::SwitchToTire() {
  mTireMode = true;
  mTireInterpolating = true;
  mBallTiltAngle = 0.f;
  mTireInterpolationSpeed = 1.f;
}

void CMorphBall::SwitchToMarble() {
  CVector3f lookDir = mPlayer.GetLookDir();
  CQuaternion tiltQ = CQuaternion::AxisAngle(
      CUnitVector3f(mPlayer.GetTransform().TransposeRotate(lookDir)),
      CRelAngle::FromRadians(mBallTiltAngle));
  mPlayer.SetTransform(mPlayer.GetTransform() * tiltQ.BuildTransform4f());
  mTireMode = false;
  mTireInterpolating = true;
  mTireInterpolationSpeed = -1.f;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::UpdateBallDynamics(CStateManager& mgr, float dt) {
  // TODO: Update contact orientation, tire/marble mode, damping and velocity history.
}

// Scaffold, not a reconstructed implementation.
float CMorphBall::BallTurnInput(const CFinalInput& input) const {
  // TODO: Use the player's Echoes control mapping: turn-left minus turn-right.
  return 0.f;
}

bool CMorphBall::CalculateBallContactInfo(CVector3f& normal, CVector3f& point) const {
  if (mCollisionInfos.GetCount() > 0) {
    normal = mCollisionInfos[0].GetNormalLeft();
    point = mCollisionInfos[0].GetPoint();
    return true;
  }
  return false;
}

CTransform4f CMorphBall::CalculateSurfaceToWorld(const CVector3f& normal, const CVector3f& point,
                                                 const CVector3f& direction) const {
  if (direction.CanBeNormalized()) {
    const CVector3f forward = direction.AsNormalized();
    CVector3f right = CVector3f::Cross(direction, normal);
    if (right.CanBeNormalized()) {
      right.Normalize();
      const CVector3f up = CVector3f::Cross(right, forward).AsNormalized();
      return CTransform4f::FromColumns(right, forward, up, point);
    }
  }
  return CTransform4f::Identity();
}

CVector3f CMorphBall::GetBallPosition() const {
  return mPlayer.GetTranslation() + CVector3f(0.f, 0.f, mRadius);
}

CTransform4f CMorphBall::GetBallToWorld() const {
  // Retail inlines GetBallPosition() here (0x800CB764) and reads mRadius at this+12 rather
  // than calling GetBallRadius(), so the ball position is spelled out.
  const CTransform4f& playerXf = mPlayer.GetTransform();
  const CVector3f ballTranslation(0.f, 0.f, mRadius);
  const CVector3f& translation = mPlayer.GetTranslation();
  const CVector3f ballPos = translation + ballTranslation;

  return CTransform4f(CTransform4f::Translate(ballPos) * playerXf.GetRotation());
}

CTransform4f CMorphBall::GetSwooshToWorld() const {
  // Retail (0x800CB7F4) materialises the last operator* at this+216 and copy-constructs it
  // into the return slot, so the product is wrapped in a real CTransform4f temporary; a plain
  // `return <product>;` is RVO'd straight into sret and drops that copy.
  return CTransform4f(
      (CTransform4f::Translate(mPlayer.GetTranslation() +
                               CVector3f(0.f, 0.f, GetBallRadius())) *
       mSurfaceToWorld.GetRotation()) *
      CTransform4f::RotateY(CRelAngle::FromRadians(mBallTiltAngle)));
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ComputeMarioMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Compute camera-relative control, friction, lift, torque and contact response.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::TransformSpiderBallState(const CQuaternion& rotation,
                                          const CVector3f& translation) {
  // TODO: Rotate the saved physics forces/normals and transform the track point about the ball.
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::CreateSpiderBallParticles(CStateManager& mgr, const CVector3f& ballPosition,
                                           const CVector3f& trackPoint) {
  // TODO: Emit/update the magnet effect along the ball-to-track segment.
}

float CMorphBall::GetSpiderBallSwingControllerMovementScalar() const {
  if (mSwingControlTime < 1.2f) {
    return 1.f;
  }
  return rstl::max_val(0.f, (2.4f - mSwingControlTime) / 1.2f);
}

void CMorphBall::UpdateSpiderBallSwingControllerMovementTimer(float movement, float dt) {
  if (CMath::AbsF(movement) < 0.05f) {
    ResetSpiderBallSwingControllerMovementTimer();
  } else if (mSwingControlDirection != CMath::Sign(movement)) {
    ResetSpiderBallSwingControllerMovementTimer();
    mSwingControlDirection = CMath::Sign(movement);
  } else {
    mSwingControlTime += dt;
  }
}

void CMorphBall::ResetSpiderBallSwingControllerMovementTimer() {
  mSwingControlDirection = 0.f;
  mSwingControlTime = 0.f;
}

// Scaffold, not a reconstructed implementation.
float CMorphBall::GetSpiderBallControllerMovement(const CFinalInput& input) const {
  // TODO: Convert mapped movement axes to signed magnitude with the Echoes angle dead zones.
  return 0.f;
}

void CMorphBall::SetSpiderBallSwingingState(bool swinging) {
  if (mSpiderBallSwinging != swinging) {
    ResetSpiderBallSwingControllerMovementTimer();
    mSpiderSwingInAir = true;
  }
  mSpiderBallSwinging = swinging;
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::FindClosestSpiderBallWaypoint(
    CStateManager& mgr, const CVector3f& center, CVector3f& trackPoint,
    CVector3f& interpolatedDirection, CVector3f& direction, float& distance, CVector3f& normal,
    ESpiderSurfaceType& surfaceType, TUniqueId& surfaceId, CTransform4f& surfaceTransform) const {
  // TODO: Search waypoint tracks, scripted surfaces and collision surfaces; populate the outputs.
  return false;
}

// Scaffold, not a reconstructed implementation.
bool CMorphBall::CheckForSwitchToSpiderBallSwinging(CStateManager& mgr) const {
  // TODO: Check Spider surface kind, attachment geometry and the player's movement state.
  return false;
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplySpiderBallRollForces(const CFinalInput& input, CStateManager& mgr, float dt) {
  // TODO: Find the attachment, project controls and apply Spider roll/attraction forces.
}

void CMorphBall::ResetSpiderBallForces() {
  mNormalizedSpiderSurfaceForces = CVector2f(0.f, 0.f);
  mSpiderTrackForceMagnitude = 0.f;
  mSpiderViewControlMagnitude = 0.f;
  mSpiderForcesReset = true;
}

// Scaffold, not a reconstructed implementation.
CVector2f CMorphBall::CalculateSpiderBallAttractionSurfaceForces(const CFinalInput& input) const {
  // TODO: Build the mapped two-axis attraction input with movement gating.
  return CVector2f(0.f, 0.f);
}

// Scaffold, not a reconstructed implementation.
CVector3f CMorphBall::TransformSpiderBallForcesXZ(CVector2f& forces, CStateManager& mgr) const {
  // TODO: Transform the XZ force plane using the player's Echoes camera mode.
  return CVector3f::Zero();
}

// Scaffold, not a reconstructed implementation.
CVector3f CMorphBall::TransformSpiderBallForcesXY(CVector2f& forces, CStateManager& mgr) const {
  // TODO: Transform the XY force plane using the player's Echoes camera mode.
  return CVector3f::Zero();
}

// Scaffold, not a reconstructed implementation.
void CMorphBall::ApplySpiderBallSwingingForces(const CFinalInput& input, CStateManager& mgr,
                                               float dt) {
  // TODO: Apply the radial constraint, swing input and gravity about the Spider track.
}

void CMorphBall::UpdateSpiderBall(const CFinalInput& input, CStateManager& mgr, float dt) {
  SetSpiderBallSwingingState(CheckForSwitchToSpiderBallSwinging(mgr));
  if (mSpiderBallSwinging) {
    ApplySpiderBallSwingingForces(input, mgr, dt);
  } else {
    ApplySpiderBallRollForces(input, mgr, dt);
  }
}

void CMorphBall::SetDamageTimer(float time) { mDamageTimer = time; }

void CMorphBall::SetDisableSpiderBallTime(float time) { mDisableSpiderBallTime = time; }

// Scaffold, not a reconstructed implementation.
bool CMorphBall::IsMovementAllowed() const {
  // TODO: Check per-player free-look controls, morph transitions and the control cooldown.
  return false;
}

void CMorphBall::ComputeBallMovement(const CFinalInput& input, CStateManager& mgr, float dt) {
  switch (mBallState) {
  case kBS_Normal:
  case kBS_Boost:
  case kBS_Spider:
  case kBS_SpiderBoost:
  case kBS_Projectile:
    ComputeBoostBallMovement(input, mgr, dt);
    ComputeMarioMovement(input, mgr, dt);
    break;
  case kBS_ScrewAttack:
  case kBS_ScrewAttackWallJump:
    ComputeScrewAttackMovement(input, mgr, dt);
    break;
  case kBS_ScrewAttackRecovery:
    UpdateScrewAttackRecovery(dt);
    break;
  }
}

// Scaffold, not a reconstructed implementation.
float CMorphBall::ForwardInput(const CFinalInput& input) const {
  // TODO: Use the player's Echoes control mapping: forward minus backward, gated by
  // IsMovementAllowed.
  return 0.f;
}

// Scaffold, not a reconstructed implementation.
// Retail 0x800CE9A4, 0x24 = 9 insns is exactly `lwz r3,-28244(r13)` /
// `bl GetBallTouchRadius__10CTweakBallCFv` and nothing else - `tools/sda.py -28244` resolves the
// `disp(r13)` to `gpTweakBall`, so `return gpTweakBall->GetBallTouchRadius();` is byte-exact and
// measures 100.00%. **It is still a scaffold here, deliberately**: `src/MetroidPrime/Tweaks/
// CTweakBall.cpp` is not in the port's `files.cmake`, so writing the call makes
// `CTweakBall::GetBallTouchRadius()` a new undefined symbol and `tools/link_check.sh --strict`
// fails the gate at 251 undefined against a baseline of 250 (GREW). Adding that unit to
// `files.cmake` is the other half of this fix and is not this item's to make.
float CMorphBall::GetBallTouchRadius() const {
  // TODO: `return gpTweakBall->GetBallTouchRadius();` - see the note above; blocked on
  // CTweakBall.cpp being in the port's files.cmake.
  return 0.f;
}

float CMorphBall::GetBallRadius() const { return mPlayer.GetTweakPlayer()->GetBallRadius(); }

// Ownership cleanup is supplied by the members' destructors.
CMorphBall::~CMorphBall() {}

// Material 59 has no established semantic name in this checkout.
CMorphBall::CMorphBall(CPlayer& player, float radius, bool multiplayer)
: mPlayer(player)
, mLoadedModelId(-1)
, mBallGlowColorIdx(0)
, mRadius(radius)
, mBoostControlForce(CVector3f::Zero())
, mControlForce(CVector3f::Zero())
, mTireMode(false)
, mTireLeanAngle(0.f)
, mBallTiltAngle(0.f)
, mCollisionSphere(
      CSphere(CVector3f(0.f, 0.f, radius), radius),
      CMaterialList(kMT_Player, kMT_Unknown59, kMT_GroundCollider, kMT_NoPlayerCollision))
, mBallModel(GetMorphBallModel(multiplayer ? "SamusMultiBallANCS" : "SamusBallCMDL", radius))
, mBallModelShader(0)
, mSpiderBallGlassModel(GetMorphBallModel("", radius))
, mSpiderBallGlassModelShader(0)
, mLowPolyBallModel(GetMorphBallModel("SamusBallLowPolyCMDL", radius))
, mLowPolyBallModelShader(0)
, mFrozenBallModel(GetMorphBallModel("SamusBallFrozenCMDL", radius))
, mLastWallCollisionFrame(-1)
, mLastFloorCollisionFrame(-1)
, mBallState(kBS_Normal)
, mPlayerToSpiderNormal(CVector3f::Zero())
, mSpiderPullMovement(1.f)
, mSpiderTrackPoint(CVector3f::Zero())
, mSpiderInterpBetweenPoints(CVector3f::Zero())
, mSpiderBetweenPoints(CVector3f::Zero())
, mLinearVelocityDamping(0.f)
, mAngularVelocityDamping(0.f)
, mSpiderNearby(false)
, mTouchingSpider(false)
, mSpiderBallSwinging(false)
, mSpiderSwingInAir(true)
, mSpiderSurfaceType(kSST_None)
, mSpiderSurfaceTransform(CTransform4f::Identity())
, mSpiderSurfacePivotAngle(0.f)
, mSpiderSurfacePivotTargetAngle(0.f)
, mRefPullVelocity(0.f)
, mPlayerToSpiderTrackDistance(0.f)
, mSwingControlDirection(0.f)
, mSwingControlTime(0.f)
, mNormalizedSpiderSurfaceForces(0.f, 0.f)
, mSpiderTrackForceMagnitude(0.f)
, mSpiderViewControlMagnitude(0.f)
, mDamageTimer(0.f)
, mSpiderForcesReset(false)
, mSurfaceToWorld(CTransform4f::Identity())
, mSlowBlueTailSwoosh(
      gpSimplePool->GetObj(multiplayer ? "SlowBlueTailSwoosh_MP" : "SlowBlueTailSwoosh"))
, mSlowBlueTailSwoosh2(
      gpSimplePool->GetObj(multiplayer ? "SlowBlueTailSwoosh2_MP" : "SlowBlueTailSwoosh2"))
, mJaggyTrail(gpSimplePool->GetObj(multiplayer ? "JaggyTrail_MP" : "JaggyTrail"))
, mSideSwoosh(gpSimplePool->GetObj("SideSwooshSide"))
, mWallSpark(gpSimplePool->GetObj("WallSpark"))
, mBallInnerGlow(gpSimplePool->GetObj("BallInnerGlow"))
, mSpiderBallMagnet(gpSimplePool->GetObj("SpiderBallMagnetEffect"))
, mBoostBallGlow(gpSimplePool->GetObj("BoostBallGlow"))
, mMorphBallTransitionFlash(gpSimplePool->GetObj("MorphBallTransitionFlash"))
, mMorphBallIceBreak(gpSimplePool->GetObj("Effect_MorphBallIceBreak"))
, mBoostEffect(gpSimplePool->GetObj("BoostEffect"))
, mDeathBallOuterShell(gpSimplePool->GetObj("DeathBallOuterShell"))
, mDeathBallSpikes(gpSimplePool->GetObj("DeathBallSpikes"))
, mScrewAttackJumpFlash(gpSimplePool->GetObj("ScrewAttackJumpFlash"))
, mSlowBlueTailSwooshGen(rs_new CParticleSwoosh(mSlowBlueTailSwoosh, 0))
, mSlowBlueTailSwooshGen2(rs_new CParticleSwoosh(mSlowBlueTailSwoosh, 0))
, mSlowBlueTailSwoosh2Gen(rs_new CParticleSwoosh(mSlowBlueTailSwoosh2, 0))
, mSlowBlueTailSwoosh2Gen2(rs_new CParticleSwoosh(mSlowBlueTailSwoosh2, 0))
, mJaggyTrailGen(rs_new CParticleSwoosh(mJaggyTrail, 0))
, mSideSwooshGen(multiplayer ? nullptr : rs_new CParticleSwoosh(mSideSwoosh, 0))
, mSideSwooshGen2(multiplayer ? nullptr : rs_new CParticleSwoosh(mSideSwoosh, 0))
, mWallSparkGen(rs_new CElementGen(mWallSpark))
, mBallInnerGlowGen(rs_new CElementGen(mBallInnerGlow))
, mSpiderBallMagnetGen(rs_new CElementGen(mSpiderBallMagnet))
, mBoostBallGlowGen(rs_new CElementGen(mBoostBallGlow))
, mBoostEffectGen(nullptr)
, mMorphBallTransitionFlashGen(nullptr)
, mMorphBallIceBreakGen(nullptr)
, mDeathBallOuterShellGen(nullptr)
, mDeathBallSpikesGen(nullptr)
, mScrewAttackJumpFlashGen(nullptr)
, mScrewAttackWallJumpFlashGen(nullptr)
, mWakeEffectIndex(-1)
, mBallInnerGlowLight(kInvalidUniqueId)
, mBallLightActive(false)
, mWorldShadow(rs_new CWorldShadow(16, 16, false))
, mActorLights(
      rs_new CActorLights(8, CVector3f::Zero(), 4, 4, 0.1f, false, false, false, false))
, mRainSplashGen(rs_new CRainSplashGenerator(mBallModel->GetScale(), 40, 2, 0.15f, 0.5f))
, mTireFactor(0.f)
, mMaxTireFactor(0.5f)
, mTireInterpolationSpeed(1.f)
, mTireInterpolating(false)
, mBoostOverLightFactor(0.f)
, mBoostLightFactor(0.f)
, mSpiderLightFactor(0.f)
, mBallOrientationAverage(CQuaternion::NoRotation())
, mBallPositionAverage(CVector3f::Zero())
, mLiftSpeedAverage(0.f)
, mLiftControlForceAverage(CVector3f::Zero())
, mFailsafeCounter(0)
, mVelocityBeforeFailsafe(CVector3f::Zero())
, mVelocityAfterFailsafe(CVector3f::Zero())
, mBoostEnabled(true)
, mTouchedFloorDuringBoost(false)
, mBoostChargeTime(0.f)
, mTimeNotInBoost(1000.f)
, x1028_(0.f)
, mBoostDrainTime(0.f)
, mBoostEffectTime(0.f)
, mBoostDamageScale(1.f)
, mDisableSpiderBallTime(0.f)
, mHasSpiderBoostDirection(false)
, mBoostTrailFadeTimer(0.f)
, mInHalfPipeMode(false)
, mInHalfPipeModeInAir(false)
, mTouchedHalfPipeRecently(false)
, mBallCloseToCollision(false)
, mCloseToCollisionTime(0.f)
, mTouchHalfPipeCooldown(0.f)
, mDisableControlCooldown(0.f)
, mTouchedHalfPipeRecentCooldown(0.f)
, mPrevHalfPipeNormal(CVector3f::Zero())
, mHalfPipeNormal(CVector3f::Zero())
, mBallAnimationIndex(0)
, mRollSfxId(0xffff)
, mLandSfxId(0xffff)
, mWallSparkFrameCountdown(1)
, mEndScrewAttackRequested(false)
, mTouchingWall(false)
, mPendingRecoil(false)
, mRecoiling(false)
, mWallJumpInputPending(false)
, mCollidedDuringRecovery(false)
, x18a8_30_(false)
, mForcedScrewJumpInput(false)
, mScrewAttackJumpCount(0)
, mWallJumpCount(0)
, mScrewAttackExitAnimationFrames(0)
, mScrewAttackGroundedFrames(0)
, mTimeSinceScrewAttackJump(0.f)
, mWallContactTime(0.f)
, mScrewAttackRecoveryCollisionTime(0.f)
, mWallNormal(CVector3f::Zero())
, mScrewAttackDirection(CVector3f::Zero())
, mBoostState(kBBS_BoostAvailable)
, mBombJumpState(kBJS_BombJumpAvailable)
, mDamageEffect(0.f)
, mDamageEffectDecaySpeed(0.f)
, mDamageTime(0.f)
, mMultiplayer(multiplayer)
, mShadow(nullptr) {
  mSpiderBallMagnetGen->SetParticleEmission(false);
  mSpiderBallMagnetGen->Update(double(1.f / 60.f));
  sBallCloseToCollisionDistance = GetBallRadius() + 0.2f;
  InitializeWakeEffects();
  mDeathBallDamageCooldowns.reserve(16);
  // TODO: recover the single-player material preparation calls (see research notes).
  mPlayer.SetCollisionAccuracyModifier(5.f);
}
