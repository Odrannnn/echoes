#ifndef _CANIMDATA
#define _CANIMDATA

// TODO: check for Echoes

#include "Kyoto/Math/CAABox.hpp"
#include "rstl/optional_object.hpp"
#include "types.h"

#include "Kyoto/Animation/CBoolPOINode.hpp"
#include "Kyoto/Animation/CCharacterInfo.hpp"
#include "Kyoto/Animation/CInt32POINode.hpp"
#include "Kyoto/Animation/CParticlePOINode.hpp"
#include "Kyoto/Animation/CSoundPOINode.hpp"
#include "MetroidPrime/ActorCommon.hpp"
#include "MetroidPrime/CAdditiveAnimPlayback.hpp"
#include "MetroidPrime/CAnimPlaybackParms.hpp"
#include "MetroidPrime/CHierarchyPoseBuilder.hpp"
#include "MetroidPrime/CParticleDatabase.hpp"
#include "MetroidPrime/CPoseAsTransforms.hpp"

#include "Kyoto/Animation/CCharAnimTime.hpp"
#include "Kyoto/Animation/CSkinnedModel.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/reserved_vector.hpp"
#include "rstl/set.hpp"

class CAnimationManager;
class CAnimSysContext;
class CAnimTreeNode;
class CCharacterFactory;
class CCharLayoutInfo;
class CSkinnedModel;
class CSkinnedModelWithAvgNormals;
class CTransitionManager;
class CVertexMorphEffect;
class CModelFlags;
class CPrimitive;

class CAnimData {
public:
  ~CAnimData();

  enum EAnimDir {
    kAD_Forward,
    kAD_Backward,
  };

  void PreRender();
  void EnableLooping(bool v) {
    x220_25_loop = v;
    x220_24_animating = true;
  }

  const TLockedToken< CSkinnedModel >& GetModelData() const { return xd8_modelData; }

  void SetIsAnimating(bool v) { x220_24_animating = v; }
  void SetParticleEffectState(const rstl::string& name, const bool active, CStateManager& mgr);

  int GetCharacterIndex() const { return x204_charIdx; }
  float GetAverageVelocity(int idx) const;

