// Carved out of an unclaimed dtk `auto_*` range by lane `carve3`.  Every number here is
// measured: the addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions
// are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_8001E070_text.s:2184-2232`
// (the same range is now `build/G2ME01/asm/MetroidPrime/Carve8001FEDC.s`), and the bodies
// below are the C those bytes are the compilation of.
//
// .text 0x8001FEDC..0x8001FF7C, 0xA0 = 160 bytes, 2 functions:
//
//   fn_8001FEDC    0x8001FEDC  0x50    20 instructions   the twin
//   fn_8001FF2C    0x8001FF2C  0x50    20 instructions   the twin, one `bl` target different
//
// **Both are byte-shape twins of one already-`Matching` function, read out of its own source
// rather than guessed.**  The seed named it; reading `include/rstl/rc_ptr.hpp:117-123` against
// retail's bytes confirms it instruction for instruction:
//
//   rstl::rc_ptr<rstl::vector<int, rstl::rmemory_allocator> >::ReleaseData()
//     `ReleaseData__Q24rstl53rc_ptr<Q24rstl36vector<i,Q24rstl17rmemory_allocator>>Fv`
//     at 0x800097C0, 0x50 = 80 bytes, `build/report.json` `main/MetroidPrime/main`, 100.00%
//
// which is the out-of-line template member declared at `include/rstl/rc_ptr.hpp:101` and
// defined at 117.  Retail compiles it to the same twenty instructions as each of these, with
// only the two `bl` targets different:
//
//   `stwu/mflr/stw/stw` frame 0x10,  `mr r31,r3`,  `lwz r4,0x4(r3)` (the `int*` refcount),
//   `lwz r3,0x0(r4)`,  `subic. r0,r3,1`,  `stw r0,0x0(r4)`  - the `--*mRefCount` of
//   `if (--*mRefCount <= 0)`,  `bgt` out of it, then the two-word body:  the `void*` at
//   `self + 0x00` reloaded from r31 into r3 with `li r4,1` for the destructor call (retail's
//   `delete GetPtr()`, which passes the deleting flag), then `self + 4` reloaded and
//   `bl Free__7CMemoryFPCv` (retail's `delete mRefCount` - the global `operator delete` is
//   `CMemory::Free(void*)`), then the epilogue.
//
// The word at +0x04 is an `int*` and the word at +0x00 is the pointee: that is retail's
// `rc_ptr<T>` layout and `include/rstl/rc_ptr.hpp:47-49` says why - the refcount is a
// **separate four-byte `CMemory` allocation**, not a `CRefData` control block.  Only the two
// fields the bytes read are modelled here; what the pointed-to object is does not change how
// this release runs.
//
// **The callees are this copy's own, and they are the only thing that differs from the twin.**
//
//   `fn_8001FEDC`  calls `__dt__13CStateManagerFv` (0x8004269C, `symbols.txt:1226`, 0x58C) -
//     which our own `build/G2ME01/src/MetroidPrime/CStateManager.o` defines as `T
//     __dt__13CStateManagerFv`, so this call resolves on the DOL link without any stand-in.
//   `fn_8001FF2C`  calls `fn_800E142C` (0x800E142C, `symbols.txt:3913`, 0x11C = 284 bytes),
//     which **no unit claims**: it sits inside dtk's own `auto_03_800E122C_text.o`, whose copy
//     of those bytes is `build/G2ME01/asm/auto_03_800E122C_text.s:163-`.  Measured from them it
//     is retail's own 284-byte destructor for a class that owns four `CGuiFrameLoader` members
//     at +0x4C/+0x50/+0x54/+0x58 (`bl __dt__15CGuiFrameLoaderFv` four times, each behind a
//     null test on the word), a `CToken` at +0x48 released through `__dt__6CTokenFv`, and two
//     0x50/0x58-byte sub-objects at +0x5C and +0x80.  At 284 bytes with six calls it is neither
//     a 64-byte twin nor a shape any matched function in this tree carries, so it is **declared
//     here and left to dtk's object** for the DOL - the same trade `src/Collision/
//     Carve8028B728.c` makes for its `fn_8028B780`.  For the port's flat link, which has no dtk
//     objects, it is the announced empty stand-in `stub_8001fedc_0` in
//     `src/MetroidPrime/PortLinkStubs.cpp`.  **Nothing here claims `fn_800E142C` is
//     decompiled.**  It is retail's `bl` at 0x8001FF5C, so this carve cannot drop the call.
//   both  call `Free__7CMemoryFPCv` (0x802CE388) under retail's own name, spelled as retail
//     spells it: MWCC's old mangling is `[A-Za-z0-9_]` only, so a C declaration names it
//     verbatim and needs no alias.  Same trick `src/MetroidPrime/Carve800E1548.c:77` uses.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap: this run sits inside the
// 0x8001E070..0x8001FF7C hole that dtk covers with `auto_03_8001E070_text`, and the functions
// either side of it in that hole are not trivial - `fn_8001FDA4` (0x8001FDA4, 0x138 = 312 bytes)
// below and whatever dtk emits for 0x8001FF7C above.  The directory is retail's own, taken from
// the nearest claimed ranges: below is `MetroidPrime/CMainFlow.cpp` (0x8001DAF4..0x8001E070),
// above is `MetroidPrime/CCredits.cpp` (0x8001FF7C..0x80023FFC).

