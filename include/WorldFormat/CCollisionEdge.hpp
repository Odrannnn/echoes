#ifndef _CCOLLISIONEDGE
#define _CCOLLISIONEDGE

#include "Kyoto/Streams/CInputStream.hpp"
#include "rstl/construct.hpp"

// Retail reads a `CCollisionEdge` from a stream out of line: its per-element helper
// `fn_8024747C` (0x8024747C, 0x2C bytes) is called from
// `rstl::vector<CCollisionEdge>::vector(CInputStream&, const Alloc&)` (retail 0x8024F0FC)
// with a stack temporary as the destination, and the temporary is then copied into the slot -
// 184 bytes. Defined in the class, this constructor is implicitly inline, mwcceppc inlines it
// and no symbol survives, so the stream constructor is 464 bytes instead. `#pragma dont_inline`
// around the class was measured and changes nothing (still 464, no out-of-line copy emitted);
// the call only appears if the constructor is defined out of class.
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
//
// The same holds for the copy: retail's copy constructor `fn_8024E998` (0x8024E998) copies with no
// per-element null test and unrolls eight elements per iteration, and its `reserve`
// `fn_8024F6D8` (0x8024F6D8) is 172 bytes against our 180 for the same reason - the placement new
// of the primary template's `construct_impl` tests the destination inside the loop. The class has
// an implicit copy assignment operator, so the trait's `*dest = src` is the whole of `T(src)`.
namespace rstl {
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CCollisionEdge)
} // namespace rstl

#endif // _CCOLLISIONEDGE