  const CBoolPOINode* GetBoolPOIList(int& count) const {
    count = x20c_passedBoolCount;
    return mBoolPOINodes.data();
  }
  const CInt32POINode* GetInt32POIList(int& count) const {
    count = x210_passedIntCount;
    return mInt32POINodes.data();
  }
  const CParticlePOINode* GetParticlePOIList(int& count) const {
    count = x214_passedParticleCount;
    return mParticlePOINodes.data();
  }
  const CSoundPOINode* GetSoundPOIList(int& count) const {
    count = x218_passedSoundCount;
    return mSoundPOINodes.data();
  }
  CParticleDatabase& GetParticleDB() { return x178_particleDB; }
  const CParticleDatabase& GetParticleDB() const { return x178_particleDB; }
  // SetIsAnimating__9CAnimDataFb
  // SetAnimDir__9CAnimDataFQ29CAnimData8EAnimDir
  CAABox GetBoundingBox() const;
  // GetBoundingBox__9CAnimDataCFRC12CTransform4f
  // GetLocatorSegId__9CAnimDataCFRCQ24rstl66basic_string
  // ResetPOILists__9CAnimDataFv
  // GetAverageVelocity__9CAnimDataCFi
  // AdvanceParticles__9CAnimDataFRC12CTransform4ffRC9CVector3fR13CStateManager
  // PoseSkinnedModel__9CAnimDataCFRC13CSkinnedModelRC17CPoseAsTransformsRCQ24rstl37optional_object<18CVertexMorphEffect>PCf
  // DrawSkinnedModel__9CAnimDataCFRC13CSkinnedModelRC11CModelFlags
  // InitializeCache__9CAnimDataFv
  // FreeCache__9CAnimDataFv
  // SetInfraModel__9CAnimDataFRC21TLockedToken<6CModel>RC26TLockedToken<10CSkinRules>
  // SetXRayModel__9CAnimDataFRC21TLockedToken<6CModel>RC26TLockedToken<10CSkinRules>
  // AdvanceAnim__9CAnimDataFR13CCharAnimTimeR9CVector3fR11CQuaternion
  // AdvanceIgnoreParticles__9CAnimDataFfR9CRandom16b
  // Advance__9CAnimDataFfRC9CVector3fR13CStateManagerb
  // DoAdvance__9CAnimDataFfRbR9CRandom16b
  void SetAnimation(const CAnimPlaybackParms& parms, bool noTrans);
  void GetAnimationPrimitives(const CAnimPlaybackParms& parms,
                              rstl::set< CPrimitive >& primsOut) const;
  // PrimitiveSetToTokenVector__9CAnimDataFRCQ24rstl72set<10CPrimitive,Q24rstl18less<10CPrimitive>,Q24rstl17rmemory_allocator>RQ24rstl42vector<6CToken,Q24rstl17rmemory_allocator>b
  // BuildPose__9CAnimDataFv
  // PreRender__9CAnimDataFv
  // SetupRender__9CAnimDataCFRC13CSkinnedModelRCQ24rstl37optional_object<18CVertexMorphEffect>PCf
  // Render__9CAnimDataCFRC13CSkinnedModelRC11CModelFlagsRCQ24rstl37optional_object<18CVertexMorphEffect>PCf
  void Render(const CSkinnedModel&, const CModelFlags&,
              const rstl::optional_object< CVertexMorphEffect >&, const float*) const;
  // RenderAuxiliary__9CAnimDataCFRC14CFrustumPlanes
  // RecalcPoseBuilder__9CAnimDataCFPC13CCharAnimTime
  float GetAnimationDuration(int animIn) const;
  float GetAnimTimeRemaining(const rstl::string& name) const;
  // IsAnimTimeRemaining__9CAnimDataCFfRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>
  bool IsAnimTimeRemaining(float, const rstl::string&) const;
  // GetLocatorTransform__9CAnimDataCFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>PC13CCharAnimTime
  // GetLocatorTransform__9CAnimDataCF6CSegIdPC13CCharAnimTime
  // CalcPlaybackAlignmentParms__9CAnimDataFRC18CAnimPlaybackParmsRCQ24rstl25ncrc_ptr<13CAnimTreeNode>
  // SetRandomPlaybackRate__9CAnimDataFR9CRandom16
  void SetPlaybackRate(float set);
  void MultiplyPlaybackRate(float scale);
  CCharAnimTime GetTimeOfUserEvent(EUserEventType type, const CCharAnimTime& time) const;
  // GetAdvancementDeltas__9CAnimDataCFRC13CCharAnimTimeRC13CCharAnimTime
  // Touch__9CAnimDataCFRC13CSkinnedModeli
  void InitializeEffects(CStateManager&, TAreaId, const CVector3f&);
  // SetPhase__9CAnimDataFf -> SetPhase__11IAnimReaderFf
  void SetPhase(float ph);
  void AddAdditiveAnimation(uint idx, float weight, bool active, bool fadeOut);
  void DelAdditiveAnimation(uint idx);
  bool IsAdditiveAnimation(uint idx) const;
  const rstl::rc_ptr< CAnimTreeNode >& GetAdditiveAnimationTree(uint idx) const;
  // GetAnimationTree__9CAnimDataCFv
  // AnimationTree__9CAnimDataFv
  // IsAdditiveAnimation__9CAnimDataCFUi
  bool IsAdditiveAnimationAdded(uint idx) const;
  // UpdateAdditiveAnims__9CAnimDataFf
  // AdvanceAdditiveAnims__9CAnimDataFf
  // AddAdditiveSegData__9CAnimDataCFRC10CSegIdListR16CSegStatementSet
  int GetEventResourceIdForAnimResourceId(int id) const;
  // GetAnimationManager__9CAnimDataFv
  // SetPoseValid__9CAnimDataFb

