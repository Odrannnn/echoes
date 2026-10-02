#include "MetroidPrime/CAnimData.hpp"

#include "Kyoto/Animation/CAnimSysContext.hpp"
#include "Kyoto/Animation/CAnimTreeNode.hpp"
#include "Kyoto/Animation/CAnimationDatabase.hpp"
#include "Kyoto/Animation/CAnimationManager.hpp"
#include "Kyoto/Animation/CCharLayoutInfo.hpp"
#include "Kyoto/Animation/CJointData_LinearStorage.hpp"
#include "Kyoto/Animation/CTransitionManager.hpp"
#include "Kyoto/Animation/IMetaAnim.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "rstl/algorithm.hpp"

typedef rstl::reserved_vector< rstl::pair< uint, CAdditiveAnimPlayback >, 8 > TAdditiveAnims;

/**
 * `CAnimationManager`'s two database lookups, retail's own unnamed functions at `.text:0x8028CA5C`
 * and `.text:0x8028CAE4`. Both are written out and matched under those `extern "C"` names in
 * `src/Kyoto/Animation/CAnimation.cpp`, so retail's callers in this unit - which dispatch to them
 * with a `bl fn_8028CA5C` / `bl fn_8028CAE4` rather than to a mangled C++ name - are written the
 * same way here. Spelled as a `CAnimationManager` member instead, the call's relocation would name
 * a mangled symbol that nothing provides and the link would fail; there is no such member.
 */
extern "C" rstl::rc_ptr< IMetaAnim > fn_8028CA5C(const CAnimationManager* mgr, uint animId);

extern "C" rstl::ncrc_ptr< CAnimTreeNode >
fn_8028CAE4(const CAnimationManager* mgr, uint animId, const CMetaAnimTreeBuildOrders& orders);

#ifdef TARGET_PC
/**
 * `src/Kyoto/Animation/CAnimation.cpp` - where the two bodies above are written out and matched -
 * is deliberately **not** in the port's build: `tools/check_files_cmake.py` records that listing
 * it takes the port's undefined count 318 -> 319, so it stays excluded. This file is in the port,
 * so as soon as it calls `fn_8028CA5C` / `fn_8028CAE4` the port link is short those two symbols and
 * `tools/link_check.sh --strict` fails on a *grew* gap.
 *
 * PORT_NOTES.md's rule for port edits is that port-only behaviour goes behind `#ifdef TARGET_PC`,
 * and this is exactly that: the same C++ those two functions have, with `fn_8028CA3C` folded into
 * `*token` because the port has no copy of that one either. It is real code, not a stub, and the
 * matching build never sees it - `CAnimation.cpp` is the only definition there, so there is no
 * duplicate.
 */
extern "C" rstl::rc_ptr< IMetaAnim > fn_8028CA5C(const CAnimationManager* mgr, uint animId) {
  TToken< CAnimationDatabase > db = mgr->GetAnimationDatabase();
  return db->GetMetaAnim(animId);
}

extern "C" rstl::ncrc_ptr< CAnimTreeNode >
fn_8028CAE4(const CAnimationManager* mgr, uint animId, const CMetaAnimTreeBuildOrders& orders) {
  TToken< CAnimationDatabase > db = mgr->GetAnimationDatabase();
  const rstl::rc_ptr< IMetaAnim >* meta = &db->GetMetaAnim(animId);
  return (*meta)->GetAnimationTree(mgr->GetSysContext(), orders);
}
#endif // TARGET_PC

/**
 * Same layout as `rstl::vector< CPASAnimState, rmemory_allocator >` - the members are `Alloc`,
 * `mCount`, `mCapacity`, `mItems` in that order and `rmemory_allocator` is empty.
 *
 * It is a view and not the `rstl::vector` itself for a mwccceppc reason, measured on this file:
 * an explicit specialisation is rejected ("object ... redefined") when a struct or typedef
 * definition sits between it and the previous definition, so the one below is declared next to the
 * file's other layout views at the top instead of next to the body that uses it.
 */
struct SPASAnimStateVector {
  rstl::rmemory_allocator mAllocator;
  int mCount;
  int mCapacity;
  CPASAnimState* mItems;
};

/**
 * The 0x28-byte element of `SAnimInfoVector`: a `rstl::string` then six floats. Six members and
 * not an array is what reproduces retail's `lfs`/`stfs` at `0x10`..`0x24` (an array member is
 * copied word-wise with `lwz`/`stw`). What is left over is retail's FP schedule - it reuses `f0`
 * for one member at a time, mwccceppc batches two loads then two stores - and no spelling tried
 * moves it; see `fn_8002E270` below.
 */
struct SAnimInfo40 {
  rstl::string x00_name;
  float x10_value0;
  float x14_value1;
  float x18_value2;
  float x1c_value3;
  float x20_value4;
  float x24_value5;
};

/**
 * Same layout as `rstl::vector< T, rstl::rmemory_allocator >` again, for the two element types
 * retail's map leaves unnamed and whose real C++ types this tree has no class for. Declared with
 * the other layout views at the top for the reason spelled out above `SPASAnimStateVector`.
 */
struct SAnimInfoVector {
  rstl::rmemory_allocator mAllocator;
  int mCount;
  int mCapacity;
  SAnimInfo40* mItems;
};

/** The 0x14-byte element of `rstl::vector< SStr20 >`: a word then a `rstl::string`. */
struct SStr20 {
  int x00;
  rstl::string x04_name;
};

/**
 * The pair of iterators retail's `~vector<T>` bodies hand to `rstl::destroy(begin, end)`. It is a
 * struct rather than two locals because retail spills **four** words for them - the value and a
 * second copy of it, at `0x8`/`0xc` and `0x10`/`0x14` - and mwccceppc only gives that shape when
 * the two are fields of one object (measured: two plain locals give 75.36% here, this gives
 * 84.45%, and what is left is the two duplicate spill stores and their schedule).
 */
struct SPASAnimStateIters {
  CPASAnimState* end;
  CPASAnimState* begin;
};

struct SStr20Iters {
  SStr20* end;
  SStr20* begin;
};


/** 0x8002EB54 - the second `rstl::less<rstl::string>` forwarder; see fn_80027394. */
extern "C" bool fn_8002EB54(rstl::less< rstl::string >* cmp, const rstl::string& a,
                           const rstl::string& b) {
  return cmp->operator()(a, b);
}

extern "C" void fn_8002E95C(CPASAnimInfo* dest, const CPASAnimInfo* src);

#pragma dont_inline on
extern "C" void fn_8002E95C(CPASAnimInfo* dest, const CPASAnimInfo* src) {
  new (dest) CPASAnimInfo(*src);
}
#pragma dont_inline reset

/** 0x8002E4B8 - the `rstl::construct` forwarder for `CPASAnimState`; see fn_8002D9E4 below. */
extern "C" void fn_8002E4B8(void* dest, const CPASAnimState& src) {
  rstl::construct_impl< CPASAnimState >(dest, src);
}

/**
 * `.text 0x8002E3CC` and `0x8002E450` - `rstl::vector< CPASAnimState >`'s copy constructor and the
 * `rstl::uninitialized_copy_n` it calls: the second pair of the `rstl/vector.hpp` constructor body
 * the `CEffectComponent` pair below spells out. Written under retail's `fn_<addr>` names for the
 * same reason, and the copy loop is kept a loop because retail calls `rstl::construct` out of line
 * from it instead of expanding the element copy (the forwarder is 0x8002E564).
 */
extern "C" CPASAnimState* fn_8002E450(const CPASAnimState* src, int n, CPASAnimState* dest) {
  const CPASAnimState* it = src;
  CPASAnimState* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    rstl::construct(&*cur, *it);
  }
  return cur;
}

extern "C" SPASAnimStateVector* fn_8002E3CC(SPASAnimStateVector* dest,
                                            const SPASAnimStateVector* other) {
  dest->mCount = other->mCount;
  dest->mCapacity = other->mCapacity;
  if (other->mCount == 0 && other->mCapacity == 0) {
    dest->mItems = nullptr;
  } else {
    dest->mAllocator.allocate(dest->mItems, dest->mCapacity);
    fn_8002E450(other->mItems, dest->mCount, dest->mItems);
  }
  return dest;
}

