// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:8028` and `:1011`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_801F0D24_text.s:1488-1498` before the claim
// existed (the same range is now this unit's own
// `build/G2ME01/asm/MetroidPrime/Carve801F21E4.s`), the bytes were re-read this run straight off
// the disc with `python3 tools/dol_read.py 0x801F21E4 0x20`, and the body below is the C those
// bytes are the compilation of.
//
// .text 0x801F21E4..0x801F2204, 0x20 = 32 bytes, 1 function:
//
//   fn_801F21E4    0x801F21E4  0x20    8 instructions   a virtual `Touch`, forwarded to the base
//
// Measured off the disc (`dol_read.py 0x801F21E4 0x20`):
//
//   94 21 ff f0  stwu r1,-0x10(r1)      80 01 00 14  lwz  r0,0x14(r1)
//   7c 08 02 a6  mflr r0                7c 08 03 a6  mtlr r0
//   90 01 00 14  stw  r0,0x14(r1)       38 21 00 10  addi r1,r1,0x10
//   4b e4 38 e9  bl   0x80035ad8        4e 80 00 20  blr
//
// **What it is: `CEnergyProjectile::Touch(CActor&, CStateManager&)`, forwarding to
// `CGameProjectile::Touch`.**  Both halves of that are read off the tables rather than guessed:
//
//   - `0x801F21E4` appears exactly **once** in `main.dol`, as the `.4byte fn_801F21E4` at
//     `build/G2ME01/asm/auto_07_803B78F0_data.s:29`, and there is **no `bl` to it anywhere** in
//     `build/G2ME01/asm/` - so it is only ever reached through a vtable.  Reading that word's
//     table back (`lbl_803B78F0`, `.data:0x803B78F0`) puts it at **+0x4C**.
//   - +0x4C is the `Touch` slot, and that is read off the tables themselves rather than off
//     `CActor.hpp`.  A `CActor` table starts `__dt__`, `TypesMatch`, `PreThink`, `Think`,
//     `AcceptScriptMsg`, `SetActive`, `ClearFluidList`, `PreRender`, `AddToRenderer`, `Render`,
//     `CanRenderUnsorted`, `PreRenderAllViewports`, `HealthInfo`, `GetHealthInfo`,
//     `GetDamageVulnerability` (no args), `GetDamageVulnerability` (3 args), `GetTouchBounds`,
//     **`Touch`**, `GetOrbitPosition`, ... - `lbl_803B78F0` holds exactly that sequence, and so do
//     the other `CActor`-derived tables in `.data` whose entry in that position is
//     `Touch__6CActorFR6CActorR13CStateManager` (0x8004C564): **52** tables in `.data` hold that
//     word, and in every one of them the word *after* it is a `GetOrbitPosition` entry (48 of them
//     the base `GetOrbitPosition__6CActorCFRC13CStateManager`, four a derived override) while the
//     word *before* it is a `GetTouchBounds` in 45 - `CActor`'s own, which `TypesMatch__6CActorCFi`
//     at 0x803B8820 identifies, has `GetTouchBounds` at 0x803B885C, `Touch` at 0x803B8860 and
//     `GetOrbitPosition` at 0x803B8864.  That is 51 independently identified tables in the same
//     relative position, and it is what makes +0x4C the `Touch` slot - the one
//     `include/MetroidPrime/CActor.hpp:93` declares as
//     `virtual void Touch(CActor&, CStateManager&)`.
//   - the slot above it, +0x48, holds `fn_801F2204` (`symbols.txt:8029`, 0x9C bytes) which is
//     `GetTouchBounds` - it tests a flag bit off `r4` and stores a byte into the returned
//     `rstl::optional_object< CAABox >` - and +0x50 holds
//     `GetOrbitPosition__6CActorCFRC13CStateManager`, which is the slot after `Touch` in the
//     sequence above.  The table identifies this class too: +0x24 holds
//     `PreRender__17CEnergyProjectileFR13CStateManager`, +0x28
//     `AddToRenderer__17CEnergyProjectileCFRC13CStateManager`, +0x2C
//     `Render__17CEnergyProjectileCFRC13CStateManager`, +0x34
//     `PreRenderAllViewports__17CEnergyProjectileFR13CStateManager`, +0x6C
//     `GetSortingBounds__17CEnergyProjectileCFRC13CStateManager` and +0x7C
//     `StopProjectile__17CEnergyProjectileFR13CStateManager`, all named in
//     `include/MetroidPrime/Weapons/CEnergyProjectile.hpp:30,33`, while +0x64 and +0x80 hold
//     `FluidFXThink__15CGameProjectileFR6CActor11EFluidStateR12CScriptWaterR13CStateManager` and
//     `RayCollisionCheckWithWorld__15CGameProjectileF...`, the two the derived class does **not**
//     override.
//   - the callee is `Touch__15CGameProjectileFR6CActorR13CStateManager` at 0x80035AD8
//     (`symbols.txt:1011`, 0x60 bytes), and its bytes are retail's own `CGameProjectile::Touch`
//     work: a frame, `bl Touch__6CActorFR6CActorR13CStateManager`, then
//     `TCastToPtr< 11CScriptDock >` on the touched actor, and a store into `self` at +0x400 when
//     that succeeds and the two `TUniqueId`s match.  `src/MetroidPrime/Weapons/CGameProjectile.cpp:87`
//     is that body - `CActor::Touch(actor, mgr)`, the dock cast, `mTouchedDock = actor.GetUniqueId()`
//     into the member at 0x400 - so this override adds nothing and forwards all three arguments
//     untouched.  That no register move precedes the `bl` is what a qualified base call produces.
//
// The shape twin named in the item's brief, `fn_80004438` (`src/MetroidPrime/Carve80004438.c`,
// `Matching`), is these eight instructions with a different `bl` target, and so is
// `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`) a third time.  That is a measure of the
// *frame*, not of the identity: what makes this one a `Touch` and not the other reading of a
// 0x20-byte forwarder, an `rstl::destroy`, is the vtable slot above and the callee's own name.
// The closest match in the tree is `fn_8016F69C` (0x8016F69C, 0x20,
// `src/MetroidPrime/Carve8016F69C.c`, `Matching`), the same eight instructions for the same
// reason - a `Touch` override that qualifies its base call, there to `CActor::Touch` and here to
// the class one step further up.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names none of these in this range.  `symbols.txt:8028` carries the `fn_<addr>` placeholder
// and this file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z12fn_801F21E4PvPvS0_` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.  The file is compiled as C for the port's host build and, like
// every other source in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`; both
// accept this file as written.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range,
// 0x801F0D24..0x801F21E4 is still dtk's `auto_03_801F0D24_text` (`fn_801F2168`, 0x7C bytes, is the
// function immediately below), and above it 0x801F2204..0x801F2E4C is the rest of the same
// unclaimed run (`fn_801F2204`, 0x9C bytes, is the function immediately above).  Both neighbours
// stay unclaimed, and the boundaries are function boundaries: `fn_801F2168` ends on the `blr` at
// 0x801F21E0 and `fn_801F2204` starts at 0x801F2204, so neither run is cut mid-function.
//
// The directory is retail's own, taken from the nearest claimed ranges:
// `MetroidPrime/CRELFileManager.cpp` (0x801F0518..0x801F0D24) is the claim below this one and
// `MetroidPrime/Carve801F3690.c` (0x801F3690..0x801F3694) the one above, so this address is in
// the `MetroidPrime/` neighbourhood.

