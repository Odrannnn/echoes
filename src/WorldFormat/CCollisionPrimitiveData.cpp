/**
 * `.text 0x80257A14..0x80257CB8`, 0x2A4 = 676 bytes, four functions, **in this order in the file
 * because mwcceppc emits definitions in reverse source order** (see
 * `tools/check_decl_order.py --unit WorldFormat/CCollisionPrimitiveData`):
 *
 * ```
 * 80257C28  __ct__23CCollisionPrimitiveDataFiiiiPCUxPCUcPCUcPCUcPC14CCollisionEdgePCUsPCUsPC9CVector3fb
 * 80257BB0  __ct__23CCollisionPrimitiveDataFv
 * 80257AF8  __dt__23CCollisionPrimitiveDataFv
 * 80257A14  fn_80257A14
 * ```
 *
 * The claim was 0x80257A14..0x80257AF8 (one function, `GetTriangle(ushort)`) and is now extended to
 * 0x80257CB8 by the three `CCollisionPrimitiveData`-named functions the class already declared in
 * `include/WorldFormat/CCollisionPrimitiveData.hpp`. Upstream left the range in the gap between
 * `WorldFormat/CAreaRenderOctTree.cpp` (which ends at 0x80255128) and
 * `Weapons/CProjectileWeapon.cpp` (which starts at 0x802591B4), so it arrived as part of dtk's
 * `main/auto_03_80255128_text` / `main/auto_03_80257AF8_text`; this claim is a sub-range of those and
 * every boundary is a retail function boundary, so nothing else moves. **The claim can only grow at
 * the top end in whole functions, and only while the whole unit keeps matching**: a claim that adds
 * a function which does not reach 100% makes the unit `NonMatching`, which `tools/report_diff.py`
 * reports as `UNLINKED` and `tools/goal_check.sh` reports as `LINKED TOTAL FELL`. `fn_80257540`
 * (retail's `GetTriangleVertexIndices`, below this claim) and the remaining nineteen functions of
 * the gap (0x80257CB8..0x802591B4) stay where they were.
 *
 * **Three code-shape rules for `fn_80257A14`, each measured, each worth a session:**
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

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "WorldFormat/CCollisionEdge.hpp"
#include "WorldFormat/CCollisionSurface.hpp"

// retail 0x800E88A8: `CCollisionSurface(v0, v1, v2, flags)` out of line, a strong `T` in
// `MetroidPrime/CDecalManager.o`, so it has to be called by that name. The host build gets it from
// src/WorldFormat/CCollisionSurface.cpp under `#ifdef TARGET_PC`.
extern "C" void fn_800E88A8(CCollisionSurface* out, const CVector3f* v0, const CVector3f* v1,
                            const CVector3f* v2, u64 flags);

#ifndef TARGET_PC
// The collision-cache tables retail addresses absolutely: `lbl_80410E24` is the .bss halfword table
// at 0x80410E24 (0x200 bytes = 512 slots) that `fn_80257498` allocates out of, and `lbl_80418968`
// is the .sdata word counter at 0x80418968 (-0x7418(r13)). Both are defined by the DOL itself, not
// by any unit, so they are declared here and never defined.
extern "C" ushort lbl_80410E24[512];
extern "C" uint lbl_80418968;

/** retail `fn_80257498` (0x80257498): allocates a collision-cache slot id. */
extern "C" ushort fn_80257498();

/**
 * retail `__ct__23CCollisionPrimitiveDataFiiiiPCUxPCUcPCUcPCUcPC14CCollisionEdgePCUsPCUsPC9CVector3fb`
 * (0x80257C28, 0x90 = 144 bytes).
 *
 * Thirteen arguments, seven in registers and six on the stack, and every one of them is stored
 * straight into its member in declaration order - `mMaterialCount` at +0x00 through `mVertices` at
 * +0x2C - with no test and no branch. `mCacheId` comes from `fn_80257498()` and `mOwnsArrays` is
 * bit 24 of the byte at +0x32, which is `rlwimi r0,r31,7,24,24` on the incoming `bool` read by
 * `lbz r31,47(r1)`. The class's own spelling is the mangled name `config/G2ME01/symbols.txt`
 * declares, so this is a plain member definition rather than an `extern "C"` one.
 */
