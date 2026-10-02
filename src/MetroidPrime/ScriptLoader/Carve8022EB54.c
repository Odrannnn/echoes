// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt:9934-9935`, the instructions
// are the ones dtk itself emitted into `build/G2ME01/asm/auto_03_8022E13C_text.s:737-767`,
// and the body below is the C those bytes are the compilation of.  The byte evidence is
// the pristine disc, not our own build: `python3 tools/dol_read.py 0x8022EB54 0x48
// orig/G2ME01/sys/main.dol`.
//
// .text 0x8022EB54..0x8022EB9C, 0x48 = 72 bytes, 2 functions:
//
//   fn_8022EB54    0x8022EB54  0x3C    15 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr. r31,r3 / beq / extsh. r0,r4
//                                       / ble / bl Free__7CMemoryFPCv /
//                                       lwz r0,0x14(r1) / mr r3,r31 / lwz r31,0xc(r1) /
//                                       mtlr r0 / addi r1,r1,0x10 / blr
//   fn_8022EB90    0x8022EB90  0xC     3 instructions
//                                       li r0,0 / stw r0,0(r3) / blr
//
// **Both are byte-shape twins of named retail functions that are already matched**, which is
// where the bodies below are read from rather than guessed:
//
//   * `fn_8022EB54` is `__dt__5CMainFv` (`.text 0x800087DC`, `size:0x3C`,
//     `symbols.txt:162`, source `src/MetroidPrime/main.cpp:517` - the empty
//     `CMain::~CMain()`) **word for word, including the `bl` target**: `./tools/dis.sh
//     0x800087DC 0x3C` gives the same 15 instructions and the same
//     `bl 802ce388 <Free__7CMemoryFPCv>`.  It is the deleting-destructor convention of a
//     class with nothing to destroy: `mr. r31,r3 / beq` is MWCC's receiver guard, `extsh.
//     r0,r4 / ble` is `flag > 0` on a **short** (`extsh.`, not the `extsb.` a `bool`
//     gives), and only then does the receiver go through `Free__7CMemoryFPCv`.  The
//     receiver comes back in r3, so the function returns `self`.  That shape is already
//     written out in this tree by `src/MetroidPrime/Cameras/Carve801E7C14.c:98-106`
//     (`fn_801E7C58`), which has the same `if (self) { ...; if (flag > 0) { Free; } }
//     return self;` with one extra statement in the middle.
//   * `fn_8022EB90` is `DisableFog__Q29CGameArea8CAreaFogFv` (`.text 0x80056DE8`,
//     `size:0xC`, `symbols.txt:1712`, source `src/MetroidPrime/CGameArea.cpp:1592`, whose
//     body is `mFogMode = kRFM_None` with `GX_FOG_NONE = 0`): `./tools/dis.sh 0x80056DE8
//     0xC` is the same `li r0,0 / stw r0,0(r3) / blr`, i.e. `*self = 0` on the first word
//     of the object it is handed.
//
// **What the callers say the arguments are**, measured with `grep -rn 'bl fn_8022EB54\|bl
// fn_8022EB90' build/G2ME01/asm/` - four calls, all in
// `build/G2ME01/asm/MetroidPrime/Player/CPlayer.s`, two each:
//
//   * 0x8001A798 and 0x8001A80C call `fn_8022EB54` on `r30+0x1300` and `r30+0x1168` with
//     `li r4,-1` each time, between the neighbouring members' own `__dt__` calls
//     (`__dt__10CModelDataFv` at 0x8001A7D4, `__dt__10CMorphBallFv` at 0x8001A800,
//     `__dt__20CDamageVulnerabilityFv` at 0x8001A818).  `li r4,-1` is the call-site
//     convention for "destroy, do not free me afterwards", which is why no caller of these
//     four ever reaches the `Free`: both members have no members of their own to destroy,
//     which is the whole body.
//   * 0x8001B73C and 0x8001BB6C call `fn_8022EB90` in the middle of a member
//     initialisation, on the same object as the `fn_8022EB54` call 0x900 bytes earlier -
//     `addi r3, r31,0x1168` at 0x8001B6CC, then a run of `stfs`/`stb` initialisers, then
//     the call.  The one word it clears is the head of the object being set up.
//
// So the two are the teardown step and the "clear my first word" step of one empty class,
// and neither callee is missing from `src/`: `Free__7CMemoryFPCv` (0x802CE388,
// `symbols.txt:12992`) is claimed in the DOL by `Kyoto/Alloc/CMemory.cpp`
// (`config/G2ME01/splits.txt`) and defined for the host link by
// `src/Kyoto/Alloc/PortMwccNew.cpp`.  Declared, never defined here, so the port's
// undefined-symbol count cannot move.  `grep -rn "fn_8022EB54\|fn_8022EB90" src/ include/`
// outside this file finds nothing, so there is no `PortLinkStubs.cpp` duplicate to
// remove.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this
// file reproduces those symbols verbatim, so the definitions have to stay C: a C++ one
// would mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the
// unit is a `.c` rather than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function, objdiff
// still happy, `unit_fit.sh` still "fits", the link still succeeding, and a broken DOL
// sha1.  Only `tools/flip_test.sh` catches it.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ...
// link order").  Below this claim the gap 0x8022E13C..0x8022EB54 is unclaimed and above it
// `MetroidPrime/ScriptLoader/EmperorIngStage3.cpp` claims 0x8022EB9C..0x8022EBC8.  The
// directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/ScriptLoader/IngBlobSwarmLoaderSet.cpp` (0x8022E134..0x8022E13C) and above
// is `MetroidPrime/ScriptLoader/EmperorIngStage3.cpp`.

/** `Free__7CMemoryFPCv` (`.text 0x802CE388`, `symbols.txt:12992`).  Declared only: the DOL
 *  link gets it from `Kyoto/Alloc/CMemory.cpp` and the host link from
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`.  `const void*` is how the other carves that call it
 *  declare it (`src/MetroidPrime/Carve800E1548.c:77`). */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `fn_8022EB90` - retail `.text:0x8022EB90`, 0xC = 12 bytes: `li r0,0 / stw r0,0(r3) /
 *  blr`, i.e. the first word of the object at `self` is cleared and nothing else happens.
 *  Twin of `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, 0xC), which is the same three
 *  instructions compiled from `mFogMode = kRFM_None`.  MWCC does not encode a pointee's
 *  type in a store's encoding, so the only thing the spelling has to get right is that a
 *  zero **word** lands at offset 0. */
void fn_8022EB90(int* self) { *self = 0; }

/** `fn_8022EB54` - retail `.text:0x8022EB54`, 0x3C = 60 bytes, 15 instructions: a frame,
 *  the receiver guard, the 16-bit flag test, `Free__7CMemoryFPCv(self)` and the epilogue
 *  that returns the receiver in r3.  Twin of `__dt__5CMainFv` (0x800087DC, 0x3C), the same
 *  15 instructions with the same `bl` target, so the body below is that empty
 *  `CMain::~CMain()` spelled in C.  The flag is a **short** because the test is `extsh.`,
 *  and `flag > 0` sits **inside** `if (self)` because the receiver's `beq` branches to the
 *  epilogue (`0x8022EB78`, the `lwz r0,0x14(r1)`).  Both of those four callers pass
 *  `li r4,-1`, so they take the `ble` and never reach the free. */
void* fn_8022EB54(void* self, short flag) {
  if (self) {
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}