template <>
rstl::vector< CPASAnimInfo >::vector(const rstl::vector< CPASAnimInfo >& other)
: mAllocator(other.mAllocator), mCount(other.mCount), mCapacity(other.mCapacity) {
  if (other.mCount == 0 && other.mCapacity == 0) {
    mItems = nullptr;
  } else {
    mAllocator.allocate(mItems, mCapacity);
    int remaining;
    CPASAnimInfo* dest = mItems;
    const CPASAnimInfo* src = other.mItems;
    remaining = mCount;
    while (remaining != 0) {
      fn_8002E95C(dest, src);
      --remaining;
      ++src;
      ++dest;
    }
  }
}

/**
 * `.text 0x8002E1EC` and `0x8002E270` - the third pair of the `rstl/vector.hpp` copy-constructor
 * body this file spells out, for the 0x28-byte element retail's map leaves unnamed (`fn_8002E1EC`
 * is the same 33 instructions as `fn_8002E3CC` and `fn_800277B8` above, with `mCapacity * 0x28`
 * where they have `* 0x34` and `* 0x1c`).
 *
 * The element is 40 bytes and **holds a `rstl::string`**: `fn_8002E270`'s loop calls
 * `rstl::basic_string<char>`'s copy constructor and then copies six floats at `0x10`..`0x24`,
 * member by member - the same counted `bdnz`-shaped loop with the placement-new null test that
 * `fn_8002E450` has, which is why the spelling below is that function's spelling. This tree has no
 * class for the element, so the copy goes through the same-layout view `SAnimInfo40`, and the
 * vector through `SAnimInfoVector`.
 */
extern "C" SAnimInfo40* fn_8002E270(const SAnimInfo40* src, int n, SAnimInfo40* dest) {
  const SAnimInfo40* it = src;
  SAnimInfo40* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    new (cur) SAnimInfo40(*it);
  }
  return cur;
}

extern "C" SAnimInfoVector* fn_8002E1EC(SAnimInfoVector* dest, const SAnimInfoVector* other) {
  dest->mCount = other->mCount;
  dest->mCapacity = other->mCapacity;
  if (other->mCount == 0 && other->mCapacity == 0) {
    dest->mItems = nullptr;
  } else {
    dest->mAllocator.allocate(dest->mItems, dest->mCapacity);
    fn_8002E270(other->mItems, dest->mCount, dest->mItems);
  }
  return dest;
}

rstl::reserved_vector< CBoolPOINode, 8 > CAnimData::mBoolPOINodes;
rstl::reserved_vector< CInt32POINode, 16 > CAnimData::mInt32POINodes;
rstl::reserved_vector< CParticlePOINode, 64 > CAnimData::mParticlePOINodes;
rstl::reserved_vector< CSoundPOINode, 48 > CAnimData::mSoundPOINodes;
static rstl::reserved_vector< CInt32POINode, 16 > sInt32TransientCache;
static CInt32POINode* sInt32TransientCacheData;
static int sPOICacheReferenceCount;

/**
 * 0x8002D9E4, 0x8002DBC8, 0x8002DDA4 - three of the seven `rstl` **forwarders** retail's map file
 * leaves unnamed (0x8002B024, 0x8002C964 and 0x8002E4B8 are the rest of this group).
 *
 * Each is the same eight instructions: `stwu/mflr/stw lr`, one `bl`, `lwz/mtlr/addi/blr`, and the
 * `bl` target sits exactly 0x20 later in `.text` - the weak `rstl::construct_impl<T>` /
 * `rstl::destroy_impl<T>` instantiation, which this file also emits and already matches at 100%.
 * Their only caller is a `rstl::uninitialized_copy_n` loop over the matching POI-node vector
 * (`fn_8002DD38`, 0x8002DD38, stride 0x30, `cmpw/blt`), so the forwarded pair is the out-of-line
 * `rstl::construct` / `rstl::destroy` for that element type: with `-inline deferred,noauto` the
 * outlined `construct<T>` keeps its call to `construct_impl<T>` as a real call instead of inlining
 * it, which is exactly the eight instructions above.
 *
 * Retail named none of them, so `config/G2ME01/symbols.txt` carries the `fn_<addr>` placeholder and
 * that spelling is kept - which is also what makes them `extern "C"`: a C++ one would mangle and
 * objdiff would pair nothing. Same convention as `fn_80025E08`/`fn_80025E0C` at the bottom of this
 * file.
 */
extern "C" void fn_8002DDA4(void* dest, const CBoolPOINode& src) {
  rstl::construct_impl< CBoolPOINode >(dest, src);
}

extern "C" void fn_8002DBC8(void* dest, const CParticlePOINode& src) {
  rstl::construct_impl< CParticlePOINode >(dest, src);
}

extern "C" void fn_8002D9E4(void* dest, const CSoundPOINode& src) {
  rstl::construct_impl< CSoundPOINode >(dest, src);
}

CAnimData::CAnimData(
    CAssetId selfId, const CCharacterInfo& charInfo, int defaultAnim, int charIdx, bool loop,
    const TLockedToken< CCharLayoutInfo >& layoutData, const TToken< CSkinnedModel >& modelData,
    const rstl::optional_object< TLockedToken< CSkinnedModelWithAvgNormals > >& iceModelData,
    const rstl::optional_object< TLockedToken< CSpatialPrimitive > >& spatialPrimitive,
    const rstl::ncrc_ptr< CAnimSysContext >& animCtx,
    const rstl::rc_ptr< CAnimationManager >& animMgr,
    const rstl::rc_ptr< CTransitionManager >& transMgr,
    const TLockedToken< CCharacterFactory >& charFactory, bool animatedScale)
: mCharFactory(charFactory)
, mCharInfo(charInfo)
, mLayoutData(layoutData)
, mModelData(modelData)
, mIceModelData(iceModelData)
, mSpatialPrimitive(spatialPrimitive)
, mXrayModel(nullptr)
, mInfraModel(nullptr)
, mAnimCtx(animCtx)
, mAnimMgr(animMgr)
, mAnimDir(kAD_Forward)
, mAabb(CAABox::MakeMaxInvertedBox())
, mParticleDB()
, mSelfId(selfId)
, mAlignPos(CVector3f::Zero())
, mAlignRot(CQuaternion::NoRotation())
, mAnimRoot()
, mTransMgr(transMgr)
, mSpeedScale(1.f)
, mCharIdx(charIdx)
, mCurrentAnim(defaultAnim)
, mPassedBoolCount(0)
, mPassedIntCount(0)
, mPassedParticleCount(0)
, mPassedSoundCount(0)
, mParticleLightIdx(0)
, x2a8_(8)
, mAnimating(false)
, mLoop(loop)
, mAligningPos(false)
, x2ac_27_(false)
, x2ac_28_(false)
, mAnimationJustStarted(false)
, mPoseBuilt(false)
, mAnimatedScale(animatedScale)
, mUniformScale(false)
, x2ad_25_(true)
, mPose(layoutData->GetBodyPartSegIds().GetCount(), animatedScale ? 1 : 0, 0)
, mPoseBuilder(CLayoutDescription(layoutData), animatedScale)
, mJointData()
, mPlaybackParms(-1, -1, 1.f, true)
, mAdditiveAnims()
, mCachedBoundsAnimId(-1)
, mCachedAnimBounds(CAABox::MakeMaxInvertedBox()) {
  if (sPOICacheReferenceCount == 0) {
    mBoolPOINodes.resize(8, CBoolPOINode(0xffffffff, kPT_EmptyBool, CCharAnimTime(0.f), -1, false,
                                         1.f, -1, 0, false));
    mInt32POINodes.resize(16, CInt32POINode(0xffffffff, kPT_EmptyInt32, CCharAnimTime(0.f), -1,
                                            false, 1.f, -1, 0, 0, rstl::string_l("root")));
    mParticlePOINodes.resize(64, CParticlePOINode(0xffffffff, kPT_Particle, CCharAnimTime(0.f), -1,
                                                  false, 1.f, -1, 0,
                                                  CParticleData(0, SObjectTag(0, 0), CSegId(0), 1.f,
                                                                CParticleData::kPM_Initial)));
    mSoundPOINodes.resize(48, CSoundPOINode(0xffffffff, kPT_Sound, CCharAnimTime(0.f), -1, false,
                                            1.f, -1, 0, 0, 0.f, 0.f, CSegId(0), 0, 0, 0.f));
  }
  ++sPOICacheReferenceCount;

  mAabb = mModelData->GetModel()->GetAABB();
  mParticleDB.CacheParticleDesc(charInfo.GetParticleResData());
  // TODO: Build mAnimRoot from the character-mapped defaultAnim with no special orders.
}

