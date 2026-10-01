/**
 * Two retail symbols this translation unit *references* rather than defines. Both are named here
 * for the same reason and both are invisible in objdiff's percentages - see `include/Kyoto/Alloc/
 * CMemory.hpp` and `src/MetroidPrime/CConsoleOutputWindowCtor.cpp` for the same recipe.
 *
 * - `lbl_803AEDD8` is retail's own `operator new` placement string for this object: the
 *   `R_PPC_ADDR16_HA`/`R_PPC_ADDR16_LO` pair that builds `VClone`'s `__nw__FUlPCcPCc` argument
 *   resolves to `0x803AEDD8`, which is seven zero bytes. Left undefined, `rs_new` expands to
 *   `new ("\?\?(\?\?)", nullptr)`, mwcceppc puts that literal in this object's own
 *   `@stringBase0`, the object grows a 7-byte `.rodata` section, and mwldeppc appends it to the
 *   global string pool - which shifts every later pool entry and puts our copy at 0x803AEDF0
 *   instead. Declared, never defined: the bytes live in `build/G2ME01/obj/auto_*_rodata.o`,
 *   which precedes this object in the link order.
 * - `lbl_8041E368` is the `1.f` of `VGetContributionOfHighestInfluence`'s
 *   `CAnimTreeEffectiveContribution(1.f, ...)`, `.sdata2:0x8041E368`, `3f800000`. Spelled as the
 *   literal, mwcceppc emits a 4-byte `@NNN` word in this object's `.sdata2`, and since no unit
 *   claims that range the linked `.sdata2` grows by 8 bytes - which moves `.sbss2`'s address and
 *   with it every `bl` displacement in the DOL. The Gekko build names it; `TARGET_PC` has no such
 *   symbol to name, so it takes the same 1.0f as a literal.
 */
extern "C" {
extern const char lbl_803AEDD8[];
#if defined(__MWERKS__)
extern const float lbl_8041E368;
#endif
}

// `rs_new` is expanded inside `VClone` below, and `CMEMORY_NEW_FILE` has to be set before any
// include; see the `CMEMORY_NEW_FILE` branch in `Kyoto/Alloc/CMemory.hpp`.
#define CMEMORY_NEW_FILE lbl_803AEDD8

#include "Kyoto/Animation/CAnimTreeAnimReaderContainer.hpp"

SAdvancementResults CAnimTreeAnimReaderContainer::VAdvanceView(const CCharAnimTime& time) {
  return mReader->VAdvanceView(time);
}

CCharAnimTime CAnimTreeAnimReaderContainer::VGetTimeRemaining() const {
  return mReader->VGetTimeRemaining();
}

CSteadyStateAnimInfo CAnimTreeAnimReaderContainer::VGetSteadyStateAnimInfo() const {
  return mReader->VGetSteadyStateAnimInfo();
}

bool CAnimTreeAnimReaderContainer::VHasOffset(const CSegId& seg) const {
  return mReader->VHasOffset(seg);
}

CVector3f CAnimTreeAnimReaderContainer::VGetOffset(const CSegId& seg) const {
  return mReader->VGetOffset(seg);
}

CQuaternion CAnimTreeAnimReaderContainer::VGetRotation(const CSegId& seg) const {
  return mReader->VGetRotation(seg);
}

uint CAnimTreeAnimReaderContainer::VGetBoolPOIList(const CCharAnimTime& time, CBoolPOINode* listOut,
                                                   uint capacity, uint iterator,
                                                   int additive) const {
  return mReader->GetBoolPOIList(time, listOut, capacity, iterator, additive);
}

uint CAnimTreeAnimReaderContainer::VGetInt32POIList(const CCharAnimTime& time,
                                                    CInt32POINode* listOut, uint capacity,
                                                    uint iterator, int additive) const {
  return mReader->GetInt32POIList(time, listOut, capacity, iterator, additive);
}

uint CAnimTreeAnimReaderContainer::VGetParticlePOIList(const CCharAnimTime& time,
                                                       CParticlePOINode* listOut, uint capacity,
                                                       uint iterator, int additive) const {
  return mReader->GetParticlePOIList(time, listOut, capacity, iterator, additive);
}

