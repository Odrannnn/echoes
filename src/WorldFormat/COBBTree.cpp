#include "WorldFormat/COBBTree.hpp"

#include "Kyoto/Alloc/CMemory.hpp"

COBBTree::CSimpleAllocator* COBBTree::CNode::spAllocator = nullptr;
COBBTree* COBBTree::sPrebuiltTrees[4] = {};

COBBTree::SIndexData::SIndexData(CInputStream& in)
: mMaterials(in)
, mVertMaterials(in)
, mEdgeMaterials(in)
, mSurfaceMaterials(in)
, mEdges(in)
, mSurfaceIndices(in)
, x60_(in)
, mVertices(in) {}

// Retail inlines this array-view setup into both constructors rather than keeping a helper:
// `__ct__8COBBTreeFR12CInputStream` (retail 0x8024ED24) carries the 13 assignments as 34
// straight-line instructions between the SIndexData construction and the SetAllocator call, and
// `__ct__8COBBTreeFRCQ28...CNode` (retail 0x8024EE8C) carries the identical block. Out of line it
// is a `W BindIndexData__8COBBTreeFv` symbol that retail does not have, and the project's
// `inline_max_size(125)` will not fold it into a caller this large, so the body is written out in
// both constructors. A shared macro keeps the two copies in step.
#define COBBTREE_BIND_INDEX_DATA()  \
  mMaterialCount = mIndexData.mMaterials.size(); \
  mVertexCount = mIndexData.mVertices.size(); \
  mEdgeCount = mIndexData.mEdges.size(); \
  mTriangleCount = mIndexData.mSurfaceIndices.size() / 3; \
  mMaterials = mIndexData.mMaterials.data(); \
  mVertexMaterials = mIndexData.mVertMaterials.data(); \
  mEdgeMaterials = mIndexData.mEdgeMaterials.data(); \
  mSurfaceMaterials = mIndexData.mSurfaceMaterials.data(); \
  mEdges = mIndexData.mEdges.data(); \
  mSurfaceIndices = mIndexData.mSurfaceIndices.data(); \
  x28_ = mIndexData.x60_.data(); \
  mVertices = mIndexData.mVertices.data(); \
  mOwnsArrays = false;

COBBTree::COBBTree(const SIndexData& indexData, const CNode* root)
: CCollisionPrimitiveData(0)
, mMemsize(root->GetMemoryUsage()), mAllocator(0), mIndexData(indexData), mRoot(root) {
  COBBTREE_BIND_INDEX_DATA()
  CNode::SetAllocator(nullptr);
}

uint verify_deaf_babe(CInputStream& in) { return in.Get< uint >(); }

uint verify_version(CInputStream& in) { return in.Get< uint >(); }

COBBTree::COBBTree(CInputStream& in)
: CCollisionPrimitiveData(0)
, mMagic(verify_deaf_babe(in))
, mVersion(verify_version(in))
, mMemsize(in.Get< uint >())
, mAllocator(mMemsize)
, mIndexData(in)
, mRoot(nullptr) {
  COBBTREE_BIND_INDEX_DATA()
  CNode::SetAllocator(&mAllocator);
  mRoot = rs_new CNode(in);
}

COBBTree::~COBBTree() {
  if (mAllocator.GetPoolMemSize() != 0) {
    CNode::SetAllocator(&mAllocator);
  } else {
    CNode::SetAllocator(nullptr);
  }
  delete mRoot;
}