/**
 * `.text 0x8002C6E8`..`0x8002C914` and `0x8002CC38`/`0x8002CD8C` - the rest of the `rstl` destructor
 * chains retail's map leaves unnamed. All of them are the same shape `rstl/vector.hpp`'s
 * `~vector()` has in retail, in which the `~vector()` body takes the `int flag` that decides
 * whether the container itself is freed:
 *
 *   0x8002C914   80 B  the `destroy(begin, end)` loop, stride 0x34
 *   0x8002C8DC   56 B  its `destroy(begin, end)` forwarder (copies both iterators to the stack)
 *   0x8002C858  132 B  `~vector< CPASAnimState >`: the loop, then `Free(mItems)`, then `Free(this)`
 *   0x8002C7A4   96 B  the same loop for the 0x14-byte element, whose destructor is one string's
 *   0x8002C76C   56 B  its forwarder
 *   0x8002C6E8  132 B  `~vector< SStr20 >`
 *   0x8002CD8C   84 B  the element destructor is trivial, so only `Free(mItems)` + the flag
 *   0x8002CC38   84 B  the same 84 bytes for a second instantiation - the three *named* 84-byte
 *                      destructors at 0x8002CA18/0x8002CA6C/0x8002CDE0 are this function too
 *
 * As with the `rstl::construct` chains further down, retail named none of these, so each body is
 * written out under its `fn_<addr>` name rather than reached through the mangled template. `It` is
 * `T**` in retail - the loops reload `*end` every iteration and take `*cur` - so the caller hands
 * over the address of a `T*` pair; the loop itself walks a `T*`, which is what puts retail's
 * `addi r31, r31, 0x34` (the element size, which is a multiple of 4 for every type here) where the
 * `bne` is.
 */
extern "C" rstl::vector< CPASParmInfo >* fn_8002CD8C(rstl::vector< CPASParmInfo >* vec, int flag) {
  if (vec != nullptr) {
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(vec);
    }
  }
  return vec;
}

extern "C" rstl::vector< CVector3f >* fn_8002CC38(rstl::vector< CVector3f >* vec, int flag) {
  if (vec != nullptr) {
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(vec);
    }
  }
  return vec;
}

/**
 * 0x8002C964 - the `rstl::destroy` forwarder for `CPASAnimState`; see fn_8002DDA4 below. It is
 * declared here and defined between `fn_8002CC38` and `fn_8002C914` because mwcceppc emits
 * definitions in reverse source order: 0x8002C964 has to sit between them in `.text`.
 */
extern "C" void fn_8002C964(CPASAnimState* ptr);

extern "C" void fn_8002C964(CPASAnimState* ptr) { rstl::destroy_impl< CPASAnimState >(ptr); }

extern "C" void fn_8002C914(CPASAnimState** first, CPASAnimState** last) {
  CPASAnimState* cur = *first;
  while (cur != *last) {
    fn_8002C964(cur);
    ++cur;
  }
}

extern "C" void fn_8002C8DC(CPASAnimState** first, CPASAnimState** last) {
  CPASAnimState* begin;
  CPASAnimState* end = *last;
  begin = *first;
  fn_8002C914(&begin, &end);
}

extern "C" rstl::vector< CPASAnimState >* fn_8002C858(rstl::vector< CPASAnimState >* vec, int flag) {
  if (vec != nullptr) {
    SPASAnimStateIters iters;
    iters.end = vec->mItems + vec->mCount;
    iters.begin = vec->mItems;
    fn_8002C8DC(&iters.begin, &iters.end);
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(vec);
    }
  }
  return vec;
}

extern "C" void fn_8002C7A4(SStr20** first, SStr20** last) {
  SStr20* cur = *first;
  while (cur != *last) {
    if (cur != nullptr) {
      cur->x04_name.~basic_string();
    }
    ++cur;
  }
}

extern "C" void fn_8002C76C(SStr20** first, SStr20** last) {
  SStr20* begin;
  SStr20* end = *last;
  begin = *first;
  fn_8002C7A4(&begin, &end);
}

extern "C" rstl::vector< SStr20 >* fn_8002C6E8(rstl::vector< SStr20 >* vec, int flag) {
  if (vec != nullptr) {
    SStr20Iters iters;
    iters.end = vec->mItems + vec->mCount;
    iters.begin = vec->mItems;
    fn_8002C76C(&iters.begin, &iters.end);
    CMemory::Free(vec->mItems);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(vec);
    }
  }
  return vec;
}

CAnimData::~CAnimData() {
  if (--sPOICacheReferenceCount == 0) {
    mBoolPOINodes.clear();
    mInt32POINodes.clear();
    mParticlePOINodes.clear();
    mSoundPOINodes.clear();
  }
}

CAABox CAnimData::GetBoundingBox() const {
  const rstl::vector< rstl::pair< rstl::string, CAABox > >& aabbList =
      mCharInfo.GetAnimBBoxList();
  if (aabbList.size() > 0) {
    CAnimTreeEffectiveContribution contrib = mAnimRoot->GetContributionOfHighestInfluence();
    rstl::string name = contrib.GetPrimitiveName();
    rstl::vector< rstl::pair< rstl::string, CAABox > >::const_iterator search =
        rstl::find_by_key(aabbList, name);
    if (search != aabbList.end()) {
      return search->second;
    }
  }
  return mAabb;
}

CAABox CAnimData::GetBoundingBox(const CTransform4f& xf) const {
  return GetBoundingBox().GetTransformedAABox(xf);
}

CAABox CAnimData::CalcBoundingBoxFromModelVerts() const {
  // TODO: Accumulate the model vertices after applying the reference pose.
  return mAabb;
}

void CAnimData::ResetPOILists() {
  mPassedBoolCount = 0;
  mPassedIntCount = 0;
  mPassedParticleCount = 0;
  mPassedSoundCount = 0;
}

float CAnimData::GetAverageVelocity(int anim) const {
  // TODO: Weight primitive velocities by their animation durations.
  return 0.f;
}

// Guessed name.
void CAnimData::CollectAnimationResources(rstl::vector< SObjectTag >& tagsOut) const {
  // TODO: Collect unique ANIM resource tags from every character animation's primitives.
}

// Guessed name.
void CAnimData::CollectAnimationTokens(rstl::vector< CToken >& tokensOut, bool lock) const {
  // TODO: Fetch the collected animation resources from the simple pool and optionally lock.
}

void CAnimData::AdvanceParticles(const CTransform4f& xf, float dt, const CVector3f& scale,
                                 CStateManager* mgr) {
  mParticleDB.Update(dt, *this, **mLayoutData, xf, scale, mgr);
}

void CAnimData::DrawSkinnedModel(const CSkinnedModel& model, const CModelFlags& flags) const {
  // TODO: Set lighting/debug render state and draw with the linear pose.
}

/** 0x8002B024 - the `rstl::construct` forwarder for `CInt32POINode`; see fn_8002D9E4 below. */
extern "C" void fn_8002B024(void* dest, const CInt32POINode& src) {
  rstl::construct_impl< CInt32POINode >(dest, src);
}

void CAnimData::InitializeCache() {
  sInt32TransientCache.clear();
  sInt32TransientCache.resize(16, CInt32POINode(0xffffffff, kPT_EmptyInt32, CCharAnimTime(0.f), -1,
                                                false, 1.f, -1, 0, 0, rstl::string_l("root")));
  sInt32TransientCacheData = sInt32TransientCache.data();
}

void CAnimData::FreeCache() {
  sInt32TransientCache.clear();
  sInt32TransientCacheData = nullptr;
}

