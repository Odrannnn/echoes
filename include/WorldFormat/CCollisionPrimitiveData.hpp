#ifndef _CCOLLISIONPRIMITIVEDATA
#define _CCOLLISIONPRIMITIVEDATA

#include "types.h"

class CCollisionEdge;
class CCollisionSurface;
class CCollisionPrimitiveData;
class CTransform4f;
class CVector3f;

// retail fn_80257A14: the unit's out-of-line `GetTriangle(ushort)`, with the hidden return
// pointer of a 48-byte `CCollisionSurface` spelled out as the first parameter - r3 is the
// destination and retail never writes r3, so it has to be the first thing in the register list.
// `self` is a const *pointer* on purpose: `const CCollisionPrimitiveData*` is what the definition
// needs to be byte-exact, and it is the whole of a 4-instruction difference - see the unit's
// comment. C linkage is what `WorldFormat/CMetroidAreaCollider.o`'s undefined `U fn_80257A14`
// resolves against; the C++ member spelling below mangles to
// `GetTriangle__23CCollisionPrimitiveDataFUs` instead.
extern "C" void fn_80257A14(CCollisionSurface* out, const CCollisionPrimitiveData* self, ushort index);

// Shared collision-array view. COBBTree owns its arrays through SIndexData instead.
class CCollisionPrimitiveData {
public:
  CCollisionPrimitiveData();
  CCollisionPrimitiveData(int materialCount, int vertexCount, int edgeCount, int triangleCount,
                          const u64* materials, const uchar* vertexMaterials,
                          const uchar* edgeMaterials, const uchar* surfaceMaterials,
                          const CCollisionEdge* edges, const ushort* surfaceIndices,
                          const ushort* extraIndices, const CVector3f* vertices, bool ownsArrays);
  ~CCollisionPrimitiveData();

  CCollisionSurface GetTriangle(ushort index) const;
  CCollisionSurface GetTriangle(ushort index, const CTransform4f* xf) const;

protected:
  int mMaterialCount;
  int mVertexCount;
  int mEdgeCount;
  int mTriangleCount;
  const u64* mMaterials;
  const uchar* mVertexMaterials;
  const uchar* mEdgeMaterials;
  const uchar* mSurfaceMaterials;
  const CCollisionEdge* mEdges;
  const ushort* mSurfaceIndices;
  const ushort* x28_; // Additional serialized index array; role unresolved.
  const CVector3f* mVertices;
  ushort mCacheId;
  bool mOwnsArrays : 1;

  // Spelled without a `::`: mwcceppc rejects `friend CCollisionSurface ::fn_80257A14(...)` with
  // "undefined identifier 'fn_80257A14'" and then aborts every translation unit that includes this
  // header, which is how one line of a header takes 38 units down with it.
  friend void fn_80257A14(CCollisionSurface* out, const CCollisionPrimitiveData* self, ushort index);
};
CHECK_SIZEOF(CCollisionPrimitiveData, 0x34)

#endif // _CCOLLISIONPRIMITIVEDATA
