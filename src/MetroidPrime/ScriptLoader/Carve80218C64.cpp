// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:9494-9495`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_80218C64_text.s`, and the bodies below are the
// C++ those bytes are the compilation of.
//
// .text 0x80218C64..0x80218C98, 0x34 = 52 bytes, 2 functions:
//
//   fn_80218C64    0x80218C64  0x8    stw     r3, gLoader_Splinter@sda21(r0)
//                                          blr
//   fn_80218C6C    0x80218C6C  0x2C    stwu r1, -0x10(r1) / mflr r0 / stw r0, 0x14(r1)
//                                          lwz r4, gLoader_SplitterMainChassis@sda21(r0)
//                                          addi r12, r4, 0x8
//                                          bl __ptmf_scall
//                                          nop
//                                          lwz r0, 0x14(r1) / mtlr r0 / addi r1, r1, 0x10
//                                          blr
//
// **fn_80218C64 is Splinter's (module 74) loader setter**, and it is the byte-shape twin of the
// matched `fn_80218C30` in `src/MetroidPrime/ScriptLoader/Carve80218C30.c` (Shrieker's) one
// range along: `build/G2ME01/asm/auto_03_80218C30_text.s` and
// `build/G2ME01/asm/auto_03_80218C64_text.s` differ in exactly the 16-bit `@sda21` displacement
// (`90 6D 96 B0` against `90 6D 96 B8`, because `gLoader_Shrieker` is at 0x80419430 and
// `gLoader_Splinter` at 0x80419438, 8 bytes apart), and every other byte is identical including
// the `4E 80 00 20` `blr`.  Its callers are already written down in
// `src/MetroidPrime/ScriptObjects/CSplinterRel.cpp`: `RELExit` (`.text:0xA4`, 0x24 bytes) does
// `li r3, 0x0 / bl fn_80218C64`, and `fn_74_E8` (`.text:0xE8`, 0x30 bytes, reached from `RELMain`
// at 0x0C8) does `stwu r0, lbl_74_bss_70@l(r3)` and then `bl fn_80218C64` with r3 still pointing
// at the record.  So it stores the *address* of the record and not a loader, which is why the
// parameter here is a pointer to one.  The import name is the plain DOL symbol `fn_80218C64`
// (`config/G2ME01/symbols.txt:9494`), not a mangled `SetLoader_...` form, so it is `extern "C"`:
// an alias would be a different symbol and module 74's two `bl`s would resolve to nothing.
//
// **fn_80218C6C dispatches the third member of the record module 75 registers**, so it is the
// same pmf-call shape as the matched `TouchPlayerActor__FR7CEntityR13CStateManager` at 0x8021BC40
// (`src/MetroidPrime/ScriptLoaderRel.cpp:103`) and it is spelled the same way:
// `(ent.*(gLoader_X.value->method))(...)`.
//
//   * The record is `lbl_75_bss_20`, `.bss:0x20`, `size:0x18` in
//     `config/G2ME01/rels/Splitter/symbols.txt`, filled by `fn_75_8254`
//     (`src/MetroidPrime/ScriptObjects/CSplitterRelMain.cpp:84-88`) with three members: two
//     4-byte loaders from its own `.text` and a 12-byte pmf copied out of `.data` - 0x14 bytes
//     of content in retail's 8-byte-aligned 0x18 object.  The pmf is therefore at **+8**, which
//     is the `addi r12, r4, 0x8` at 0x80218C7C.
//   * The two loaders are the ones the `Matching` neighbour
//     `src/MetroidPrime/ScriptLoader/SplitterMainChassis.cpp` already reads -
//     `LoadSplitterMainChassis` 0x80218CC4 dispatches `+0x00` and `LoadSplitterCommandModule`
//     0x80218C98 dispatches `+0x04` (its asm is `lwz r12, 0x0(r6)` / `lwz r12, 0x4(r6)` then
//     `mtctr r12 / bctrl`).  Nothing else in the tree reads `+0x08`.
//   * The pmf's argument list is `(float)`, measured on the module side: `fn_75_5A54` (0x5A54,
//     0x2C bytes) stores the incoming `f1` at `+0xef0`, and `CSplitterRelMain.cpp` already spells
//     the member `void (CEntity::*method)(float)`.  **`fn_80218C6C` itself shows no float
//     instruction at all**, so the DOL cannot say how many arguments the caller passes; it takes
//     the one the record is measured to have, and `__ptmf_scall` (`src/Runtime/ptmf.c:31`) reads
//     only r12 and r3 and leaves `f1` alone, so the argument reaches the callee untouched either
//     way.  The spelling below is chosen to agree with the `Matching` unit that measured it, not
//     because these 44 bytes can tell.
//   * **`r4`, not `r5`, holds the loaded slot**, and that says the signature has no second
//     integer argument: every loader thunk in this family has three (`mgr`, `input`, `info` in
//     r3/r4/r5) and every one of them loads into `r6` - `LoadSplinter` 0x80218C38,
//     `LoadSplitterCommandModule` 0x80218C98, `LoadPlayerActor` 0x8021BC6C all do
//     `lwz r6, gLoader_...; lwz r12, 0x0(r6); mtctr r12; bctrl`.  `TouchPlayerActor` uses r5
//     because r4 is its live `mgr`.  So r4 being free here means r3 is the only integer
//     parameter, i.e. the receiver.
//
// **Nothing calls `fn_80218C6C`, and that is measured, not assumed.**  A scan of every `bl` in
// `.text` (`build/G2ME01/main.dol`, offsets 0x640..0x3A22A0, target = site + sign-extended
// I-form displacement) finds **no** branch to 0x80218C64 and none to 0x80218C6C, while the same
// scan does find all 28 `bl __ptmf_scall` sites and `bl 0x8021BA94` at 0x8003CB18, so the scan
// works.  A scan for the literal 4-byte big-endian value 0x80218C6C anywhere in the 3,969,024-byte
// image finds nothing either, so there is no data reference either, and no REL imports it
// (`config/G2ME01/rels/*/symbols.txt` and `config/G2ME01/symbols.txt` have nothing but the one
// `symbols.txt` line).  Retail left it unreferenced.  Nothing therefore needs a `force_active:`
// entry in `config/G2ME01/config.yml`, and nothing in the port has to call it - which is why this
// is a carve of bytes and not a fix.
//
// Both `.sbss` slots stay with the units that already claim and define them: `Splinter.cpp`
// claims `.sbss 0x80419438..0x80419440` and `SplitterMainChassis.cpp` claims
// `0x80419440..0x80419448`, so this unit claims `.text` only and takes both slots as `extern`.
// MWCC does not encode a variable's type in its name, so the `extern` declarations below resolve
// to `gLoader_Splinter` and `gLoader_SplitterMainChassis` whatever the two neighbours spell the
// slot as; `Carve80218B68.c` does the same thing for `gLoader_MetroidAlpha`.
//
// Definitions are in descending retail text order, and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim,
// so an ascending file is a permuted `.text` - 100.00% per function and a broken DOL hash, which
// only `tools/flip_test.sh` catches.