CCollisionPrimitiveData::CCollisionPrimitiveData(
    int materialCount, int vertexCount, int edgeCount, int triangleCount, const u64* materials,
    const uchar* vertexMaterials, const uchar* edgeMaterials, const uchar* surfaceMaterials,
    const CCollisionEdge* edges, const ushort* surfaceIndices, const ushort* extraIndices,
    const CVector3f* vertices, bool ownsArrays) {
  mMaterialCount = materialCount;
  mVertexCount = vertexCount;
  mEdgeCount = edgeCount;
  mTriangleCount = triangleCount;
  mMaterials = materials;
  mVertexMaterials = vertexMaterials;
  mEdgeMaterials = edgeMaterials;
  mSurfaceMaterials = surfaceMaterials;
  mEdges = edges;
  mSurfaceIndices = surfaceIndices;
  x28_ = extraIndices;
  mVertices = vertices;
  mCacheId = fn_80257498();
  mOwnsArrays = ownsArrays;
}

/**
 * retail `__ct__23CCollisionPrimitiveDataFv` (0x80257BB0, 0x78 = 120 bytes).
 *
 * One `li r0,0` and twelve `stw r0,N(r3)` - the four counts, `mMaterialCount` at +0x00 through
 * `mVertices` at +0x2C, all seven pointers included - then the same `fn_80257498()` cache id and
 * the same bit-24 write of `mOwnsArrays`, with `r4 = 0` feeding the `rlwimi`. It does *not* tail
 * into the thirteen-argument constructor.
 */
CCollisionPrimitiveData::CCollisionPrimitiveData() {
  mMaterialCount = 0;
  mVertexCount = 0;
  mEdgeCount = 0;
  mTriangleCount = 0;
  mMaterials = nullptr;
  mVertexMaterials = nullptr;
  mEdgeMaterials = nullptr;
  mSurfaceMaterials = nullptr;
  mEdges = nullptr;
  mSurfaceIndices = nullptr;
  x28_ = nullptr;
  mVertices = nullptr;
  mCacheId = fn_80257498();
  mOwnsArrays = false;
}

/**
 * retail `__dt__23CCollisionPrimitiveDataFv` (0x80257AF8, 0xB8 = 184 bytes).
 *
 * The shape is MWCC's deleting destructor: `mr. r30,r3` / `beq` is `if (self)`, the return value is
 * `self`, and the delete flag is compared as a *sign-extended halfword* - the `extsh. r0,r31` /
 * `ble` pair - which is what `static_cast< short >(flag) > 0` compiles to (the same reading as
 * `fn_80248410` in `src/WorldFormat/CMetroidAreaCollider.cpp:1074`). The flag is read into `r31` by
 * the prologue and tested only at the end, so the seven `Free`s run for *any* nonzero flag.
 *
 * The arrays freed are the seven pointers at +0x10..+0x28, which is every array the class owns -
 * `mMaterials` included, and `x28_` last. `mOwnsArrays` is bit 24 of the byte at +0x32, which is
 * `rlwinm. r0,r0,25,31,31`.
 *
 * The tail is two statements retail really has whose values nothing reads: the .sdata word at
 * 0x80418968 is incremented, and the collision-cache slot is masked and stored back. **The `ble`
 * does not test either of them**: `addi`, `stw`, `sthx` and the mask (`rlwinm` without the `.`)
 * write no condition register, so the branch is still decided by the `extsh.` above and reads
 * `(short)flag <= 0`. The mask is the load-bearing oddity and it is measured, not guessed:
 *
 *   - `x &= 0xFFFE0000` and `x &= ~0x1FFFFu` and `x &= (uint)0xFFFE0000` and a `uint` temporary all
 *     compile to `54 80 00 1c` (`clrrwi r0,r4,17`) - four bytes off;
 *   - `x &= 0x7FFFu` compiles to `54 80 04 7e`, which is retail's `clrlwi r0,r4,17` byte for byte.
 *
 * So the source is the 16-bit one, `&= 0x7FFF` - which is also what the allocator's
 * `ori r0,r0,32768` in `fn_80257498` implies: bit 15 is the slot's in-use flag and this clears it.
 * **Do not "fix" the constant to 0xFFFE0000**: on a 16-bit lvalue mwcceppc 2.7 narrows that mask
 * to a different rotate-mask form and the object stops matching.
 */
extern "C" void* __dt__23CCollisionPrimitiveDataFv(CCollisionPrimitiveData* self, int flag) {
  if (self != nullptr) {
    if (self->mOwnsArrays) {
      CMemory::Free(self->mMaterials);
      CMemory::Free(self->mVertexMaterials);
      CMemory::Free(self->mEdgeMaterials);
      CMemory::Free(self->mSurfaceMaterials);
      CMemory::Free(self->mEdges);
      CMemory::Free(self->mSurfaceIndices);
      CMemory::Free(self->x28_);
    }
    lbl_80410E24[self->mCacheId] &= 0x7FFF;
    ++lbl_80418968;
    if (static_cast< short >(flag) > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
#endif

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