CAABox COBBTree::CalculateLocalAABox() const {
  if (mRoot) {
    return mRoot->GetOBB().CalculateAABox(CTransform4f::Identity());
  }
  return CAABox(0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
}

rstl::auto_ptr< COBBTree > COBBTree::BuildOrientedBoundingBoxTree(const CVector3f& extent,
                                                                  const CVector3f& center) {
  const CVector3f halfExtent = extent * 0.5f;
  SIndexData indexData(GetPrebuiltTree(kPBT_UnitCube)->mIndexData);
  for (int i = 0; i < 8; ++i) {
    indexData.mVertices[i] = CVector3f::ByElementMultiply(indexData.mVertices[i], extent) + center;
  }

  rstl::vector< ushort > surfaces;
  surfaces.reserve(12);
  for (ushort i = 0; i < 12; ++i) {
    surfaces.push_back_unsafe(i);
  }
  CNode::SetAllocator(nullptr);
  CLeafData* leaf = rs_new CLeafData(surfaces);
  CNode* root = rs_new CNode(CTransform4f::Translate(center), halfExtent, nullptr, nullptr, leaf);
  return rs_new COBBTree(indexData, root);
}

void COBBTree::SetPrebuiltTree(COBBTree* tree, EPreBuiltTrees which) {
  sPrebuiltTrees[which] = tree;
}

COBBTree* COBBTree::GetPrebuiltTree(EPreBuiltTrees which) { return sPrebuiltTrees[which]; }

COBBTree::CNode::CNode(const CTransform4f& xf, const CVector3f& extents, const CNode* left,
                       const CNode* right, const CLeafData* leaf)
: mObb(xf, extents), mIsLeaf(leaf != nullptr), mLeft(left), mRight(right), mLeaf(leaf) {}

COBBTree::CNode::CNode(CInputStream& in)
: mObb(in)
, mIsLeaf(in.Get< bool >())
, mLeft(mIsLeaf ? nullptr : rs_new CNode(in))
, mRight(mIsLeaf ? nullptr : rs_new CNode(in))
, mLeaf(mIsLeaf ? rs_new CLeafData(in) : nullptr) {}

COBBTree::CNode::~CNode() {
  delete mLeft;
  delete mRight;
  delete mLeaf;
}

uint COBBTree::CNode::GetMemoryUsage() const {
  uint size = sizeof(CNode);
  if (mIsLeaf && mLeaf) {
    size += mLeaf->GetMemoryUsage();
  } else {
    if (mLeft) {
      size += mLeft->GetMemoryUsage();
    }
    if (mRight) {
      size += mRight->GetMemoryUsage();
    }
  }
  if (size & 3) {
    size += 4 - (size & 3);
  }
  return size;
}

void COBBTree::CNode::SetAllocator(CSimpleAllocator* allocator) { spAllocator = allocator; }

// `!(spAllocator == nullptr)` is `spAllocator != nullptr`; it is spelled that way because
// mwcceppc 2.7 folds every other spelling of the test (`spAllocator`, `!= nullptr`,
// `!= (CSimpleAllocator*)nullptr`, `(size_t)spAllocator != 0`, a ternary, an if/else, a local
// copy) to one predicate and lays the blocks out with `Alloc` as the fallthrough. Retail
// (0x8024E10C) branches over the `rs_new char[]` block instead, and only the double negation
// survives the simplifier to produce that `bne`.
void* COBBTree::CNode::operator new(size_t size, const char* file, int line) {
  if (!(spAllocator == nullptr)) {
    return spAllocator->Alloc(size);
  }
  return rs_new char[size];
}

void COBBTree::CNode::operator delete(void* ptr, size_t size) {
  if (!spAllocator && ptr) {
    delete[] static_cast< char* >(ptr);
  }
}

COBBTree::CLeafData::CLeafData(const rstl::vector< ushort >& surfaces) : mSurfaces(surfaces) {}

COBBTree::CLeafData::CLeafData(CInputStream& in) : mSurfaces(in) {}

uint COBBTree::CLeafData::GetMemoryUsage() const {
  uint size = sizeof(CLeafData) + mSurfaces.size() * sizeof(ushort);
  if (size & 3) {
    size += 4 - (size & 3);
  }
  return size;
}

COBBTree::CSimpleAllocator::CSimpleAllocator(uint size)
: mBuffer(rs_new char[size]), mSize(size), mOffset(0) {}

COBBTree::CSimpleAllocator::~CSimpleAllocator() {
  if (mBuffer) {
    delete[] mBuffer;
  }
}

void* COBBTree::CSimpleAllocator::Alloc(size_t size) {
  void* result = mBuffer + mOffset;
  mOffset += size;
  if (mOffset & 3) {
    mOffset += 4 - (mOffset & 3);
  }
  return result;
}
