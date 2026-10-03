// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8022EC64_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8022EC64..0x8022EC6C, 0x8 = 8 bytes, 1 function:
//
//   fn_8022EC64    0x8022EC64  0x8    stw     r3, gLoader_SwampBossStage1@sda21(r0)
//                                          blr
//
// This is the **first** run in that dtk unit, which covers 0x8022EC64..0x8022FF98 (0x1334
// bytes, 19 functions); this file claims those 8 bytes and nothing else, and
// `build/report.json` listed all 19 under `main/auto_03_8022EC64_text` before this carve.
//
// **What it is: SwampBossStage1's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of `fn_8022A024` in
// `src/MetroidPrime/ScriptLoader/Carve8022A024.c`
// (`stw r3, gLoader_IngSpiderBallGuardian@sda21(r0)` / `blr`), which is the whole shape of the
// family: store the argument into the loader pointer's `.sbss` slot and return.  The setter
// family differs in exactly the 16-bit `@sda21` displacement, because the slots are 8 bytes
// apart.  Measured with `build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf` against
// `_SDA_BASE_ = 0x8041FD80` (`build/binutils/powerpc-eabi-nm`):
//
//   fn_8022EC64  gLoader_SwampBossStage1  0x80419608   90 6D 98 88   -0x6778 (0x9888)
//
// Every other byte is identical to both twins above, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 78's (SwampBossStage1) own listing and neither calls it a setter
// by name, so the argument is read off them
// (`build/G2ME01/SwampBossStage1/asm/auto_00_00000000_text.s`):
//
//   * `RELExit` at .text 0xEC (0x24 bytes) does `li r3, 0x0` then `bl fn_8022EC64` - the
//     module tears its loader down on the way out.
//   * `fn_78_130` at .text 0x130 (0x30 bytes, reached from `RELMain` at 0x110) does
//     `lis r4, fn_78_160@ha` , `addi r0, r4, fn_78_160@l` , `lis r3, lbl_78_bss_20@ha` ,
//     `stwu r0, lbl_78_bss_20@l(r3)` and then `bl fn_8022EC64` with r3 still pointing at
//     `lbl_78_bss_20`.  So the argument is that record's address, and the record is **4 bytes -
//     one `FScriptLoader`** (`build/G2ME01/SwampBossStage1/asm/auto_05_00000000_bss.s` reads
//     `# .bss:0x20 | 0x20 | size: 0x4` / `.obj lbl_78_bss_20, global` / `.skip 0x4`; the same
//     listing holds `lbl_78_bss_0` `size:0x8` at 0x0 and `lbl_78_bss_8` `size:0x18` at 0x8, and
//     only the last is the loader slot).  Module 78 stores its one word, the address of
//     `fn_78_160`; `src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp` is that record in C++ -
//     `SLoaderSlot { FScriptLoader* value; unsigned int padding; }` - and its
//     `LoadSwampBossStage1` calls through at `+0`, which is exactly the word `fn_78_130`
//     stores.  `src/MetroidPrime/ScriptObjects/CSwampBossStage1Rel.cpp` is that module's head
//     in C++ and already records the same reading.
//
// The slot is `gLoader_SwampBossStage1` at `.sbss 0x80419608`
// (`type:object size:0x8 data:4byte`, `config/G2ME01/symbols.txt:20783`), which
// `SwampBossStage1.cpp` - a `Matching` unit claiming `.text 0x8022EC38..0x8022EC64` and
// `.sbss 0x80419608..0x80419610` - already defines and already reads through at `+0`.  So this
// unit claims `.text` only and takes the pointer as `extern`.
//
// REL modules import this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_8022EC64` in the DOL link, which is what module 78's two
// `bl fn_8022EC64` resolve against.  Listing this file in `files.cmake` costs the flat host
// link nothing: it adds a definition, and `build/goal/judge/undef.base.txt`'s undefined count
// is a floor, not a ceiling.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder
// (`symbols.txt:9943`: `fn_8022EC64 = .text:0x8022EC64; // type:function size:0x8 align:4`) and
// this file reproduces that symbol verbatim, so the definition has to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are claimed units: `SwampBossStage1.cpp`'s `.text` ends at 0x8022EC64 and
// `MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp` starts at 0x8022FF98, which is where
// this dtk unit ends.  The whole neighbourhood is retail's own loader-thunk family and is why
// the unit goes in `MetroidPrime/ScriptLoader/`; `docs/research/rel_loaders.md` has every
// loader in it.

/** The record module 78 hands over, 0x4 bytes as measured above.  Only its address is
 *  stored, so the field type describes the shape rather than a layout this unit reads;
 *  `SwampBossStage1.cpp`'s `SLoaderSlot` holds the same record by pointer and is where the
 *  loader is named.  Declared **above** the prototype below, not inside it: a struct named in
 *  a parameter list is scoped to that list, and the host build then rejects the definition as
 *  a conflicting type. */
struct SSwampBossStage1Loaders {
  void* swampBossStage1; /* FScriptLoader - fn_78_160, read by LoadSwampBossStage1 */
};

/** `SwampBossStage1.cpp` defines this in `.sbss 0x80419608` and dereferences its `value` at
 *  `+0`; MWCC does not encode a variable's type in its name, so this references
 *  `gLoader_SwampBossStage1` itself whatever the type is spelled - here the 4-byte pointer
 *  member of its `SLoaderSlot`, which is the only word retail stores. */
extern struct SSwampBossStage1Loaders* gLoader_SwampBossStage1;

void fn_8022EC64(struct SSwampBossStage1Loaders* loaders) { gLoader_SwampBossStage1 = loaders; }
