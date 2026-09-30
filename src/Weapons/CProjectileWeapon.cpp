#include "Weapons/CProjectileWeapon.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Math/CRelAngle.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/Particles/CElementGen.hpp"
#include "Kyoto/Particles/CParticleGlobals.hpp"
#include "Kyoto/Particles/CParticleSwoosh.hpp"
#include "Kyoto/Particles/IElement.hpp"
#include "Weapons/CCollisionResponseData.hpp"
#include "Weapons/IWeaponRenderer.hpp"
#include "rstl/math.hpp"
#include <float.h>

uint CProjectileWeapon::skGlobalSeed = 99;
bool CProjectileWeapon::sDisableAlphaUpdates = false;

CProjectileWeapon::CProjectileWeapon(const TToken< CWeaponDescription >& description,
                                     const CVector3f& worldOffset, const CTransform4f& localToWorld,
                                     const CVector3f& scale, int flags)
: mWeaponDesc(description)
, mRandom(skGlobalSeed)
, mRenderRandom(skGlobalSeed)
, mLocalToWorldXf(localToWorld)
, mLocalXf(CTransform4f::Identity())
, mWorldOffset(worldOffset)
, mPreviousLocalOffset(CVector3f::Zero())
, mLocalOffset(CVector3f::Zero())
, mInterpolationOffset(CVector3f::Zero())
, mProjOffset(CVector3f::Zero())
, mScale(CVector3f::One())
, mLocalOffset2(CVector3f::Zero())
, mGlobalScale(scale)
, mVelocity(CVector3f::Zero())
, mGravity(CVector3f::Zero())
, mBillboard1Offset(CVector3f::Zero())
, mBillboard2Offset(CVector3f::Zero())
, mTrailOffset(CVector3f::Zero())
, mParticleTranslationOffset(CVector3f::Zero())
, mAmbientLightColor(CColor::White())
, mBillboard1Size(0.5f)
, mBillboard2Size(0.5f)
, mTrailSize(0.5f)
, mTrailLength(2.f)
, mCurTime(0.0)
, mRemainderTime(0.0)
, mMaxTurnRate(0.f)
, mFlags(flags)
, mLifetime(0)
, mChildSystemUpdateRate(0)
, mCurFrame(0)
, mLastParticleFrame(-1)
, x160_(0)
, mAPSMGen(nullptr)
, mAPS2Gen(nullptr)
, x16c_(nullptr)
, mSwoosh1(nullptr)
, mSwoosh2(nullptr)
, mSwoosh3(nullptr)
, mActive(true)
, mAPSO(false)
, mAP11(false)
, mAP21(false)
, mAS11(false)
, mAS12(false)
, mAS13(false)
, mVMD2(false)
, mHasBillboardEffects(false)
, mUseParticleTranslationOffset(false) {
  CGlobalRandom random(mRandom);
  mVMD2 = mWeaponDesc->mVMD2;
  mAPSO = mWeaponDesc->mAPSO;
  // Retail keeps the masked flag in a register across both CElementGen allocations;
  // writing (mFlags & 1) inline makes mwcc spill and reload mFlags for each one.
  const uint twoParticleFlags = mFlags & 1;
  if (mWeaponDesc->mAPSM) {
    mAPSMGen = rs_new CElementGen(*mWeaponDesc->mAPSM, CElementGen::kMOT_Normal,
                                  twoParticleFlags ? CElementGen::kOSF_Two : CElementGen::kOSF_One);
    mAPSMGen->SetGlobalScale(scale);
  }
  if (mWeaponDesc->mAPS2) {
    mAPS2Gen = rs_new CElementGen(*mWeaponDesc->mAPS2, CElementGen::kMOT_Normal,
                                  twoParticleFlags ? CElementGen::kOSF_Two : CElementGen::kOSF_One);
    mAPS2Gen->SetGlobalScale(scale);
  }
  if (mWeaponDesc->mASW1) {
    mSwoosh1 = rs_new CParticleSwoosh(*mWeaponDesc->mASW1, 0);
    mSwoosh1->SetGlobalScale(scale);
  }
  if (mWeaponDesc->mASW2) {
    mSwoosh2 = rs_new CParticleSwoosh(*mWeaponDesc->mASW2, 0);
    mSwoosh2->SetGlobalScale(scale);
  }
  if (mWeaponDesc->mASW3) {
    mSwoosh3 = rs_new CParticleSwoosh(*mWeaponDesc->mASW3, 0);
    mSwoosh3->SetGlobalScale(scale);
  }

  if (mWeaponDesc->mPSLT) {
    mWeaponDesc->mPSLT->GetValue(0, mLifetime);
  } else {
    mLifetime = 0x7fffff;
  }
  if (mWeaponDesc->mIVEC) {
    mWeaponDesc->mIVEC->GetValue(0, mVelocity);
  }
  if (mWeaponDesc->mIORN) {
    CTransform4f orientation = CTransform4f::Identity();
    CVector3f angles(0.f, 0.f, 0.f);
    mWeaponDesc->mIORN->GetValue(0, angles);
    orientation.RotateLocalX(CRelAngle::FromDegrees(angles.GetX()));
    orientation.RotateLocalY(CRelAngle::FromDegrees(angles.GetY()));
    orientation.RotateLocalZ(CRelAngle::FromDegrees(angles.GetZ()));
    SetRelativeOrientation(orientation);
  } else {
    SetRelativeOrientation(CTransform4f::Identity());
  }
  if (mWeaponDesc->GetOHEF()) {
    mModel = *mWeaponDesc->GetOHEF();
  }

  if (mWeaponDesc->mB1TX || mWeaponDesc->mB2TX || mWeaponDesc->mTTEX) {
    mHasBillboardEffects = true;
  }
  mAP11 = mWeaponDesc->mAP11;
  mAP21 = mWeaponDesc->mAP21;
  mAS11 = mWeaponDesc->mAS11;
  mAS12 = mWeaponDesc->mAS12;
  mAS13 = mWeaponDesc->mAS13;
  UpdateChildParticleSystems(1.f / 60.f, false);
  UpdateBillboardEffects();
}

