#include "Kyoto/Animation/CAnimSourceReaderBase.hpp"

#include "Kyoto/Animation/CAnimPOIData.hpp"

#include "Kyoto/Animation/CBoolPOINode.hpp"
#include "Kyoto/Animation/CInt32POINode.hpp"
#include "Kyoto/Animation/CParticlePOINode.hpp"
#include "Kyoto/Animation/CSoundPOINode.hpp"
#include "rstl/math.hpp"

#ifdef TARGET_PC
// CAnimTreeTweenBase.cpp is not part of the port build, so keep this definition here for its
// _getPOIList call. The matching build gets the retail-unit definition from that source file.
CBoolPOINode CBoolPOINode::CopyNodeMinusStartTime(const CBoolPOINode& node,
                                                  const CCharAnimTime& startTime) {
  return CBoolPOINode(node.GetNameHash(), node.GetPoiType(), node.GetTime() - startTime,
                      node.GetIndex(), node.GetSaveState(), node.GetWeight(),
                      node.GetCharacterIndex(), node.GetFlags(), node.GetValue());
}
#endif

template < class T >
uint _getPOIList(const CCharAnimTime& time, T* listOut, uint capacity, uint iterator, int additive,
                 const rstl::vector< T >& stream, const CCharAnimTime& curTime,
                 const IAnimSourceInfo& sourceInfo, int passedCount) {
  uint ret = 0;
  int count = stream.size();
  if (count > 0) {
    const CCharAnimTime duration = sourceInfo.GetAnimationDuration();
    CCharAnimTime totalTime = curTime + time;
    CCharAnimTime endTime = rstl::min_val(duration, totalTime);
    if (passedCount < count) {
      int index = passedCount;
      CCharAnimTime nodeTime(stream[index].GetTime());
      while (index < count && nodeTime <= endTime) {
        const T& node = stream[index];
        if (ret + iterator < capacity) {
          listOut[iterator + ret] = T::CopyNodeMinusStartTime(node, curTime);
          ++ret;
        }
        ++index;
        if (index < count) {
          nodeTime = stream[index].GetTime();
        }
      }
    }
  }
  return ret;
}

uint CAnimSourceReaderBase::VGetBoolPOIList(const CCharAnimTime& time, CBoolPOINode* listOut,
                                            uint capacity, uint iterator, int additive) const {
  return _getPOIList(time, listOut, capacity, iterator, additive,
                     mPOIData->GetBoolPOIStream(), mCurTime, *mSourceInfo, mPassedBoolCount);
}

uint CAnimSourceReaderBase::VGetInt32POIList(const CCharAnimTime& time, CInt32POINode* listOut,
                                             uint capacity, uint iterator, int additive) const {
  return _getPOIList(time, listOut, capacity, iterator, additive,
                     mPOIData->GetInt32POIStream(), mCurTime, *mSourceInfo, mPassedIntCount);
}

uint CAnimSourceReaderBase::VGetParticlePOIList(const CCharAnimTime& time, CParticlePOINode* listOut,
                                                uint capacity, uint iterator, int additive) const {
  return _getPOIList(time, listOut, capacity, iterator, additive,
                     mPOIData->GetParticlePOIStream(), mCurTime, *mSourceInfo, mPassedParticleCount);
}

uint CAnimSourceReaderBase::VGetSoundPOIList(const CCharAnimTime& time, CSoundPOINode* listOut,
                                             uint capacity, uint iterator, int additive) const {
  return _getPOIList(time, listOut, capacity, iterator, additive,
                     mPOIData->GetSoundPOIStream(), mCurTime, *mSourceInfo, mPassedSoundCount);
}

bool CAnimSourceReaderBase::VGetBoolPOIState(uint nameHash) const {
  for (int i = 0; i < mBoolStates.size(); ++i) {
    if (mBoolStates[i].first == nameHash) {
      return mBoolStates[i].second;
    }
  }
  return false;
}

s32 CAnimSourceReaderBase::VGetInt32POIState(uint nameHash) const {
  for (int i = 0; i < mInt32States.size(); ++i) {
    if (mInt32States[i].first == nameHash) {
      return mInt32States[i].second;
    }
  }
  return 0;
}

CParticleData::EParentedMode CAnimSourceReaderBase::VGetParticlePOIState(uint nameHash) const {
  for (int i = 0; i < mParticleStates.size(); ++i) {
    if (mParticleStates[i].first == nameHash) {
      return mParticleStates[i].second;
    }
  }
  return CParticleData::kPM_Initial;
}

