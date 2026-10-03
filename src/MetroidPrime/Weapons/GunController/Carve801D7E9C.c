// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the address and
// size come from `config/G2ME01/symbols.txt:7609`
// (`fn_801D7E9C = .text:0x801D7E9C; // type:function size:0x20 align:4`), the instructions are the
// ones dtk itself emitted into `build/G2ME01/asm/auto_03_801D72D0_text.s:834-844` before this claim
// existed (the same range is now this unit's own
// `build/G2ME01/asm/MetroidPrime/Weapons/GunController/Carve801D7E9C.s`), the bytes were re-read
// this run straight off the disc with `python3 tools/dol_read.py 0x801D7E9C 0x20`, and the body
// below is the C those bytes are the compilation of.
//
// .text 0x801D7E9C..0x801D7EBC, 0x20 = 32 bytes, 1 function:
//
//   fn_801D7E9C    0x801D7E9C  0x20    8 instructions   `CGunWeapon::Update(float, CStateManager&)`
//
// Measured off the disc (`dol_read.py 0x801D7E9C 0x20`):
//
//   94 21 ff f0  stwu r1,-0x10(r1)      80 01 00 14  lwz  r0,0x14(r1)
//   7c 08 02 a6  mflr r0                7c 08 03 a6  mtlr r0
//   90 01 00 14  stw  r0,0x14(r1)       38 21 00 10  addi r1,r1,0x10
//   48 00 34 19  bl   0x801db2c0        4e 80 00 20  blr
//
// **What it is: retail's `CGunWeapon::Update(float, CStateManager&)`, unnamed, forwarding to the
// 0x26C-byte `Update__10CGunWeaponFfR13CStateManager` at 0x801DB2C0.**  All four legs of that are
// read off tables and the disc rather than guessed:
//
//   - The **callee is retail-named**: `symbols.txt:7661` has
//     `Update__10CGunWeaponFfR13CStateManager = .text:0x801DB2C0; // type:function size:0x26C`, and
//     the `bl` displacement in the bytes above is 0x3419 from 0x801D7EA8, i.e. 0x801DB2C0.  That
//     range is inside `MetroidPrime/Weapons/CGunWeapon.cpp`'s existing claim
//     (0x801D8254..0x801DC1B8) and its own object defines the symbol
//     (`nm build/G2ME01/obj/MetroidPrime/Weapons/CGunWeapon.o`:
//     `0000306c T Update__10CGunWeaponFfR13CStateManager`), so the DOL link already supplies it and
//     this carve adds no undefined symbol to it.  In the host link the same function is
//     `_ZN10CGunWeapon6UpdateEfR13CStateManager` - Itanium mangling - so the MWCC spelling is
//     unresolved there and the port's announced stand-in `stub_carve801d7e9c_0` in
//     `src/MetroidPrime/PortLinkStubs.cpp` defines the name, the same trade `stub_80004438_0`
//     makes for `Carve80004438.c`'s callee.
//   - **The function is a vtable slot, and which slot is measured two ways.**  0x801D7E9C occurs
//     exactly **once** in `main.dol`, as a `.data` word at file 0x3B4344 = 0x803B7264 (`.data`
//     0x803B0C00@0x3ADCE0).  That word is base **+ 0x2C** of the table at 0x803B7238, and reading
//     that table out of the disc in order gives
//     `fn_801D8090` (the destructor), `Reset__10CGunWeaponFR13CStateManager`,
//     `PlayAnim__10CGunWeaponFQ212NWeaponTypes12EGunAnimTypeb`,
//     `PreRenderGunFx__10CGunWeaponFRC13CStateManagerRC12CTransform4f`, `fn_801D7FC4`,
//     `fn_801D7EBC`, `fn_801D76BC`, `EnableFx__10CGunWeaponFb`, `fn_801D744C`,
//     `Draw__10CGunWeaponCFbiRC13CStateManagerRC11CModelFlagsPC12CActorLights`,
//     `DrawMuzzleFx__10CGunWeaponCFRC13CStateManager`, **fn_801D7E9C**,
//     `UpdateMuzzleFx__10CGunWeaponFfRC9CVector3fRC9CVector3fb`,
//     `ActivateCharge__10CGunWeaponFbb`, `Unk8__10CGunWeaponFv`, `fn_801D72D0`, `fn_801D7688`,
//     `fn_801D7628`, `fn_801D7584`, `fn_801D75A4`, `SetModelTouchEnabled__10CGunWeaponFb` -
//     which is `include/MetroidPrime/Weapons/CGunWeapon.hpp:66-107`'s `virtual` list in
//     declaration order, twelve slots in, and `include/MetroidPrime/Weapons/CGunWeapon.hpp:93`
//     is `virtual void Update(float dt, CStateManager& mgr);`.
//   - **An independently identified table has its `Update` at the same 0x2C.**  `CPowerBeam`'s table
//     starts at 0x803B71D8 - its base slot is `__dt__10CPowerBeamFv` (0x801D5AD8,
//     `symbols.txt:7573`) - and base + 0x2C holds `Update__10CPowerBeamFfR13CStateManager`
//     (0x801D5528, `symbols.txt:7568`, size 0x1CC), a retail-named override of the same slot.  Two
//     tables, the same offset, one of them named: that is what pins 0x2C to `Update` without
//     relying on the header.
//   - **And retail's own source says the base implementation is a forwarder.**
//     `src/MetroidPrime/Weapons/CPowerBeam.cpp:129-131` is `void CPowerBeam::Update(float dt,
//     CStateManager& mgr) { CGunWeapon::Update(dt, mgr); ...` in a `Matching` unit, and the callee
//     those eight instructions call is exactly that base method.  The body below is what an
//     override's first line has to be.
//
// The shape twin named in the item's brief, `fn_80004438` (`src/MetroidPrime/Carve80004438.c`,
// `Matching`), is these eight instructions with a different `bl` target, and so are `fn_80004C4C`
// (0x80004C4C, `Player/Carve80004C4C.c`), `__sys_free` (0x80008A28, `src/MetroidPrime/main.cpp`)
// and `fn_8016F69C` (0x8016F69C, `src/MetroidPrime/Carve8016F69C.c`) a fourth time.  That measures
// the *frame*, not the identity - a 0x20-byte frame-and-one-call is also what an `rstl::destroy`
// looks like - and the vtable slot above, with the named `CPowerBeam::Update` beside it, is what
// makes this one a `CGunWeapon::Update`.
//
// **The signature is measured too.**  There is no register move anywhere in the eight
// instructions, so `r3` (`this`), `f1` (the `float`) and `r4` (the `CStateManager&`) are already in
// the argument registers the callee wants and the forwarder passes all three through untouched -
// which is what a qualified base call produces, and why the declaration below takes the float by
// value in the same position retail reads it.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names this one nothing: `symbols.txt` carries the `fn_801D7E9C` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C - a C++ one would mangle to
// `_Z<len>fn_801D7E9C` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.  The file is compiled as C for the port's host build and, like every other source
// in `files.cmake`, is syntax-checked as C++ by `tools/probe_sources.sh`; both accept it as
// written.  The callee's own name is retail's MWCC spelling, which is what a plain C definition
// site emits the `bl` against, so it is declared verbatim below and never defined here.
//
// Its own unit because a claim may not span an unclaimed gap.  Below this range
// 0x801D72D0..0x801D7E9C is still dtk's `auto_03_801D72D0_text` (`fn_801D76BC`, 0x7E0, ends exactly
// at this claim), and above it 0x801D7EBC..0x801D8248 is the rest of the same unclaimed run
// (`fn_801D7EBC`, 0x108, is the function immediately above).  Both neighbours stay unclaimed, and
// so does `fn_801D8248`'s claim between this one and `MetroidPrime/Weapons/CGunWeapon.cpp`.
//
// The directory is retail's own, taken from the nearest claimed ranges:
// `MetroidPrime/Weapons/GunController/CGunMotion.cpp` (0x801D6BDC..0x801D72D0) is the claim below
// this one, `MetroidPrime/Weapons/GunController/Carve801D8248.c` (0x801D8248..0x801D8254) the one
// above.  Every function in this gap is a `CGunWeapon` or `CPowerBeam` method, so the `Weapons/`
// neighbourhood is what the address itself says, and `GunController/` is where the already-carved
// pieces of the same `auto_*` run live.