CProjectileWeapon::~CProjectileWeapon() {
  delete mAPSMGen;
  delete mAPS2Gen;
  delete x16c_;
  delete mSwoosh1;
  delete mSwoosh2;
  delete mSwoosh3;
}

bool CProjectileWeapon::Update(float dt) {
  CGlobalRandom random(mRandom);
  double actualTime = mCurFrame * (1.0 / 60.0);
  mChildSystemUpdateRate = 0;
  double useDt = close_enough(dt, 1.f / 60.f, 1.6666666851961054e-5f) ? 1.0 / 60.0 : dt;
  // Retail scales the step here. The multiply is a no-op, but it is the only
  // reference to the 1.0 double in the unit, so dropping it loses a .sdata2 entry.
  double timeScale = 1.0;
  useDt *= timeScale;
  if (useDt < 0.0) {
    useDt = 0.0;
  }
  mCurTime += useDt;

  int lastUpdatedFrame = -1;
  while (actualTime < mCurTime && !close_enough(actualTime, mCurTime, 1.6666666666666667e-5)) {
    if (mCurFrame < mLifetime) {
      CParticleGlobals::SetEmitterTime(mCurFrame);
      CParticleGlobals::SetParticleLifetime(mLifetime);
      CParticleGlobals::UpdateParticleLifetimeTweenValues(mCurFrame);
      UpdatePSTranslationAndOrientation();
      lastUpdatedFrame = mCurFrame;
    }
    ++mCurFrame;
    ++mChildSystemUpdateRate;
    actualTime += 1.f / 60.f;
  }
  if (lastUpdatedFrame != -1) {
    int currentFrame = mCurFrame;
    mCurFrame = lastUpdatedFrame;
    UpdateBillboardEffects();
    mCurFrame = currentFrame;
  }

  if (close_enough(actualTime, mCurTime, 1.6666666666666667e-5)) {
    mCurTime = actualTime;
  }
  mRemainderTime = static_cast< float >((actualTime - mCurTime) / (1.0 / 60.0));
  mInterpolationOffset =
      static_cast< float >(mRemainderTime) * (mPreviousLocalOffset - mLocalOffset);
  if (mCurFrame < mLifetime) {
    mMaxTurnRate = 0.f;
    if (mWeaponDesc->mTRAT) {
      mWeaponDesc->mTRAT->GetValue(mCurFrame, mMaxTurnRate);
    }
  }
  return false;
}

void CProjectileWeapon::UpdateParticleFX() {
  if (mAPSMGen && mWeaponDesc->mSPS1) {
    mAPSMGen->Update(1.f / 60.f);
  }
  if (mAPS2Gen && mWeaponDesc->mSPS2) {
    mAPS2Gen->Update(1.f / 60.f);
  }
  for (int i = 0; i < mChildSystemUpdateRate; ++i) {
    UpdateChildParticleSystems(1.f / 60.f, false);
  }
}

const CTransform4f CProjectileWeapon::GetTransform() const { return mLocalToWorldXf * mLocalXf; }

const CVector3f CProjectileWeapon::GetTranslation() const {
  return mLocalToWorldXf * (mLocalOffset + mInterpolationOffset + mLocalXf * mProjOffset) +
         mWorldOffset + mParticleTranslationOffset;
}

void CProjectileWeapon::SetRelativeOrientation(const CTransform4f& orientation) {
  mLocalXf = orientation;
}

void CProjectileWeapon::SetWorldSpaceOrientation(const CTransform4f& orientation) {
  mLocalXf = mLocalToWorldXf.GetInverse() * orientation;
}

