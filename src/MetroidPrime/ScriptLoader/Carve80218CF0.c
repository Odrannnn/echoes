// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80218CF0_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80218CF0..0x80218CF8, 0x8 = 8 bytes, 1 function:
//
//   fn_80218CF0    0x80218CF0  0x8    stw     r3, gLoader_SplitterMainChassis@sda21(r0)
//                                          blr
//
// **What it is: SplitterMainChassis's loader setter.**  It is the byte-shape twin of the
// matched `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (SpacePirate's),
// of the matched `fn_802189D0` in `src/MetroidPrime/ScriptLoader/Carve802189D0.c` (SandBoss's)
// and of the matched `fn_80200F30` in `src/MetroidPrime/ScriptLoader/Carve80200F30.c`
// (PillBug's), which between them are the whole shape of the family: store the argument into
// the loader pointer's `.sbss` slot and return.  The four differ in exactly the 16-bit
// `@sda21` displacement, because their slots are spread across `.sbss` -
// `gLoader_SpacePirate` at 0x80419358 is `90 6D 95 D8`, `gLoader_SandBoss` at 0x804193E0 is
// `90 6D 96 60`, and `gLoader_SplitterMainChassis` at 0x80419440 is `90 6D 96 C0`.  Every
// other byte of all four is identical, including the `4E 80 00 20` `blr`.
//
// Both callers are in module 75's own listing and neither calls it a setter by name, so the
// argument is read off them (`build/G2ME01/Splitter/asm/MetroidPrime/ScriptObjects/
// CSplitterRelMain.s`, the same listing
// `src/MetroidPrime/ScriptObjects/CSplitterRelMain.cpp` writes from):
//
//   * `RELExit` (0x8210, 0x24 bytes, `li r3, 0x0` / `bl fn_80218CF0`) passes null - the
//     module tears the loader down on the way out.
//   * `fn_75_8254` (0x8254, 0x5C bytes, reached from `RELMain`) does
//     `lis r3, lbl_75_bss_20@ha` , `stwu r8, lbl_75_bss_20@l(r3)` and then
//     `bl fn_80218CF0` with `r3` still pointing at `lbl_75_bss_20`.  So the argument is that
//     record's address, and the record is **5 words written into a 0x18-byte `.bss` object**:
//     `fn_75_82B0` at +0, `fn_75_FC` at +4, and three words copied out of `.data` at +8, +0xc
//     and +0x10 (`auto_04_00000000_data.s`, `lbl_75_data_CAC`) which is one 12-byte
//     CodeWarrior member-function pointer.  `config/G2ME01/rels/Splitter/symbols.txt:669` gives
//     `lbl_75_bss_20 = .bss:0x00000020; size:0x18 data:4byte`, and the 0x18 is `.bss`'s
//     `align:8` rounding the 0x14 the body writes.  So the record is the same
//     `SSplitter_FuncPtrs` `CSplitterRelMain.cpp` already declares: two `FScriptLoader`s and
//     `void (CEntity::*)(float)`.
//
// The slot is `gLoader_SplitterMainChassis` at `.sbss 0x80419440` (`config/G2ME01/symbols.txt:
// 20721`, `type:object size:0x8 data:4byte`), which `SplitterMainChassis.cpp` already claims
// and already defines - `SLoaderSlot { SSplitterMainChassisLoaders* value; unsigned int
// padding; }` - and whose `LoadSplitterMainChassis` (0x80218CC4) and
// `LoadSplitterCommandModule` (0x80218C98) read `value` at `+0`, which is why the store below
// hands it `&lbl_75_bss_20` and why that record is two loaders plus a member pointer.
// `SplitterMainChassis.cpp`'s own header reserved these eight bytes for a separate unit: "The
// 8-byte setter at 0x80218CF0 is deliberately NOT claimed: REL modules import it by its retail
// name, so it cannot be renamed and must stay in dtk's auto unit."  That is this file.  So
// this unit claims `.text` only and takes the pointer as `extern`.  REL modules import this
// function by its retail name, so it cannot be renamed; defining it here keeps the unmangled
// `fn_80218CF0` in the DOL link, which is what module 75's two `bl fn_80218CF0` resolve
// against.  `CSplitterRelMain.cpp` already declares it `void
// fn_80218CF0(SSplitter_FuncPtrs* record)` inside its `extern "C"` block and calls it under
// that name; MWCC does not encode a parameter type in a function name, so the two declarations
// agree.
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
// are claimed units: `SplitterMainChassis.cpp` ends at 0x80218CF0 and
// `MetroidPrime/ScriptLoader/ChozoGhost.cpp` starts at 0x80218CF8, so this run slots between
// them with no unclaimed gap on either side.

/** The record module 75 hands over, the 5 words `fn_75_8254` writes.  MWCC does not encode a
 *  variable's type in its name, so the `extern` below references `gLoader_SplitterMainChassis`
 *  itself whatever the type is spelled; `SplitterMainChassis.cpp` spells the same slot
 *  `SSplitterMainChassisLoaders* value` at `+0`, and this describes the record the module's
 *  `.bss` holds rather than a layout this unit reads.  The third member is a 12-byte
 *  CodeWarrior pointer-to-member-function written as three words here because this is C.
 *  Declared **above** the prototype below, not inside it: a struct named in a parameter list is
 *  scoped to that list, and the host build then rejects the definition as a conflicting type. */
struct SSplitterFuncPtrs {
  unsigned int slot0;    /* FScriptLoader */
  unsigned int slot1;    /* FScriptLoader */
  unsigned int method[3];/* void (CEntity::*)(float) - 3 words */
};

/** `SplitterMainChassis.cpp` defines this in `.sbss 0x80419440` and reads `value` at `+0`. */
extern struct SSplitterFuncPtrs* gLoader_SplitterMainChassis;

void fn_80218CF0(struct SSplitterFuncPtrs* record) { gLoader_SplitterMainChassis = record; }