// Retail installs the point-search workspace through the out-of-line
// `single_ptr<CPFPointSearchState>::operator=(T* const)` (0x8014137C) rather than an inlined
// store, so ask for the out-of-line form of that one member; see include/rstl/single_ptr.hpp.
#define RSTL_SINGLE_PTR_ASSIGN_OUT_OF_LINE 1
#include "MetroidPrime/PathFinding/CPathFindArea.hpp"

#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Math/CloseEnough.hpp"
#include "Kyoto/SObjectTag.hpp"

#include "rstl/algorithm.hpp"

#include <dolphin/os.h>
#include <float.h>

// Retail compiled FLT_MAX as a literal here: this unit's `.sdata2` holds 0x7f7fffff at offset 0,
// ahead of the 3.f / 0.f / 1e-4f pool at +4 / +8 / +12, and the retail unit object references no
// `__float_max` at all. libc/float.h's `(*(float*)__float_max)` would drop the leading word and
// shift every other constant in the unit up by four bytes.
#undef FLT_MAX
#define FLT_MAX 3.402823466e+38f

// Sixteen functions this unit emits out of line are TU-local template/weak instantiations that
// `dtk` could not name, so `config/G2ME01/symbols.txt` called them `fn_8013FED0` ... `fn_8014195C`
// and objdiff paired them by name and scored every one of them 0% - including
// `rstl::vector<CPFRegionData>::reserve` / `::resize`, `uninitialized_fill_n`, `~CPFArea`'s
// members and `CPFRegionData`'s copy constructor, all of which this compiler already emits
// byte-for-byte. They are renamed in `symbols.txt` to the mangled name `nm` reads off our own
// object, which is the mechanism `docs/RUNNING_THE_DECOMP.md` documents for exactly this case
// ("An unnamed function is often a template instantiation you can identify by diffing it").
// No code changes: each renamed symbol was already at 100% byte-for-byte before the rename.

class CVParamTransfer;

// `CPFPointSearchState`'s constructor is not part of this unit's object: retail calls it out of
// line from `CPFArea`'s constructor (0x801F8B48, in the unit that owns 0x801F86A8..0x801F8B48).
// It writes the point count at +0, resizes the `SPointData` vector at +4 and the `int` vector at
// +0x14, so this is that constructor and not a guess.
extern "C" void* __nw__FUlPCcPCc(uint size, const char* file, const char* function);
extern "C" CPFPointSearchState* fn_801F8B48(CPFPointSearchState* self, int pointCount);

class CPFMemoryStream {
public:
  CPFMemoryStream(uchar* data, int size) : mData(data), mSize(size), mCurrent(data) {}
  int ReadInt32() {
    int value = *reinterpret_cast< int* >(mCurrent);
    mCurrent += sizeof(int);
    return value;
  }
  void* GetBlock(int count, int size) {
    void* block = mCurrent;
    mCurrent += count * size;
    return block;
  }
  uchar*& Cursor() { return mCurrent; }

private:
  uchar* mData;
  int mSize;
  uchar* mCurrent;
};

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

rstl::prereserved_vector< CPFRegion* >*
CPFAreaOctree::GetRegionList(const CVector3f& point) {
  if (mIsLeaf) {
    return &mRegions;
  }
  return mChildren[GetChildIndex(point)]->GetRegionList(point);
}

void CPFAreaOctree::GetRegionListList(
    rstl::reserved_vector< rstl::prereserved_vector< CPFRegion* >*, 32 >& lists,
    const CVector3f& point, float padding) {
  if (lists.size() >= lists.capacity()) {
    return;
  }
  if (mIsLeaf) {
    lists.push_back(&mRegions);
  } else {
    for (int i = 0; i < 8; ++i) {
      if (mChildren[i]->IsPointInsidePaddedAABox(point, padding)) {
        mChildren[i]->GetRegionListList(lists, point, padding);
      }
    }
  }
}

// `fn_80141594` is retail's out-of-line read of one `int` from the memory stream: 24 bytes at
// 0x80141594, called once from the constructor for `mVersion` and inlined everywhere else.
// It takes `(int& out, CPFMemoryStream& stream)` - `this` arrives in `r4`, not `r3` - so it is a
// free function over the stream, not a member. The cursor has to be bound to a reference and
// the output assigned last: written as two statements through `stream.Cursor()` the compiler
// reloads `mCurrent` after the store through `out` and scores 48% instead of 100%.
extern "C" void fn_80141594(int& out, CPFMemoryStream& stream);
extern "C" void fn_80141594(int& out, CPFMemoryStream& stream) {
  uchar*& current = stream.Cursor();
  const int value = *reinterpret_cast< const int* >(current);
  current += sizeof(int);
  out = value;
}