  float GetAdditiveAnimationWeight(uint idx);

  short GetCurrentAnimation() const { return x208_currentAnim; }
  const CCharacterInfo& GetCharacterInfo() const { return xc_charInfo; }
  // GetCharLayoutInfo__9CAnimDataCFv
  // GetDeltaRotation__9CAnimDataCFv
  // GetDeltaOffset__9CAnimDataCFv
  // IsDeltaOffsetInUse__9CAnimDataCFv
  // GetAdvancementDeltas__19CAdvancementResultsCFv
  // SetDeltaRotation__9CAnimDataFRC11CQuaternionb
  // SetDeltaOffset__9CAnimDataFRC9CVector3fb
  // SetDeltaOffsetInUse__9CAnimDataFv
  // IsDeltaRotationInUse__9CAnimDataCFv
  // IsDeltaOffsetPrimed__9CAnimDataCFv
  // GetAnimDir__9CAnimDataCFv
  // GetIsLoop__9CAnimDataCFv
  // IsAnimating__9CAnimDataCFv
  // SetPoseBuilderValid__9CAnimDataFb
  // GetAnimationManager__9CAnimDataCFv
  // GetPoseValid__9CAnimDataCFv
  // GetPoseBuilderValid__9CAnimDataCFv
  // GetAnimSysContext__9CAnimDataCFv
  // CacheInt32PoiList__9CAnimDataFRC13CCharAnimTimeiRCQ24rstl25ncrc_ptr<13CAnimTreeNode>

  // GetIceModel__9CAnimDataCFv
  const CPASDatabase& GetPASDatabase() const { return xc_charInfo.GetPASDatabase(); }
  // EnableLooping__9CAnimDataFb
  // GetSkinnedModel__9CAnimDataCFv
  // GetXRayModel__9CAnimDataCFv
  // GetInfraModel__9CAnimDataCFv
  // GetPose__9CAnimDataCFv
  // PoseBuilder__9CAnimDataCFv
  // GetPlaybackRate__9CAnimDataCFv
  // Pose__9CAnimDataFv
  // GetPoseBuilder__9CAnimDataCFv

  // CacheSoundPoiList__9CAnimDataFRCQ24rstl25ncrc_ptr<13CAnimTreeNode>RC13CCharAnimTimei
  // CacheParticlePoiList__9CAnimDataFRCQ24rstl25ncrc_ptr<13CAnimTreeNode>RC13CCharAnimTimei
  // CacheBoolPoiList__9CAnimDataFRCQ24rstl25ncrc_ptr<13CAnimTreeNode>RC13CCharAnimTimei
  // CacheInt32PoiList__9CAnimDataFRCQ24rstl25ncrc_ptr<13CAnimTreeNode>RC13CCharAnimTimei

