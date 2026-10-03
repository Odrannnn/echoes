// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218B68_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218B68..0x80218B70, 0x8 = 8 bytes, 1 function:
//
//   fn_80218B68    0x80218B68  0x8    stw     r3, gLoader_MetroidAlpha@sda21(r0)
//                                          blr
//
// **What it is: MetroidAlpha's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's), of
// `fn_80213CB8` in `Carve80213CB8.c` (Sporb's, same carve shape) and of `fn_80200E70` in
// `Carve80200E70.c` (Kralee's): store the argument into the loader pointer's `.sbss` slot and
// return.  The four differ only in the 16-bit `@sda21` displacement, because their slots are 8
// bytes apart - `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_Kralee` at
// 0x80419360 is `90 6D 95 E0`, `gLoader_SporbBase` at 0x804193A8 is `90 6D 96 28`, and
// `gLoader_MetroidAlpha` at 0x80419418 is `90 6D 96 98`.  Every other byte of the four is
// identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 40's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/Metroid/asm/MetroidPrime/ScriptObjects/CMetroidRel.s`):
//
//   * `RELExit` (`.text:0xE8`, 0x24 bytes) does `li r3, 0x0` and then `bl fn_80218B68` - the
//     module tears the loader down on the way out.
//   * `fn_40_12C` (`.text:0x12C`, 0x50 bytes, the loader registration `RELMain` reaches) does
//     `stwu r7` into `lbl_40_bss_10` at `+0`, copies three words out of `lbl_40_data_358` into
//     `+4`, `+8` and `+0xC`, and only then calls `fn_80218B68` with `r3` still pointing at
//     `lbl_40_bss_10`.  So the argument is that record's address, and the record is **0x10
//     bytes**: one 4-byte `FScriptLoader` and one 12-byte CodeWarrior
//     pointer-to-member-function (`build/G2ME01/Metroid/asm/auto_05_00000000_bss.s` gives
//     `lbl_40_bss_10 size:0x10`, and `CMetroidRel.cpp`'s `SMetroidAlpha_FuncPtrs` says what
//     the two members are).  It is not the four bytes most of this family uses, and the DOL is
//     what says so twice over: `LoadMetroidAlpha` at 0x80218B3C calls word 0 as the loader
//     (`lwz r6, gLoader_MetroidAlpha@sda21(r0); lwz r12, 0x0(r6)`,
//     `build/G2ME01/asm/MetroidPrime/ScriptLoader/MetroidAlpha.s`), and
//     `OnDockTouch__13CMetroidAlphaFR13CStateManager` at 0x80218B10 does
//     `addi r12, r5, 0x4; bl __ptmf_scall` on it
//     (`build/G2ME01/asm/auto_03_80218B08_text.s`), so words 4..15 are the pmf.
//
// The slot is `gLoader_MetroidAlpha` at `.sbss 0x80419418..0x80419420`, which
// `MetroidAlpha.cpp` already claims and already defines (`SLoaderSlot { FScriptLoader* value;
// unsigned int padding; }`), so this unit claims `.text` only and takes the pointer as `extern`.
// That unit's header records why the setter was deliberately left unclaimed - "REL modules
// import it by its retail name, so it cannot be renamed and must stay in dtk's auto unit" - and
// the carve is what removes that exception without renaming anything: the import name is the
// plain `fn_80218B68` that `config/G2ME01/symbols.txt:9485` already gives it (checked in the
// module's own `build/G2ME01/Metroid/Metroid.preplf` import table), so defining it here keeps
// the unmangled symbol in the DOL link and module 40's two `bl fn_80218B68` calls still
// resolve against it.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the two neighbours
// are claimed units: `MetroidAlpha.cpp` ends at 0x80218B68 and `GunTurretBase.cpp` starts at
// 0x80218B70.

/** The record module 40 hands over, 0x10 bytes as measured above.  Only its address is stored,
 *  so the field types describe the shape rather than a layout this unit reads;
 *  `CMetroidRel.cpp`'s `SMetroidAlpha_FuncPtrs` is the same record in C++ and says what the
 *  two members are.  Declared **above** the prototype below, not inside it: a struct named in
 *  a parameter list is scoped to that list, and the host build then rejects the definition as
 *  a conflicting type. */
struct SMetroidAlphaFuncPtrs {
  void* loader;                    /* FScriptLoader */
  unsigned int onDockTouch[3];     /* void (CEntity::*)(CStateManager&) - 3 words */
};

/** `MetroidAlpha.cpp` defines this in `.sbss 0x80419418` and reads `value` at `+0`; MWCC does
 *  not encode a variable's type in its name, so this references `gLoader_MetroidAlpha`
 *  itself whatever the type is spelled. */
extern struct SMetroidAlphaFuncPtrs* gLoader_MetroidAlpha;

void fn_80218B68(struct SMetroidAlphaFuncPtrs* record) { gLoader_MetroidAlpha = record; }