void CProjectileWeapon::UpdatePSTranslationAndOrientation() {
  if (mLifetime >= mCurFrame && mActive) {
    mPreviousLocalOffset = mLocalOffset;
    if (mWeaponDesc->mPSVM) {
      mWeaponDesc->mPSVM->GetValue(mCurFrame, mVelocity, mLocalOffset);
    }
    if (mVMD2) {
      const CVector3f velocity = mLocalXf * mVelocity;
      mLocalOffset += velocity;
    } else {
      mLocalOffset += mVelocity;
    }
    mVelocity += mGravity * (1.f / 60.f);

    if (mWeaponDesc->mPSOV) {
      CVector3f angles(0.f, 0.f, 0.f);
      mWeaponDesc->mPSOV->GetValue(mCurFrame, angles);
      CTransform4f orientation = mLocalXf;
      orientation.RotateLocalX(CRelAngle::FromDegrees(angles.GetX()));
      orientation.RotateLocalY(CRelAngle::FromDegrees(angles.GetY()));
      orientation.RotateLocalZ(CRelAngle::FromDegrees(angles.GetZ()));
      SetRelativeOrientation(orientation);
    }
    if (mWeaponDesc->mPSCL) {
      mWeaponDesc->mPSCL->GetValue(mCurFrame, mScale);
    }
    if (mWeaponDesc->mPCOL) {
      mWeaponDesc->mPCOL->GetValue(mCurFrame, mAmbientLightColor);
    }
    if (mWeaponDesc->mPOFS) {
      mWeaponDesc->mPOFS->GetValue(mCurFrame, mLocalOffset2);
    }
    if (mWeaponDesc->mOFST) {
      mWeaponDesc->mOFST->GetValue(mCurFrame, mProjOffset);
    }
  }
}

void CProjectileWeapon::UpdateChildParticleSystems(float dt, bool translationOnly) {
  double useDt = close_enough(dt, 1.f / 60.f, 1.6666666851961054e-5f) ? 1.0 / 60.0 : dt;
  // Retail materialises this || chain into a bool (li r3,0 / li r3,1 / clrlwi) before
  // branching on it, so it has to be a named value rather than an if-condition.
  const bool hasChildSystems = mAPSMGen || mAPS2Gen || mSwoosh1 || mSwoosh2 || mSwoosh3;
  if (hasChildSystems) {
    const CVector3f translation = GetTranslation() - mParticleTranslationOffset;
    const CVector3f globalTranslation =
        mUseParticleTranslationOffset ? translation + mParticleTranslationOffset : translation;
    const CTransform4f orientation = GetTransform();

    if (mAPSMGen) {
      if (mLastParticleFrame != mCurFrame || translationOnly) {
        if (mCurFrame > mLifetime) {
          mAPSMGen->SetParticleEmission(false);
          mAPSMGen->EndLifetime();
          if (mWeaponDesc->mDP1C) {
            mAPSMGen->DestroyParticles();
          }
        } else {
          if (mAP11) {
            mAPSMGen->SetGlobalTranslation(globalTranslation);
          } else {
            mAPSMGen->SetTranslation(translation);
            if (mUseParticleTranslationOffset) {
              mAPSMGen->SetGlobalTranslation(mParticleTranslationOffset);
            }
          }
          if (mAPSO) {
            mAPSMGen->SetOrientation(orientation);
          }
        }
      }
      if (!mWeaponDesc->mSPS1 && !translationOnly) {
        mAPSMGen->Update(useDt);
      }
      if (mAPSMGen->IsSystemDeletable() == TRUE) {
        delete mAPSMGen;
        mAPSMGen = nullptr;
      }
    }

    if (mAPS2Gen) {
      if (mLastParticleFrame != mCurFrame || translationOnly) {
        if (mCurFrame > mLifetime) {
          mAPS2Gen->SetParticleEmission(false);
          mAPS2Gen->EndLifetime();
          if (mWeaponDesc->mDP2C) {
            mAPS2Gen->DestroyParticles();
          }
        } else {
          if (mAP21) {
            mAPS2Gen->SetGlobalTranslation(globalTranslation);
          } else {
            mAPS2Gen->SetTranslation(translation);
            if (mUseParticleTranslationOffset) {
              mAPS2Gen->SetGlobalTranslation(mParticleTranslationOffset);
            }
          }
          if (mAPSO) {
            mAPS2Gen->SetOrientation(orientation);
          }
        }
      }
      if (!mWeaponDesc->mSPS2 && !translationOnly) {
        mAPS2Gen->Update(useDt);
      }
      if (mAPS2Gen->IsSystemDeletable() == TRUE) {
        delete mAPS2Gen;
        mAPS2Gen = nullptr;
      }
    }

    if (mSwoosh1) {
      if (mLastParticleFrame != mCurFrame || translationOnly) {
        if (mCurFrame > mLifetime) {
          mSwoosh1->SetParticleEmission(false);
        } else {
          if (mAS11) {
            mSwoosh1->SetGlobalTranslation(globalTranslation);
          } else {
            mSwoosh1->SetTranslation(translation);
            if (mUseParticleTranslationOffset) {
              mSwoosh1->SetGlobalTranslation(mParticleTranslationOffset);
            }
          }
          mSwoosh1->SetOrientation(orientation);
        }
      }
      if (!translationOnly) {
        mSwoosh1->SetWarmUp();
        mSwoosh1->Update(0.0);
      }
      if (mSwoosh1->IsSystemDeletable() == TRUE) {
        delete mSwoosh1;
        mSwoosh1 = nullptr;
      }
    }

    if (mSwoosh2) {
      if (mLastParticleFrame != mCurFrame || translationOnly) {
        if (mCurFrame > mLifetime) {
          mSwoosh2->SetParticleEmission(false);
        } else {
          if (mAS12) {
            mSwoosh2->SetGlobalTranslation(globalTranslation);
          } else {
            mSwoosh2->SetTranslation(translation);
            if (mUseParticleTranslationOffset) {
              mSwoosh2->SetGlobalTranslation(mParticleTranslationOffset);
            }
          }
          mSwoosh2->SetOrientation(orientation);
        }
      }
      if (!translationOnly) {
        mSwoosh2->SetWarmUp();
        mSwoosh2->Update(0.0);
      }
      if (mSwoosh2->IsSystemDeletable() == TRUE) {
        delete mSwoosh2;
        mSwoosh2 = nullptr;
      }
    }

    if (mSwoosh3) {
      if (mLastParticleFrame != mCurFrame || translationOnly) {
        if (mCurFrame > mLifetime) {
          mSwoosh3->SetParticleEmission(false);
        } else {
          if (mAS13) {
            mSwoosh3->SetGlobalTranslation(globalTranslation);
          } else {
            mSwoosh3->SetTranslation(translation);
            if (mUseParticleTranslationOffset) {
              mSwoosh3->SetGlobalTranslation(mParticleTranslationOffset);
            }
          }
          mSwoosh3->SetOrientation(orientation);
        }
      }
      if (!translationOnly) {
        mSwoosh3->SetWarmUp();
        mSwoosh3->Update(0.0);
      }
      if (mSwoosh3->IsSystemDeletable() == TRUE) {
        delete mSwoosh3;
        mSwoosh3 = nullptr;
      }
    }
  }

  if (x16c_ && !translationOnly) {
    x16c_->Update(useDt);
    if (x16c_->IsSystemDeletable() == TRUE) {
      delete x16c_;
      x16c_ = nullptr;
    }
  }
  mLastParticleFrame = mCurFrame;
}