void CAnimData::SetSkinnedModel(const TLockedToken< CSkinnedModel >& model) {
  mModelData = model;
  mAabb = mModelData->GetModel()->GetAABB();
}

void CAnimData::SetXRayModel(const TLockedToken< CModel >& model,
                             const TLockedToken< CSkinRules >& skin) {
  mXrayModel =
      rstl::rc_ptr< CSkinnedModel >(rs_new CSkinnedModel(model, skin, mModelData->GetLayoutInfo()));
}

void CAnimData::SetInfraModel(const TLockedToken< CModel >& model,
                              const TLockedToken< CSkinRules >& skin) {
  mInfraModel =
      rstl::rc_ptr< CSkinnedModel >(rs_new CSkinnedModel(model, skin, mModelData->GetLayoutInfo()));
}

void CAnimData::AdvanceAnim(CCharAnimTime& time, CVector3f& offset, CQuaternion& rotation) {
  const float dt = time.GetSeconds();
  SAdvancementResults results(CCharAnimTime(0.f),
                              SAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation()));
  rstl::optional_object< rstl::ownership_transfer< IAnimReader > > simplified;

  if (mAnimDir == kAD_Forward) {
    results = mAnimRoot->VAdvanceView(time);
    simplified = mAnimRoot->Simplified();
  }

  if (simplified.valid()) {
    mAnimRoot = Cast(simplified.data());
  }

  if (x2ac_28_ || x2ac_27_) {
    const int count = mPassedIntCount;
    const CInt32POINode* node = mInt32POINodes.data();
    if (count > 0) {
      for (int i = 0; i < count; ++i, ++node) {
        if (node->GetPoiType() == kPT_UserEvent) {
          switch (node->GetValue()) {
          case kUE_AlignTargetPosStart:
            mAligningPos = true;
            break;
          case kUE_AlignTargetPos:
            mAlignPos = CVector3f::Zero();
            x2ac_28_ = false;
            mAligningPos = false;
            break;
          case kUE_AlignTargetRot:
            mAlignRot = CQuaternion::NoRotation();
            x2ac_27_ = false;
            break;
          }
        }
      }
    }
  }

  const SAdvancementDeltas deltas = results.mDeltas;
  const CVector3f& deltaPos = deltas.GetOffsetDelta();
  const CQuaternion& deltaRot = deltas.GetOrientationDelta();

  offset += deltaPos;
  if (mAligningPos) {
    offset += mAlignPos * dt;
  }

  CQuaternion alignRot = deltaRot * mAlignRot;
  rotation *= alignRot;
  mAlignPos = alignRot.BuildInverted().Transform(mAlignPos);

  time = results.mRemTime;
}

CAdvancementDeltas CAnimData::AdvanceIgnoreParticles(float dt, CRandom16& random,
                                                     bool advanceTree) {
  // Retail has no store to the out-parameter: `DoAdvance` opens by setting it
  // (`li r0,0 ; stb r0,0(r29)` at 0x80029DBC). Prime 1 spells the same thing as an
  // uninitialised local, and that is the only spelling that drops the store here too.
  // Caveat: this tree's `DoAdvance` is still a TODO stub that does not write the
  // parameter, so until `DoAdvance` is decompiled this reads an uninitialised byte.
  bool suspendEffects;
  return DoAdvance(dt, suspendEffects, random, advanceTree);
}

CAdvancementDeltas CAnimData::Advance(float dt, float minParticleWeight, const CVector3f& scale,
                                      CStateManager* mgr, CRandom16& random, TAreaId areaId,
                                      bool advanceTree) {
  // Retail has no store to the out-parameter: `DoAdvance` opens by setting it
  // (`li r0,0 ; stb r0,0(r29)` at 0x80029DBC), and Prime 1 spells the same thing as an
  // uninitialised local, which is the only spelling that drops the store here too.
  // Caveat: this tree's `DoAdvance` is still a TODO stub that does not write the
  // parameter, so until `DoAdvance` is decompiled this reads an uninitialised byte.
  bool suspendEffects;
  CAdvancementDeltas deltas = DoAdvance(dt, suspendEffects, random, advanceTree);

  if (suspendEffects) {
    mParticleDB.SuspendAllActiveEffects(mgr);
  }

  const int passedParticleCount = mPassedParticleCount;
  for (int i = 0; i < passedParticleCount; ++i) {
    const CParticlePOINode& node = mParticlePOINodes[i];
    const int charIdx = node.GetCharacterIndex();
    if (charIdx == -1 || charIdx == mCharIdx) {
      if (node.GetMaximumDistance() > minParticleWeight ||
          (mgr != nullptr && !mgr->IsMultiplayer() &&
           mgr->GetCameraManager(0)->IsInCinematicCamera())) {
        mParticleDB.AddParticleEffect(node.GetNameHash(), node.GetFlags(), node.GetParticleData(),
                                      scale, mgr, areaId, false, mParticleLightIdx);
      }
    }
  }

  return deltas;
}

CAdvancementDeltas CAnimData::DoAdvance(float dt, bool& suspendEffects, CRandom16& random,
                                        bool advanceTree) {
  // TODO: Advance the animation tree, process POIs and combine additive deltas.
  return CAdvancementDeltas(CVector3f::Zero(), CQuaternion::NoRotation());
}

// Guessed name.
rstl::ncrc_ptr< CAnimTreeNode >
CAnimData::BuildAnimationTree(const CAnimPlaybackParms& parms) const {
  // TODO: Build the requested animation or a blend of the two requested animations.
  return rstl::ncrc_ptr< CAnimTreeNode >();
}

// Guessed name.
rstl::ncrc_ptr< CAnimTreeNode >
CAnimData::BuildTransitionTree(const CAnimPlaybackParms& parms) const {
  rstl::ncrc_ptr< CAnimTreeNode > target = BuildAnimationTree(parms);
  rstl::ncrc_ptr< CAnimTreeNode > result = mTransMgr->GetTransitionTree(mAnimRoot, target);
  target.ReleaseData();
  return result;
}

void CAnimData::SetAnimation(const CAnimPlaybackParms& parms, bool noTrans) {
  // TODO: Construct the new tree/transition, reset POIs and set playback alignment.
}

void CAnimData::GetAnimationPrimitives(const CAnimPlaybackParms& parms,
                                       rstl::set< CPrimitive >& primsOut) const {
  // Retail reads both of the parms' animation ids before it makes the first call
  // (`lwz r0,0(r4)` / `lwz r31,4(r4)` at 0x8002981C) and keeps `animB` in `r31` across both
  // lookups, so the second id is hoisted rather than read inside the guard.
  const uint animResA = mCharInfo.GetAnimationIndexList()[parms.GetAnimationId()];
  const int animB = parms.GetSecondAnimationId();
  fn_8028CA5C(GetAnimationManager().GetPtr(), animResA)->GetUniquePrimitives(primsOut);

  if (animB != -1) {
    const uint animResB = mCharInfo.GetAnimationIndexList()[animB];
    fn_8028CA5C(GetAnimationManager().GetPtr(), animResB)->GetUniquePrimitives(primsOut);
  }
}

void CAnimData::BuildPoseIfNecessary() const {
  if (!mPoseBuilt) {
    RecalcPoseBuilder(nullptr);
    mPoseBuilt = true;
  }
}

void CAnimData::BuildPose() const { BuildPoseIfNecessary(); }

void CAnimData::PreRender() { BuildPoseIfNecessary(); }

void CAnimData::SetupRender() const { BuildPoseIfNecessary(); }

void CAnimData::Render(const CSkinnedModel& model, const CModelFlags& flags) const {
  SetupRender();
  DrawSkinnedModel(model, flags);
}