void CAnimSourceReaderBase::UpdatePOIStates() {
  const rstl::vector< CBoolPOINode >& boolNodes = mPOIData->GetBoolPOIStream();
  const rstl::vector< CInt32POINode >& int32Nodes = mPOIData->GetInt32POIStream();
  const rstl::vector< CParticlePOINode >& particleNodes = mPOIData->GetParticlePOIStream();
  const rstl::vector< CSoundPOINode >& soundNodes = mPOIData->GetSoundPOIStream();
  int boolCount = boolNodes.size();
  int int32Count = int32Nodes.size();
  int particleCount = particleNodes.size();
  int soundCount = soundNodes.size();
  while (mPassedBoolCount < boolCount && boolNodes[mPassedBoolCount].GetTime() <= mCurTime) {
    const CBoolPOINode& node = boolNodes[mPassedBoolCount];
    int index = node.GetIndex();
    if (index >= 0 && index < mBoolStates.size()) {
      mBoolStates[index] = rstl::pair< uint, bool >(mBoolStates[index].first, node.GetValue());
    }
    ++mPassedBoolCount;
  }
  while (mPassedIntCount < int32Count && int32Nodes[mPassedIntCount].GetTime() <= mCurTime) {
    const CInt32POINode& node = int32Nodes[mPassedIntCount];
    int index = node.GetIndex();
    if (index >= 0 && index < mInt32States.size()) {
      mInt32States[index] = rstl::pair< uint, int >(mInt32States[index].first, node.GetValue());
    }
    ++mPassedIntCount;
  }
  while (mPassedParticleCount < particleCount &&
         particleNodes[mPassedParticleCount].GetTime() <= mCurTime) {
    const CParticlePOINode& node = particleNodes[mPassedParticleCount];
    int index = node.GetIndex();
    if (index >= 0 && index < mParticleStates.size()) {
      mParticleStates[index] = rstl::pair< uint, CParticleData::EParentedMode >(
          mParticleStates[index].first, node.GetParticleData().GetParentedMode());
    }
    ++mPassedParticleCount;
  }
  while (mPassedSoundCount < soundCount && soundNodes[mPassedSoundCount].GetTime() <= mCurTime) {
    ++mPassedSoundCount;
  }
}

rstl::set< rstl::pair< uint, int > > CAnimSourceReaderBase::GetUniqueBoolPOIs() const {
  const rstl::vector< CBoolPOINode >& nodes = mPOIData->GetBoolPOIStream();
  int count = nodes.size();
  rstl::set< rstl::pair< uint, int > > ret;
  for (int i = 0; i < count; ++i) {
    const CBoolPOINode& node = nodes[i];
    if (node.GetSaveState()) {
      ret.insert(rstl::pair< uint, int >(node.GetNameHash(), node.GetIndex()));
    }
  }
  return ret;
}

rstl::set< rstl::pair< uint, int > > CAnimSourceReaderBase::GetUniqueInt32POIs() const {
  const rstl::vector< CInt32POINode >& nodes = mPOIData->GetInt32POIStream();
  int count = nodes.size();
  rstl::set< rstl::pair< uint, int > > ret;
  for (int i = 0; i < count; ++i) {
    const CInt32POINode& node = nodes[i];
    if (node.GetSaveState()) {
      ret.insert(rstl::pair< uint, int >(node.GetNameHash(), node.GetIndex()));
    }
  }
  return ret;
}

rstl::set< rstl::pair< uint, int > > CAnimSourceReaderBase::GetUniqueParticlePOIs() const {
  const rstl::vector< CParticlePOINode >& nodes = mPOIData->GetParticlePOIStream();
  int count = nodes.size();
  rstl::set< rstl::pair< uint, int > > ret;
  for (int i = 0; i < count; ++i) {
    const CParticlePOINode& node = nodes[i];
    if (node.GetSaveState()) {
      ret.insert(rstl::pair< uint, int >(node.GetNameHash(), node.GetIndex()));
    }
  }
  return ret;
}

void CAnimSourceReaderBase::PostConstruct(const CCharAnimTime& time) {
  mPassedBoolCount = 0;
  mPassedIntCount = 0;
  mPassedParticleCount = 0;
  mPassedSoundCount = 0;
  const rstl::set< rstl::pair< uint, int > > boolPOIs = GetUniqueBoolPOIs();
  const rstl::set< rstl::pair< uint, int > > int32POIs = GetUniqueInt32POIs();
  const rstl::set< rstl::pair< uint, int > > particlePOIs = GetUniqueParticlePOIs();
  const int boolCount = boolPOIs.size();
  const int int32Count = int32POIs.size();
  const int particleCount = particlePOIs.size();
  mBoolStates.resize(boolCount, rstl::pair< uint, bool >(0, false));
  mInt32States.resize(int32Count, rstl::pair< uint, int >(0, 0));
  mParticleStates.resize(particleCount,
                         rstl::pair< uint, CParticleData::EParentedMode >(
                             0, CParticleData::kPM_Initial));
  for (rstl::set< rstl::pair< uint, int > >::const_iterator it = boolPOIs.begin();
       it != boolPOIs.end(); ++it) {
    int index = it->second;
    uint name = it->first;
    if (index >= 0 && index < mBoolStates.size()) {
      mBoolStates[index] = rstl::pair< uint, bool >(name, false);
    }
  }
  for (rstl::set< rstl::pair< uint, int > >::const_iterator it = int32POIs.begin();
       it != int32POIs.end(); ++it) {
    int index = it->second;
    uint name = it->first;
    if (index >= 0 && index < mInt32States.size()) {
      mInt32States[index] = rstl::pair< uint, int >(name, 0);
    }
  }
  for (rstl::set< rstl::pair< uint, int > >::const_iterator it = particlePOIs.begin();
       it != particlePOIs.end(); ++it) {
    int index = it->second;
    uint name = it->first;
    if (index >= 0 && index < mParticleStates.size()) {
      mParticleStates[index] =
          rstl::pair< uint, CParticleData::EParentedMode >(name, CParticleData::kPM_Initial);
    }
  }
  CCharAnimTime remaining = time;
  if (remaining.GreaterThanZero()) {
    while (remaining.GreaterThanZero()) {
      remaining = VAdvanceView(remaining).GetRemainder();
    }
  } else {
    UpdatePOIStates();
    if (!time.GreaterThanZero()) {
      mPassedBoolCount = 0;
      mPassedIntCount = 0;
      mPassedParticleCount = 0;
      mPassedSoundCount = 0;
    }
  }
}