/** 0x801DB2C0, `symbols.txt:7661`, 0x26C = 620 bytes: `CGunWeapon::Update(float, CStateManager&)`,
 *  retail's own name for the method this claim's forwarder calls.  Declared under that exact
 *  spelling, which is what makes the `bl` land on it.  Declared, never defined here: the range
 *  belongs to `MetroidPrime/Weapons/CGunWeapon.cpp`'s existing claim, so that unit's own object
 *  supplies the bytes in the DOL link; in the host link the announced stand-in
 *  `stub_carve801d7e9c_0` in `src/MetroidPrime/PortLinkStubs.cpp` defines the MWCC name, because a
 *  host compiler emits the same method as `_ZN10CGunWeapon6UpdateEfR13CStateManager`.  Nothing
 *  here claims 0x801DB2C0 is decompiled. */
extern void Update__10CGunWeaponFfR13CStateManager(void* self, float dt, void* mgr);

/** `fn_801D7E9C` - retail `.text:0x801D7E9C`, 0x20 = 32 bytes, 8 instructions:
 *  `CGunWeapon::Update`, the `Update` slot (base + 0x2C) of the vtable at 0x803B7238, a frame and
 *  one call to `Update__10CGunWeaponFfR13CStateManager` above with all three arguments untouched -
 *  no register move before the `bl`, which is what a qualified base call produces.  Its twin is
 *  `fn_80004438` (0x80004438, 0x20, `src/MetroidPrime/Carve80004438.c`, `Matching`), the same
 *  eight instructions with a different `bl` target. */
void fn_801D7E9C(void* self, float dt, void* mgr);

void fn_801D7E9C(void* self, float dt, void* mgr) {
  Update__10CGunWeaponFfR13CStateManager(self, dt, mgr);
}