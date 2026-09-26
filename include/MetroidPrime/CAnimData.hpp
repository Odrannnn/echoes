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

  // The skinned model in retail is an `rc_ptr` at 0x13C/0x144 (see the ladder at the bottom); the old
  // `TLockedToken` at "0xd8" never existed.
  const CSkinnedModel* GetModelData() const { return x13c_xrayModel.GetPtr(); }

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
  // **The whole ladder below is retail layout, read off two instructions per member, and the evidence
  // is at the bottom of this class. Do not "fix" a member from the output of mwcceppc - that is
  // what made this class 0x78 too long and cost three generations of names.**
  TLockedToken< CCharacterFactory > x0_charFactory; //!< 0x000, 0x0C: ctor `stw ...,8(r25)` @0x8002D1DC
  // The retail member at 0x00C is 0xF8 bytes, not 0xC0: one constructor call (`addi r3,r25,12` @
  // 0x8002D1E8) and one destructor call (`addi r3,r30,12` @0x8002C5EC) cover the whole
  // 0x00C..0x104 range, so there is **no separate member at 0x0CC**. The 0x38 that was missing
  // was `CCharacterInfo`'s: its `CParticleResData` is **six** `vector<CAssetId>` (0x60, not 0x40)
  // and it has three members above Prime 1's last one. `CCharacterInfo` is now
  // `CHECK_SIZEOF(..., 0xf8)` and the `xcc_unk[0x38]` pad this line used to carry is **gone** -
  // `sizeof(CAnimData)` is unchanged at 0x5B8 because the pad's 0x38 moved into the class it
  // was standing in for. The full ladder is in `Kyoto/Animation/CCharacterInfo.hpp`.
  CCharacterInfo xc_charInfo; //!< 0x00C, 0xF8
  // 0x104 is a `TLockedToken` (the ctor calls `Lock()`); 0x110 is a bare `CToken` and the ctor
  // does **not** - it calls `GetObj` instead. That is a 4-byte difference, and it is the only
  // thing between this ladder and the retail one: with a `TLockedToken` at 0x110 every
  // member from 0x118 up lands 4 bytes high and `sizeof` comes out 0x5BC. Measured.
  TLockedToken< CModel > x104_modelData; //!< 0x104, 0x0C: ctor `addi r16,r25,260` @0x8002D1F0
  CToken x110_charCtx;                    //!< 0x110, 0x08: ctor `addi r16,r25,272` @0x8002D210
  // 0x118 is a **bare 4-byte pointer**, not a holder and not an `optional_object`. The only writer
  // is 0x8002ACEC (`stw r0,280(r30)`, `r0` = the `+8` of the argument), and its pointee is the same
  // class the two `rc_ptr`s below point at: `GetNumShaders` (0x800E4BFC) does
  // `x10->x118` then `->x8` then `->x1C`, and 0x1C is `CModel::x1c_numParts`. A `CAABox` does
  // **not** live at 0x108 - there is no member between 0x110 and 0x118.
  void* x118_normalModel;     //!< 0x118, 0x04
  // 0x11C and 0x12C are each a 0x10 `optional_object`: flag at +0xC, `__dt__6CTokenFv` on the
  // base. Retail: 0x8002C588/0x8002C55C (dtor), 0x8002D234/0x8002D26C (`stb ...,296/312`).
  rstl::optional_object< TLockedToken< CModel > > x11c_optional; //!< 0x11C, 0x10
  rstl::optional_object< TLockedToken< CModel > > x12c_optional; //!< 0x12C, 0x10
  // **0x13C and 0x144 are `rstl::rc_ptr`, two words each - object at +0, refcount pointer at +4.**
  // Not pointers to a holder elsewhere. The constructor stores 0 and then a freshly `new`ed
  // `int(1)` into the two halves (0x8002D2A8/0x8002D2CC and 0x8002D2DC/0x8002D2FC), the assign
  // site compares, releases, stores both halves and increments the refcount
  // (`SetXRayModel` 0x8002AC6C..0x8002AC9C for 0x13C, `SetInfraModel` 0x8002AB88..0x8002ABB8
  // for 0x144), and the destructor calls `fn_8002F270` on each (0x8002C53C, 0x8002C54C).
  rstl::rc_ptr< CSkinnedModel > x13c_xrayModel;  //!< 0x13C, 0x08 - index 2 in `fn_800E4E50`
  rstl::rc_ptr< CSkinnedModel > x144_infraModel; //!< 0x144, 0x08 - index 1 in `fn_800E4E50`
  rstl::rc_ptr< CSkinnedModel > x14c_;           //!< 0x14C, 0x08: dtor `fn_8002F2C0` @0x8002C524
  // 0x154 is one 0x24 member: an `rc_ptr` at 0x154/0x158 and 0x1C of scalars (the ctor writes
  // 0x15C..0x174; `fn_8002ACC4` writes six words at 0x160..0x174).
  rstl::rc_ptr< CSkinnedModel > x154_; //!< 0x154, 0x08
  uchar x15c_unk[0x1C];                //!< 0x15C - 0x178
  // 0x178 confirmed by three independent instructions: `addi r3,r3,376` in `CActor::SetModelData`,
  // `addi r3,r25,376` in the ctor @0x8002D308, and `addi r3,r30,376 / bl fn_800A94EC` in the dtor
  // @0x8002C508. **0xE0, not 0x100**: the destructor calls
  // `fn_800A94EC` once on 0x178 and the next member it destroys is 0x278, so the 0x20 bytes at
  // 0x258..0x278 that the constructor fills are *trivial* and emit no destructor code, so the
  // `CHECK_SIZEOF(CParticleDatabase, 0xe0)` already in this tree is right after all.
  CParticleDatabase x178_particleDB; //!< 0x178, 0xE0
  CAssetId x1d8_selfId;              //!< 0x258, ctor `stw ...,0x258` @0x8002D38C
  CVector3f x1dc_alignPos;           //!< 0x25C
  CQuaternion x1e8_alignRot;         //!< 0x268, 0x10
  rstl::rc_ptr< CAnimTreeNode > x1f8_animRoot;     //!< 0x278, 0x08: dtor `fn_8002EFFC` @0x8002C500
  rstl::rc_ptr< CTransitionManager > x1fc_transMgr; //!< 0x280
  float x200_speedScale;                            //!< 0x288
  int x204_charIdx;                                 //!< 0x28C
  short x208_currentAnim;                           //!< 0x290
  short x20a_padding;                               //!< 0x292
  int x20c_passedBoolCount;                         //!< 0x294
  int x210_passedIntCount;                          //!< 0x298
  int x214_passedParticleCount;                     //!< 0x29C
  int x218_passedSoundCount;                        //!< 0x2A0
  int x21c_particleLightIdx;                        //!< 0x2A4
  int x220_28_unk;                                  //!< 0x2A8, measured (`stw ...,0x2A8`)
  uchar x220_24_animating : 1;                      //!< the flag byte is 0x2AC
  uchar x220_25_loop : 1;
  uchar x220_26_aligningPos : 1;
  uchar x220_27_ : 1;
  uchar x220_28_ : 1;
  uchar x220_29_animationJustStarted : 1;
  uchar x220_30_poseBuilt : 1;
  uchar x220_31_poseCached : 1;
  uchar x22d_flags2; //!< 0x2AD, the second retail flag byte (`stb ...,0x2ad` @0x8002D4C4)
  uchar x22e_pad[2]; //!< 0x2AE - 0x2B0, nothing in retail writes these two bytes
  // 0x2B0..0x2F4 is **one** 0x44 member (dtor `addi r3,r30,688 / bl fn_8002CC8C` @0x8002C4D4 -
  // a single call for the whole 68 bytes). The `CPoseAsTransforms` in this tree is 0xD8, so the old
  // `x224_pose` name never fitted this slot; it is a pad until the member is identified.
  uchar x2b0_unk[0x44]; //!< 0x2B0
  // 0x2F4..0x40C is **one** 0x118 member (dtor `addi r3,r30,756 / bl fn_8002CE34` @0x8002C4C8).
  // The `CHierarchyPoseBuilder` in this tree is `CHECK_SIZEOF(..., 0x110)`, i.e. 8 bytes short of the
  // slot, so the old `x2fc_poseBuilder` name sat 8 bytes into the wrong member.
  uchar x2f4_unk[0x118]; //!< 0x2F4 - 0x40C
  // 0x40C..0x438 is **one** 0x2C member whose first 8 bytes are an `rstl::auto_ptr`
  // (`stb ...,0x40C` for the flag, `stw ...,0x410` for the item, and the dtor calls
  // `fn_802B2DC0(*(this+0x410), 1)` @0x8002C4BC). The `CAnimPlaybackParms` in this tree is
  // `CHECK_SIZEOF(..., 0x28)`, 4 bytes short of the slot.
  uchar x40c_unk[0x2C]; //!< 0x40C - 0x438
  // 0x438..0x5B8 is the **last** member, 0x180 bytes: a word at 0x438 destroyed with a deleting
  // flag (`addi r3,r30,1080 / bl fn_8002CFF4` @0x8002C49C), 0x43C..0x59C untouched by the ctor, an
  // `int` at 0x59C and six floats at 0x5A0..0x5B4. The old `x434_additiveAnims`
  // (`rstl::reserved_vector<...>`) is nowhere near this.
  uchar x438_unk[0x180]; //!< 0x438 - 0x5B8, the end of the class

  static rstl::reserved_vector< CBoolPOINode, 8 > mBoolPOINodes;
  static rstl::reserved_vector< CInt32POINode, 16 > mInt32POINodes;
  static rstl::reserved_vector< CParticlePOINode, 20 > mParticlePOINodes;
  static rstl::reserved_vector< CSoundPOINode, 20 > mSoundPOINodes;
  // in cpp -> rstl::reserved_vector< CInt32POINode, 16 > sInt32TransientCache;
};
// CHECK_SIZEOF(CAnimData, 0x578)  // superseded twice, see below
// CHECK_SIZEOF(CAnimData, 0x620)  // superseded, see below
CHECK_SIZEOF(CAnimData, 0x5B8)  // **measured against retail, 2026-09-26 - see the note below**
//
// ## Three generations, and every one of them was the wrong question
//
// The ladder used to be built from member *names* (`0x434 + 0x144` = 0x578), then from mwcceppc
// own output (0x630, with `x120_unk` 0x10 too long), then from a single retail instruction
// (`addi r3,r3,376` in `CActor::SetModelData` -> 0x620). All three were measurements of the wrong
// thing: a host/mwcceppc `sizeof` is *our* layout, and one instruction that reads a member pins
// that member, not the end of the class.
//
// ## `sizeof(CAnimData) == 0x5B8`, pinned by two independent retail instructions
//
// 1. **`li r3,1464` at 0x80030184** in `fn_8002FED8`, immediately before
//    `bl __nw__FUlPCcPCc` (`operator new`). The chain is closed, not assumed:
//      * `fn_8002FED8` is called by `CModelData::CModelData(const CAnimRes&)` at 0x800E6A04
//        (0x800E6900) with `r3 = sp+0x10`, and its result is stored into an `rstl::auto_ptr` at
//        0x80030240 (`stb r0,0(r24)` / `stw r31,4(r24)`) that is then stolen into
//        `CModelData::x0c/x10` at 0x800E6A58/0x800E6A60;
//      * so 1464 = 0x5B8 is the size of the object at `CModelData::x10`;
//      * 1464 is not any other allocation in the same function: the three `operator new` calls
//        before it are `li r3,8` (0x8002FF04), `li r3,4` (0x8002FF6C) and `li r3,4`
//        (0x8002FFB0-area) - refcounts and an 8-byte control block.
// The `mulli r0,r26,248` in `fn_8002FED8` is a **different** class: 248 = 0xF8 is the stride of
// per-character array in `CAnimRes` (`fn_8002FEC8` is a 0x10-byte one-liner doing
// `mulli r0,r4,248 ; lwz r3,16(r3) ; add r3,r3,r0 ; blr`). It is not `CAnimData` and it is not
// `CCharacterInfo`.
//
// 2. **`stfs f0,1460(r25)` at 0x8002D598**, the last store of the constructor `fn_8002D178`
//    (0x8002D178, `r25` = `this`). 0x5B4 + 4 = 0x5B8. The stores before it are 0x438 and
//    0x59C/0x5A0..0x5B0, so the tail is not a single flat array - it is
//    `word @0x438`, then 0x160 bytes the constructor never touches, then
//    `int @0x59C` and `float[6] @0x5A0..0x5B4`.
//
// 3. The **destructor** `fn_8002C340` (0x8002C340) agrees independently: it walks the members in
//    descending order and its **highest** member is `addi r3,r30,1080 / bl fn_8002CFF4` at
//    0x8002C49C, i.e. 0x438, with a *deleting* flag. Everything from 0x438 up is therefore
//    trivially destructible POD, which is why the constructor has to initialise it by hand.
//
// **The whole ladder is then confirmed by mwcceppc**, not just the total: a probe compiled with
// `tools/probe_cc.sh` that writes `&((CAnimData*)0)->member` into a `.data` array (the host cannot
// be used - it is 64-bit and MWCC is 32-bit) yields 0x000, 0x00C, 0x0CC, 0x104, 0x110, 0x118, 0x11C,
// 0x12C, 0x13C, 0x144, 0x14C, 0x154, 0x15C, 0x178, 0x258, 0x25C, 0x268, 0x278, 0x280, 0x288,
// 0x28C, 0x290, 0x294, 0x2A4, 0x2A8, 0x2AD, 0x2AE, 0x2B0, 0x2F4, 0x40C, 0x438 and
// `sizeof = 0x5B8`, every one of them the retail offset in the table.
//
// `fn_8002C340` is the retail `CAnimData::~CAnimData` and it is also what identifies the members:
// every entry below is one call in that ladder, and each is a *different* function, so the
// boundaries are exact rather than inferred.
//
// | member | retail slot | the instruction that fixes it |
// | --- | --- | --- |
// | 0x000 `CToken` | 0x0C | ctor `stw ...,8` @0x8002D1DC; dtor `__dt__6CTokenFv(this,0)` @0x8002C608 |
// | 0x00C `CCharacterInfo` | **0xF8** | one ctor call `addi r3,r25,12` @0x8002D1E8 and one dtor call `addi r3,r30,12` @0x8002C5EC cover all of 0x00C..0x104 |
// | 0x104 `CToken` | 0x0C | ctor `addi r16,r25,260` + `__ct__6CToken` + `Lock` @0x8002D1F0 |
// | 0x110 `CToken` | 0x08 | ctor `addi r16,r25,272` @0x8002D210; dtor `__dt__6CTokenFv` @0x8002C5C4 |
// | 0x118 | 0x04 | only writer `stw r0,280(r30)` @0x8002ACEC (`fn_8002ACC4`) |
// | 0x11C `optional_object` | 0x10 | dtor flag `lbz 0x128` @0x8002C590; ctor `stb 0x128` @0x8002D234 |
// | 0x12C `optional_object` | 0x10 | dtor flag `lbz 0x138` @0x8002C564; ctor `stb 0x138` @0x8002D26C |
// | 0x13C `rc_ptr` | 0x08 | dtor `fn_8002F270(this+0x13C)` @0x8002C54C; ctor `stw 0x13C`/`stw 0x140` @0x8002D2A8/0x8002D2CC |
// | 0x144 `rc_ptr` | 0x08 | dtor `fn_8002F270(this+0x144)` @0x8002C53C; ctor @0x8002D2DC/0x8002D2FC |
// | 0x14C `rc_ptr` | 0x08 | dtor `fn_8002F2C0` @0x8002C524; ctor `stw 0x14C`/`stw 0x150` @0x8002D314/0x8002D31C |
// | 0x154 `rc_ptr` + 0x1C | 0x24 | dtor `fn_8002F0C4(this+0x154)` @0x8002C51C |
// | 0x178 `CParticleDatabase` | 0xE0 | dtor `addi r3,r30,376 / bl fn_800A94EC` @0x8002C508 |
// | 0x278 `rc_ptr` | 0x08 | dtor `fn_8002EFFC` @0x8002C500; ctor `stw 0x278`/`stw 0x27C` @0x8002D400/0x8002D404 |
// | 0x280 `rc_ptr` | 0x30 | dtor `fn_8002F1A4` @0x8002C4EC; ctor `stw 0x280`/`stw 0x284` @0x8002D41C/0x8002D424 |
// | 0x2B0 | 0x44 | dtor `addi r3,r30,688 / bl fn_8002CC8C` @0x8002C4D4 |
// | 0x2F4 | 0x118 | dtor `addi r3,r30,756 / bl fn_8002CE34` @0x8002C4C8 |
// | 0x40C | 0x2C | dtor flag `lbz 0x40C` + `fn_802B2DC0(*(this+0x410),1)` @0x8002C4B0/0x8002C4BC |
// | 0x438 | 0x180 | dtor `addi r3,r30,1080 / bl fn_8002CFF4` @0x8002C49C |
//
// ## What 0x118, 0x13C and 0x144 actually are - the question this ladder answers
//
// * **0x118 is a bare 4-byte pointer**, written only by `fn_8002ACC4` (0x8002ACEC) as
//   `this->x118 = arg->x8`, where `arg` is a `TLockedToken`-shaped holder. It is **not** a
//   `CAABox` and not an `optional_object`; the old note that read 0x118 as `x108_aabb` was reading
//   our own layout back. Its pointee is the class the two `rc_ptr`s below also point at:
//   `GetNumShaders` (0x800E4BFC) does `x10->x118` then `->x8` then `->x1C`, and 0x1C is
//   `CModel::x1c_numParts`; so the pointee has a `CModel*` at +8. `fn_8002AAFC` (0x8002AB40) and
//   `fn_8002ABE0` (0x8002AC24) read the same word and pass `pointee + 0x18` to `fn_8030F588`
//   with two `CToken&`s, and `operator new` a **36-byte** object a few instructions earlier
//   (`li r3,36` @0x8002AB30 / 0x8002AC14) whose two words become the `rc_ptr` halves. The
//   pointee is 0x24 bytes with a `CModel*` at +8 and a sub-object at +0x18 that a
//   `TLockedToken<CModel>` pair is built into. Not yet named; do not name it from the shape.
// * **0x13C and 0x144 are `rstl::rc_ptr`, two words each** - object at +0, refcount pointer at
//   +4. The constructor is the proof: `stw 0,0x13C` then `operator new(4)` with `*(int*)r3 = 1`
//   stored at 0x140 (0x8002D2A8..0x8002D2CC), and identically at 0x144/0x148
//   (0x8002D2DC..0x8002D2FC). The assign sites are the rc_ptr copy-assign shape end to end -
//   compare, `fn_8002F270` release, store both halves, `++*refcount` - at 0x8002AC6C..0x8002AC9C
//   (`SetXRayModel`, 0x13C) and 0x8002AB88..0x8002ABB8 (`SetInfraModel`, 0x144). And the
//   destructor calls `fn_8002F270` on each separately (0x8002C53C, 0x8002C54C).
// * So `SModelHolder { char[8]; CModel*; char[4]; }` in `CModelDataModelSlots.cpp`
//   is **not** the retail shape - but its *code* is still byte-exact, because both selectors only
//   *load* the one word at 0x118/0x13C/0x144 and return it, which is true of the first word of an `rc_ptr`
//   word just as much as of a pointer to a holder. The `char x11c_pad[0x20]` is the two
//   two `optional_object` at 0x11C and 0x12C and the `char x140_pad[4]` is the refcount word of 0x13C.
//
// ## Still open, and now bounded
//
//   * `0x00C..0x104` is one member of 0xF8, and it is **resolved**: it is a `CCharacterInfo` of
//     0xF8, laid out member by member from retail's own copy constructor (`fn_8002DE3C`) and its
//     stream constructor (`fn_8029269C`, reachable from `CInputStream`'s `Get<CCharacterInfo>`
//     helper). `CHECK_SIZEOF(CCharacterInfo, 0xc0)` was the *Metroid Prime 1* size - the header
//     was copied from the Prime 1 tree - and the 0x38 is gone. See the table in
//     `Kyoto/Animation/CCharacterInfo.hpp`.
//   * `0x2B0` (0x44), `0x2F4` (0x118), `0x40C` (0x2C) and `0x438` (0x180) are each **one** member
//     with a deleting destructor, and the nearest types in this tree are 0x94 (CPoseAsTransforms
//     0xD8), 0x08 (CHierarchyPoseBuilder 0x110), 0x04 (CAnimPlaybackParms 0x28) and a long way
//     off (a `reserved_vector` is 0x10). The pads are honest placeholders, not names.
//   * Nothing in the port constructs or destroys a `CAnimData`, so no unit bytes depend on these
//     types - which is why retyping them is safe today and will stop being safe the moment
//     somebody writes `fn_8002D178`.
#endif // _CANIMDATA
