// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:14959-14960`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_8032EE68_text.s:334-366`, and the body below
// is the C those bytes are the compilation of.  The byte evidence is the pristine disc, not our
// own build: `python3 tools/dol_read.py 0x8032F2B8 0x64 orig/G2ME01/sys/main.dol`.
//
// .text 0x8032F2B8..0x8032F31C, 0x64 = 100 bytes, 2 functions:
//
//   fn_8032F2B8    0x8032F2B8  0x58    22 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r4 / stw r30,0x8(r1) /
//                                       mr. r30,r3 / beq / lwz r3,0(r30) / li r4,1 /
//                                       bl __dt__24CSpawnSystemKeyframeDataFv /
//                                       extsh. r0,r31 / ble / mr r3,r30 /
//                                       bl Free__7CMemoryFPCv /
//                                       lwz r0,0x14(r1) / mr r3,r30 / lwz r31,0xc(r1) /
//                                       lwz r30,0x8(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//   fn_8032F310    0x8032F310  0xC     3 instructions
//                                       li r0,0 / stw r0,0(r3) / blr
//
// **Both are byte-shape twins of named retail functions that are already matched**, which is where
// the bodies below are read from rather than guessed.  `./tools/dis.sh` on both pairs gives the
// same instructions in the same order, with only the two `bl` displacements differing, and those
// are address-relative:
//
//   * `fn_8032F2B8` is `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv` (`.text 0x80006AE0`,
//     `size:0x58`, `symbols.txt:124`, source `src/MetroidPrime/main.cpp` - the
//     `rstl::single_ptr<CGameGlobalObjects>` instantiation) - the *same* 22 instructions and the
//     same `bl 802ce388 <Free__7CMemoryFPCv>`, with `li r4,1 / bl __dt__18CGameGlobalObjectsFv`
//     in place of this copy's `li r4,1 / bl __dt__24CSpawnSystemKeyframeDataFv`.  It is the
//     MWCC deleting-destructor convention the sibling carves already describe
//     (`src/MetroidPrime/Factories/Carve80032674.c:30-35`): `mr. r30,r3 / beq` guards the
//     receiver, `extsh. r0,r31 / ble` re-tests the incoming **short**, `Free(self)` is reached
//     only for a positive flag, and `mr r3,r30` returns the receiver on every path.  So
//     `if (flag > 0)` has to stay **inside** `if (self)`, or the `beq` lands on the `extsh.`
//     instead of the epilogue.
//   * `fn_8032F310` is `DisableFog__Q29CGameArea8CAreaFogFv` (`.text 0x80056DE8`, `size:0xC`,
//     `symbols.txt:1712`, source `src/MetroidPrime/CGameArea.cpp:1592`, whose body is
//     `mFogMode = kRFM_None` with `GX_FOG_NONE = 0`): `./tools/dis.sh 0x80056DE8 0xC` is the
//     same `li r0,0 / stw r0,0(r3) / blr`, i.e. `*self = 0` on the first word of the object it
//     is handed.  MWCC does not encode a pointee's type in a store's encoding, so the only thing
//     this spelling has to get right is that a zero **word** lands at offset 0.
//
// **What the callers say the arguments are**, measured with `grep -rn 'bl fn_8032F2B8\|bl
// fn_8032F310' build/G2ME01/asm/` - three calls, all in the unclaimed
// `build/G2ME01/asm/auto_03_8032EE68_text.s` this claim sits inside:
//
//   * `fn_8032F2B8` at 0x8032F080 and 0x8032F1D4, both with `li r4,0x1` - the deleting flag, the
//     same `1` `src/MetroidPrime/main.cpp` records for a `~single_ptr<T>()`.  At 0x8032F080 it
//     runs on the `+0x4` member of the object whose head word was just set to the base-class
//     destructor vtable `lbl_803BB3F8` (0x8032F06C), and it is guarded by `cmplwi r3,0` on that
//     member (0x8032F074) - the guard `~single_ptr` has already got inside itself, so the
//     caller does not need one.
//   * `fn_8032F310` at 0x8032EFC0, immediately after `__nw__FUlPCcPCc` (0x8032EFB4) and the
//     `mr. r31,r3 / beq` that tests its result, so it is handed the freshly allocated object and
//     clears its first word.
//
// **That makes `fn_8032F2B8` the teardown step of a `rstl::single_ptr<CSpawnSystemKeyframeData>`
// member, and its first callee is that class's own destructor**, `__dt__24CSpawnSystemKeyframeDataFv`
// (`.text 0x80322DC8`, `size:0x58`, `symbols.txt:14693`, `scope:weak`).  That address is inside
// the `.text` range 0x80322C1C..0x80323038 that `Kyoto/Particles/CGenDescription.cpp` claims
// (`config/G2ME01/splits.txt`), so the DOL link resolves the name from there.  Declared, never
// defined here, so the port's undefined-symbol count cannot move.  Neither callee is missing from
// `src/`: `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) is claimed in the DOL by
// `Kyoto/Alloc/CMemory.cpp` and defined for the host link by `src/Kyoto/Alloc/PortMwccNew.cpp`.
// `grep -rn "fn_8032F2B8\|fn_8032F310" src/ include/` outside this file finds nothing, so there is
// no `PortLinkStubs.cpp` duplicate to remove.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to stay C: a C++ one would mangle to
// `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is a `.c` rather
// than a `.cpp`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1.  Only
// `tools/flip_test.sh` catches that.  Check it with
// `python3 tools/check_decl_order.py --unit Kyoto/Math/Carve8032F2B8.c`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts and ends inside `auto_03_8032EE68_text` (0x8032EE68..0x8032F8F8),
// the function below is 0x8032F1F0 (0x2C bytes) and the one above is 0x8032F2A8.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Kyoto/Math/Carve8032E444.c` (`.text` 0x8032E444..0x8032E44C) and above is
// `Kyoto/Math/Carve803359F4.c` (0x803359F4..0x80335A0C), so this address sits in the
// `Kyoto/Math` neighbourhood - the same reasoning `src/Kyoto/Math/Carve8032E444.c` records.