  static void InitializeCache();
  static void FreeCache();

private:
  // Every offset below is measured with mwcceppc, not assumed - see the note at the bottom of
  // this class. 0xC0 of the class is `CCharacterInfo`.
  TLockedToken< CCharacterFactory > x0_charFactory; //!< 0x000
  CCharacterInfo xc_charInfo;                        //!< 0x00C, 0xC0 bytes
  TLockedToken< CCharLayoutInfo > xcc_layoutData;    //!< 0x0CC
  TLockedToken< CSkinnedModel > xd8_modelData;        //!< 0x0D8
  rstl::optional_object< TLockedToken< CSkinnedModelWithAvgNormals > > xe4_iceModelData;
  rstl::rc_ptr< CSkinnedModel > xf4_xrayModel;
  rstl::rc_ptr< CSkinnedModel > xf8_infraModel;
  rstl::rc_ptr< CAnimSysContext > xfc_animCtx;
  rstl::rc_ptr< CAnimationManager > x100_animMgr;
  EAnimDir x104_animDir;
  CAABox x108_aabb;
  uchar x120_unk[0x58]; // 0x130. Echoes added 0x58 here. **The particle DB is at 0x188, not
                        // 0x178** - the name was 0x10 low, and `CParticleDatabase` is 0xE0.
  CParticleDatabase x178_particleDB; //!< 0x188
  CAssetId x1d8_selfId;              //!< 0x268, not 0x1D8 - the name was 0x90 low
  CVector3f x1dc_alignPos;           //!< 0x26C
  CQuaternion x1e8_alignRot;         //!< 0x278
  rstl::rc_ptr< CAnimTreeNode > x1f8_animRoot;     //!< 0x288
  rstl::rc_ptr< CTransitionManager > x1fc_transMgr; //!< 0x290
  float x200_speedScale;                            //!< 0x298
  int x204_charIdx;                                 //!< 0x29C
  short x208_currentAnim;                           //!< 0x2A0
  short x20a_padding;                               //!< 0x2A2
  int x20c_passedBoolCount;                         //!< 0x2A4
  int x210_passedIntCount;                          //!< 0x2A8
  int x214_passedParticleCount;                     //!< 0x2AC
  int x218_passedSoundCount;                        //!< 0x2B0
  int x21c_particleLightIdx;                        //!< 0x2B4
  uchar x220_24_animating : 1;                      //!< the flag byte is 0x2BC
  uchar x220_25_loop : 1;
  uchar x220_26_aligningPos : 1;
  uchar x220_27_ : 1;
  uchar x220_28_ : 1;
  uchar x220_29_animationJustStarted : 1;
  uchar x220_30_poseBuilt : 1;
  uchar x220_31_poseCached : 1;
  CPoseAsTransforms x224_pose;
  CHierarchyPoseBuilder x2fc_poseBuilder;
  CAnimPlaybackParms x40c_playbackParms;  //!< 0x4A4
  rstl::reserved_vector< rstl::pair< int, CAdditiveAnimPlayback >, 8 > x434_additiveAnims; //!< 0x4CC

  static rstl::reserved_vector< CBoolPOINode, 8 > mBoolPOINodes;
  static rstl::reserved_vector< CInt32POINode, 16 > mInt32POINodes;
  static rstl::reserved_vector< CParticlePOINode, 20 > mParticlePOINodes;
  static rstl::reserved_vector< CSoundPOINode, 20 > mSoundPOINodes;
  // in cpp -> rstl::reserved_vector< CInt32POINode, 16 > sInt32TransientCache;
};
// CHECK_SIZEOF(CAnimData, 0x434 + 0x144)

// **Measured 2026-09-26 with mwcceppc's own flags: `sizeof(CAnimData)` is 0x630, not 0x578.** The
// `0x434 + 0x144` above is 0x578, so the class is 0xB8 = 184 bytes larger than the only figure
// this header ever claimed, and `x434_additiveAnims` - the member that figure is built from - is
// really at 0x4CC. Two member *names* are also below where their members are: `x178_particleDB`
// is at 0x188 and `x1d8_selfId` at 0x268. Every other name from `xcc_layoutData` (0x0CC) to
// `x40c_playbackParms` (0x4A4) is at retail's offset, and the ladder is self-consistent, so the
// drift is in the tail, not scattered.
//
// Two consequences, both measured rather than argued:
//
//   * **No `CAnimData` unit can be `Matching` until retail's size is settled.** A constructor or
//     destructor of the wrong size cannot claim a range.
//   * `CModelDataModelSlots.cpp` reads three holders at 0x118, 0x13C and 0x144 through
//     `CModelData`'s `xc_animData.x4_item`, and **0x118 in this class is `x108_aabb`**, a
//     `CAABox`. So retail's three holders are not where this class puts its own, which is the
//     same 0x10-scale disagreement seen from the other side.
#endif // _CANIMDATA