uint CAnimTreeAnimReaderContainer::VGetSoundPOIList(const CCharAnimTime& time,
                                                    CSoundPOINode* listOut, uint capacity,
                                                    uint iterator, int additive) const {
  return mReader->GetSoundPOIList(time, listOut, capacity, iterator, additive);
}

bool CAnimTreeAnimReaderContainer::VGetBoolPOIState(uint nameHash) const {
  return mReader->VGetBoolPOIState(nameHash);
}

s32 CAnimTreeAnimReaderContainer::VGetInt32POIState(uint nameHash) const {
  return mReader->VGetInt32POIState(nameHash);
}

CParticleData::EParentedMode
CAnimTreeAnimReaderContainer::VGetParticlePOIState(uint nameHash) const {
  return mReader->VGetParticlePOIState(nameHash);
}

void CAnimTreeAnimReaderContainer::VGetSegStatementSet(const CSegIdList& list,
                                                       CSegStatementSet& setOut) const {
  mReader->VGetSegStatementSet(list, setOut);
}

void CAnimTreeAnimReaderContainer::VGetSegStatementSet(const CSegIdList& list,
                                                       CSegStatementSet& setOut,
                                                       const CCharAnimTime& time) const {
  mReader->VGetSegStatementSet(list, setOut, time);
}

void CAnimTreeAnimReaderContainer::VGetSegData(const CCharLayoutInfo& layout,
                                               CJointData_LinearStorage& data,
                                               const CCharAnimTime& time) const {
  mReader->VGetSegData(layout, data, time);
}

void CAnimTreeAnimReaderContainer::VGetSegData(const CCharLayoutInfo& layout,
                                               CJointData_LinearStorage& data) const {
  mReader->VGetSegData(layout, data);
}

rstl::ownership_transfer< IAnimReader > CAnimTreeAnimReaderContainer::VClone() const {
  return rs_new CAnimTreeAnimReaderContainer(mReader->Clone(), mName, mAnimDbIdx);
}

CAnimTreeEffectiveContribution
CAnimTreeAnimReaderContainer::VGetContributionOfHighestInfluence() const {
#if defined(__MWERKS__)
  return CAnimTreeEffectiveContribution(lbl_8041E368, mName,
                                        mReader->GetSteadyStateAnimInfo(),
                                        mReader->GetTimeRemaining(), mAnimDbIdx);
#else
  return CAnimTreeEffectiveContribution(1.f, mName, mReader->GetSteadyStateAnimInfo(),
                                        mReader->GetTimeRemaining(), mAnimDbIdx);
#endif
}

rstl::optional_object< rstl::ownership_transfer< IAnimReader > >
CAnimTreeAnimReaderContainer::VSimplified() {
  return rstl::optional_object_null();
}

void CAnimTreeAnimReaderContainer::VSetPhase(float phase) { mReader->VSetPhase(phase); }

SAdvancementResults
CAnimTreeAnimReaderContainer::VGetAdvancementResults(const CCharAnimTime& time,
                                                     const CCharAnimTime& startOffset) const {
  return mReader->VGetAdvancementResults(time, startOffset);
}

rstl::rc_ptr< CAnimTreeNode > CAnimTreeAnimReaderContainer::VGetBestUnblendedChild() const {
  return rstl::rc_ptr< CAnimTreeNode >();
}

void CAnimTreeAnimReaderContainer::VGetWeightedReaders(
    float weight, rstl::reserved_vector< rstl::pair< float, IAnimReader* >, 16 >& out) const {
  // Spelled as the two statements `push_back` is, and not as `push_back`.
  // `pair<float, IAnimReader*>` is not one of the types `rstl/construct.hpp` declares trivially
  // constructible, so `push_back` reaches `new (dest) T(src)` and mwcceppc emits placement new's
  // null guard - `addic. r3,r3,4 ; beq` - which retail's ten instructions from 0x802A5E30 do not
  // have. Assigning through `data()[size()]` and bumping `mCount` (public, for the reason the
  // header gives) is the same work without the guard: `stfs f1,4(r3) ; stw r5,8(r3)` and then the
  // reload of the count, which is all retail has.
  out.data()[out.size()] =
      rstl::pair< float, IAnimReader* >(weight, const_cast< IAnimReader* >(&*mReader));
  ++out.mCount;
}
