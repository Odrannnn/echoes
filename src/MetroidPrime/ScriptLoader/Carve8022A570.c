// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8022A570_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8022A570..0x8022A580, 0x10 = 16 bytes, 2 functions:
//
//   fn_8022A578    0x8022A578  0x8    stw     r3, lbl_804195C8@sda21(r0)
//                                          blr
//   fn_8022A570    0x8022A570  0x8    stw     r3, gLoader_EmperorIngStage2Tentacle@sda21(r0)
//                                          blr
//
// **What they are: the two loader setters either side of `EmperorIngStage2Tentacle`.**  Both
// are the byte-shape twin of the matched `fn_80200E3C` in
// `src/MetroidPrime/ScriptLoader/Carve80200E3C.c` (`stw r3, gLoader_SpacePirate@sda21(r0)`
// / `blr`), which is the whole shape of the family: store the argument into a loader
// pointer's `.sbss` slot and return.
//
// **Who calls them, and what the argument is.**  Both callers are REL modules, and both
// modules' own listings call them without naming them a setter, so the argument is read off
// the call sites:
//
//   * `fn_8022A570` - `build/G2ME01/EmperorIngStage2Tentacle/asm/auto_00_0000009C_text.s`
//     calls it twice (0xD8 and 0x128), matching the module name and the adjacent
//     `EmperorIngStage2Tentacle.cpp` accessor, so the slot is that unit's
//     `gLoader_EmperorIngStage2Tentacle` at `.sbss 0x804195C0`.  `EmperorIngStage2Tentacle.cpp`
//     already claims and defines that `.sbss` range, so this file takes it `extern` and does
//     not claim it twice.
//   * `fn_8022A578` - `build/G2ME01/SwarmBasics/asm/MetroidPrime/Enemies/CSwarmBasicsREL.s`
//     calls it (0x74) from its `RELExit` with `li r3, 0`, and
//     `build/G2ME01/SwarmBasics/asm/auto_00_000000A8_text.s` calls it (0xC4) from the module
//     constructor `fn_80_A8` with the address of a 4-byte record the module writes a function
//     pointer into.  So the slot is SwarmBasics' loader pointer, the 8 bytes immediately above
//     `gLoader_EmperorIngStage2Tentacle` at `.sbss 0x804195C8`; `symbols.txt` carries it as the
//     placeholder `lbl_804195C8` and `auto_10_804195C8_sbss.s` gives it as 8 bytes,
//     `.balign 8`, `.skip 0x8` - byte for byte the shape of the
//     `gLoader_EmperorIngStage2Tentacle` next to it.  This unit therefore **claims** that
//     `.sbss` range and defines it, like every other unit in this family: the store has to
//     land somewhere the host build can see, and a slot only this unit writes is this unit's
//     to own.  Claiming it also retires `auto_10_804195C8_sbss` rather than leaving a second
//     definition of the same eight bytes.
//
// `EmperorIngStage2Tentacle.cpp` and `EmperorIngStage3.cpp` both say in their headers that
// these setters are "deliberately NOT claimed ... must stay in dtk's auto unit".  That is
// half the requirement: the point of leaving them unclaimed is that REL modules import them
// **by their retail name**, so they cannot be renamed.  Reproducing the `fn_<addr>` symbol
// verbatim in a `.c` file satisfies that - the unmangled definition lands in the DOL link,
// which is what `bl fn_8022A570` and `bl fn_8022A578` resolve against, exactly as
// `Carve80200E3C.c` does for module 72.  What a rename would break is the name; what a carve
// changes is only who supplies the bytes.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` order
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, a broken DOL,
// and a module hash that fails on a few bytes.  Only `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit for the reason every carve has one: a unit may not claim two discontiguous
// ranges in one section (`dtk dol split` fails with "Cyclic dependency ... link order"), and
// both neighbours are already claimed units - `EmperorIngStage2Tentacle.cpp` ends at
// 0x8022A570 and `BacteriaSwarm.cpp` starts at 0x8022A580.
//
// The directory is retail's own, taken from the nearest claimed range below: this address is
// 0x2C bytes into `MetroidPrime/ScriptLoader/EmperorIngStage2Tentacle.cpp`, so the code is
// that unit's neighbourhood.  For an anonymous function that is the only evidence there is,
// and it beats a lane picking the directory it happened to own.
//
// The `.sbss` claim is 0x804195C8..0x804195D0, 8 bytes, 1 object:
//
//   lbl_804195C8  .sbss 0x804195C8  0x8    .balign 8, .skip 0x8
//
// so it is defined here as the same two-word `SLoaderSlot` shape every other unit in this
// family uses - the first word is what both setters store, the second is the padding
// `auto_10_804195C8_sbss.s` also skips.  Declared **above** the prototype below for the reason
// `Carve80200E3C.c` records: a struct named in a parameter list is scoped to that list.
struct SLoaderSlot {
  void* value;
  unsigned int padding;
};

struct SLoaderSlot lbl_804195C8;

/** `EmperorIngStage2Tentacle.cpp` defines this in `.sbss 0x804195C0` and reads `value` at
 *  `+0`; MWCC does not encode a variable's type in its name, so this references
 *  `gLoader_EmperorIngStage2Tentacle` itself whatever the type is spelled. */
extern struct SLoaderSlot* gLoader_EmperorIngStage2Tentacle;

void fn_8022A578(struct SLoaderSlot* loader) { lbl_804195C8.value = loader; }

void fn_8022A570(struct SLoaderSlot* loader) { gLoader_EmperorIngStage2Tentacle = loader; }