// Retail 0x800295BC, `symbols.txt`'s `fn_800295BC`, is the out-of-line `CAnimData` step that
// `CModelData::RenderParticles` (0x800E5C4C) is the only caller of: it does nothing but reach the
// particle database at +0x178 and hand it to `AddToRendererClipped`. It lives in this unit's claim
// (`tools/range_owner.py .text 0x800295BC 0x800295E0 -> MetroidPrime/CAnimData.cpp`), so it is
// defined here rather than reached through the database from the caller - which is the only way
// `RenderParticles` gets retail's 44 bytes instead of 48. It sits between `Render` (0x800295E0)
// and `RecalcPoseBuilder` (0x8002945C) because that is where 0x800295BC falls, and
// `tools/check_decl_order.py --unit MetroidPrime/CAnimData` says so.
extern "C" void fn_800295BC(const CAnimData& animData, const CFrustumPlanes& planes) {
  animData.GetParticleDB().AddToRendererClipped(planes);
}

void CAnimData::RecalcPoseBuilder(const CCharAnimTime* time) const {
  // TODO: Sample the root into joint storage, add additive segments and build the linear pose.
  // The inherited IAnimReader virtual interface must be recovered before dispatching here.
}

rstl::ncrc_ptr< CAnimSysContext > CAnimData::GetAnimSysContext() const { return mAnimCtx; }

float CAnimData::GetAnimationDuration(int anim) const {
  // TODO: Query the selected animation tree's steady-state duration.
  return 0.f;
}

float CAnimData::GetAnimTimeRemaining(const rstl::string& name) const {
  float remTime = mAnimRoot->VGetTimeRemaining().GetSeconds();
  if (mSpeedScale > 0.f) {
    remTime /= mSpeedScale;
  }
  return remTime;
}

bool CAnimData::IsAnimTimeRemaining(float tolerance, const rstl::string& name) const {
  if (mAnimRoot.GetPtr() != 0) {
    const float remTime = mAnimRoot->VGetTimeRemaining().GetSeconds();
    return !close_enough(remTime, 0.f, tolerance);
  }
  return false;
}

CTransform4f CAnimData::GetLocatorTransform(const rstl::string& name,
                                            const CCharAnimTime* time) const {
  CSegId seg = mLayoutData->GetSegIdFromString(name);
  CSegId segCopy = seg;
  return GetLocatorTransform(segCopy, time);
}

CTransform4f CAnimData::GetLocatorTransform(CSegId id, const CCharAnimTime* time) const {
  if (id.val() != 0xFF) {
    if (time != nullptr || !mPoseBuilt) {
      RecalcPoseBuilder(time);
      mPoseBuilt = time == nullptr;
    }
    return CTransform4f(mPose.GetRotation(id), mPose.GetOffset(id));
  }
  return CTransform4f::Identity();
}

/**
 * `CQuaternion::BuildInverted` - retail `BuildInverted__11CQuaternionCFv` =
 * `_ZNK11CQuaternion13BuildInvertedEv`, `.text 0x80028E08`, 0x30 = 48 bytes.
 *
 * It was `src/Kyoto/Math/CQuaternionBuildInverted.cpp` on master. Upstream's `splits.txt` puts
 * 0x80028E08 inside this unit, and the port build compiles both files, so the definition moves
 * here and that file keeps only its note. `include/Kyoto/Math/CQuaternion.hpp` already declares
 * the member, and its other callers - `CBoneTracking`, `CSamusHud`, `CQuaternion` - link against
 * whatever object provides it.
 */
CQuaternion CQuaternion::BuildInverted() const {
  return CQuaternion(w, -imaginary.GetX(), -imaginary.GetY(), -imaginary.GetZ());
}

void CAnimData::CalcPlaybackAlignmentParms(const CAnimPlaybackParms& parms,
                                           const rstl::ncrc_ptr< CAnimTreeNode >& tree) {
  // TODO: Recover alignment events and the locator-relative position/rotation adjustments.
}

void CAnimData::SetRandomPlaybackRate(CRandom16& random) {
  for (int i = 0; i < mPassedIntCount; ++i) {
    const CInt32POINode& poi = mInt32POINodes[i];
    if (poi.GetPoiType() == kPT_RandRate) {
      const float scale = static_cast< float >(random.Next() % poi.GetValue()) / 100.f;
      if ((random.Next() % 100) < 50) {
        mSpeedScale = 1.f + scale;
      } else {
        mSpeedScale = 1.f - scale;
      }
      break;
    }
  }
}

void CAnimData::SetPlaybackRate(float rate) { mSpeedScale = rate; }

void CAnimData::MultiplyPlaybackRate(float scale) { mSpeedScale *= scale; }

CCharAnimTime CAnimData::GetTimeOfUserEventForAnimation(int anim, EUserEventType type) const {
  const uint animRes = mCharInfo.GetAnimationIndexList()[anim];
  // The tree is a named local, not a temporary: retail copy-initialises it out of `fn_8028CAE4`'s
  // return slot with an `++*mRefCount` of its own (0x800281AC..0x800281C8) and releases the return
  // slot, then releases the local again on the way out (0x80028204).
  rstl::ncrc_ptr< CAnimTreeNode > tree =
      fn_8028CAE4(GetAnimationManager().GetPtr(), animRes, CMetaAnimTreeBuildOrders::NoSpecialOrders());
  return GetTimeOfUserEvent(type, CCharAnimTime(GetAnimationDuration(anim)), tree);
}

CCharAnimTime CAnimData::GetTimeOfUserEvent(EUserEventType type, const CCharAnimTime& time) const {
  return GetTimeOfUserEvent(type, time, mAnimRoot);
}

CCharAnimTime CAnimData::GetTimeOfUserEvent(EUserEventType type, const CCharAnimTime& time,
                                            const rstl::ncrc_ptr< CAnimTreeNode >& tree) const {
  // TODO: Search the supplied tree's int POIs and reset the transient cache afterward.
  return CCharAnimTime::Infinity();
}

// Guessed name.
int CAnimData::CountUserEvents(EUserEventType type, const CCharAnimTime& time,
                               const rstl::ncrc_ptr< CAnimTreeNode >& tree) const {
  const int count = tree->GetInt32POIList(time, sInt32TransientCacheData, 16, 0, 64);
  int result = 0;
  for (int i = 0; i < count; ++i) {
    CInt32POINode& poi = sInt32TransientCacheData[i];
    if (poi.GetPoiType() == kPT_UserEvent) {
      const int value = poi.GetValue();
      if (value == static_cast< int >(type)) {
        ++result;
      }
    }
    sInt32TransientCacheData[i] =
        CInt32POINode(0xffffffff, kPT_EmptyInt32, CCharAnimTime(0.f), -1, false, 1.f, -1, 0, 0,
                      rstl::string_l("root"));
  }
  return result;
}

rstl::rc_ptr< CAnimationManager > CAnimData::GetAnimationManager() const { return mAnimMgr; }

// Guessed name.
int CAnimData::CountUserEventsForAnimation(int anim, EUserEventType type) const {
  const uint animRes = mCharInfo.GetAnimationIndexList()[anim];
  rstl::ncrc_ptr< CAnimTreeNode > tree =
      fn_8028CAE4(GetAnimationManager().GetPtr(), animRes, CMetaAnimTreeBuildOrders::NoSpecialOrders());
  return CountUserEvents(type, CCharAnimTime(GetAnimationDuration(anim)), tree);
}

/**
 * `.text 0x80027AE8` and `0x80027B44` - the two loops that make a `CModel` resident. They were
 * `src/MetroidPrime/CModelTouchParts.cpp` on master; upstream's `splits.txt` puts both ranges
 * inside this unit, so the bodies live here now and that file keeps only its note.
 *
 *   0x80027B44  0x24 bytes: touch one part
 *   0x80027AE8  0x5C bytes: touch every part, 0..mMatSets.mCount
 *
 * Each has exactly one caller in the whole DOL: 0x80027B44 from `fn_800E5D20` and 0x80027AE8 from
 * `fn_800E5C78`. `CModel`'s `mMatSets` is private and exposes no size accessor, and the offset the
 * loop bound is read from (+0x1C) is only a `mMatSets.mCount` under the layout upstream declares
 * (`CModel` is 0x34 bytes with `mMatSets` a `vector<SShader>` at 0x18), so the one word claimed is
 * spelled as the local shape `CModelTouchParts.cpp` documented - naming the member would mean an
 * additive edit to a header the matching build compiles.
 *
 * The holder parameter is `const` and that is load-bearing: retail schedules the `lwz rX,8(rX)`
 * between `mflr r0` and the prologue's `stw r0,20(r1)`, which a non-`const` parameter does not.
 * `CFilePreload::Read` has the same nine-instruction shape and the same order.
 */
