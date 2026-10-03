// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7881-7882`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801E8070_text.s:756-766` while this range was
// still unclaimed, and the body below is the C those bytes are the compilation of.
// `build/G2ME01/asm/MetroidPrime/Cameras/Carve801E8ACC.s` is this unit's own generated listing,
// not retail's - it can confirm, never establish, what retail had.
//
// .text 0x801E8ACC..0x801E8AEC, 0x20 = 32 bytes, 1 function:
//
//   fn_801E8ACC    0x801E8ACC  0x20     8 instructions
//
// **What it is: some `CGameCamera` subclass's `AcceptScriptMsg`, an override whose whole body is
// a call to the base class's.**  Both halves of that are measured, not assumed:
//
//   - The eight instructions are a frame, one unconditional `bl
//     AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg`, and an epilogue.  There is
//     no `mr` anywhere in the range, so r3, r4 and r5 reach the callee exactly as the caller
//     passed them - three arguments, forwarded untouched, with no load, no test and no return
//     value.
//   - Retail puts this address in a vtable.  `build/G2ME01/asm/auto_07_803B75A8_data.s:117-154`
//     is `lbl_803B7708`, a 0x90-byte `.data` object (36 entries) in an unclaimed dtk data range,
//     and **entry 6 of it is `fn_801E8ACC`**.  Entry 6 is where `AcceptScriptMsg` sits: retail's
//     own `__vt__11CGameCamera` (`build/G2ME01/asm/MetroidPrime/Cameras/CGameCamera.s:1573-1610`)
//     is the same shape, and its entry 6 is
//     `AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg`.  Counting the two nulls as
//     entries 0 and 1, entries 2, 3, 4, 7, 8, 9, 10 and 12 of the two tables hold the **same
//     names** (`__dt__`/`fn_801E95B4`, `TypesMatch`, `PreThink`, `SetActive__11CGameCameraFb`,
//     `ClearFluidList__11CGameCameraFR13CStateManager`, `PreRender__6CActor`,
//     `AddToRenderer__6CActor`, `CanRenderUnsorted__6CActor`), which is what pins the slot
//     rather than merely suggesting it; entry 5 differs (`Think__6CActorFfR13CStateManager` against
//     `fn_801E8AF4`), i.e. the subclass overrides it, exactly as it overrides entry 6.  The class behind
//     `lbl_803B7708` is **not** identified here: retail names nothing in it, and `symbols.txt:18298`
//     carries only `lbl_803B7708 = .data:0x803B7708; // type:object size:0x90`.  Its destructor is
//     `fn_801E95B4`, which calls `__dt__11CGameCameraFv`, so it derives from `CGameCamera`; that
//     is as far as the evidence goes and the file claims nothing more.
//
// The callee, `AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg`
// (`symbols.txt:7078`, 0x801B0EC0, 0x20 = 32 bytes), is `CGameCamera::AcceptScriptMsg` itself and
// **is already claimed**, by `MetroidPrime/Cameras/CGameCamera.cpp` (`.text`
// 0x801B0500..0x801B1A18, `splits.txt:1206-1208`) - and it is a Matching unit, so the matching
// build's link resolves this `bl` out of that object.  It is declared here and never defined.
//
// **The twin, measured.**  The item names `fn_80004438` (`src/MetroidPrime/Carve80004438.c`),
// these same eight instructions with `bl fn_80004458` in place of the `bl`.  A closer twin
// exists and is the better evidence, because it is the same *kind* of function: `fn_8010EECC`
// (0x8010EECC, 0x20 = 32 bytes, `src/MetroidPrime/Carve8010EE5C.c:132`), another subclass's
// `AcceptScriptMsg` forwarding to its base's, is
// `build/G2ME01/asm/MetroidPrime/Carve8010EE5C.s:41-50` - the identical instruction sequence,
// including the absence of any register move, with `bl AcceptScriptMsg__6CActorFR13CStateManagerRC10CScriptMsg`
// in place of this `bl`.  Its source declares all three parameters and forwards all three, which
// is the spelling reproduced here; a two-parameter reading could not compile to the same bytes,
// because the callee takes `CStateManager&` and `const CScriptMsg&` and retail moved neither.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range,
// `fn_801E8874` (0x801E8874..0x801E8ACC) ends exactly where this claim begins and no unit claims
// it; above it, `fn_801E8AEC` (0x801E8AEC..0x801E8AF4) begins exactly where this claim ends and is
// already claimed by `MetroidPrime/ScriptObjects/Carve801E8AEC.c`.  Both sit inside
// `auto_03_801E8070_text` (0x801E8070..0x801E8AEC), whose end this carve takes; the rest of that
// range stays where the seeder found it.  The nearest claimed range below is
// `MetroidPrime/Cameras/Carve801E8028.c` (0x801E8028..0x801E8070), the two-step `rstl::construct`
// chain for `CCameraShakerData`, and the one above is the same camera neighbourhood's
// `MetroidPrime/ScriptObjects/Carve801E8AEC.c` - so `MetroidPrime/Cameras/` is where this file
// sits, which is what the item's target says too.  Note the carve takes the **tail** of the
// auto range, so that range merely shortens; it is not split in two, which is the shape that
// produced the `CFrustumPlanes.cpp` link-order cycle recorded in `RUNNING_THE_DECOMP.md`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  One function here, so the order is trivially satisfied; it
// must stay that way if a second function is ever added.
//
// Retail names neither this function nor its class, so `symbols.txt` carries the `fn_<addr>`
// placeholder and this file reproduces that symbol verbatim; the definitions therefore have to
// stay C, because a C++ one would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.
// That is also why the unit is a `.c` rather than a `.cpp`.  The file is compiled as C for the
// port's host build and, like every other source in `files.cmake`, is syntax-checked as C++ by
// `tools/probe_sources.sh` - hence the explicit casts, which are compile-time only and leave the
// object byte-identical.

