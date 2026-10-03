// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218BC8_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218BC8..0x80218BD0, 0x8 = 8 bytes, 1 function:
//
//   fn_80218BC8    0x80218BC8  0x8    stw     r3, gLoader_GunTurretBase@sda21(r0)
//                                          blr
//
// **What it is: GunTurretBase's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`), which is the whole shape of the family:
// store the argument into the loader pointer's `.sbss` slot and return.  The setter family
// differs in exactly the 16-bit `@sda21` displacement, because the slots are 8 bytes apart.
// Measured with `build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` against
// `_SDA_BASE_ = 0x8041FD80` (`nm`):
//
//   fn_80200E3C  gLoader_SpacePirate     0x80419358   90 6D 95 D8   -0x6A28
//   fn_80200E70  gLoader_Kralee          0x80419360   90 6D 95 E0   -0x6A20
//   fn_80200EFC  gLoader_Parasite        0x80419368   90 6D 95 E8   -0x6A18
//   fn_80200F30  gLoader_PillBug         0x80419370   90 6D 95 F0   -0x6A10
//   fn_80213CB8  gLoader_SporbBase       0x804193A8   90 6D 96 28   -0x69D8
//   fn_8021887C  gLoader_Sandworm        0x804193C0   90 6D 96 40   -0x69C0
//   fn_802188E4  gLoader_DarkSamus       0x804193D0   90 6D 96 50   -0x69B0
//   fn_80218918  gLoader_Ings            0x804193D8   90 6D 96 58   -0x69A8
//   fn_802189D0  gLoader_SandBoss        0x804193E0   90 6D 96 60   -0x69A0
//   fn_80218BC8  gLoader_GunTurretBase   0x80419420   90 6D 96 A0   -0x6960
//
// Every other byte of all ten is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 28's (GunTurret) own listing and neither calls it a setter by
// name, so the argument is read off them (`build/G2ME01/GunTurret/asm/
// auto_00_00000404_text.s`):
//
//   * `RELExit` at .text 0x430 (0x24 bytes) does `li r3, 0x0` then `bl fn_80218BC8` - the
//     module tears its loaders down on the way out.
//   * `fn_28_474` at .text 0x474 (0x3C bytes, reached from `RELMain` at 0x454) does
//     `lis r5, fn_28_4B0@ha` , `addi r5, r5, fn_28_4B0@l` , `lis r4, fn_28_7B98@ha` ,
//     `addi r0, r4, fn_28_7B98@l` , `lis r3, lbl_28_bss_60@ha` , `stwu r5, lbl_28_bss_60@l(r3)`
//     , `stw r0, 0x4(r3)` and then `bl fn_80218BC8` with `r3` still pointing at
//     `lbl_28_bss_60`.  So the argument is that record's address, and the record is **8 bytes -
//     two `FScriptLoader`s** (`build/G2ME01/GunTurret/asm/auto_05_00000000_bss.s` reads
//     `# .bss:0x60 | 0x60 | size: 0x8` / `.obj lbl_28_bss_60, global`, and this module has six
//     such 0x8-byte loader records at 0x0, 0x20, 0x40, 0x60 plus three 0x18-byte ones and two
//     0xC-byte ones).  `src/MetroidPrime/ScriptLoader/GunTurretBase.cpp` is that record in
//     C++ - `SGunTurretBaseLoaders { FScriptLoader slot0; FScriptLoader slot1; }` - and its
//     `LoadGunTurretBase` reads `+0` while `LoadGunTurretTop` reads `+4`, which is exactly the
//     two words `fn_28_474` stores; which module function is which of the two is not settled
//     here, so the field comments below do not claim it.
//
// The slot is `gLoader_GunTurretBase` at `.sbss 0x80419420` (`type:object size:0x8
// data:4byte`, `config/G2ME01/symbols.txt:20717`), which `GunTurretBase.cpp` - a `Matching`
// unit claiming `.text 0x80218B70..0x80218BC8` and `.sbss 0x80419420..0x80419428` - already
// defines (`SLoaderSlot { SGunTurretBaseLoaders* value; unsigned int padding; }`) and already
// reads through at `+0`.  Its own header reserves these eight bytes for a separate unit: "The
// 8-byte setter at 0x80218BC8 is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit."  That is this file.
// So this unit claims `.text` only and takes the pointer as `extern`.
//
// REL modules import this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_80218BC8` in the DOL link, which is what module 28's
// `bl fn_80218BC8` resolves against.  Listing this file in `files.cmake` costs the flat host
// link nothing: it adds a definition, and `build/goal/judge/undef.base.txt`'s undefined count
// is a floor, not a ceiling.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the two
// neighbours are both claimed units: `GunTurretBase.cpp` ends at 0x80218BC8 and
// `MetroidPrime/ScriptLoader/Lumite.cpp` starts at 0x80218BD0, so this run slots between them
// with no unclaimed gap on either side.  The whole neighbourhood 0x80218850..0x80218E28 is
// one loader-thunk family - Sandworm, CommandPirate, DarkSamus, Ings, SandBoss, FlyingPirate,
// Grenchler, MediumIng, MinorIng, ElitePirate, Blogg, MetroidAlpha, GunTurretBase, this
// setter, Lumite, Shrieker, Splinter, SplitterMainChassis, ChozoGhost - which is retail's own
// naming and is why the unit goes in `MetroidPrime/ScriptLoader/`.

/** The record module 28 hands over, 0x8 bytes as measured above.  Only its address is
 *  stored, so the field types describe the shape rather than a layout this unit reads;
 *  `GunTurretBase.cpp`'s `SGunTurretBaseLoaders` is the same record in C++ and is where the
 *  two loaders are named.  Declared **above** the prototype below, not inside it: a struct
 *  named in a parameter list is scoped to that list, and the host build then rejects the
 *  definition as a conflicting type. */
struct SGunTurretBaseLoaders {
  void* gunTurretBase; /* FScriptLoader at +0 - read by LoadGunTurretBase */
  void* gunTurretTop;  /* FScriptLoader at +4 - read by LoadGunTurretTop */
};

/** `GunTurretBase.cpp` defines this in `.sbss 0x80419420` and dereferences it at `+0`; MWCC
 *  does not encode a variable's type in its name, so this references `gLoader_GunTurretBase`
 *  itself whatever the type is spelled - here the 4-byte pointer member of its
 *  `SLoaderSlot`, which is the only word retail stores. */
extern struct SGunTurretBaseLoaders* gLoader_GunTurretBase;

void fn_80218BC8(struct SGunTurretBaseLoaders* loaders) { gLoader_GunTurretBase = loaders; }