/** 0x800E142C, `symbols.txt:3913`, 0x11C = 284 bytes: retail's unclaimed destructor that
 *  `fn_8001FF2C` calls for the object its `rc_ptr` holds.  Declared, never defined here; the
 *  DOL link gets dtk's own `auto_03_800E122C_text.o` copy and the port link gets the announced
 *  empty stand-in `stub_8001fedc_0` in `src/MetroidPrime/PortLinkStubs.cpp`.  The second
 *  parameter is retail's deleting flag: both callers pass `li r4, 1` before the `bl`. */
extern void fn_800E142C(void* self, int deleting);

/** `CMemory::Free(void*)` (0x802CE388) under retail's own name - MWCC's old mangling is
 *  `[A-Za-z0-9_]` only, so a C declaration names it verbatim and needs no alias.  This is the
 *  global `operator delete` both copies of retail's `delete mRefCount` reach. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `CStateManager::~CStateManager()` under retail's own name (0x8004269C, `symbols.txt:1226`),
 *  defined by our own `build/G2ME01/src/MetroidPrime/CStateManager.o`.  `fn_8001FEDC` passes it
 *  the `void*` at +0x00 with retail's deleting flag, and **the flag is measured, not assumed**:
 *  retail emits `li r4, 1` at 0x8001FF08, and declaring one parameter here dropped that `li` and
 *  made the function 0x4C bytes instead of 0x50 - which permutes the unit, because `fn_8001FF2C`
 *  then starts at 0x8001FF28. */
extern void __dt__13CStateManagerFv(void* self, int deleting);

/** `rstl::rc_ptr<T>`'s two words, retail's layout (`include/rstl/rc_ptr.hpp:47-49`): the
 *  pointee at +0x00 and a **separate** `CMemory`-allocated refcount at +0x04.  Neither function
 *  touches the object behind +0x00, so it is modelled as the `void*` retail's `delete
 *  GetPtr()` sees. */
struct SCarve8001FEDCRcPtr {
  void* x0_ptr;
  int* x4_refCount;
};

void fn_8001FF2C(struct SCarve8001FEDCRcPtr* self);
void fn_8001FEDC(struct SCarve8001FEDCRcPtr* self);

void fn_8001FF2C(struct SCarve8001FEDCRcPtr* self) {
  if (--*self->x4_refCount <= 0) {
    fn_800E142C(self->x0_ptr, 1);
    Free__7CMemoryFPCv(self->x4_refCount);
  }
}

void fn_8001FEDC(struct SCarve8001FEDCRcPtr* self) {
  if (--*self->x4_refCount <= 0) {
    __dt__13CStateManagerFv(self->x0_ptr, 1);
    Free__7CMemoryFPCv(self->x4_refCount);
  }
}