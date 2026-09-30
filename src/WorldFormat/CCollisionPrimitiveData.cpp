/**
 * `.text 0x80257A14..0x80257AF8`, 0xE4 = 228 bytes, one function:
 *
 * ```
 * 80257A14  fn_80257A14
 * ```
 *
 * `CCollisionPrimitiveData::GetTriangle(ushort)` in retail. Upstream left the range in the gap
 * between `WorldFormat/CAreaRenderOctTree.cpp` (which ends at 0x80255128) and
 * `Weapons/CProjectileWeapon.cpp` (which starts at 0x802591B4), so it arrived as part of dtk's
 * `main/auto_03_80255128_text`; this claim is a sub-range of that auto unit and both boundaries are
 * retail function boundaries, so nothing else moves. `fn_80257540` below it (retail's
 * `GetTriangleVertexIndices`) and the two constructors stay where they were.
 *
 * **Three code-shape rules, each measured, each worth a session:**
 *
 * 1. **The hidden return pointer is the first parameter.** `CCollisionSurface` is 0x30 bytes, so a
 *    by-value return travels through a pointer the caller passes in r3, and retail never writes r3
 *    between the prologue and the `bl` - r3 already holds the destination and is passed straight
 *    through. Spelled as a by-value return, mwcceppc gives the *caller* a home for the returned
 *    temporary and emits three instructions retail does not have (`stw r31,12(r1)` / `mr r31,r3` /
 *    `lwz r31,12(r1)`): 240 bytes against a 228-byte claim. Spelling the pointer out as an
 *    explicit first parameter - here and on `fn_800E88A8` - is what makes the object 228 bytes
 *    exactly. Put it anywhere but first and the register allocator reuses r3 for `self` and emits
 *    an `mr r3,r4` in each branch instead.
 *
 * 2. **`self` is a `const CCollisionPrimitiveData*`, a const *pointer*, not a pointer to const.**
 *    This is the whole of the last four instructions, and it is invisible in the source's meaning:
 *    `CCollisionPrimitiveData* self` and `const CCollisionPrimitiveData* self` compile the same
 *    reads, but they differ in 4 of the 57 instructions' positions (79 bytes) - the scheduler emits
 *    the `lwz r6,0x1c(r4)`, the `lis r5,0x100` and the `lwz r11,0x20(r4)` early instead of late.
 *    Measured over 78 statement-order and narrowing-spelling permutations, every one of them lands
 *    on exactly those 79 bytes, and the const pointer lands on 0. See docs/goal-notes/.
 *
 * 3. **The winding flag is bit 24 of the material's low word, and the material is a `u64`.**
 *    `mMaterials` is `const u64*`, so the test is a 64-bit AND that mwcceppc splits across two
 *    words and both halves are visible in retail: `li r6,0` / `and r0,r7,r6` is the high half of
 *    the constant and `lis r5,0x100` / `and r5,r8,r5` is the low half. The mask is therefore
 *    `0x1000000` **on the low word**; a `u32` mask in a `u64` expression puts the constant in the
 *    wrong half. Prime 1's `material & 0x2000000` does not compile to these bytes (measured:
 *    `1ULL << 24` and `0x1000000` both give the right half, `0x2000000` and `1ULL << 56` do not).
 *
 * Prime 1's counterpart is `CAreaOctTree::GetMasterListTriangle`
 * (prime-ref/src/WorldFormat/CAreaOctTree.cpp:161). Echoes folds it into the base class and
 * **changes the third-vertex rule**: Prime 1 searches edge1's first index against both of edge0's
 * and falls back to the second, whereas retail branches on the winding flag and takes edge1's low
 * half when the flag is set and its high half when it is clear - the same choice that decides
 * whether edge0 is read (idx2, idx1) or (idx1, idx2).
 *
 * The two edge subscripts are `clrlslwi rX,rY,16,2` - a shift with a 16-bit field mask, not the
 * `slwi` the same multiply gives without it. The compiler only narrows when the source says so, and
 * the spelling that does it is a `uint` local narrowed back to `ushort` at the subscript: the load
 * widens to 32 bits and the `static_cast` puts the 16-bit knowledge back where the multiply
 * happens. Widening at the load alone is not enough and narrowing at the load is not enough; both,
 * in that order, are. The three vertex subscripts are plain `mulli ...,0xc` instead, because 12 is
 * not a power of two and there is no shift to narrow.
 */
#include "WorldFormat/CCollisionPrimitiveData.hpp"

#include "Kyoto/Math/CVector3f.hpp"
#include "WorldFormat/CCollisionEdge.hpp"
#include "WorldFormat/CCollisionSurface.hpp"

// retail 0x800E88A8: `CCollisionSurface(v0, v1, v2, flags)` out of line, a strong `T` in
// `MetroidPrime/CDecalManager.o`, so it has to be called by that name. The host build gets it from
// src/WorldFormat/CCollisionSurface.cpp under `#ifdef TARGET_PC`.
extern "C" void fn_800E88A8(CCollisionSurface* out, const CVector3f* v0, const CVector3f* v1,
                            const CVector3f* v2, u64 flags);

extern "C" void fn_80257A14(CCollisionSurface* out, const CCollisionPrimitiveData* self, ushort index) {
  const int start = index * 3;
  const uint edge0Index = self->mSurfaceIndices[start];
  const uint edge1Index = self->mSurfaceIndices[start + 1];
  const CCollisionEdge& edge0 = self->mEdges[static_cast< ushort >(edge0Index)];
  const CCollisionEdge& edge1 = self->mEdges[static_cast< ushort >(edge1Index)];
  const u64 material = self->mMaterials[self->mSurfaceMaterials[index]];
  if (material & 0x1000000) {
    fn_800E88A8(out, &self->mVertices[edge0.GetVertIndex2()], &self->mVertices[edge0.GetVertIndex1()],
                &self->mVertices[edge1.GetVertIndex1()], material);
  } else {
    fn_800E88A8(out, &self->mVertices[edge0.GetVertIndex1()], &self->mVertices[edge0.GetVertIndex2()],
                &self->mVertices[edge1.GetVertIndex2()], material);
  }
}