/** 0x801B0EC0, `symbols.txt:7078`, 0x20 = 32 bytes: `CGameCamera::AcceptScriptMsg(CStateManager&,
 *  const CScriptMsg&)`, the callee of `fn_801E8ACC` below.  It is claimed by the `Matching` unit
 *  `MetroidPrime/Cameras/CGameCamera.cpp` (0x801B0500..0x801B1A18), so the matching build's link
 *  resolves it from that object and nothing here defines it.  `CScriptMsg` is spelled
 *  `const void*` for the reason the twin `src/MetroidPrime/Carve8010EE5C.c:123-128` gives: this
 *  file never dereferences it, and the twin forwards its pointer the same way. */
extern void AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg(void* self, void* mgr,
                                                                          const void* msg);

/** `fn_801E8ACC` - retail `.text:0x801E8ACC`, 0x20 = 32 bytes: the `AcceptScriptMsg` override of
 *  the class behind vtable `lbl_803B7708`, a frame and one call to `CGameCamera::AcceptScriptMsg`
 *  with all three arguments forwarded untouched and nothing else.  Its byte-for-byte twin is
 *  `fn_8010EECC` (`src/MetroidPrime/Carve8010EE5C.c:132`), these eight instructions exactly. */
void fn_801E8ACC(void* self, void* mgr, const void* msg);

void fn_801E8ACC(void* self, void* mgr, const void* msg) {
  AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg(self, mgr, msg);
}

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg`
// is `CGameCamera::AcceptScriptMsg`, and `src/MetroidPrime/Cameras/CGameCamera.cpp` is not in
// `files.cmake` (only `CGameCameraSetAspectRatio.cpp` and `CGameCameraGetPerspectiveMatrix.cpp`
// are), so the host has no definition of it - `grep -rn 'AcceptScriptMsg__11CGameCamera' src/ include/`
// returns this block and nothing else.  The block exists so the host link resolves the `bl` in
// `fn_801E8ACC`, and it is the same announced-empty-body trade
// `src/MetroidPrime/Cameras/Carve801E8028.c:98-117` makes for `fn_801E8070` and
// `src/MetroidPrime/Cameras/Carve801E7C14.c:120-141` makes for `__dt__17CCameraShakerDataFv`.
// It drops every script message that reaches this override; that is unreachable in the port
// anyway, because the only thing that reaches it is `lbl_803B7708`, a dtk-only data object in an
// unclaimed range with no instance of the class for the port to dispatch on.
//
// The guard is `__MWERKS__`, not `TARGET_PC`, for the reason the two blocks above give: the
// matching build must take the symbol from `CGameCamera.cpp`'s own object, and a second
// definition there would be a duplicate.
void AcceptScriptMsg__11CGameCameraFR13CStateManagerRC10CScriptMsg(void* self, void* mgr,
                                                                    const void* msg) {
  (void)self;
  (void)mgr;
  (void)msg;
}
#endif