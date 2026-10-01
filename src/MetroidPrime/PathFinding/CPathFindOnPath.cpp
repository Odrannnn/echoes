// `CPathFindSearch::OnPath` and the four functions it calls, for the host port.
//
// **Why these five and not `CPathFindSearch.cpp`.**  `CPatterned::NoPathNodes`
// (src/MetroidPrime/Enemies/CPatternedAiFunctions.cpp:233) asks its `CPathFindSearch*` whether
// the actor's own translation sits on a walkable region, and that call is the only reason the
// port's link asks for `CPathFindSearch::OnPath(CVector3f const&) const` - it is
// `_ZNK15CPathFindSearch6OnPathERK9CVector3f`, in
// `docs/research/port_link_baseline.txt` as one of 250 undefined symbols.  The body is already
// written, in `src/MetroidPrime/PathFinding/CPathFindSearch.cpp:360`, but that file is a
// `NonMatching` unit (configure.py:535) covering the whole search: the 2520-byte A* `Search`, the
// second `Search`, `NearlyOnPath`, `GetHeightOfPointAboveMesh`, the `CPFOpenList` ctor and two
// un-named retail functions `dtk` could not pair.  Listing it is measured at 12 symbols opened
// and none closed (`tools/check_files_cmake.py`'s EXCLUDED entry), so it is a net rise of the
// kind this port never wants.  Same arrangement as `CGameAreaSetAreaAttributes.cpp` and
// `CGrappleArmReturnToDefault.cpp`: one closed group per file, so the undefined count can only
// fall.
//
// **The closure, and why it stops at five.**  `OnPath` is not self-contained; the four
// callees below are the whole of it, and every one of them is *already* in the port's link or in
// this file:
//
//   OnPath -> CPFArea::FindRegions(regions, CVector3f const&, uint, uint, bool)
//           -> CPFArea::GetOctreeRegionList(CVector3f const&)
//              -> close_enough(CVector3f const&, CVector3f const&, float)   [CloseEnough.cpp,
//                                                                              listed in
//                                                                              files.cmake]
//              -> CPFAreaOctree::GetRegionList(CVector3f const&)  -> GetChildIndex  [this file]
//           -> CPFRegion::IsPointInside / GetObstructionCount / PointHeight
//                                                          [CPathFindRegion.cpp, listed in
//                                                           files.cmake]
//
// and `mArea->GetTransform().TransposeMultiply(point)` is inline in `CTransform4f.hpp`.  So this
// file adds no new undefined symbol at all and closes one: the port's gap goes 250 -> 249.  A
// `CPFArea` is never constructed on the host - `CPFArea`'s constructor is in the same
// `NonMatching` `CPathFindArea.cpp` and is not listed either - so `mArea` is null whenever
// `OnPath` runs today and the `!mArea` early return is what it takes.  That is retail's own
// first statement, unchanged, not a stand-in.
//
// **Every body here is the one the decompilation already wrote**, copied verbatim from
// `CPathFindSearch.cpp` and `CPathFindArea.cpp`; nothing is re-derived for the host.  Four of
// the five are at 100.00% against retail in `build/report.json` - `FindRegions` 328 B,
// `GetOctreeRegionList` 124 B, `GetRegionList` 96 B, `GetChildIndex` 72 B - and `OnPath` itself
// is 96.29% of 232 B.
//
// **This file is not a `configure.py` unit**, so the matching build never compiles it and
// `main.dol` is byte-identical with or without it.  It duplicates five symbols that
// `CPathFindSearch.cpp` and `CPathFindArea.cpp` also define, which is safe only because neither
// is in `files.cmake`; when either becomes listable this file's copies must go with it, exactly
// as `tools/check_files_cmake.py`'s EXCLUDED entries say for `rstl_string_l` and
// `CGrappleArm.cpp`.
#include "MetroidPrime/PathFinding/CPathFindArea.hpp"

#include "Kyoto/Math/CloseEnough.hpp"

#include "MetroidPrime/PathFinding/CPathFindSearch.hpp"

CPathFindSearch::EResult CPathFindSearch::OnPath(const CVector3f& point) const {
  if (!mArea) {
    return kR_InvalidArea;
  }
  CVector3f localPoint = mArea->GetTransform().TransposeMultiply(point);
  if (!(mFlags & 2) && !(mFlags & 4)) {
    localPoint[kDZ] += 0.5f;
  }
  rstl::reserved_vector< CPFRegion*, 8 > regions;
  if (mArea->FindRegions(regions, localPoint, mFlags, mIndexMask, false) == 0) {
    return kR_NoSourcePoint;
  }
  return kR_Success;
}

rstl::prereserved_vector< CPFRegion* >* CPFArea::GetOctreeRegionList(const CVector3f& point) {
  if (mHasCachedRegionList && close_enough(point, mCachedRegionListPoint)) {
    return mCachedRegionList;
  }
  return mOctree.back().GetRegionList(point);
}

int CPFArea::FindRegions(rstl::reserved_vector< CPFRegion*, 8 >& regions, const CVector3f& point,
                         uint flags, uint indexMask, bool ignoreObstructions) {
  rstl::prereserved_vector< CPFRegion* >* list = GetOctreeRegionList(point);
  for (int i = 0; i < list->size(); ++i) {
    CPFRegion* region = (*list)[i];
    if ((region->GetFlags() & 0xff & flags) && ((region->GetFlags() >> 16) & 0xff & indexMask) &&
        region->IsPointInside(point) &&
        (ignoreObstructions ||
         (region->GetObstructionCount(kPFO_Unknown2) <= 0 &&
          ((flags & 0x100) == 0 || region->GetObstructionCount(kPFO_Unknown0) <= 0) &&
          ((flags & 0x200) == 0 || region->GetObstructionCount(kPFO_Unknown1) <= 0))) &&
        ((flags & 6) || region->PointHeight(point) < 3.f)) {
      regions.push_back(region);
      if (regions.size() == regions.capacity()) {
        break;
      }
    }
  }
  return regions.size();
}

rstl::prereserved_vector< CPFRegion* >* CPFAreaOctree::GetRegionList(const CVector3f& point) {
  if (mIsLeaf) {
    return &mRegions;
  }
  return mChildren[GetChildIndex(point)]->GetRegionList(point);
}

uint CPFAreaOctree::GetChildIndex(const CVector3f& point) const {
  uint index = 0;
  if (point[kDX] > mCenter[kDX]) {
    index = 1;
  }
  if (point[kDY] > mCenter[kDY]) {
    index |= 2;
  }
  if (point[kDZ] > mCenter[kDZ]) {
    index |= 4;
  }
  return index;
}
