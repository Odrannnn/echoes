// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9749-9750` and `:20788-20789`,
// the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_802274F4_text.s:10-17`, and the bodies below are the C those
// bytes are the compilation of.
//
// .text 0x802274F4..0x80227504, 0x10 = 16 bytes, 2 functions:
//
//   fn_802274F4    0x802274F4  0x8    stw     r3, gLoader_Krocus@sda21(r0)
//                                          blr
//   fn_802274FC    0x802274FC  0x8    stw     r3, lbl_80419558@sda21(r0)
//                                          blr
//
// **What they are: two loader-slot setters.**  Each is the byte-shape twin of the matched
// `fn_80200E3C` in `src/MetroidPrime/ScriptLoader/Carve80200E3C.c`
// (`stw r3, gLoader_SpacePirate@sda21(r0)` / `blr`), which is the whole shape of the family:
// store the argument into a loader pointer's `.sbss` slot and return.  The argument is not a
// loader but the **address of the record that holds one**, and the callers say so:
//
//   * `fn_802274F4` is Krocuss' (module 38, `config/G2ME01/config.yml:201`) setter.  Its two
//     callers are in `build/G2ME01/Krocuss/asm/auto_00_000000D8_text.s`:
//     `fn_38_104` (0x104, lines 22-30) passes `li r3, 0` - the module tears the loader down on
//     the way out - and `fn_38_148` (0x148, lines 40-54) does
//     `lis r3, lbl_38_bss_0@ha / lis r4, fn_38_178@ha / stwu r0, lbl_38_bss_0@l(r3)` and then
//     calls it, so `r3` is that record's address and the record is the 4 bytes
//     `config/G2ME01/rels/Krocuss/symbols.txt:110` gives `lbl_38_bss_0` (`.bss:0x0 size:0x4`):
//     one word holding `&fn_38_178`, the module's entity loader.
//   * `fn_802274FC` is GeomBlobV2's (module 25, `config/G2ME01/config.yml:136`) setter, and it
//     is the same shape in the same two places: `fn_25_48A8` passes `li r3, 0` and `fn_25_48CC`
//     fills `lbl_25_bss_8` (`.bss:0x8 size:0x4`, `rels/GeomBlobV2/symbols.txt:195`) with
//     `&fn_25_48FC` before calling it - both in
//     `build/G2ME01/GeomBlobV2/asm/auto_00_00002584_text.s:2533-2566`, and
//     `src/MetroidPrime/ScriptObjects/CGeomBlobV2Rel.cpp:37-41` already names this setter.
//
// The word at +0 of each slot is a `FScriptLoader`, i.e.
// `CEntity* (*)(CStateManager&, CInputStream&, const CEntityInfo&)`
// (`include/MetroidPrime/ScriptLoader.hpp:13`), which is what the reader uses: the claim
// immediately below is `MetroidPrime/ScriptLoader/Krocus.cpp` (.text 0x802274C8..0x802274F4),
// whose `LoadKrocus` is `(*gLoader_Krocus.value)(mgr, input, info)` - it loads the slot's word
// and calls it.  Neither function below reads what it stores, so this file needs the slot's
// *address* only, and takes the type as `void*`.
//
// **The two slots are not claimed here.**  `gLoader_Krocus` is `.sbss 0x80419550`, which
// `Krocus.cpp` already claims and already defines, so this unit references it as `extern` - the
// arrangement `Carve80200E3C.c` uses for `gLoader_SpacePirate`, for the same reason (a second
// definition under MWCC is a duplicate the moment it is listed).  `lbl_80419558`
// (`.sbss 0x80419558 size:0x8`) is claimed by nobody: it is dtk's own
// `build/G2ME01/asm/auto_10_80419558_sbss.s`, and it stays there, because claiming it would
// widen this carve past the range the item gives.  Nothing in the DOL reads that slot - a grep
// for `lbl_80419558` over `build/G2ME01/asm/` returns only the store in this range - so its
// size is a fact about retail's layout and not something this unit depends on.
//
// REL modules import both functions by their retail name, so neither can be renamed: defining
// them here keeps the unmangled `fn_802274F4` / `fn_802274FC` in the DOL link, which is what
// module 38's and module 25's `bl` resolve against.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk
// `dol split` fails with "Cyclic dependency ... link order"), and because the claim immediately
// below (`Krocus.cpp`, ending 0x802274F4) and the one above (`AIMannedTurret.cpp`, starting
// 0x80227504) are already units - this range is exactly the gap between them, which is where
// retail put the two setters the Krocuss loader thunk and the AIMannedTurret one sit between.

/** `.sbss 0x80419550`, `symbols.txt:20788`, `size:0x8 data:4byte`: Krocuss' loader slot, read
 *  by `LoadKrocus` in `Krocus.cpp` at `+0`.  Declared, never defined here. */
extern void* gLoader_Krocus;

/** `.sbss 0x80419558`, `symbols.txt:20789`, `size:0x8 data:4byte`: GeomBlobV2's loader slot.
 *  Unclaimed by any unit of ours, so the matching build takes it from dtk's own
 *  `auto_10_80419558_sbss.o`, and this file only stores into it. */
extern void* lbl_80419558;

/** GeomBlobV2's registration, same shape: `&lbl_25_bss_8`, or `nullptr` from `fn_25_48A8`. */
void fn_802274FC(void* loader) { lbl_80419558 = loader; }

/** Krocuss' registration.  `loader` is `&lbl_38_bss_0`, the module's own 4-byte record holding
 *  its entity loader; `nullptr` is what `fn_38_104` passes on the way out. */
void fn_802274F4(void* loader) { gLoader_Krocus = loader; }

#ifdef TARGET_PC
// Host-only definition of the one symbol this unit references that nothing else in `src/`
// defines.  The matching build does not compile this block (PORT_NOTES.md, "TARGET_PC, and the
// rule for port edits"), so `main.dol` still takes the real 0x80419558 from dtk's auto object
// above and both functions keep their retail bytes.  Without it the port's flat link - which
// carries our sources and not dtk's objects - loses `lbl_80419558`, and
// `tools/probe_sources.sh` (whose `link_check.sh --strict` fails when the undefined count
// grows) names it as a new symbol.  Zero-initialised on purpose and unreferenced by the port:
// nothing in `src/` calls `fn_802274FC`, so this cannot change what the game does.
void* lbl_80419558 = 0;
#endif