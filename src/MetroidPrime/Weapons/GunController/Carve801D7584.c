// Carved out of an unclaimed dtk `auto_*` range (goal item `carve-801d7584`).  Every number here
// is measured: the address and size come from `config/G2ME01/symbols.txt:7604`, the instructions
// are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_801D72D0_text.s:205-215` while
// this range was unclaimed, and the body below is the C those bytes are the compilation of.
//
// .text 0x801D7584..0x801D75A4, 0x20 = 32 bytes, 1 function:
//
//   fn_801D7584    0x801D7584  0x20     8 instructions
//       0x801D7584  94 21 FF F0   stwu   r1, -0x10(r1)
//       0x801D7588  7C 08 02 A6   mflr   r0
//       0x801D758C  90 01 00 14   stw    r0, 0x14(r1)
//       0x801D7590  48 00 25 95   bl     IsLoaded__10CGunWeaponCFv
//       0x801D7594  80 01 00 14   lwz    r0, 0x14(r1)
//       0x801D7598  7C 08 03 A6   mtlr   r0
//       0x801D759C  38 21 00 10   addi   r1, r1, 0x10
//       0x801D75A0  4E 80 00 20   blr
//
// **What it is: the `IsLoaded() const` virtual of the class whose vtable is `lbl_803B7310`,
// forwarding straight to `CGunWeapon::IsLoaded()` and adding nothing.**  That is measured from
// the vtable, not inferred.  `lbl_803B7310` (`symbols.txt:18284`, `.data:0x803B7310`, size 0x60,
// unclaimed, dtk's own `auto_07_803B7310_data`) holds 24 words; word 2 is `fn_801D8090`
// (`symbols.txt:7613`, 0xD0 bytes - its own prologue stores `lbl_803B7310` into `this+0`,
// `auto_03_801D72D0_text.s:989-991`) and word 23 is `SetModelTouchEnabled__10CGunWeaponFb`, the
// last virtual `include/MetroidPrime/Weapons/CGunWeapon.hpp:107` declares.  Laid side by side
// with `__vt__10CPowerBeam` (`build/G2ME01/asm/MetroidPrime/Weapons/CPowerBeam.s:945-970`, the
// vtable at 0x803B72B0, immediately below this one in `.data`) the two agree word for word on
// every slot that `CGunWeapon` itself defines - `Reset`, `PlayAnim`, `PreRenderGunFx`,
// `EnableFx`, `Draw`, `DrawMuzzleFx`, `UpdateMuzzleFx`, `ActivateCharge`, `Unk8`,
// `SetModelTouchEnabled` - and differ only where this class overrides.  The 21st word is where
// `__vt__10CPowerBeam` has `IsLoaded__10CPowerBeamCFv` and `lbl_803B7310` has **`fn_801D7584`**,
// so this is that class's `IsLoaded() const` override and nothing else.  The constructor that
// installs the vtable is `fn_801D81C8` (`symbols.txt:7615`, 0x80 bytes,
// `auto_03_801D72D0_text.s:1071-1104`): it calls `__ct__10CGunWeaponF11EWeaponType9TUniqueIdRC9CVector3fi`
// with `li r4, 0x2` - `EWeaponType 2` is `kWT_Light` (`include/MetroidPrime/Weapons/WeaponTypes.hpp:10`)
// - then `__construct_array` at `this + 0x274` and `stw r0, 0x2a8(this)`.
//
// **The 8 instructions are a frame, one call and an epilogue - the shape of `fn_80004438`.**  The
// measured twin is `fn_80004438` (0x80004438, 0x20, `symbols.txt:73`, the `Matching` unit
// `src/MetroidPrime/Carve80004438.c:97`, `void fn_80004438(void* self) { fn_80004458(self); }`):
// the same eight instructions with the same encodings apart from the `bl` displacement - see
// `build/G2ME01/obj/MetroidPrime/Carve80004438.o`, whose `.text` is
// `94 21 ff f0 / 7c 08 02 a6 / 90 01 00 14 / 48 00 00 01 / 80 01 00 14 / 7c 08 03 a6 / 38 21 00 10 / 4e 80 00 20`.
// The two compute unrelated things; only the emitted bytes are shared, which is what makes
// this a carve rather than a translation: retail's body here is `return IsLoaded(self);` with
// the result left in `r3`, because there is no `mr r3, ...` and no test after the call.
//
// The callee is retail's own linker name in the matching build and the port's own mangled one
// outside it.  `CGunWeapon::IsLoaded` **is** in the port - `src/MetroidPrime/Weapons/CGunWeapon.cpp:300`,
// `bool CGunWeapon::IsLoaded() const { return mLoaded; }` - but the host compiler mangles it the
// Itanium way, so under `#else` the call below names `_ZNK10CGunWeapon8IsLoadedEv` (measured with
// `build/binutils/powerpc-eabi-nm` on
// `build-port-link/CMakeFiles/mp_game.dir/src/MetroidPrime/Weapons/CGunWeapon.cpp.o`, which defines
// it) and the port gets the real `mLoaded` bit at +0x270.  Only the matching build needs retail's
// linker name, and there dtk's own `CGunWeapon.o` supplies the definition.  The same
// `#ifdef __MWERKS__` split is the one `src/MetroidPrime/Carve8010EE5C.c:143-149` uses.
//
// **The name is retail's, so the unit is `.c`.**  `symbols.txt:7604` declares
// `fn_801D7584 = .text:0x801D7584; // type:function size:0x20 align:4` - retail names no such
// function and the `fn_<addr>` placeholder is all there is.  A `.cpp` unit would mangle it to
// `_Z<len>fn_801D7584Pv` and objdiff would pair nothing; `.c` is compiled with `-lang=c`, so the
// definition below *is* the symbol.  No other unit in `src/` defines it -
// `grep -rn fn_801D7584 src/ include/` finds nothing - so there is exactly one definition and no
// `PortLinkStubs.cpp` duplicate to delete.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Its own unit because a claim may not span an unclaimed gap.  `MetroidPrime/Weapons/
// GunController/CGunMotion.cpp` claims `.text` up to 0x801D72D0 and `MetroidPrime/Weapons/
// CGunWeapon.cpp` claims from 0x801D8254, and this 32-byte function sits in the dtk
// `auto_03_801D72D0_text` gap between them.  What is left of that gap, 0x801D72D0..0x801D7584
// and 0x801D75A4..0x801D8248, is deliberately still unclaimed: `fn_801D72D0`, `fn_801D744C` and
// the rest of the `CGunWeapon`-adjacent block, none of which is a copy of anything already
// matched.  The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/Weapons/GunController/CGunMotion.cpp` and above is
// `MetroidPrime/Weapons/GunController/Carve801D8248.c` (0x801D8248..0x801D8254).
//
// The port compiles this file too (`files.cmake`) and it adds no undefined symbol: under `#else`
// the callee it names is one `CGunWeapon.cpp` already defines.

#ifdef __MWERKS__
extern int IsLoaded__10CGunWeaponCFv(const void* self);
#define cgw_isloaded IsLoaded__10CGunWeaponCFv
#else
extern int _ZNK10CGunWeapon8IsLoadedEv(const void* self);
#define cgw_isloaded _ZNK10CGunWeapon8IsLoadedEv
#endif

/** 0x801D7584, `symbols.txt:7604`, size 0x20: this class's `IsLoaded() const` override - the
 *  21st word of `lbl_803B7310`, where `__vt__10CPowerBeam` has `IsLoaded__10CPowerBeamCFv` -
 *  forwarding to `CGunWeapon::IsLoaded()` and adding nothing of its own.  The `const void*` is
 * `this`; the callee reads the `mLoaded` bit at +0x270 and the result stays in `r3`. */
int fn_801D7584(const void* self);

int fn_801D7584(const void* self) { return cgw_isloaded(self); }