struct SModelHolder {
  char x0_pad[8];
  CModel* x8_model;
  char xc_pad[4];
};

struct SShaderCount {
  char x0_pad[0x1c];
  int mNumShaders;
};

extern "C" void fn_80027B44(const SModelHolder* holder, int part) {
  holder->x8_model->Touch(part);
}

extern "C" void fn_80027AE8(const SModelHolder* holder) {
  const CModel* const model = holder->x8_model;
  const int count = reinterpret_cast< const SShaderCount* >(model)->mNumShaders;
  for (int part = 0; part < count; ++part) {
    model->Touch(part);
  }
}

void CAnimData::InitializeEffects(CStateManager& mgr, TAreaId areaId, const CVector3f& scale) {
  const CCharacterInfo::TEffectList& effects = mCharInfo.GetEffects();
  const uint effectCount = effects.size();
  for (uint i = 0; i < effectCount; ++i) {
    CCharacterInfo::TEffectList::value_type effect = effects[i];
    const uint componentCount = effect.second.size();
    for (uint j = 0; j < componentCount; ++j) {
      const CEffectComponent& component = effect.second[j];
      mParticleDB.CacheParticleDesc(component.GetParticleTag());
      {
        const CParticleData data(0, component.GetParticleTag(), component.GetSegmentId(),
                                 component.GetScale(), component.GetParentedMode());
        mParticleDB.AddParticleEffect(component.GetComponentNameHash(), component.GetFlags(),
                                      data, scale, &mgr, areaId, true, mParticleLightIdx);
      }
      mParticleDB.SetParticleEffectState(component.GetComponentNameHash(), false, &mgr);
    }
  }
}

CParticleGenInfo* CAnimData::GetFirstParticleEffect(const rstl::string& name) {
  const CCharacterInfo::TEffectList& effects = mCharInfo.GetEffects();
  CCharacterInfo::TEffectList::const_iterator it = rstl::find_by_key(effects, name);
  if (it != effects.end()) {
    const rstl::vector< CEffectComponent >& components = it->second;
    if (components.size() != 0) {
      return mParticleDB.GetParticleEffect(components[0].GetComponentNameHash());
    }
  }
  return nullptr;
}

void CAnimData::SetEffectState(const rstl::string& name, bool active, CStateManager& mgr) {
  const CCharacterInfo::TEffectList effects = mCharInfo.GetEffects();
  CCharacterInfo::TEffectList::const_iterator it = rstl::find_by_key(effects, name);
  if (it != effects.end()) {
    const rstl::vector< CEffectComponent >& components = it->second;
    rstl::vector< CEffectComponent >::const_iterator comp = components.begin();
    rstl::vector< CEffectComponent >::const_iterator end = components.end();
    for (; comp != end; ++comp) {
      mParticleDB.SetParticleEffectState(comp->GetComponentNameHash(), active, &mgr);
    }
  }
}

typedef rstl::vector< CEffectComponent, rstl::rmemory_allocator > CEffectComponentVector;
typedef rstl::pair< rstl::string, CEffectComponentVector > TEffectEntry;

/**
 * `.text 0x80027728`..`0x80027898` - `rstl::vector< CEffectComponent >`'s copy constructor and the
 * `rstl::construct` chain in front of it, for the element type of `CCharacterInfo::TEffectList`.
 *
 *   0x80027728  32 B  8 instructions  `rstl::construct<T>`
 *   0x80027748  40 B 10 instructions  `rstl::construct_impl<T>`
 *   0x80027770  72 B 18 instructions  `T::T(const T&)`
 *   0x800277B8 132 B 33 instructions  `rstl::vector<CEffectComponent>::vector(const vector&)`
 *   0x8002783C 96 B 24 instructions  `rstl::uninitialized_copy_n` over the same elements
 *
 * Same reasoning as the `mAdditiveAnims` chain above: retail's map names all five `fn_<addr>`, so
 * each body is written out under that name rather than reached through the mangled template.
 *
 * `fn_800277B8` is `rstl/vector.hpp`'s copy constructor - the mem-init list plus the
 * "empty stays empty, otherwise allocate and copy" body, which this file already spells out for
 * `rstl::vector< CPASAnimInfo >` above and which matches retail at 100% there. `mAllocator` is
 * `rmemory_allocator`, an empty struct, so the `mAllocator(other.mAllocator)` in the mem-init list
 * emits nothing; `allocate(mItems, mCapacity)` is the `count * sizeof(T)` form that lands as
 * `mCapacity * 28`, and `uninitialized_copy_n` is called rather than spelled as a loop, which is
 * why it is a separate 96-byte symbol here.
 *
 * All four of `fn_80027728`/`48`/`70`/`B8` return their `this` in `r3` (the last two instructions
 * of the ctor bodies), so they are written to return the pointer: that live range across
 * `fn_80026F68`'s call is what makes mwccceppc pick the same callee-saved registers retail used.
 */
extern "C" CEffectComponent* fn_8002783C(const CEffectComponent* src, int n, CEffectComponent* dest) {
  // `rstl/construct.hpp`'s `uninitialized_copy_n` loop, spelled out because mwccceppc emits that
  // template out of line (it has two callers in this unit) and retail inlines it here - retail's
  // 96 bytes are the counted loop plus the placement-new null test from `construct_impl`.
  const CEffectComponent* it = src;
  CEffectComponent* cur = dest;
  for (int remaining = n; remaining != 0; --remaining, ++it, ++cur) {
    new (cur) CEffectComponent(*it);
  }
  return cur;
}

extern "C" CEffectComponentVector* fn_800277B8(CEffectComponentVector* dest,
                                              const CEffectComponentVector* other) {
  dest->mCount = other->mCount;
  dest->mCapacity = other->mCapacity;
  if (other->mCount == 0 && other->mCapacity == 0) {
    dest->mItems = nullptr;
  } else {
    dest->mAllocator.allocate(dest->mItems, dest->mCapacity);
    fn_8002783C(other->mItems, dest->mCount, dest->mItems);
  }
  return dest;
}

extern "C" TEffectEntry* fn_80027770(TEffectEntry* dest, const TEffectEntry* src) {
  new (&dest->first) rstl::string(src->first);
  fn_800277B8(&dest->second, &src->second);
  return dest;
}

extern "C" void fn_80027748(void* dest, const TEffectEntry& src) {
  if (dest != nullptr) {
    fn_80027770(static_cast< TEffectEntry* >(dest), &src);
  }
}

extern "C" void fn_80027728(void* dest, const TEffectEntry& src) { fn_80027748(dest, src); }

/**
 * `.text 0x8002750C`..`0x800275B8` - the `rstl::destroy` chain for the **same** `TEffectEntry`
 * `fn_80027728`/`48`/`70`/`B8` above is the `rstl::construct` chain for, so it is written the same
 * way. Retail named all four, and the two halves are the same shape from the outside in:
 *
 *   0x800275B8  132 B  `~vector< CEffectComponent >`: free the items, then `Free(this)` on the flag
 *   0x80027550  104 B  `~TEffectEntry`: the vector at `+0x10`, then the string at `+0`
 *   0x8002752C   36 B  `rstl::destroy<T>` with the flag hard-wired to -1 (`li r4,-1`)
 *   0x8002750C   32 B  the outlined `rstl::destroy<T>` forwarder
 *
 * The `li r4,-1` at 0x80027534 is what makes 0x8002752C nine instructions and not the eight of the
 * plain forwarders above, and the two `beq`s at the top of each body are the null tests that
 * `rstl::destroy_impl` puts on `in`.
 */
extern "C" CEffectComponentVector* fn_800275B8(CEffectComponentVector* vec, int flag) {
  if (vec != nullptr) {
    CEffectComponent* first = vec->mItems;
    CEffectComponent* last = first + vec->mCount;
    for (CEffectComponent* cur = first; cur != last; ++cur) {
      cur->~CEffectComponent();
    }
    CMemory::Free(first);
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(vec);
    }
  }
  return vec;
}

