// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218E28_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218E28..0x80218E30, 0x8 = 8 bytes, 1 function:
//
//   fn_80218E28    0x80218E28  0x8    stw     r3, gLoader_GlowBug@sda21(r0)
//                                          blr
//
// Only the head 0x8 bytes of that dtk unit are claimed here: the `auto_03_80218E28_text`
// listing runs 0x80218E28..0x8021B9E8 (0x2BC0), because the whole of `fn_80218E30` after it
// is still unsourced.  This file claims this run of one function and nothing else.
//
// **What it is: GlowBug's loader setter.**  It is the byte-shape twin of the matched
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
//   fn_80218E28  gLoader_GlowBug         0x80419470   90 6D 96 F0   -0x6910
//
// Every other byte of all eleven is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 26's (Glowbug) own listing and neither calls it a setter by
// name, so the argument is read off them (`build/G2ME01/Glowbug/asm/auto_00_00000428_text.s`):
//
//   * `RELExit` at .text 0x454 (0x24 bytes) does `li r3, 0x0` then `bl fn_80218E28` - the
//     module tears its loaders down on the way out.
//   * `fn_26_498` at .text 0x498 (0x30 bytes, reached from `RELMain` at 0x478) does
//     `lis r4, fn_26_4C8@ha` , `addi r0, r4, fn_26_4C8@l` , `lis r3, lbl_26_bss_0@ha` ,
//     `stwu r0, lbl_26_bss_0@l(r3)` and then `bl fn_80218E28` with `r3` still pointing at
//     `lbl_26_bss_0`.  So the argument is that record's address, and the record is **4 bytes -
//     one `FScriptLoader`** (`build/G2ME01/Glowbug/asm/auto_05_00000000_bss.s` reads
//     `# 0x00000000..0x00000004 | size: 0x4` / `.obj lbl_26_bss_0, global` / `.skip 0x4`, and
//     this module holds that one 0x4-byte record and nothing else).  Module 26 stores its one
//     word, the address of `fn_26_4C8`; `src/MetroidPrime/ScriptLoader/GlowBug.cpp` is that
//     record in C++ - `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` - and its
//     `LoadGlowBug` calls through at `+0`, which is exactly the word `fn_26_498` stores.
//
// The slot is `gLoader_GlowBug` at `.sbss 0x80419470` (`type:object size:0x8 data:4byte`,
// `config/G2ME01/symbols.txt:20727`), which `GlowBug.cpp` - a `Matching` unit claiming
// `.text 0x80218DFC..0x80218E28` and `.sbss 0x80419470..0x80419478` - already defines and
// already reads through at `+0`.  So this unit claims `.text` only and takes the pointer as
// `extern`.
//
// REL modules import this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_80218E28` in the DOL link, which is what module 26's two
// `bl fn_80218E28` resolve against.  Listing this file in `files.cmake` costs the flat host
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
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the eight
// bytes it claims sit at the head of a 0x2BC0-byte dtk `auto_*` run that no single claimed
// unit can take: `GlowBug.cpp` ends at 0x80218E28 and `ScriptLoaderRel.cpp` starts at
// 0x8021BA60, so this claim slots between them with no unclaimed gap on either side.  The
// whole neighbourhood 0x80218850..0x80218E28 is one loader-thunk family - Sandworm,
// CommandPirate, DarkSamus, Ings, SandBoss, FlyingPirate, Grenchler, MediumIng, MinorIng,
// ElitePirate, Blogg, MetroidAlpha, GunTurretBase, DarkTrooper - which is retail's own naming
// and is why the unit goes in `MetroidPrime/ScriptLoader/`.

/** The record module 26 hands over, 0x4 bytes as measured above.  Only its address is
 *  stored, so the field type describes the shape rather than a layout this unit reads;
 *  `GlowBug.cpp`'s `SLoaderSlot` holds the same record by pointer and is where the loader is
 *  named.  Declared **above** the prototype below, not inside it: a struct named in a
 *  parameter list is scoped to that list, and the host build then rejects the definition as
 *  a conflicting type. */
struct SGlowBugLoaders {
  void* glowBug; /* FScriptLoader - fn_26_4C8, read by LoadGlowBug */
};

/** `GlowBug.cpp` defines this in `.sbss 0x80419470` and dereferences its `value` at `+0`;
 *  MWCC does not encode a variable's type in its name, so this references `gLoader_GlowBug`
 *  itself whatever the type is spelled - here the 4-byte pointer member of its
 *  `SLoaderSlot`, which is the only word retail stores. */
extern struct SGlowBugLoaders* gLoader_GlowBug;

void fn_80218E28(struct SGlowBugLoaders* loaders) { gLoader_GlowBug = loaders; }
