// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8022A5AC_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8022A5AC..0x8022A5B4, 0x8 = 8 bytes, 1 function:
//
//   fn_8022A5AC    0x8022A5AC  0x8    stw     r3, gLoader_BacteriaSwarm@sda21(r0)
//                                          blr
//
// **What it is: BacteriaSwarm's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`) and of `fn_8022A570` in
// `src/MetroidPrime/ScriptLoader/Carve8022A570.c` (`stw r3,
// gLoader_EmperorIngStage2Tentacle@sda21(r0)` / `blr`), which between them are the whole
// shape of the family: store the argument into a loader pointer's `.sbss` slot and return.
// `docs/research/rel_loaders.md` has every loader in it.
//
// **Who calls it, and what the argument is.**  Both callers are in BacteriaSwarm's (module
// 6) own listing, `build/G2ME01/BacteriaSwarm/asm/MetroidPrime/ScriptObjects/CBacteriaSwarmRel.s`,
// and neither calls it a setter by name, so the argument is read off them:
//
//   * `RELExit` at `.text 0x2C` (0x24 bytes) does `li r3, 0` / `bl fn_8022A5AC` - the module
//     tears its loader down on the way out, so this is the "clear the pointer" call.
//   * `fn_6_70` at `.text 0x70` (0x30 bytes), the module's loader registration, loads
//     `fn_6_A0` and `lbl_6_bss_10` into r4 and r3, stores the function pointer with
//     `stwu r0, lbl_6_bss_10@l(r3)` so the store writes the slot itself and leaves r3 holding
//     its address, then `bl fn_8022A5AC`.  So the argument is the **address** of the module's
//     own 4-byte loader-pointer copy, `lbl_6_bss_10` at `.bss:0x00000010` with
//     `size:0x4 data:4byte` (`config/G2ME01/rels/BacteriaSwarm/symbols.txt:158`) - not a
//     loader.
//
// That is why the slot in the DOL is 4 bytes wide and why the store writes a pointer to a
// pointer: `src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp` - a `Matching` unit whose
// `.text` ends exactly at 0x8022A5AC - reads this slot as `lwz r6,
// gLoader_BacteriaSwarm@sda21(r0)` / `lwz r12, 0x0(r6)` / `mtctr r12` / `bctrl`, i.e. it
// loads the *slot* and then indirects through the module's copy.  `BacteriaSwarm.cpp` claims
// and defines `.sbss 0x804195D0..0x804195D8` (`gLoader_BacteriaSwarm`, `size:0x8`) and reads
// it, so this unit takes it `extern` and does not claim it twice - the same arrangement as
// `Carve8022A570.c` with `gLoader_EmperorIngStage2Tentacle`.
//
// `BacteriaSwarm.cpp`'s header records that the setter was deliberately left unclaimed
// because REL modules import it by its retail name, so it cannot be renamed.  That is half
// the requirement: reproducing the `fn_<addr>` symbol verbatim in a `.c` file satisfies it -
// the unmangled definition lands in the DOL link, which is what `bl fn_8022A5AC` resolves
// against, exactly as `Carve80200E3C.c` does for module 72.  Measured in the module's own
// import table, `build/G2ME01/BacteriaSwarm/BacteriaSwarm.preplf` matches the string
// `fn_8022A5AC`, the plain placeholder name and not a mangled `SetLoader_...` form, so no
// `symbols.txt` rename is needed and the module's bytes are untouched.  What a rename would
// break is the name; what a carve changes is only who supplies the bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL,
// and a module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.  A
// one-function file cannot get it wrong.
//
// Retail names none of this.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit for the reason every carve has one: a unit may not claim two discontiguous
// ranges in one section (`dtk dol split` fails with "Cyclic dependency ... link order"), and
// both neighbours are already claimed units - `BacteriaSwarm.cpp` ends at 0x8022A5AC and
// `MetroidPrime/Player/CPlayerVisor.cpp` starts at 0x8022AF0C, the gap above being dtk's
// unclaimed `auto_03_8022A5AC_text` tail.
//
// The directory is retail's own, taken from the nearest claimed range below: this address is
// 0x2C bytes into `MetroidPrime/ScriptLoader/BacteriaSwarm.cpp`, so the code is that unit's
// neighbourhood.  For an anonymous function that is the only evidence there is, and it beats
// a lane picking the directory it happened to own.
//
// The shape below is the same two-word `SLoaderSlot` every other unit in this family uses -
// the first word is what the setter stores, the second is the padding the `.sbss` object's
// 8-byte alignment accounts for.  Declared **above** the prototype for the reason
// `Carve80200E3C.c` records: a struct named in a parameter list is scoped to that list, and
// the host build then rejects the definition as a conflicting type.  MWCC does not encode a
// variable's type in its name, so `gLoader_BacteriaSwarm` is referenced here whatever the
// type is spelled.
struct SLoaderSlot {
  void* value;
  unsigned int padding;
};

extern struct SLoaderSlot* gLoader_BacteriaSwarm;

void fn_8022A5AC(struct SLoaderSlot* loader) { gLoader_BacteriaSwarm = loader; }