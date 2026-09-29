#include "Kyoto/Animation/CPASDatabase.hpp"
#include "Kyoto/Animation/CPASAnimParmData.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/algorithm.hpp"

template <>
rstl::vector< CPASAnimState >::iterator
rstl::vector< CPASAnimState >::insert(iterator it, const CPASAnimState& value);

CPASDatabase::CPASDatabase(CInputStream& in) : mDefaultState(-1) {
  in.Get< int >();

  const uint stateCount = in.Get< uint >();
  mStates.reserve(stateCount);
  const int defaultState = in.Get< int >();
  for (int i = 0; i < stateCount; i++) {
    AddAnimState(CPASAnimState(in));
  }

  if (stateCount != 0) {
    SetDefaultState(defaultState);
  }
}

rstl::pair< float, int > CPASDatabase::FindBestAnimation(const CPASAnimParmData& data,
                                                         int ignoreAnim) const {
  rstl::vector< CPASAnimState >::const_iterator it =
      rstl::binary_find(mStates.begin(), mStates.end(), CPASAnimState(data.GetStateId()));
  if (it != mStates.end()) {
    CRandom16 random(0x1234);
    return it->FindBestAnimation(data.GetAnimParmData(), random, ignoreAnim);
  }
  return rstl::pair< float, int >(0.f, -1);
}

rstl::pair< float, int > CPASDatabase::FindBestAnimation(const CPASAnimParmData& data,
                                                         CRandom16& random, int ignoreAnim) const {
  rstl::vector< CPASAnimState >::const_iterator it =
      rstl::binary_find(mStates.begin(), mStates.end(), CPASAnimState(data.GetStateId()));
  if (it != mStates.end()) {
    return it->FindBestAnimation(data.GetAnimParmData(), random, ignoreAnim);
  }
  return rstl::pair< float, int >(0.f, -1);
}

bool CPASDatabase::HasState(int id) const {
  rstl::vector< CPASAnimState >::const_iterator it =
      rstl::binary_find(mStates.begin(), mStates.end(), CPASAnimState(id));
  return it != mStates.end();
}

const CPASAnimState* CPASDatabase::GetAnimState(int id) const {
  return rstl::binary_find(mStates.begin(), mStates.end(), CPASAnimState(id)).get_pointer();
}

void CPASDatabase::SetDefaultState(int state) { mDefaultState = state; }

// Retail has insert and its helpers right after AddAnimState, which MWCC's reverse source order
// gives a definition placed here; the implicit instantiation would land in the trailing pool.
template <>
rstl::vector< CPASAnimState >::iterator
rstl::vector< CPASAnimState >::insert(iterator it, const CPASAnimState& value) {
  iterator::difference_type diff = it.operator->() - mItems;
  const_counting_iterator< CPASAnimState > in(&value, 0);
  insert_into(it, 1, in);
  return iterator(mItems) + diff;
}

void CPASDatabase::AddAnimState(const CPASAnimState& state) {
  const rstl::vector< CPASAnimState >::iterator it =
      rstl::lower_bound(mStates.begin(), mStates.end(), state);
  mStates.insert(it, state);
}