/** 0x80035AD8, `symbols.txt:1011`, 0x60 = 96 bytes: `CGameProjectile::Touch(CActor&, CStateManager&)`,
 *  the callee of `fn_801F21E4` below.  Declared here under retail's own mangled name, which is what
 *  a plain C definition site emits the `bl` against.  Declared, never defined here: the range
 *  belongs to `MetroidPrime/Weapons/CGameProjectile.cpp`'s existing claim (0x80032C3C..0x80036200),
 *  so dtk's object for that unit supplies the bytes in the DOL link and the symbol is nothing this
 *  carve can or should claim; in the host link the shim
 *  `Touch__15CGameProjectileFR6CActorR13CStateManager` in `src/Kyoto/Alloc/PortMwccNew.cpp` defines
 *  the name, and it calls the host's own `CGameProjectile::Touch`
 *  (`src/MetroidPrime/Weapons/CGameProjectile.cpp:87`). */
extern void Touch__15CGameProjectileFR6CActorR13CStateManager(const void* self, void* actor,
                                                              void* mgr);

/** `fn_801F21E4` - retail `.text:0x801F21E4`, 0x20 = 32 bytes, 8 instructions:
 *  `CEnergyProjectile::Touch`, a frame and one call to `CGameProjectile::Touch` above with all
 *  three arguments untouched - no register moves before the `bl`, which is what a qualified base
 *  call produces.  Its twin is `fn_80004438` (0x80004438, 0x20,
 *  `src/MetroidPrime/Carve80004438.c`, `Matching`), the same eight instructions with a different
 *  `bl` target. */
void fn_801F21E4(const void* self, void* actor, void* mgr);

void fn_801F21E4(const void* self, void* actor, void* mgr) {
  Touch__15CGameProjectileFR6CActorR13CStateManager(self, actor, mgr);
}
