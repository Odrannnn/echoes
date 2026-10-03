// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8022D5A8_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x8022D5A8..0x8022D5B0, 0x8 = 8 bytes, 1 function:
//
//   fn_8022D5A8    0x8022D5A8  0x8    stw     r3, gLoader_MetareeSwarm@sda21(r0)
//                                          blr
//
// **What it is: MetareeSwarm's loader setter.**  It is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`, Matching 1/1), which is the whole shape
// of the family: store the argument into the loader pointer's `.sbss` slot and return.  The
// retail words are byte-identical apart from the two symbol names - 0x8022D5A8 is `90 6D 98
// 60 / 4E 80 00 20`, 0x80200E3C is `90 6D 95 D8 / 4E 80 00 20` - so the body below is the
// twin's body with this pair's names.
//
// Both callers are in module 43's own listing and neither calls it a setter by name, so the
// argument is read off them
// (`build/G2ME01/MetareeSwarm/asm/MetroidPrime/ScriptObjects/CMetareeSwarmRel.s`):
//
//   * `RELExit` (0x64, 0x24 bytes) passes `li r3, 0` - the module tears the loader down on the
//     way out.
//   * `fn_43_A8` (the module constructor, 0xA8, 0x30 bytes) stores `&fn_43_D8` into
//     `lbl_43_bss_20` (`lis r3, lbl_43_bss_20@ha ; stwu r0, lbl_43_bss_20@l(r3)`, so the slot
//     is the four bytes at `.bss:0x20` and the value is one function pointer) and then calls
//     `fn_8022D5A8` with `r3` still pointing at it.  So the argument is that slot's address,
//     not a loader.
//
// The slot is `gLoader_MetareeSwarm` at `.sbss 0x804195E0` (`symbols.txt`: `type:object
// size:0x8 data:4byte`), which `MetroidPrime/ScriptLoader/MetareeSwarm.cpp` already claims
// (`.sbss 0x804195E0..0x804195E8`) and already defines.  That unit's reader is
// `LoadMetareeSwarm` at 0x8022D57C, and its own emitted listing shows the pairing this
// function exists for:
//
//   8022D588  lwz   r6, gLoader_MetareeSwarm@sda21(r0)   <- reads the pointer just stored
//   8022D58C  lwz   r12, 0x0(r6)                        <- and the loader out of it
//   8022D590  mtctr r12 / bctrl
//
// so what is stored is the *address* of a record whose first word is an `FScriptLoader`, which
// is why `fn_43_A8` hands over `&lbl_43_bss_20` and why that word is four bytes wide.  This
// unit therefore claims `.text` only and takes the slot as `extern`.
//
// REL modules import this function by its retail name, so it cannot be renamed; defining it
// here keeps the unmangled `fn_8022D5A8` in the DOL link, which is what module 43's
// `bl fn_8022D5A8` resolves against.
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
// claimed units: `ScriptLoader/MetareeSwarm.cpp` ends exactly at 0x8022D5A8 and
// `ScriptLoader/Carve8022D758.c` starts at 0x8022D758.  The rest of that auto range -
// 0x8022D5B0..0x8022D758, four functions - stays retail, which is why
// `main/auto_03_8022D5A8_text` is 5 functions before this carve and 4 after it.

/** The record module 43 hands over, and the shape of the `.sbss` slot that receives its
 *  address.  Only word 0 is ever read or written (`LoadMetareeSwarm` does `lwz r12, 0(r6)`,
 *  and `MetareeSwarm.cpp`'s own `SLoaderSlot` spells the slot `FScriptLoader* value;
 *  unsigned int padding;` against the measured `size:0x8`), so these field types describe
 *  the shape rather than a layout this unit reads.  Declared **above** the prototype below,
 *  not inside it: a struct named in a parameter list is scoped to that list, and the host
 *  build then rejects the definition as a conflicting type. */
struct SMetareeLoaderSlot {
  void* loader;            /* FScriptLoader, at +0 */
  unsigned int padding;    /* at +4; never written by this function */
};

/** `MetroidPrime/ScriptLoader/MetareeSwarm.cpp` defines this in `.sbss 0x804195E0` and reads
 *  `value` at `+0`; MWCC does not encode a variable's type in its name, so this references
 *  `gLoader_MetareeSwarm` itself whatever the type is spelled. */
extern struct SMetareeLoaderSlot* gLoader_MetareeSwarm;

void fn_8022D5A8(struct SMetareeLoaderSlot* loader) { gLoader_MetareeSwarm = loader; }