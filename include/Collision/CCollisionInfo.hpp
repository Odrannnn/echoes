#ifndef _CCOLLISIONINFO
#define _CCOLLISIONINFO

#include "Collision/CMaterialList.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "MetroidPrime/TGameTypes.hpp"

class CAABox;

class CCollisionInfo {
public:
  // Original Wii export; the native invalid constructor ignores the tag.
  enum EInvalid { kI_Invalid, kI_Valid };

  CCollisionInfo(EInvalid = kI_Invalid);
  CCollisionInfo(const CVector3f& point, const CMaterialList& rightMaterial,
                 const CMaterialList& leftMaterial, const CVector3f& normal, ushort value);
  CCollisionInfo(const CVector3f& point, const CMaterialList& rightMaterial,
                 const CMaterialList& leftMaterial, const CVector3f& leftNormal,
                 const CVector3f& rightNormal, ushort value);
  CCollisionInfo(const CAABox& box, const CMaterialList& rightMaterial,
                 const CMaterialList& leftMaterial, const CVector3f& leftNormal,
                 const CVector3f& rightNormal, ushort value);

  bool IsValid() const { return mValid; }
  bool HasExtents() const { return mHasExtents; }
  const CVector3f& GetPoint() const { return mPoint; }
  CVector3f GetExtreme() const;
  const CMaterialList& GetMaterialLeft() const { return mMaterialLeft; }
  const CMaterialList& GetMaterialRight() const { return mMaterialRight; }
  const CVector3f& GetNormalLeft() const { return mNormalLeft; }
  const CVector3f& GetNormalRight() const { return mNormalRight; }
  TUniqueId GetObjectId() const { return mObjectId; }
  void Swap();

private:
  CVector3f mPoint;
  CVector3f mExtentX;
  CVector3f mExtentY;
  CVector3f mExtentZ;
  CMaterialList mMaterialLeft;
  CMaterialList mMaterialRight;
  CVector3f mNormalLeft;
  CVector3f mNormalRight;
  TUniqueId mObjectId;
  bool mValid : 1;
  bool mHasExtents : 1;
};
CHECK_SIZEOF(CCollisionInfo, 0x60)

//!< Retail's out-of-line copy of one `CCollisionInfo`, 0x64 bytes at `fn_800D042C`
//!< (`config/G2ME01/symbols.txt`). Six of our units call it in retail - every unit that reaches
//!< `CCollisionInfoList::Add` - so it is retail's `rstl::construct` for this type, not one unit's
//!< accident. Written out in `src/Collision/CCollidableSphere.cpp`, the first unit that needs it,
//!< as `fn_80143CD4` is written out in `CGameState.cpp`; `src/Collision/CCollisionInfo.cpp` is
//!< `MatchingFor` and byte-exact, so defining it there would change `main.dol`.
extern "C" void fn_800D042C(CCollisionInfo* self, const CCollisionInfo& other);

namespace rstl {
RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE(CCollisionInfo)

//!< `fn_800D042C`, not a placement `new`.
//!
//!< mwcceppc 2.7 expands `new (dest) T(src)` into "call `operator new`, test the result against
//!< null, then construct", and that test survives inlining. Retail's inlined
//!< `CCollisionInfoList::Add` has no such test - it ends in a bare `bl` - because its copy is a call
//!< to a function the compiler has no body for. Measured on `Collision/CCollidableSphere.cpp`:
//!< through the placement-`new` form `Add` is left outlined and `Collide::Sphere_Sphere` sits at
//!< 93.33% and `Collide::Sphere_AABox` at 95.54%, each short by the outlined call plus an
//!< `addic.`/`beq` pair; through this form both match byte for byte.
inline void construct_impl(void* dest, const CCollisionInfo& src) {
  fn_800D042C(static_cast< CCollisionInfo* >(dest), src);
}
}

#endif // _CCOLLISIONINFO
