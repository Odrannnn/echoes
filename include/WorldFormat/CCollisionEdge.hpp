#ifndef _CCOLLISIONEDGE
#define _CCOLLISIONEDGE

#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/construct.hpp"

class CCollisionEdge {
public:
  CCollisionEdge(ushort index1, ushort index2) : mIndex1(index1), mIndex2(index2) {}
  explicit CCollisionEdge(CInputStream& in) {
    mIndex1 = in.Get< ushort >();
    mIndex2 = in.Get< ushort >();
  }

  ushort GetVertIndex1() const { return mIndex1; }
  ushort GetVertIndex2() const { return mIndex2; }

private:
  ushort mIndex1;
  ushort mIndex2;
};
CHECK_SIZEOF(CCollisionEdge, 4)

// The class declares no destructor, and retail's bytes agree: `rstl::vector<CCollisionEdge>`'s
// destructor, called by `COBBTree::SIndexData::~SIndexData` (retail 0x8024E7B4) with r3 =
// `this + 0x40`, is the 0x54-byte `fn_8024E8B4` at 0x8024E8B4 - instruction-for-instruction the
// same function as the instantiations retail does take the "nothing to tear down" path for
// (`rstl::vector<ushort>`'s, called from the same destructor at `this + 0x50`). Without this
// specialisation the primary template says false, `destroy_impl(begin, end)`'s loop survives,
// and the instantiation is 0x84 bytes instead.
namespace rstl {
template <>
struct is_trivially_destructible< CCollisionEdge > {
  enum { value = true };
};
} // namespace rstl

#endif // _CCOLLISIONEDGE
