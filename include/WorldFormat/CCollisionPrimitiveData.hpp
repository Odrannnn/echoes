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

// retail `__dt__23CCollisionPrimitiveDataFv` (0x80257AF8): the deleting destructor takes the
// delete flag as a second argument and returns `this`, and it is written out of line under its own
// mangled name because MWCC's implicit member teardown does not reproduce it - same shape as
// `src/MetroidPrime/main.cpp`'s `__dt__18CGameGlobalObjectsFv`. C linkage, so the name is the one
// `config/G2ME01/symbols.txt` declares.
extern "C" void* __dt__23CCollisionPrimitiveDataFv(CCollisionPrimitiveData* self, int flag);

// Shared collision-array view. COBBTree owns its arrays through SIndexData instead.
class CCollisionPrimitiveData {
public:
  CCollisionPrimitiveData();
  // Retail's two `COBBTree` constructors set `r4 = 0` immediately before they call
  // `__ct__23CCollisionPrimitiveDataFv` (0x8024eeb0 and 0x8024ed38), although that
  // constructor takes no argument and never reads `r4`. mwcceppc 2.7 only materialises that
  // `li r4,0` when the base initialiser names a constructor that has a parameter, so this
  // overload exists to give the generated code the shape retail has; it is initialised with
  // `0` and has no definition, because retail has no such symbol.
  CCollisionPrimitiveData(int);
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
  friend void* __dt__23CCollisionPrimitiveDataFv(CCollisionPrimitiveData* self, int flag);
};
CHECK_SIZEOF(CCollisionPrimitiveData, 0x34)

#endif // _CCOLLISIONPRIMITIVEDATA
