// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8022FFF8_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8022FFF8..0x80230000, 0x8 = 8 bytes, 1 function:
//
//   fn_8022FFF8    0x8022FFF8  0x8    stw     r3, gLoader_PlantScarabSwarm@sda21(r0)
//                                          blr
//
// **What it is: PlantScarabSwarm's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`, Matching 1/1), which is the whole shape
// of the family: store the argument into the loader pointer's `.sbss` slot and return.  The
// nearest sibling is `fn_8022D5A8` in `src/MetroidPrime/ScriptLoader/Carve8022D5A8.c`
// (MetareeSwarm's setter, Matching 1/1) - all three retail words are byte-identical apart from
// the two symbol names: 0x8022FFF8 is `90 6D 98 98 / 4E 80 00 20`, 0x8022D5A8 is
// `90 6D 98 60 / 4E 80 00 20`, 0x80200E3C is `90 6D 95 D8 / 4E 80 00 20` - so the body below
// is that pair's body with this pair's names.
//
// Both callers are in module 49's own listing and neither calls it a setter by name, so the
// argument is read off them
// (`build/G2ME01/PlantScarabSwarm/asm/MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.s`):
//
//   * `RELExit` (0x64, 0x24 bytes) does `li r3, 0x0` then `bl fn_8022FFF8` - the module tears
//     the loader down on the way out.
//   * `fn_49_A8` (the module constructor, 0xA8, 0x30 bytes) does `lis r4, fn_49_D8@ha ;
//     lis r3, lbl_49_bss_20@ha ; addi r0, r4, fn_49_D8@l ; stwu r0, lbl_49_bss_20@l(r3)` and
//     then calls `fn_8022FFF8` with `r3` still pointing at it.  So the argument is that slot's
//     address, not a loader.  `lbl_49_bss_20` is `.bss:0x20`, `size:0x4 data:4byte`
//     (`config/G2ME01/rels/PlantScarabSwarm/symbols.txt:91`), so one function pointer.
//
// The slot is `gLoader_PlantScarabSwarm` at `.sbss 0x80419618`, `size:0x8 data:4byte`
// (`config/G2ME01/symbols.txt:20785`), which `MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp`
// already claims (`.sbss 0x80419618..0x80419620`) and already defines.  That unit's reader
// is `LoadPlantScarabSwarm` at 0x8022FFCC, and retail's own listing shows the pairing this
// function exists for:
//
//   8022ffd8  lwz   r6, gLoader_PlantScarabSwarm@sda21(r13)  <- reads the pointer just stored
//   8022ffdc  lwz   r12, 0x0(r6)                             <- and the loader out of it
//   8022ffe0  mtctr r12 / 8022ffe4 bctrl
//
// The reader's displacement is the setter's, `-26472(r13)` = `98 98` in both, so the store and
// the load address one slot.  What is stored is the *address* of a record whose first word is
// an `FScriptLoader`, which is why `fn_49_A8` hands over `&lbl_49_bss_20` and why that word is
// four bytes wide.  This unit therefore claims `.text` only and takes the slot as `extern`.
//
// REL module 49 imports this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_8022FFF8` in the DOL link, which is what
// `CPlantScarabSwarmRel.cpp`'s `bl fn_8022FFF8` and its host twin
// `mp_relexit_plantscarabswarm()` resolve against.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` order verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  A one-function file cannot get it wrong.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this file
// reproduces that symbol verbatim, so the definition has to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c`
// rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because both neighbours are
// claimed units: `ScriptLoader/PlantScarabSwarm.cpp` ends exactly at 0x8022FFF8 and
// `ScriptLoader/SkyRipple.cpp` starts at 0x80232308.  The rest of that auto range -
// 0x80230000..0x80232308, 15 functions - stays retail, which is why
// `main/auto_03_8022FFF8_text` is 16 functions before this carve and 15 after it.  That tail
// is `CPortalTransition`'s and `CScriptPortalTransition`'s own methods, and three of them carry
// retail names (`TouchModels__17CPortalTransitionFv` at 0x80230A20,
// `CreateTransition__23CScriptPortalTransitionCFR13CStateManager`, and the destructors), so it
// needs its own mangling and a `.cpp` - out of reach for a carve.

/** The record module 49 hands over, and the shape of the `.sbss` slot that receives its
 *  address.  Only word 0 is ever read or written (`LoadPlantScarabSwarm` does `lwz r12, 0(r6)`,
 *  and `PlantScarabSwarm.cpp`'s own `SLoaderSlot` spells the slot `FScriptLoader* value;
 *  unsigned int padding;` against the measured `size:0x8`), so these field types describe
 *  the shape rather than a layout this unit reads.  Declared **above** the prototype below,
 *  not inside it: a struct named in a parameter list is scoped to that list, and the host
 *  build then rejects the definition as a conflicting type. */
struct SPlantScarabLoaderSlot {
  void* loader;          /* FScriptLoader, at +0 */
  unsigned int padding;  /* at +4; never written by this function */
};

/** `MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp` defines this in `.sbss 0x80419618` and
 *  reads `value` at `+0`; MWCC does not encode a variable's type in its name, so this
 *  references `gLoader_PlantScarabSwarm` itself whatever the type is spelled. */
extern struct SPlantScarabLoaderSlot* gLoader_PlantScarabSwarm;

void fn_8022FFF8(struct SPlantScarabLoaderSlot* loader) { gLoader_PlantScarabSwarm = loader; }