void CProjectileWeapon::UpdateBillboardEffects() {
  if (!mHasBillboardEffects) {
    return;
  }
  CGlobalRandom random(mRandom);
  CParticleGlobals::SetEmitterTime(mCurFrame);
  CParticleGlobals::SetParticleLifetime(mLifetime);
  CParticleGlobals::UpdateParticleLifetimeTweenValues(mCurFrame);
  const CWeaponDescription& description = **mWeaponDesc;

  if (description.mB1TX) {
    if (description.mB1SE) {
      description.mB1SE->GetValue(mCurFrame, mBillboard1Size);
    }
    if (description.mB1PO) {
      description.mB1PO->GetValue(mCurFrame, mBillboard1Offset);
    }
  }
  if (description.mB2TX) {
    if (description.mB2SE) {
      description.mB2SE->GetValue(mCurFrame, mBillboard2Size);
    }
    if (description.mB2PO) {
      description.mB2PO->GetValue(mCurFrame, mBillboard2Offset);
    }
  }
  if (description.mTTEX) {
    if (description.mTSZE) {
      description.mTSZE->GetValue(mCurFrame, mTrailSize);
    }
    if (description.mTLEN) {
      description.mTLEN->GetValue(mCurFrame, mTrailLength);
    }
    if (description.mTLPO) {
      description.mTLPO->GetValue(mCurFrame, mTrailOffset);
    }
  }
}

const bool CProjectileWeapon::IsSystemDeletable() const {
  bool ret = true;
  if (mAPSMGen && !mAPSMGen->IsSystemDeletable()) {
    ret = false;
  } else if (mAPS2Gen && !mAPS2Gen->IsSystemDeletable()) {
    ret = false;
  } else if (mSwoosh1 && !mSwoosh1->IsSystemDeletable()) {
    ret = false;
  } else if (mSwoosh2 && !mSwoosh2->IsSystemDeletable()) {
    ret = false;
  } else if (mSwoosh3 && !mSwoosh3->IsSystemDeletable()) {
    ret = false;
  } else if (x16c_ && !x16c_->IsSystemDeletable()) {
    ret = false;
  } else if (mActive) {
    ret = mCurFrame >= mLifetime;
  }
  return ret;
}

void CProjectileWeapon::Render() const {
  if (mCurFrame <= mLifetime && mActive) {
    if (mModel) {
      const CTransform4f localXf = CTransform4f::Translate(mLocalOffset + mInterpolationOffset +
                                                           mLocalXf * mProjOffset + mLocalOffset2);
      const CTransform4f worldXf = CTransform4f::Translate(mWorldOffset);
      const CTransform4f scaleXf = CTransform4f::Scale(mScale.GetX(), mScale.GetY(), mScale.GetZ());
      CGraphics::SetModelMatrix(worldXf * mLocalToWorldXf * localXf * scaleXf * mLocalXf);
      CLight light = CLight::BuildLocalAmbient(CVector3f::Zero(), mAmbientLightColor);
      (*mModel)->Draw(CModelFlags(CModelFlags::kT_Opaque, 1.f));
    }
    RenderBillboardEffects();
  }
}