extern "C" TEffectEntry* fn_80027550(TEffectEntry* entry, int flag) {
  if (entry != nullptr) {
    fn_800275B8(&entry->second, -1);
    entry->first.~basic_string();
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(entry);
    }
  }
  return entry;
}

extern "C" TEffectEntry* fn_8002752C(TEffectEntry* entry) { return fn_80027550(entry, -1); }

extern "C" TEffectEntry* fn_8002750C(TEffectEntry* entry) { return fn_8002752C(entry); }

/** 0x80027394 - the `rstl::less<rstl::string>` forwarder; see fn_8002DDA4 above for the shape. */
extern "C" bool fn_80027394(rstl::less< rstl::string >* cmp, const rstl::string& a,
                           const rstl::string& b) {
  return cmp->operator()(a, b);
}

void CAnimData::SetEffectComponentExternalParam(const rstl::string& name, int index, float value) {
  const CCharacterInfo::TEffectList effects = mCharInfo.GetEffects();
  CCharacterInfo::TEffectList::const_iterator it = rstl::find_by_key(effects, name);
  if (it != effects.end()) {
    const rstl::vector< CEffectComponent >& components = it->second;
    if (components.begin() != components.end()) {
      mParticleDB.SetParticleExternalParam(components[0].GetComponentNameHash(), index, value);
    }
  }
}

void CAnimData::SetPhase(float phase) { mAnimRoot->VSetPhase(phase); }

void CAnimData::SetKeepJSPose(bool keep) {
  if (!keep) {
    mJointData = rstl::auto_ptr< CJointData_LinearStorage >();
  } else if (mJointData.null()) {
    mJointData = rstl::auto_ptr< CJointData_LinearStorage >(rs_new CJointData_LinearStorage(
        mLayoutData->GetBodyPartSegIds().GetCount(), CJointData_LinearStorage::kAF_Heap));
    mJointData->SetZeroRotation();
    mJointData->ResetScales();
    mJointData->SetReferenceOffsets(**mLayoutData);
  }
}

// Guessed name.
void CAnimData::SetAnimationTreeLimit(int limit) { x2a8_ = limit; }

rstl::rc_ptr< CAnimationManager > CAnimData::GetAnimationManager() { return mAnimMgr; }

/**
 * `.text 0x80026EE0`..`0x80026FD8` - the out-of-line `rstl::construct` chain retail emits for
 * `rstl::pair< uint, CAdditiveAnimPlayback >`, the element type of `mAdditiveAnims`.
 *
 * Four symbols, from the outside in, all named `fn_<addr>` in `config/G2ME01/symbols.txt`:
 *
 *   0x80026EE0  32 B  8 instructions  `rstl::construct<T>`      (outlined copy)
 *   0x80026F00  40 B 10 instructions  `rstl::construct_impl<T>` (`cmplwi r3,0` / `beq`)
 *   0x80026F28  64 B 16 instructions  `T::T(const T&)`
 *   0x80026F68 100 B 25 instructions  `CAdditiveAnimPlayback::CAdditiveAnimPlayback(const&)`
 *
 * **The whole chain is spelled out by hand rather than reached through `rstl/construct.hpp`,**
 * and that is forced: retail's map leaves all four unnamed, so objdiff can only pair them with
 * `fn_<addr>` names, and a C++ spelling would mangle (`construct_impl<...>__4rstl`,
 * `__ct__Q24rstl4pair<...>FRC...`) and pair nothing. The bodies are the ones the header already
 * spells - `construct` forwards to `construct_impl`, `construct_impl` is `new (dest) T(src)` and
 * so keeps the placement-new null test, and the two copy bodies are member-wise in declaration
 * order. `rstl::rc_ptr<T>`'s inline copy is `{ mPtr, mRefCount, ++*mRefCount }`; its assignment
 * operator would instead test `mPtr != other.mPtr` and release first, so the reference-count bump
 * is written where the copy constructor has it, between the `+0x10` and `+0x18` stores.
 *
 * `CAdditiveAnimPlayback`'s members and `rstl::rc_ptr<T>`'s are private, so the two copy bodies go
 * through same-layout views with public members (`SAdditiveAnimPlayback`, 0x28;
 * `SAnimTreeRefCount`, 8) - the device the file already uses for `SModelHolder`/`SShaderCount`.
 */
struct SAnimTreeRefCount {
  const CAnimTreeNode* mPtr;
  int* mRefCount;
};

struct SAdditiveAnimPlayback {
  CAdditiveAnimationInfo x0_info;
  SAnimTreeRefCount x8_anim;
  float x10_targetWeight;
  float x14_curWeight;
  bool x18_active;
  float x1c_weightTimer;
  CAdditiveAnimPlayback::EPlaybackPhase x20_phase;
  bool x24_needsFadeOut;
};

extern "C" void fn_80026F68(SAdditiveAnimPlayback* dest, const SAdditiveAnimPlayback* src) {
  dest->x0_info = src->x0_info;
  dest->x8_anim.mPtr = src->x8_anim.mPtr;
  dest->x8_anim.mRefCount = src->x8_anim.mRefCount;
  ++*dest->x8_anim.mRefCount;
  dest->x10_targetWeight = src->x10_targetWeight;
  dest->x14_curWeight = src->x14_curWeight;
  dest->x18_active = src->x18_active;
  dest->x1c_weightTimer = src->x1c_weightTimer;
  dest->x20_phase = src->x20_phase;
  dest->x24_needsFadeOut = src->x24_needsFadeOut;
}

typedef rstl::pair< uint, CAdditiveAnimPlayback > TAdditiveAnimEntry;

extern "C" TAdditiveAnimEntry* fn_80026F28(TAdditiveAnimEntry* dest, const TAdditiveAnimEntry* src) {
  dest->first = src->first;
  fn_80026F68(reinterpret_cast< SAdditiveAnimPlayback* >(&dest->second),
              reinterpret_cast< const SAdditiveAnimPlayback* >(&src->second));
  return dest;
}

extern "C" void fn_80026F00(void* dest, const TAdditiveAnimEntry& src) {
  if (dest != nullptr) {
    fn_80026F28(static_cast< TAdditiveAnimEntry* >(dest), &src);
  }
}

extern "C" void fn_80026EE0(void* dest, const TAdditiveAnimEntry& src) {
  fn_80026F00(dest, src);
}

void CAnimData::AddAdditiveAnimation(uint idx, float weight, bool active, bool fadeOut) {
  // TODO: Create or update the character-mapped additive animation and its fade parameters.
}

void CAnimData::DelAdditiveAnimation(uint idx) {
  const uint anim = mCharInfo.GetAnimationIndexList()[idx];
  rstl::pair< uint, CAdditiveAnimPlayback >* end = mAdditiveAnims.end();
  rstl::pair< uint, CAdditiveAnimPlayback >* search = mAdditiveAnims.begin();
  while (search != end) {
    if (anim == search->first) {
      break;
    }
    ++search;
  }
  if (search != end) {
    CAdditiveAnimPlayback& playback = search->second;
    const CAdditiveAnimPlayback::EPlaybackPhase phase = playback.GetFadingMode();
    if (phase != CAdditiveAnimPlayback::kPP_FadingOut &&
        phase != CAdditiveAnimPlayback::kPP_FadedOut) {
      playback.FadeOut();
    }
  }
}

void CAnimData::DelAdditiveAnimationImmediately(uint idx) {
  const uint anim = mCharInfo.GetAnimationIndexList()[idx];
  rstl::pair< uint, CAdditiveAnimPlayback >* end = mAdditiveAnims.end();
  rstl::pair< uint, CAdditiveAnimPlayback >* search = mAdditiveAnims.begin();
  while (search != end) {
    if (anim == search->first) {
      mAdditiveAnims.erase(search);
      return;
    }
    ++search;
  }
}

float CAnimData::GetAdditiveAnimationWeight(uint idx) {
  const uint anim = mCharInfo.GetAnimationIndexList()[idx];
  rstl::pair< uint, CAdditiveAnimPlayback >* end = mAdditiveAnims.end();
  rstl::pair< uint, CAdditiveAnimPlayback >* search = mAdditiveAnims.begin();
  while (search != end) {
    if (anim == search->first) {
      return search->second.GetWeight();
    }
    ++search;
  }
  return 0.f;
}

