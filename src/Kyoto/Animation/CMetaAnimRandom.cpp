// Retail's object defines `rstl::uninitialized_copy_n` (80 bytes) out of line, immediately after
// the vector copy constructor that calls it (0x802956DC -> 0x80295760), so that copy is 132 bytes
// and holds a `bl` instead of the loop. The project-wide inline_max_size(125) inlines it, which
// leaves the object with neither the call nor the function. 121..125 is the measured window in
// which uninitialized_copy_n stays out of line and every other function in the unit is still
// expanded: 120 and below also outlines `destroy_impl`, leaving `destroy` a 0x38-byte forwarder
// (40.25%), and 122 and above inlines the copy (copy ctor 46.36%, uninitialized_copy_n 0%).
#pragma inline_max_size(121)
#include "Kyoto/Animation/CMetaAnimRandom.hpp"
#include "Kyoto/Animation/CAnimSysContext.hpp"
#include "Kyoto/Animation/CAnimTreeNode.hpp"
#include "Kyoto/Animation/CMetaAnimFactory.hpp"
#include "Kyoto/CRandom16.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "Kyoto/Streams/COutputStream.hpp"
#include "rstl/rc_ptr.hpp"

CMetaAnimRandom::CMetaAnimRandom(CInputStream& in) : mRandomData(CreateRandomData(in)) {}

rstl::ncrc_ptr< CAnimTreeNode >
CMetaAnimRandom::VGetAnimationTree(const CAnimSysContext& animSys,
                                   const CMetaAnimTreeBuildOrders& orders) const {
  const int r = animSys.GetRandomNumberGenerator().Range(1, 100);

  CMetaAnimRandom::RandomData::const_iterator rd = mRandomData.begin();
  bool found = false;
  while (!found) {
    if (r <= rd->second) {
      found = true;
    } else {
      rd++;
    }
  }

  const rstl::ncrc_ptr< CAnimTreeNode >& tree = rd->first->GetAnimationTree(animSys, orders);
  return tree;
}

void CMetaAnimRandom::GetUniquePrimitives(rstl::set< CPrimitive >& primsOut) const {
  CMetaAnimRandom::RandomData::const_iterator it = mRandomData.begin();
  CMetaAnimRandom::RandomData::const_iterator end = mRandomData.end();
  for (; it != end; ++it)
    it->first->GetUniquePrimitives(primsOut);
}

void CMetaAnimRandom::WriteAnimData(COutputStream& out) const {
  CMetaAnimRandom::RandomData::const_iterator it = mRandomData.begin();
  CMetaAnimRandom::RandomData::const_iterator end = mRandomData.end();
  out.WriteInt32(mRandomData.size());
  while (it != end) {
    rstl::rc_ptr< IMetaAnim > anim = it->first;
    int weight = it->second;
    anim->PutTo(out);
    out.WriteLong(weight);
    ++it;
  }
}

CMetaAnimRandom::RandomData CMetaAnimRandom::CreateRandomData(CInputStream& in) {
  CMetaAnimRandom::RandomData ret;
  int randCount = in.Get< int >();
  ret.reserve(randCount);

  for (int i = 0; i < randCount; ++i) {
    rstl::rc_ptr< IMetaAnim > metaAnim = CMetaAnimFactory::CreateMetaAnim(in);
    ret.push_back_unsafe(rstl::pair< rstl::rc_ptr< IMetaAnim >, int >(metaAnim, in.ReadInt32()));
  }

  return ret;
}
