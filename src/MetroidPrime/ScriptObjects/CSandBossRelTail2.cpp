// CSandBossRelTail2.cpp - SandBoss's (module 55) out-of-line destruction cluster, .text
// 0x4D78..0x4E94: the three destructors of the `CCameraShakerData` / `CMayaSpline` pair that the
// module's own translation units emitted out of line. They sit in the middle of the big unclaimed
// run 0x178..0x1073C, above the module's class code and below `CSandBossRelTail.cpp`'s claim.
//
//   0x4D78 fn_55_4D78  0x70  `CCameraShakerData::~CCameraShakerData()`: the three `CMayaSpline`
//                            members at +0x18/+0x5C/+0xA0 destroyed in reverse declaration order,
//                            then the deleting flag's `CMemory::Free`
//   0x4DE8 fn_55_4DE8  0x58  `CMayaSpline::~CMayaSpline()`: the `rstl::vector<CMayaSplineKnot>`
//                            at +8 through fn_55_4E40, then the deleting flag's free
//   0x4E40 fn_55_4E40  0x54  that vector's own destructor: `Free(*(void**)(self + 0xC))`, then
//                            the deleting flag's free
//
// Each body is word for word a DOL function that is already `Matching` at 100.00% in
// `build/report.json`, so the spells here are copied from those sources rather than invented:
//
//   fn_55_4D78 = `__dt__17CCameraShakerDataFv` (0x8009D174, 0x70, `main/MetroidPrime/TypesMatch`)
//   fn_55_4DE8 = `__dt__11CMayaSplineFv` (0x800327FC, 0x58, `main/MetroidPrime/Factories/Carve80032774`)
//   fn_55_4E40 = `fn_80032854` (0x80032854, 0x54, same unit) - retail's name for the member at +8
//
// The member offsets come from `include/MetroidPrime/Cameras/CCameraShakerData.hpp`
// (`CHECK_SIZEOF(CCameraShakerData, 0xf4)`: `mHorizontalMotion` at +0x18, `mForwardMotion` at
// +0x5C, `mVerticalMotion` at +0xA0, destroyed 0xA0/0x5C/0x18) and
// `include/Kyoto/Math/CMayaSpline.hpp` (`CHECK_SIZEOF(CMayaSpline, 0x44)`, `mKnots` at +8, the
// vector's pointer at +0xC). `Carve80032774.cpp` writes the same two destructors as free
// functions and they match there, which is why they are `extern "C"` over `void*` here: the
// callee resolves to this unit's own low address, not to the DOL symbol.
//
// **The bodies are inside `#ifdef __MWERKS__` and the host branch is empty, so listing this file
// in `files.cmake` adds no undefined reference** - the arrangement `CSandBossRelTail.cpp` uses.
// Every call is either module-local (`fn_55_4DE8`, `fn_55_4E40`) or the DOL's `CMemory::Free`,
// so a host body would make the port link names it does not have.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only the module's sha1 would catch it.

#ifdef __MWERKS__

/** 0x802CE388, `symbols.txt`: `CMemory::Free(void const*)`. Declared under retail's own emitted
 *  spelling so the call needs no header. */
extern "C" void Free__7CMemoryFPCv(const void* ptr);

/** 0x4E40, 0x54. The `rstl::vector<CMayaSplineKnot>` destructor: the pointer at +0xC released
 *  unconditionally, then the receiver itself behind the positive deleting flag. Retail's flag is
 *  the `extsh.` a `short` gives; `fn_80032854` is this exact body in the DOL. */
extern "C" void* fn_55_4E40(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)((unsigned char*)self + 0xC));
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x4DE8, 0x58. `CMayaSpline::~CMayaSpline()`: one member teardown, the `mKnots` vector at +8,
 *  through fn_55_4E40 with the `-1` flag that means "destroy, do not free me". */
extern "C" void* fn_55_4DE8(void* self, short flag) {
  if (self) {
    fn_55_4E40((unsigned char*)self + 8, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

/** 0x4D78, 0x70. `CCameraShakerData::~CCameraShakerData()`: the three `CMayaSpline` members in
 *  reverse declaration order - +0xA0 (`mVerticalMotion`), +0x5C (`mForwardMotion`), +0x18
 *  (`mHorizontalMotion`) - then the receiver behind the deleting flag. */
extern "C" void* fn_55_4D78(void* self, short flag) {
  if (self) {
    fn_55_4DE8((unsigned char*)self + 0xA0, -1);
    fn_55_4DE8((unsigned char*)self + 0x5C, -1);
    fn_55_4DE8((unsigned char*)self + 0x18, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#endif