void CProjectileWeapon::AddToRenderer() const {
  if (mAPSMGen) {
    IWeaponRenderer::GetRenderer()->AddParticleGen(*mAPSMGen);
  }
  if (mAPS2Gen) {
    IWeaponRenderer::GetRenderer()->AddParticleGen(*mAPS2Gen);
  }
  if (mSwoosh1) {
    IWeaponRenderer::GetRenderer()->AddParticleGen(*mSwoosh1);
  }
  if (mSwoosh2) {
    IWeaponRenderer::GetRenderer()->AddParticleGen(*mSwoosh2);
  }
  if (mSwoosh3) {
    IWeaponRenderer::GetRenderer()->AddParticleGen(*mSwoosh3);
  }
  if (x16c_) {
    IWeaponRenderer::GetRenderer()->AddParticleGen(*x16c_);
  }
}

void CProjectileWeapon::RenderParticles() const {
  if (mAPSMGen) {
    mAPSMGen->Render();
  }
  if (mAPS2Gen) {
    mAPS2Gen->Render();
  }
  if (mSwoosh1) {
    mSwoosh1->Render();
  }
  if (mSwoosh2) {
    mSwoosh2->Render();
  }
  if (mSwoosh3) {
    mSwoosh3->Render();
  }
  if (x16c_) {
    x16c_->Render();
  }
}