CPFArea::CPFArea(const rstl::auto_ptr< uchar >& data, int size)
: mBestPointDistSq(FLT_MAX)
, mClosestPoint(CVector3f::Zero())
, mCachedRegionList(nullptr)
, mCachedRegionListPoint(CVector3f::Zero())
, mHasCachedRegionList(false)
, mRegionFindCookie(0)
, mVersion(-1)
, mData(data.release())
, mTransform(CTransform4f::Identity()) {
  CPFMemoryStream stream(mData.get(), size);
  int version;
  fn_80141594(version, stream);
  mVersion = version;

  int numNodes = stream.ReadInt32();
  mNodes.set_size(numNodes);
  mNodes.set_data(static_cast< CPFNode* >(stream.GetBlock(numNodes, sizeof(CPFNode))));
  int numLinks = stream.ReadInt32();
  mLinks.set_size(numLinks);
  mLinks.set_data(static_cast< CPFLink* >(stream.GetBlock(numLinks, sizeof(CPFLink))));
  const int numRegions = stream.ReadInt32();
  mRegions.set_size(numRegions);
  mRegions.set_data(static_cast< CPFRegion* >(stream.GetBlock(numRegions, sizeof(CPFRegion))));
  mRegionData.reserve(numRegions);
  CPFRegionData dataValue = CPFRegionData();
  mRegionData.resize(numRegions, dataValue);
  int maxRegionNodes = 0;
  int i;
  for (i = 0; i < numRegions; ++i) {
    mRegions[i].Fixup(*this, maxRegionNodes);
  }
  maxRegionNodes = maxRegionNodes > 4 ? maxRegionNodes : 4;
  mPolyPoints.reserve(maxRegionNodes);

  int numWords = (numRegions * (numRegions - 1) / 2 + 31) / 32;
  mConnectionsGround.set_size(numWords);
  mConnectionsGround.set_data(static_cast< uint* >(stream.GetBlock(numWords, sizeof(uint))));
  mConnectionsFlyers.set_size(numWords);
  mConnectionsFlyers.set_data(static_cast< uint* >(stream.GetBlock(numWords, sizeof(uint))));
  stream.GetBlock(((numRegions * numRegions + 31) / 32 - numWords) * 2, sizeof(uint));

  int numRegionPtrs = stream.ReadInt32();
  mOctreeRegions.set_size(numRegionPtrs);
  mOctreeRegions.set_data(
      static_cast< CPFRegion** >(stream.GetBlock(numRegionPtrs, sizeof(CPFRegion*))));
  for (i = 0; i < numRegionPtrs; ++i) {
    CPFRegion* const& region = mOctreeRegions[i];
    mOctreeRegions[i] = &mRegions[reinterpret_cast< intptr_t >(region)];
  }
  int numOctreeNodes = stream.ReadInt32();
  mOctree.set_size(numOctreeNodes);
  mOctree.set_data(
      static_cast< CPFAreaOctree* >(stream.GetBlock(numOctreeNodes, sizeof(CPFAreaOctree))));
  for (i = 0; i < numOctreeNodes; ++i) {
    mOctree[i].Fixup(*this);
  }
  if (mVersion > 4) {
    // The point-search workspace is 0x24 bytes of retail memory built by another TU, and retail
    // calls that constructor rather than inlining it (0x801F8B48, reached through a
    // `__nw__FUlPCcPCc(0x24, …, nullptr)` and a null test). `fn_801F8B48` is that constructor - it
    // stores the point count at +0 and fills the two vectors at +4 and +0x14 - so the workspace is
    // real here too, not a placeholder.
    const int numPoints = stream.ReadInt32();
    if (numPoints > 0) {
      mPoints.set_size(numPoints);
      mPoints.set_data(static_cast< CPFPoint* >(stream.GetBlock(numPoints, sizeof(CPFPoint))));
      const int numPointLinks = stream.ReadInt32();
      mPointLinks.set_size(numPointLinks);
      mPointLinks.set_data(static_cast< int* >(stream.GetBlock(numPointLinks, sizeof(int))));
      const int numLinkData = stream.ReadInt32();
      mPointLinkData.set_size(numLinkData);
      mPointLinkData.set_data(static_cast< uint* >(stream.GetBlock(numLinkData, sizeof(uint))));
      const int numPointWords = (numPoints * (numPoints - 1) / 2 + 31) / 32;
      mPointConnections.set_size(numPointWords);
      mPointConnections.set_data(
          static_cast< uint* >(stream.GetBlock(numPointWords, sizeof(uint))));
      for (i = 0; i < numPoints; ++i) {
        mPoints[i].Fixup(*this);
      }
    }
    // `rs_new`'s spelling: retail's own `operator new` argument here is its `lbl_803A91C0`, which
    // is the shared `"\?\?(\?\?)"` literal, not this unit's name - the `lis`/`addi 0` pair at
    // 0x80141570 has no displacement, and the object at 0x803A91C0 reads `??(??)`.
    CPFPointSearchState* const workspace = static_cast< CPFPointSearchState* >(
        __nw__FUlPCcPCc(sizeof(CPFPointSearchState), "\?\?(\?\?)", nullptr));
    mPointSearchState = workspace != nullptr ? fn_801F8B48(workspace, numPoints) : workspace;
  }
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

int CPFArea::FindRegions(rstl::reserved_vector< CPFRegion*, 8 >& regions, const CAABox& box,
                         uint flags, uint indexMask, bool ignoreObstructions) {
  for (int i = 0; i < mOctreeRegions.size(); ++i) {
    CPFRegion* region = mOctreeRegions[i];
    if ((region->GetFlags() & 0xff & flags) && ((region->GetFlags() >> 16) & 0xff & indexMask) &&
        region->Intersects(box) &&
        (ignoreObstructions ||
         (region->GetObstructionCount(kPFO_Unknown2) <= 0 &&
          ((flags & 0x100) == 0 || region->GetObstructionCount(kPFO_Unknown0) <= 0) &&
          ((flags & 0x200) == 0 || region->GetObstructionCount(kPFO_Unknown1) <= 0))) &&
        ((flags & 6) ||
         region->PointHeight(box.ClosestPointAlongVector(region->GetNormal())) < 3.f)) {
      regions.push_back(region);
      if (regions.size() == regions.capacity()) {
        break;
      }
    }
  }
  return regions.size();
}

CPFRegion* CPFArea::FindClosestRegion(const CVector3f& point, uint flags, uint indexMask,
                                      float padding) {
  rstl::reserved_vector< rstl::prereserved_vector< CPFRegion* >*, 32 > lists;
  CPFRegion* result = nullptr;
  OSGetTick();
  int i, j;
  uint searchTicks = 0;
  mOctree.back().GetRegionListList(lists, point, padding);
  OSGetTick();
  for (i = 0; i < lists.size(); ++i) {
    rstl::prereserved_vector< CPFRegion* >* list = lists[i];
    for (j = 0; j < list->size(); ++j) {
      CPFRegion* region = (*list)[j];
      if (region->Data()->GetCookie() != mRegionFindCookie) {
        region->Data()->SetCookie(mRegionFindCookie);
        if ((region->GetFlags() & 0xff & flags) &&
            ((region->GetFlags() >> 16) & 0xff & indexMask) &&
            region->GetObstructionCount(kPFO_Unknown2) <= 0) {
          // Retail's own chain at 0x8014089C: the flag has to be *set* for its obstruction count
          // to be tested, and a clear flag branches over the call to the next test. A flat `&&`
          // chain sends the clear case to the reject label instead, so these have to be nested.
          if ((flags & 0x100) == 0 || region->GetObstructionCount(kPFO_Unknown0) <= 0) {
            if ((flags & 0x200) == 0 || region->GetObstructionCount(kPFO_Unknown1) <= 0) {
              if (region->IsPointInsidePaddedAABox(point, padding)) {
                uint startTick = OSGetTick();
                if ((flags & 6) || region->PointHeight(point) < 3.f) {
                  if (region->FindBestPoint(mPolyPoints, point, flags, padding * padding)) {
                    padding = CMath::FastSqrtF(region->Data()->GetBestDistanceSquared());
                    result = region;
                    mClosestPoint = region->Data()->GetBestPoint();
                  }
                  searchTicks += OSGetTick() - startTick;
                }
              }
            }
          }
        }
      }
    }
  }
  OSGetTick();
  ++mRegionFindCookie;
  return result;
}

CVector3f CPFArea::FindClosestReachablePoint(rstl::reserved_vector< CPFRegion*, 8 >& regions,
                                             const CVector3f& point, uint flags, uint indexMask) {
  CVector3f result = CVector3f::Zero();
  float closestDistanceSq = FLT_MAX;
  for (int i = 0; i < GetNumRegions(); ++i) {
    CPFRegion& region = GetRegion(i);
    if ((region.GetFlags() & 0xff & flags) && ((region.GetFlags() >> 16) & 0xff & indexMask) &&
        (region.GetObstructionCount(kPFO_Unknown2) <= 0 &&
         ((flags & 0x100) == 0 || region.GetObstructionCount(kPFO_Unknown0) <= 0) &&
         ((flags & 0x200) == 0 || region.GetObstructionCount(kPFO_Unknown1) <= 0))) {
      for (int j = 0; j < regions.size(); ++j) {
        CPFRegion* source = regions[j];
        if (PathExists(source, &region, flags)) {
          const CVector3f& delta = region.GetCentroid() - point;
          float distanceSq = delta.MagSquared();
          if (distanceSq < closestDistanceSq) {
            closestDistanceSq = distanceSq;
            result = region.GetCentroid();
            break;
          }
        }
      }
    }
  }
  return result;
}

bool CPFArea::PathExists(const CPFRegion* source, const CPFRegion* destination, uint flags) const {
  if (source == destination || (flags & 0x14)) {
    return true;
  }
  int numRegions = GetNumRegions();
  int sourceIndex = source->GetIndex();
  int destinationIndex = destination->GetIndex();
  const rstl::prereserved_vector< uint >& connections =
      (flags & 2) ? mConnectionsFlyers : mConnectionsGround;
  int low = sourceIndex;
  int high = destinationIndex;
  if (sourceIndex > destinationIndex) {
    low = destinationIndex;
    high = sourceIndex;
  }
  int totalConnections = numRegions * (numRegions - 1) / 2;
  int remainingConnections = (numRegions - low - 1) * (numRegions - low) / 2;
  uint bit = totalConnections - remainingConnections + high - (low + 1);
  return (connections[bit / 32] >> (bit % 32)) & 1;
}

void CPFArea::SetTransform(const CTransform4f& transform) {
  const CTransform4f delta = mTransform.GetInverse() * transform;
  for (int i = 0; i < mPoints.size(); ++i) {
    CPFPoint& point = mPoints[i];
    point.SetPosition(transform.GetTranslation() + delta.Rotate(point.GetPosition()));
  }
  mTransform = transform;
}

int CPFArea::GetPointIndex(const CPFPoint& point) const { return &point - &mPoints[0]; }

// Retail's 0x801403A8. `dtk` could not name it, so `symbols.txt` calls it `fn_801403A8`; another
// unit (0x801F86F4) calls it as well as `PointPathExists` below, so it is a real out-of-line
// member and not a spelling of `PathExists`. Its body is `PathExists`' with the two members
// swapped: `mPoints.size()` at +0x18C where `PathExists` reads `mRegions.size()`, and
// `mPointConnections`' data at +0x1A0 where `PathExists` selects ground/flyers by `flags & 2`.
// There is no `flags` here, hence no `& 0x14` early return either - only the `a == b` one.
bool CPFArea::PointConnectionsTest(int a, int b) {
  if (a == b) {
    return true;
  }
  int n = mPoints.size();
  if (a > b) {
    const int tmp = a;
    a = b;
    b = tmp;
  }
  int totalConnections = n * (n - 1) / 2;
  int remainingConnections = (n - a - 1) * (n - a) / 2;
  uint bit = totalConnections - remainingConnections + b - (a + 1);
  return (mPointConnections[bit / 32] >> (bit % 32)) & 1;
}

// Retail's 0x80140324, the pointer-taking wrapper. The `/28` is `sizeof(CPFPoint)`: the source
// subtracts `CPFPoint*` and mwcceppc strength-reduces it to a byte difference divided by the
// element size, with the same `0x92492493` / `srawi 4` sequence `GetPointIndex` emits.
bool CPFArea::PointPathExists(const CPFPoint* source, const CPFPoint* destination) {
  if (source == nullptr || destination == nullptr) {
    return false;
  }
  if (source == destination) {
    return true;
  }
  return PointConnectionsTest(source - &mPoints[0], destination - &mPoints[0]);
}

CPFArea::~CPFArea() {}

const CFactoryFnReturn FPathFindAreaFactory(const SObjectTag& tag,
                                            const rstl::auto_ptr< uchar >& data, int size,
                                            const CVParamTransfer& xfer) {
  return rs_new CPFArea(data, size);
}