#include "MetroidPrime/ScriptLoader.hpp"

class CEntity;

/** The record `fn_80218C64` stores, measured as one bare `FScriptLoader`:
 *  `config/G2ME01/rels/Splinter/symbols.txt` gives `lbl_74_bss_70` 0x4 bytes of content,
 *  `fn_74_E8` stores one word into it, and the DOL has exactly one reader of
 *  `gLoader_Splinter` - `LoadSplinter` at 0x80218C38, which reads word 0 and nothing else.  The
 *  slot is 8 bytes wide (`config/G2ME01/symbols.txt:20720`), so only the first word is named
 *  here. */
struct SSplinterFuncPtrs {
  FScriptLoader loader;
};

/** `Splinter.cpp` defines this 8-byte slot in `.sbss 0x80419438` and reads `value` at `+0`. */
struct SSplinterSlot {
  SSplinterFuncPtrs* value;
  unsigned int padding;
};
extern struct SSplinterSlot gLoader_Splinter;

/** The record module 75 registers through `fn_80218CF0`, 0x14 bytes of content in a 0x18-byte
 *  `.bss` object (`config/G2ME01/rels/Splitter/symbols.txt`, `lbl_75_bss_20 size:0x18`).  The two
 *  loaders are named from the `Matching` `SplitterMainChassis.cpp` that dispatches `+0x00` and
 *  `+0x04`; the member at `+0x08` is what `fn_80218C6C` below dispatches and
 *  `CSplitterRelMain.cpp`'s `SSplitter_FuncPtrs` is the same record spelled in C++. */
struct SSplitterFuncPtrs {
  FScriptLoader slot0;
  FScriptLoader slot1;
  void (CEntity::*method)(float);
};

/** `SplitterMainChassis.cpp` defines this 8-byte slot in `.sbss 0x80419440` and reads `value` at
 *  `+0`. */
struct SSplitterMainChassisSlot {
  SSplitterFuncPtrs* value;
  unsigned int padding;
};
extern struct SSplitterMainChassisSlot gLoader_SplitterMainChassis;

extern "C" {

// .text 0x8 | 0x80218C6C | size: 0x2C. Declared first because it is the higher retail offset.
void fn_80218C6C(CEntity& entity, float arg) {
  (entity.*(gLoader_SplitterMainChassis.value->method))(arg);
}

// .text 0x0 | 0x80218C64 | size: 0x8
void fn_80218C64(SSplinterFuncPtrs* loader) { gLoader_Splinter.value = loader; }

} // extern "C"