void CProjectileWeapon::RenderBillboardEffects() const {
  if (!mHasBillboardEffects) {
    return;
  }

  CGlobalRandom random(mRenderRandom);
  CParticleGlobals::SetEmitterTime(mCurFrame);
  CParticleGlobals::SetParticleLifetime(mLifetime);
  CParticleGlobals::UpdateParticleLifetimeTweenValues(mCurFrame);
  const CWeaponDescription& description = **mWeaponDesc;
  CColor billboard1Color = CColor::Grey();
  CColor billboard2Color = CColor::Grey();
  CColor trailStartColor = CColor::Grey();
  CColor trailEndColor = CColor::Grey();
  // Retail's stores at the top of RenderBillboardEffects (0x80259E18) are
  //   lfs f1, lbl_8041DE24 (= 0.0f) / lfs f0, lbl_8041DE2C (= 1.0f)
  //   stfs f1,52(r1) / stfs f1,48(r1)          - the two rotations
  //   stfs f1,f0 pairs at {340,324,308}        - the three UV rects
  // so the rotations seed at 0.0f and each rect at {0, 0, 1, 1}. The rotations are
  // only overwritten when mB1RT/mB2RT exists and are then tested against 0.f below,
  // so the seed decides the rotated-view arm for a texture with no rotation element.
  float billboard1Rotation = 0.f;
  float billboard2Rotation = 0.f;
  // Each rect is still written as four separate stores rather than one aggregate
  // initialiser: the aggregate makes mwcc emit a .rodata literal, and the three
  // blobs it left were the only .rodata content retail does not have.
  SUVElementSet billboard1UV;
  SUVElementSet billboard2UV;
  SUVElementSet trailUV;
  billboard1UV.xMin = 0.f;
  billboard1UV.yMin = 0.f;
  billboard1UV.xMax = 1.f;
  billboard1UV.yMax = 1.f;
  billboard2UV.xMin = 0.f;
  billboard2UV.yMin = 0.f;
  billboard2UV.xMax = 1.f;
  billboard2UV.yMax = 1.f;
  trailUV.xMin = 0.f;
  trailUV.yMin = 0.f;
  trailUV.xMax = 1.f;
  trailUV.yMax = 1.f;

  if (description.mB1TX) {
    description.mB1TX->GetValueUV(mCurFrame, billboard1UV);
    if (description.mB1CL) {
      description.mB1CL->GetValue(mCurFrame, billboard1Color);
    }
    if (description.mB1RT) {
      description.mB1RT->GetValue(mCurFrame, billboard1Rotation);
    }
  }
  if (description.mB2TX) {
    description.mB2TX->GetValueUV(mCurFrame, billboard2UV);
    if (description.mB2CL) {
      description.mB2CL->GetValue(mCurFrame, billboard2Color);
    }
    if (description.mB2RT) {
      description.mB2RT->GetValue(mCurFrame, billboard2Rotation);
    }
  }
  if (description.mTTEX) {
    description.mTTEX->GetValueUV(mCurFrame, trailUV);
    if (description.mTSCL) {
      description.mTSCL->GetValue(mCurFrame, trailStartColor);
    }
    if (description.mTECL) {
      description.mTECL->GetValue(mCurFrame, trailEndColor);
    }
  }

  const CTransform4f orientation = GetTransform();
  CVector3f direction = orientation * GetVelocity();
  const float speed = direction.Magnitude();
  // Retail tests `speed > FLT_EPSILON` and puts the reset arm out of line; the other
  // order makes mwcc emit a cror + bne where retail has a plain ble.
  if (speed > FLT_EPSILON) {
    direction *= 1.f / speed;
  } else {
    direction = CVector3f(0.f, 1.f, 0.f);
  }
  const CVector3f position = GetTranslation();
  const CTransform4f scale = CTransform4f::Scale(mGlobalScale);

  CGraphics::SetUseNormalMatrix(false);
  static CTevCombiners::CTevPass pass(
      CTevCombiners::ColorPass(CTevCombiners::kCS_Zero, CTevCombiners::kCS_RasterColor,
                               CTevCombiners::kCS_TextureColor, CTevCombiners::kCS_Zero),
      CTevCombiners::AlphaPass(CTevCombiners::kAS_Zero, CTevCombiners::kAS_RasterAlpha,
                               CTevCombiners::kAS_TextureAlpha, CTevCombiners::kAS_Zero));
  pass.SetColorScale(CTevCombiners::kTS_Scale2);
  CGraphics::SetTevOp(kTS_Stage0, pass);
  CGraphics::SetTevOp(kTS_Stage1, CGraphics::kEnvPassthru);
  CGraphics::SetDepthWriteMode(true, kE_LEqual, false);
  CGraphics::SetCullMode(kCM_None);
  CGraphics::SetAlphaCompare(kAF_Greater, 0, kAO_And, kAF_Always, 0);
  if (sDisableAlphaUpdates) {
    GXSetAlphaUpdate(GX_FALSE);
  }

  if (description.mB1TX) {
    CGraphics::SetModelMatrix(
        CTransform4f::Translate(position + orientation.Rotate(mBillboard1Offset)) * scale);
    CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, description.mRB1A ? kBF_One : kBF_InvSrcAlpha,
                            kLO_Clear);
    CVector3f up = mBillboard1Size * CGraphics::GetViewMatrix().GetUp();
    CVector3f right = mBillboard1Size * CGraphics::GetViewMatrix().GetRight();
    if (!close_enough(billboard1Rotation, 0.f)) {
      CTransform4f rotatedView = CGraphics::GetViewMatrix();
      rotatedView.RotateLocalY(CRelAngle::FromDegrees(billboard1Rotation));
      up = mBillboard1Size * rotatedView.GetUp();
      right = mBillboard1Size * rotatedView.GetRight();
    }
    description.mB1TX->GetValueTexture(mCurFrame)->Load(GX_TEXMAP0, CTexture::kCM_Clamp);

    CGraphics::StreamBegin(kP_Quads);
    CGraphics::StreamColor(billboard1Color);
    CGraphics::StreamTexcoord(billboard1UV.xMax, billboard1UV.yMax);
    CVector3f vertex = 0.5f * up + 0.5f * right;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(billboard1UV.xMin, billboard1UV.yMax);
    vertex -= right;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(billboard1UV.xMin, billboard1UV.yMin);
    vertex -= up;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(billboard1UV.xMax, billboard1UV.yMin);
    vertex += right;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamEnd();
  }

  if (description.mB2TX) {
    CGraphics::SetModelMatrix(
        CTransform4f::Translate(position + orientation.Rotate(mBillboard2Offset)) * scale);
    CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, description.mRB2A ? kBF_One : kBF_InvSrcAlpha,
                            kLO_Clear);
    CVector3f up = mBillboard2Size * CGraphics::GetViewMatrix().GetUp();
    CVector3f right = mBillboard2Size * CGraphics::GetViewMatrix().GetRight();
    if (!close_enough(billboard2Rotation, 0.f)) {
      CTransform4f rotatedView = CGraphics::GetViewMatrix();
      rotatedView.RotateLocalY(CRelAngle::FromDegrees(billboard2Rotation));
      up = mBillboard2Size * rotatedView.GetUp();
      right = mBillboard2Size * rotatedView.GetRight();
    }
    description.mB2TX->GetValueTexture(mCurFrame)->Load(GX_TEXMAP0, CTexture::kCM_Clamp);

    CGraphics::StreamBegin(kP_Quads);
    CGraphics::StreamColor(billboard2Color);
    CGraphics::StreamTexcoord(billboard2UV.xMax, billboard2UV.yMax);
    CVector3f vertex = 0.5f * up + 0.5f * right;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(billboard2UV.xMin, billboard2UV.yMax);
    vertex -= right;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(billboard2UV.xMin, billboard2UV.yMin);
    vertex -= up;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(billboard2UV.xMax, billboard2UV.yMin);
    vertex += right;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamEnd();
  }

  if (description.mTTEX) {
    CGraphics::SetBlendMode(kBM_Blend, kBF_SrcAlpha, description.mRTLA ? kBF_One : kBF_InvSrcAlpha,
                            kLO_Clear);
    CGraphics::SetModelMatrix(CTransform4f::Translate(position + orientation.Rotate(mTrailOffset)) *
                              scale);
    description.mTTEX->GetValueTexture(mCurFrame)->Load(GX_TEXMAP0, CTexture::kCM_Clamp);

    const CVector3f toCamera = position - CGraphics::GetViewMatrix().GetTranslation();
    CVector3f width = CVector3f::Cross(toCamera, direction);
    const float widthSquared = width.MagSquared();
    if (widthSquared > FLT_EPSILON) {
      width *= 0.5f * mTrailSize * CMath::FastInvSqrtF(widthSquared);
    }
    const CVector3f length = mTrailLength * (mTrailSize * direction);

    CGraphics::StreamBegin(kP_Quads);
    CGraphics::StreamColor(trailStartColor);
    CGraphics::StreamTexcoord(trailUV.xMax, trailUV.yMax);
    CVector3f vertex = 0.5f * width;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(trailUV.xMin, trailUV.yMax);
    vertex -= length;
    CGraphics::StreamColor(trailEndColor);
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(trailUV.xMin, trailUV.yMin);
    vertex -= width;
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamTexcoord(trailUV.xMax, trailUV.yMin);
    vertex += length;
    CGraphics::StreamColor(trailStartColor);
    CGraphics::StreamVertex(vertex);
    CGraphics::StreamEnd();
  }

  CGraphics::SetCullMode(kCM_Front);
  if (sDisableAlphaUpdates) {
    GXSetAlphaUpdate(GX_TRUE);
  }
  CGraphics::SetUseNormalMatrix(true);
}