/** `__dt__24CSpawnSystemKeyframeDataFv` (`.text 0x80322DC8`, `symbols.txt:14693`).  Declared
 *  only: the DOL link gets it from `Kyoto/Particles/CGenDescription.cpp`, which claims
 *  0x80322C1C..0x80323038.  The `short` is the deleting flag - `li r4,1` at 0x8032F2DC - and it
 *  is a `short` rather than a `bool` because the same call site's own test of it is `extsh.`;
 *  that is also the spelling `src/MetroidPrime/Factories/Carve80032674.c:82` uses for the same
 *  shape one callee over. */
extern void* __dt__24CSpawnSystemKeyframeDataFv(void* self, short flag);

/** `Free__7CMemoryFPCv` (`.text 0x802CE388`, `symbols.txt:12992`).  Declared only: the DOL link
 *  gets it from `Kyoto/Alloc/CMemory.cpp` and the host link from
 *  `src/Kyoto/Alloc/PortMwccNew.cpp`.  `const void*` is how the other carves that call it declare
 *  it (`src/MetroidPrime/ScriptLoader/Carve8022EB54.c:88`). */
extern void Free__7CMemoryFPCv(const void* ptr);

/** `fn_8032F310` - retail `.text:0x8032F310`, 0xC = 12 bytes: `li r0,0 / stw r0,0(r3) / blr`, i.e.
 *  the first word of the object at `self` is cleared and nothing else happens.  Twin of
 *  `DisableFog__Q29CGameArea8CAreaFogFv` (0x80056DE8, 0xC), which is the same three instructions
 *  compiled from `mFogMode = kRFM_None`. */
void fn_8032F310(int* self) { *self = 0; }

/** `fn_8032F2B8` - retail `.text:0x8032F2B8`, 0x58 = 88 bytes, 22 instructions: a frame, the
 *  receiver guard, `lwz r3,0(r30) / li r4,1 / bl <T's own destructor>`, the 16-bit flag test,
 *  `Free__7CMemoryFPCv(self)` and the epilogue that returns the receiver in r3.  Twin of
 *  `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv` (0x80006AE0, 0x58), the same 22
 *  instructions with the same `bl` target, so the body below is that `~single_ptr<T>()` spelled in
 *  C with this copy's `T`.  The flag is a **short** because the test is `extsh.`, and `flag > 0`
 *  sits **inside** `if (self)` because the receiver's `beq` branches to the epilogue (0x8032F2F4,
 *  the `lwz r0,0x14(r1)`).  Both callers pass `li r4,1`, so both take the free. */
void* fn_8032F2B8(void* self, short flag) {
  if (self) {
    __dt__24CSpawnSystemKeyframeDataFv(*(void**)self, 1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}