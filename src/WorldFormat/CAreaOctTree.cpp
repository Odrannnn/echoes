#include "WorldFormat/CAreaOctTree.hpp"

#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "WorldFormat/CCollisionEdge.hpp"

static CAABox BoxFromIndex(int index, const CVector3f& min, const CVector3f& center,
                           const CVector3f& max) {
  switch (index) {
  case 0:
    return CAABox(min, center);
  case 1:
    return CAABox(CVector3f(center.GetX(), min.GetY(), min.GetZ()),
                  CVector3f(max.GetX(), center.GetY(), center.GetZ()));
  case 2:
    return CAABox(CVector3f(min.GetX(), center.GetY(), min.GetZ()),
                  CVector3f(center.GetX(), max.GetY(), center.GetZ()));
  case 3:
    return CAABox(CVector3f(center.GetX(), center.GetY(), min.GetZ()),
                  CVector3f(max.GetX(), max.GetY(), center.GetZ()));
  case 4:
    return CAABox(CVector3f(min.GetX(), min.GetY(), center.GetZ()),
                  CVector3f(center.GetX(), center.GetY(), max.GetZ()));
  case 5:
    return CAABox(CVector3f(center.GetX(), min.GetY(), center.GetZ()),
                  CVector3f(max.GetX(), center.GetY(), max.GetZ()));
  case 6:
    return CAABox(CVector3f(min.GetX(), center.GetY(), center.GetZ()),
                  CVector3f(center.GetX(), max.GetY(), max.GetZ()));
  case 7:
    return CAABox(center, max);
  default:
    return CAABox(min, max);
  }
}

CAreaOctTree::Node CAreaOctTree::Node::GetChild(int index) const {
  const ETreeType type = GetChildType(index);
  const uint* offsets = reinterpret_cast< const uint* >(static_cast< const uchar* >(mPtr) + sizeof(uint));
  const void* node = static_cast< const uchar* >(mPtr) + 9 * sizeof(uint) + offsets[index];
  if (type == kTT_Leaf) {
    const CAABox bounds = *reinterpret_cast< const CAABox* >(node);
    return Node(node, bounds, GetOwner(), type);
  }
  const CVector3f center = 0.5f * (mAabb.GetMinPoint() + mAabb.GetMaxPoint());
  const CAABox bounds = BoxFromIndex(index, mAabb.GetMinPoint(), center, mAabb.GetMaxPoint());
  return Node(node, bounds, GetOwner(), type);
}

CAreaOctTree::TriListReference CAreaOctTree::Node::GetTriangleArray() const {
  static const ushort skDeadArray[2] = {0, 0};
  if (GetTreeType() != kTT_Leaf) {
    return TriListReference(skDeadArray);
  }
  return TriListReference(mPtr);
}

CAreaOctTree::CAreaOctTree(const CAABox& bounds, Node::ETreeType treeType, const uchar* buffer,
                           const void* treeBuffer, int materialCount, int vertexCount,
                           int edgeCount, int triangleCount, const u64* materials,
                           const uchar* vertexMaterials, const uchar* edgeMaterials,
                           const uchar* surfaceMaterials, const CCollisionEdge* edges,
                           const ushort* surfaceIndices, const ushort* extraIndices,
                           const CVector3f* vertices)
: CCollisionPrimitiveData(materialCount, vertexCount, edgeCount, triangleCount, materials,
                          vertexMaterials, edgeMaterials, surfaceMaterials, edges, surfaceIndices,
                          extraIndices, vertices, false)
, mAabb(bounds)
, mTreeType(treeType)
, mBuf(buffer)
, mTreeBuf(treeBuffer) {}

// The packed area starts with two discarded longs, then the root box, the tree type and the
// tree's byte size; the octree itself follows and every collision array after it is a
// (count, data) pair. Materials are 64-bit here, the three material arrays are byte-wide, and
// the two index arrays are three `ushort` per triangle: the surface edges and, behind its own
// header, the parallel array `CCollisionPrimitiveData` keeps as `x28_`. The vertex count comes
// last, ahead of the vertices.
void CAreaOctTree::MakeFromMemory(void* buffer, uint bufferLength, CAreaOctTree** treeOut,
                                  bool* valid) {
  CMemoryInStream in(buffer, bufferLength, CMemoryInStream::kOS_NotOwned);
  in.ReadInt32();
  in.ReadInt32();
  const CAABox bounds(in);
  const Node::ETreeType treeType = static_cast< Node::ETreeType >(in.ReadInt32());
  const uint treeSize = in.ReadInt32();

  const uchar* treeBuf = static_cast< const uchar* >(buffer) + in.GetReadPosition();
  const uint* materialHeader = reinterpret_cast< const uint* >(treeBuf + treeSize);
  const uint materialCount = *materialHeader;
  const u64* materials = reinterpret_cast< const u64* >(materialHeader + 1);

  const uint* vertexMaterialHeader = reinterpret_cast< const uint* >(materials + materialCount);
  const uchar* vertexMaterials = reinterpret_cast< const uchar* >(vertexMaterialHeader + 1);
  const uint* edgeMaterialHeader =
      reinterpret_cast< const uint* >(vertexMaterials + *vertexMaterialHeader);
  const uchar* edgeMaterials = reinterpret_cast< const uchar* >(edgeMaterialHeader + 1);
  const uint* surfaceMaterialHeader =
      reinterpret_cast< const uint* >(edgeMaterials + *edgeMaterialHeader);
  const uchar* surfaceMaterials = reinterpret_cast< const uchar* >(surfaceMaterialHeader + 1);

  const uint* edgeHeader = reinterpret_cast< const uint* >(surfaceMaterials + *surfaceMaterialHeader);
  const uint edgeCount = *edgeHeader;
  const CCollisionEdge* edges = reinterpret_cast< const CCollisionEdge* >(edgeHeader + 1);
  const uint* surfaceHeader = reinterpret_cast< const uint* >(edges + edgeCount);
  const uint indexCount = *surfaceHeader;
  const int triangleCount = indexCount / 3;
  const ushort* surfaceIndices = reinterpret_cast< const ushort* >(surfaceHeader + 1);
  const uchar* extraIndicesEnd = reinterpret_cast< const uchar* >(surfaceIndices);
  const ushort* extraIndices =
      reinterpret_cast< const ushort* >(extraIndicesEnd + 6 * triangleCount + sizeof(uint));
  const uint* vertexHeader = reinterpret_cast< const uint* >(
      reinterpret_cast< const uchar* >(extraIndices) + 6 * triangleCount);
  const int vertexCount = *vertexHeader;
  const CVector3f* vertices = reinterpret_cast< const CVector3f* >(vertexHeader + 1);

  *treeOut = rs_new CAreaOctTree(bounds, treeType, static_cast< const uchar* >(buffer), treeBuf,
                                 materialCount, vertexCount, edgeCount, triangleCount, materials,
                                 vertexMaterials, edgeMaterials, surfaceMaterials, edges,
                                 surfaceIndices, extraIndices, vertices);
  *valid = true;
}