void CProjectileWeapon::DeactivateProjectile() {
  mActive = false;
  if (mAPSMGen) {
    mAPSMGen->SetParticleEmission(false);
    if (mWeaponDesc->mDP1C) {
      mAPSMGen->DestroyParticles();
    }
  }
  if (mAPS2Gen) {
    mAPS2Gen->SetParticleEmission(false);
    if (mWeaponDesc->mDP2C) {
      mAPS2Gen->DestroyParticles();
    }
  }
  if (mSwoosh1) {
    mSwoosh1->SetParticleEmission(false);
  }
  if (mSwoosh2) {
    mSwoosh2->SetParticleEmission(false);
  }
  if (mSwoosh3) {
    mSwoosh3->SetParticleEmission(false);
  }
}

rstl::optional_object< TLockedToken< CGenDescription > >
CProjectileWeapon::CollisionOccured(EWeaponCollisionResponseTypes type, bool deflected,
                                    bool useTarget, bool keepActive, const CVector3f& position,
                                    const CVector3f& normal, const CVector3f& target) {
  mLocalOffset = mLocalToWorldXf.TransposeRotate(position - mWorldOffset) - mProjOffset;
  mInterpolationOffset = CVector3f::Zero();

  if (deflected) {
    const CVector3f toTarget = target - GetTranslation();
    if (useTarget && toTarget.CanBeNormalized()) {
      SetWorldSpaceOrientation(CTransform4f::LookAt(CVector3f::Zero(), toTarget.AsNormalized()));
    } else {
      // Retail binds a reference to the column of the GetTransform() temporary, which
      // forces that temporary to be spilled; taking it by value lets mwcc drop the
      // spill and every local in the function moves down by 12 bytes.
      const CVector3f& col = GetTransform().GetColumn(kDY);
      const CTransform4f lookXf = CTransform4f::LookAt(
          CVector3f::Zero(), col - ((CVector3f::Dot(normal, col) * 2.f) * normal), normal);
      SetWorldSpaceOrientation(lookXf);
    }
    return rstl::optional_object_null();
  }

  if (!keepActive) {
    DeactivateProjectile();
  }
  if (!mWeaponDesc->GetCollisionResponse() || mCurFrame < x160_) {
    return rstl::optional_object_null();
  }
  TToken< CCollisionResponseData > response = *mWeaponDesc->GetCollisionResponse();
  return response->GetParticleDescription(type);
}

uint CProjectileWeapon::GetSoundIdForCollision(EWeaponCollisionResponseTypes type) const {
  if (!mWeaponDesc->GetCollisionResponse()) {
    return uint(-1);
  }
  TToken< CCollisionResponseData > response = *mWeaponDesc->GetCollisionResponse();
  return response->GetSoundEffectId(type);
}

rstl::optional_object< TLockedToken< CDecalDescription > >
CProjectileWeapon::GetDecalForCollision(EWeaponCollisionResponseTypes type) const {
  if (!mWeaponDesc->GetCollisionResponse()) {
    return rstl::optional_object_null();
  }
  TToken< CCollisionResponseData > response = *mWeaponDesc->GetCollisionResponse();
  return response->GetDecalDescription(type);
}

float CProjectileWeapon::GetAudibleRange() const {
  if (!mWeaponDesc->GetCollisionResponse()) {
    return 0.f;
  }
  TToken< CCollisionResponseData > response = *mWeaponDesc->GetCollisionResponse();
  return response->GetAudibleRange();
}

