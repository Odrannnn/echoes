#include "MetroidPrime/Player/CStaticInterference.hpp"

#include "Kyoto/Math/CMath.hpp"
#include "rstl/math.hpp"

// Retail interleaves these out-of-line vector instantiations with the source functions - reserve
// right after the constructor, which calls it, and erase right after RemoveSource, which calls it.
// mwcceppc's implicit instantiations all land in one trailing pool instead, so the unit's bytes
// come out permuted. Defining them where MWCC's reverse source order puts them where retail has
// them is the fix; the bodies are the ones in rstl/vector.hpp, unchanged.

template <>
void rstl::vector< CStaticInterferenceSource >::reserve(int newSize) {
  if (newSize <= mCapacity) {
    return;
  }

  CStaticInterferenceSource* newData;
  mAllocator.allocate(newData, newSize);
  uninitialized_copy(begin(), end(), newData);
  destroy(mItems, mItems + mCount);
  mAllocator.deallocate(mItems);
  mItems = newData;
  mCapacity = newSize;
}

CStaticInterference::CStaticInterference(int sourceCount) { sources.reserve(sourceCount); }

void CStaticInterference::AddSource(TUniqueId id, float magnitude, float duration) {
  float clampedMagnitude = CMath::Clamp(0.f, magnitude, 1.f);

  rstl::vector< CStaticInterferenceSource >::iterator search = sources.begin();
  for (; search != sources.end(); ++search) {
    if (search->GetSourceId() == id) {
      break;
    }
  }

  if (search != sources.end()) {
    search->SetIntensity(clampedMagnitude);
    search->SetTime(duration);
  } else {
    if (sources.size() < sources.capacity()) {
      sources.push_back_unsafe(CStaticInterferenceSource(id, clampedMagnitude, duration));
    }
  }
}

template <>
rstl::vector< CStaticInterferenceSource >::iterator
rstl::vector< CStaticInterferenceSource >::erase(iterator first, iterator last) {
  destroy(first, last);

  const typename iterator::difference_type tmp = first - begin();

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
rstl::vector< CStaticInterferenceSource >::iterator
rstl::vector< CStaticInterferenceSource >::erase(iterator it) {
  return erase(it, it + 1);
}

void CStaticInterference::RemoveSource(const TUniqueId id) {
  rstl::vector< CStaticInterferenceSource >::iterator search = sources.begin();
  for (; search != sources.end(); ++search) {
    if ((*search).GetSourceId() == id) {
      break;
    }
  }

  if (search != sources.end()) {
    sources.erase(search);
  }
}

float CStaticInterference::GetTotalInterference() const {
  float validAccum = 0.f;
  float invalidAccum = 0.f;

  rstl::vector< CStaticInterferenceSource >::const_iterator it = sources.begin();
  for (; it != sources.end(); ++it) {
    float v = it->GetIntensity();
    if (it->GetSourceId() == kInvalidUniqueId) {
      invalidAccum += v;
    }
    if ((*it).GetSourceId() != kInvalidUniqueId) {
      validAccum += v;
    }
  }
  if (validAccum > 0.8f) {
    validAccum = 0.8f;
  }

  return rstl::min_val(validAccum + invalidAccum, 1.f);
}

void CStaticInterference::Update(const CStateManager&, float dt) {
  rstl::vector< CStaticInterferenceSource >::iterator it = sources.begin();
  rstl::vector< CStaticInterferenceSource > toRemove;
  toRemove.reserve(sources.size());
  for (; it != sources.end(); ++it) {
    if (it->GetTime() < 0.f) {
      toRemove.push_back_unsafe(*it);
    } else {
      it->SetTime(it->GetTime() - dt);
    }
  }

  for (rstl::vector< CStaticInterferenceSource >::iterator it = toRemove.begin();
       it != toRemove.end(); ++it) {
    RemoveSource(it->GetSourceId());
  }
}