bool CAnimData::IsAdditiveAnimationActive(uint idx) const {
  const uint anim = mCharInfo.GetAnimationIndexList()[idx];
  const rstl::pair< uint, CAdditiveAnimPlayback >* end = mAdditiveAnims.end();
  const rstl::pair< uint, CAdditiveAnimPlayback >* search = mAdditiveAnims.begin();
  while (search != end) {
    if (anim == search->first) {
      return true;
    }
    ++search;
  }
  return false;
}

rstl::rc_ptr< CAnimTreeNode > CAnimData::GetAdditiveAnimationTree(uint idx) const {
  const uint anim = mCharInfo.GetAnimationIndexList()[idx];
  const rstl::pair< uint, CAdditiveAnimPlayback >* end = mAdditiveAnims.end();
  const rstl::pair< uint, CAdditiveAnimPlayback >* search = mAdditiveAnims.begin();
  while (search != end) {
    if (anim == search->first) {
      break;
    }
    ++search;
  }
  if (search == end) {
    return rstl::rc_ptr< CAnimTreeNode >(nullptr);
  }
  return search->second.GetAnimationTree();
}

const rstl::ncrc_ptr< CAnimTreeNode >& CAnimData::GetAnimationTree() const { return mAnimRoot; }

bool CAnimData::IsAdditiveAnimation(uint idx) const {
  // TODO: Search the animation database's additive-animation information.
  return false;
}

SAdvancementResults CAnimData::AdvanceAdditiveAnim(rstl::rc_ptr< CAnimTreeNode >& anim,
                                                   CCharAnimTime time) {
  SAdvancementResults ret = anim->VAdvanceView(time);
  rstl::optional_object< rstl::ownership_transfer< IAnimReader > > simplified = anim->Simplified();
  if (simplified.valid()) {
    anim = Cast(simplified.data());
  }
  return ret;
}

CAdvancementDeltas CAnimData::UpdateAdditiveAnims(float dt) {
  rstl::pair< uint, CAdditiveAnimPlayback >* it = mAdditiveAnims.begin();
  rstl::pair< uint, CAdditiveAnimPlayback >* const begin = mAdditiveAnims.begin();
  while (it != begin + mAdditiveAnims.size()) {
    CAdditiveAnimPlayback& playback = it->second;
    playback.Update(dt);
    const CCharAnimTime remTime = playback.AnimationTree()->VGetTimeRemaining();
    const CAdditiveAnimPlayback::EPlaybackPhase phase = playback.GetFadingMode();
    if (close_enough(remTime.GetSeconds(), 0.f) && playback.IsFadeOutWhenAnimOver() != 0 &&
        phase != CAdditiveAnimPlayback::kPP_FadedOut &&
        phase != CAdditiveAnimPlayback::kPP_FadingOut) {
      playback.FadeOut();
    }
    if (phase == CAdditiveAnimPlayback::kPP_FadedOut) {
      it = mAdditiveAnims.erase(it);
    } else {
      ++it;
    }
  }
  return AdvanceAdditiveAnims(dt);
}

CAdvancementDeltas CAnimData::AdvanceAdditiveAnims(float dt) {
  CQuaternion rotDelta(CQuaternion::NoRotation());
  float posDeltaX = 0.f;
  float posDeltaY = 0.f;
  float posDeltaZ = 0.f;

  const uint count = mAdditiveAnims.size();
  for (uint i = 0; i < count; ++i) {
    CAdditiveAnimPlayback& playback = mAdditiveAnims[i].second;
    rstl::rc_ptr< CAnimTreeNode >& anim = playback.AnimationTree();

    CCharAnimTime time(dt);

    if (playback.IsLoop()) {
      while (time.GreaterThanZero() && !close_enough(time.GetSeconds(), 0.f)) {
        mPassedIntCount +=
            anim->GetInt32POIList(time, mInt32POINodes.data(), 16, mPassedIntCount, 0);
        mPassedBoolCount +=
            anim->GetBoolPOIList(time, mBoolPOINodes.data(), 8, mPassedBoolCount, 0);
        mPassedParticleCount += anim->GetParticlePOIList(time, mParticlePOINodes.data(), 64,
                                                          mPassedParticleCount, 0);
        mPassedSoundCount +=
            anim->GetSoundPOIList(time, mSoundPOINodes.data(), 48, mPassedSoundCount, 0);

        const SAdvancementResults advResult = AdvanceAdditiveAnim(anim, time);
        const SAdvancementDeltas deltas = advResult.mDeltas;
        const CQuaternion thisRot = deltas.GetOrientationDelta();

        posDeltaX += deltas.GetOffsetDelta().GetX();
        posDeltaY += deltas.GetOffsetDelta().GetY();
        posDeltaZ += deltas.GetOffsetDelta().GetZ();
        rotDelta = rotDelta * thisRot;
        time = advResult.GetRemainder();
      }
    } else {
      CCharAnimTime remTime = anim->VGetTimeRemaining();

      while (!close_enough(remTime.GetSeconds(), 0.f) && !close_enough(time.GetSeconds(), 0.f)) {
        mPassedIntCount +=
            anim->GetInt32POIList(time, mInt32POINodes.data(), 16, mPassedIntCount, 0);
        mPassedBoolCount +=
            anim->GetBoolPOIList(time, mBoolPOINodes.data(), 8, mPassedBoolCount, 0);
        mPassedParticleCount += anim->GetParticlePOIList(time, mParticlePOINodes.data(), 64,
                                                          mPassedParticleCount, 0);
        mPassedSoundCount +=
            anim->GetSoundPOIList(time, mSoundPOINodes.data(), 48, mPassedSoundCount, 0);

        const SAdvancementResults advResult = AdvanceAdditiveAnim(anim, time);
        const SAdvancementDeltas deltas = advResult.mDeltas;
        const CQuaternion thisRot = deltas.GetOrientationDelta();

        posDeltaX += deltas.GetOffsetDelta().GetX();
        posDeltaY += deltas.GetOffsetDelta().GetY();
        posDeltaZ += deltas.GetOffsetDelta().GetZ();
        rotDelta = rotDelta * thisRot;
        time = advResult.GetRemainder();

        remTime = anim->VGetTimeRemaining();
        time = CCharAnimTime(rstl::min_val(time.GetSeconds(), remTime.GetSeconds()));
      }
    }
  }

  return CAdvancementDeltas(CVector3f(posDeltaX, posDeltaY, posDeltaZ), rotDelta);
}

void CAnimData::AddAdditiveSegData(CJointData_LinearStorage& data) const {
  // TODO: Accumulate weighted additive rotations, translations and scales into joint storage.
}

/**
 * `.text 0x80025E08` and `0x80025E0C` - two four-byte thunks, each a bare `blr`.
 *
 * They were `src/rstl/Carve80025E08.c` on master; upstream's `splits.txt` puts both inside this
 * unit (its `.text` starts at 0x80025D3C, immediately after the range that file had claimed), so
 * the bodies moved here and that file was removed.
 *
 * Nothing in the DOL calls either of them with a `bl`, and they are eight bytes apart, so they are
 * the out-of-line copies a `rstl` header emits for an empty inline at file scope rather than
 * anything in `CAnimData`. `config/G2ME01/symbols.txt` carries the `fn_<addr>` placeholder for
 * both and that spelling is kept, which is also what makes them `extern "C"`: a C++ one would
 * mangle and objdiff would pair nothing.
 */
extern "C" void fn_80025E0C() {}

extern "C" void fn_80025E08() {}

// Guessed name.
int CAnimData::FindBestAnimation(const CPASAnimParmData& parms) const {
  return GetPASDatabase().FindBestAnimation(parms, -1).second;
}

// Guessed name.
void CAnimData::SetModelScale(const CVector3f& scale) {
  mUniformScale =
      close_enough(scale.GetX(), scale.GetY()) && close_enough(scale.GetX(), scale.GetZ());
  mPose.SetUniformScale(mUniformScale);
}

void CAnimData::AddAnimatedScale() {
  mAnimatedScale = true;
  mPose.AllocateScale();
}