float CProjectileWeapon::GetAudibleFallOff() const {
  if (!mWeaponDesc->GetCollisionResponse()) {
    return 0.f;
  }
  TToken< CCollisionResponseData > response = *mWeaponDesc->GetCollisionResponse();
  return response->GetAudibleFallOff();
}

float CProjectileWeapon::GetMaxTurnRate() const { return mMaxTurnRate; }

void CProjectileWeapon::SetVelocity(const CVector3f& velocity) { mVelocity = velocity; }

const CVector3f& CProjectileWeapon::GetVelocity() const { return mVelocity; }

void CProjectileWeapon::SetGravity(const CVector3f& gravity) { mGravity = gravity; }

const CVector3f& CProjectileWeapon::GetGravity() const { return mGravity; }

void CProjectileWeapon::SetGlobalSeed(uint seed) { skGlobalSeed = seed; }

rstl::optional_object< CAABox > CProjectileWeapon::GetBounds() const {
  CAABox result = CAABox::MakeMaxInvertedBox();
  bool hasBounds = false;

  if (mAPSMGen) {
    rstl::optional_object< CAABox > bounds = mAPSMGen->GetBounds();
    if (bounds) {
      // Retail keeps the optional's value address in r31 across both AccumulateBounds
      // calls (`addi r31,r1,320` then `mr r4,r31` / `addi r4,r31,12`); going through
      // `bounds->` makes mwcc recompute the address each time.
      CAABox& b = *bounds;
      result.AccumulateBounds(b.GetMinPoint());
      result.AccumulateBounds(b.GetMaxPoint());
      hasBounds = true;
    }
  }

  if (mAPS2Gen) {
    rstl::optional_object< CAABox > bounds = mAPS2Gen->GetBounds();
    if (bounds) {
      CAABox& b = *bounds;
      result.AccumulateBounds(b.GetMinPoint());
      result.AccumulateBounds(b.GetMaxPoint());
      hasBounds = true;
    }
  }

  if (mSwoosh1) {
    rstl::optional_object< CAABox > bounds = mSwoosh1->GetBounds();
    if (bounds) {
      CAABox& b = *bounds;
      result.AccumulateBounds(b.GetMinPoint());
      result.AccumulateBounds(b.GetMaxPoint());
      hasBounds = true;
    }
  }

  if (mSwoosh2) {
    rstl::optional_object< CAABox > bounds = mSwoosh2->GetBounds();
    if (bounds) {
      CAABox& b = *bounds;
      result.AccumulateBounds(b.GetMinPoint());
      result.AccumulateBounds(b.GetMaxPoint());
      hasBounds = true;
    }
  }

  if (mSwoosh3) {
    rstl::optional_object< CAABox > bounds = mSwoosh3->GetBounds();
    if (bounds) {
      CAABox& b = *bounds;
      result.AccumulateBounds(b.GetMinPoint());
      result.AccumulateBounds(b.GetMaxPoint());
      hasBounds = true;
    }
  }

  if (mHasBillboardEffects) {
    float size = 0.f;
    float offsetSquared = 0.f;
    if (mWeaponDesc->mB1TX) {
      size = mBillboard1Size;
      offsetSquared = mBillboard1Offset.MagSquared();
    }
    if (mWeaponDesc->mB2TX) {
      size = rstl::max_val(size, mBillboard2Size);
      offsetSquared = rstl::max_val(offsetSquared, mBillboard2Offset.MagSquared());
    }
    if (mWeaponDesc->mTTEX) {
      size = rstl::max_val(size, mTrailSize);
      size = rstl::max_val(size, mTrailLength);
      offsetSquared = rstl::max_val(offsetSquared, mTrailOffset.MagSquared());
    }
    const float scale =
        rstl::max_val(rstl::max_val(mGlobalScale.GetX(), mGlobalScale.GetY()), mGlobalScale.GetZ());
    const float radius = (size + CMath::FastSqrtF(offsetSquared)) * scale;
    const CVector3f extent(radius, radius, radius);
    const CVector3f center = GetTranslation();
    // Retail builds the corners into one CAABox and accumulates its two points:
    // `__ct__6CAABoxFRC9CVector3fRC9CVector3f` into 44(r1), then AccumulateBounds
    // with 44(r1) and 56(r1). Accumulating the two corners directly instead leaves
    // `center` live across the first call and forces f28-f31 to be saved.
    const CAABox box(center - extent, center + extent);
    result.AccumulateBounds(box.GetMinPoint());
    result.AccumulateBounds(box.GetMaxPoint());
    hasBounds = true;
  }
  if (hasBounds) {
    return result;
  }
  return rstl::optional_object_null();
}

float CProjectileWeapon::GetTickTime() { return 1.f / 60.f; }

void CProjectileWeapon::SetParticleTranslationOffset(const CVector3f& offset) {
  mUseParticleTranslationOffset = true;
  mParticleTranslationOffset = offset;
  UpdateChildParticleSystems(1.f / 60.f, true);
}
