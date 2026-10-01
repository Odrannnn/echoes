#include "MetroidPrime/Player/CPlayerEnergyDrain.hpp"

#include "MetroidPrime/CStateManager.hpp"
#include "rstl/algorithm.hpp"

// Retail interleaves the out-of-line rstl instantiations with the source functions, each right
// after its caller. mwcceppc's implicit instantiations all land in one trailing pool instead, so
// they are explicit specializations defined where reverse source order puts them where retail has
// them (see CStaticInterference.cpp); the bodies are the headers', unchanged.

template <>
void rstl::vector< CEnergyDrainSource >::reserve(int newSize) {
  if (newSize <= mCapacity) {
    return;
  }

  CEnergyDrainSource* newData;
  mAllocator.allocate(newData, newSize);
  uninitialized_copy(begin(), end(), newData);
  destroy(mItems, mItems + mCount);
  mAllocator.deallocate(mItems);
  mItems = newData;
  mCapacity = newSize;
}

typedef rstl::vector< CEnergyDrainSource >::iterator CEnergyDrainSourceIt;

template <>
CEnergyDrainSourceIt rstl::lower_bound(CEnergyDrainSourceIt start, CEnergyDrainSourceIt end,
                                       const CEnergyDrainSource& value) {
  int dist = distance(start, end);
  CEnergyDrainSourceIt it = start;
  while (dist > 0) {
    int halfDist = dist / 2;
    it = start;
    advance(it, halfDist);
    if (*it < value) {
      start = it;
      ++start;
      dist = (dist - halfDist) - 1;
    } else {
      dist = halfDist;
    }
  }
  return start;
}

CPlayerEnergyDrain::CPlayerEnergyDrain(uint numSources) : mEnergyDrainTime(0.f) {
  mSources.reserve(numSources);
}

template <>
rstl::vector< CEnergyDrainSource >::iterator rstl::vector< CEnergyDrainSource >::insert(iterator it, const CEnergyDrainSource& value) {
  iterator::difference_type diff = it.operator->() - mItems;
  const_counting_iterator< CEnergyDrainSource > in(&value, 0);
  insert_into(it, 1, in);
  return iterator(mItems) + diff;
}

bool CPlayerEnergyDrain::AddEnergyDrainSource(TUniqueId id, float intensity) {
  const CEnergyDrainSource source(id, intensity);
  rstl::vector< CEnergyDrainSource >::iterator it =
      rstl::binary_find(mSources.begin(), mSources.end(), source);
  if (it != mSources.end()) {
    it->SetEnergyDrainIntensity(intensity);
    return true;
  }

  if (mSources.size() < mSources.capacity()) {
    rstl::vector< CEnergyDrainSource >::iterator insertAt =
        rstl::lower_bound(mSources.begin(), mSources.end(), source);
    mSources.insert(insertAt, source);
    return true;
  }

  return false;
}

template <>
rstl::vector< CEnergyDrainSource >::iterator rstl::vector< CEnergyDrainSource >::erase(iterator first, iterator last) {
  destroy(first, last);

  const iterator::difference_type tmp = first - begin();

  int newCount = tmp;

  for (iterator it = last, moved = iterator(mItems + tmp); it != end();
       ++moved, ++newCount, ++it) {
    construct(&*moved, *it);
    destroy(&*it);
  }
  mCount = newCount;

  return first;
}

template <>
rstl::vector< CEnergyDrainSource >::iterator rstl::vector< CEnergyDrainSource >::erase(iterator it) {
  return erase(it, it + 1);
}

void CPlayerEnergyDrain::RemoveEnergyDrainSource(TUniqueId id) {
  const CEnergyDrainSource source(id, 0.f);
  rstl::vector< CEnergyDrainSource >::iterator it =
      rstl::binary_find(mSources.begin(), mSources.end(), source);
  if (it != mSources.end()) {
    mSources.erase(it);
  }
}

void CPlayerEnergyDrain::ProcessEnergyDrain(const CStateManager& mgr, float dt) {
  for (rstl::vector< CEnergyDrainSource >::iterator it = mSources.begin(); it != mSources.end();
       ++it) {
    if (!mgr.GetObjectById(it->GetEnergyDrainSourceId())) {
      RemoveEnergyDrainSource(it->GetEnergyDrainSourceId());
    }
  }

  mEnergyDrainTime = mSources.size() > 0 ? mEnergyDrainTime + dt : 0.f;
}

template <>
void rstl::vector< CEnergyDrainSource >::clear() {
  destroy(begin(), end());
  mCount = 0;
}

void CPlayerEnergyDrain::Clear() {
  mSources.clear();
  mEnergyDrainTime = 0.f;
}
