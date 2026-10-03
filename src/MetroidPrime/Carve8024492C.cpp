// Carved out of dtk's unclaimed `main/auto_03_80243ED4_text` by goal item `carve-8024492c`.
// Every number here is measured: the addresses and sizes come from `config/G2ME01/symbols.txt:
// 10258-10259`, the instructions are the ones dtk itself emitted into
// `build/G2ME01/asm/auto_03_80243ED4_text.s:757-791`, and the bytes themselves were read off the
// pristine disc with `python3 tools/dol_read.py 0x8024492C 0xA8 orig/G2ME01/sys/main.dol` - not
// from `build/G2ME01/main.elf`, which is our own link and therefore circular once this unit is
// configured.
//
// .text 0x8024492C..0x802449D4, 0xA8 = 168 bytes, 2 functions:
//
//   fn_8024492C    0x8024492C  0x64    25 instructions
//                                       stwu r1,-0x30(r1) / mflr r0 / stw r0,0x34(r1) /
//                                       stw r31,0x2c(r1) / mr r31,r6 / stw r30,0x28(r1) /
//                                       mr r30,r5 / li r5,0xc / stw r29,0x24(r1) /
//                                       mr r29,r3 / addi r3,r1,8 / bl memcpy / mr r3,r29 /
//                                       mr r4,r30 / mr r5,r31 / addi r12,r1,8 /
//                                       bl __ptmf_scall / nop / lwz r0,0x34(r1) /
//                                       lwz r31,0x2c(r1) / lwz r30,0x28(r1) / lwz r29,0x24(r1) /
//                                       mtlr r0 / addi r1,r1,0x30 / blr
//   fn_80244990    0x80244990  0x44    17 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       bl fn_8024426C / lwz r7,kAllAreas@sda21(r0) /
//                                       addi r3,r1,8 / lha r10,kMedPriority@sda21(r0) /
//                                       li r4,0x5e1 / li r5,0x7f / li r6,0x40 / li r8,0 / li r9,0
//                                       / bl SfxStart__11CSfxManagerFUsssibbs /
//                                       lwz r0,0x14(r1) / mtlr r0 / addi r1,r1,0x10 / blr
//
// **Both bodies are byte-shape twins of retail functions that are already matched**, which is
// where they are read from rather than guessed. Read out of the disc, each pair is its twin's
// words for words apart from the `bl` displacements, which are address-relative:
//
//   * `fn_8024492C` is
//     `Function__60TNonStaticCallback2<15CSaveGameScreen,CP14CGuiTableGroup,Ci>FPCvPCvP14CGuiTableGroupi`
//     at 0x8017D124, 0x64 - the same 25 instructions, with `4b e8 60 99` / `48 1c 82 f1` where
//     this copy has `4b db e8 91` / `48 10 0a e9`. That instantiation is `Kyoto/TFunctor.hpp`'s
//     `TNonStaticCallback2<T,P1,P2>::Function` (`include/Kyoto/TFunctor.hpp:117-127`), and the
//     tree's own `build/G2ME01/asm/MetroidPrime/CSaveGameScreen.s:904-930` is the matched twin's
//     identical body. So this copy is retail's **second** instantiation of that template - same
//     12-byte `memcpy` of the method pointer into a stack slot, same `__ptmf_scall` - for the
//     screen whose class `symbols.txt` does not name. The body below is that template's logic
//     verbatim with this copy's own (unnamed) receiver class.
//   * `fn_80244990` is `DoSelectionChange__15CQuitGameScreenFP14CGuiTableGroupi` at 0x80221DE4,
//     0x44 - the same 17 instructions, with `4b ff fbfd` / `48 07 c6 55` where this copy has
//     `4b ff f8 d1` / `48 05 9a a9`. Its source is `src/MetroidPrime/CQuitGameScreen.cpp:85-91`
//     (Matching): `SetColors();` then
//     `CSfxManager::SfxStart(1505, 127, 64, CSfxManager::kAllAreas, false, false,
//     CSfxManager::kMedPriority);` with neither argument read.
//
// The `SetColors` half of `fn_80244990` is `fn_8024426C` (0x8024426C, 0xA8,
// `symbols.txt:10251`), **declared and never defined here**. Measured against
// `CQuitGameScreen::SetColors`'s own source (`src/MetroidPrime/CQuitGameScreen.cpp:94-113`) the
// two are the same function: two `CColor`s built up front (`li r5,0xc8 / li r4,0xff` ->
// `stb` to `0xc(r1)`.. `0xf(r1)` for the light grey, `li r0,0x32` -> `0x8(r1)`..`0xb(r1)` for
// the dark one), then a loop over the choice table's two worker widgets whose
// `cmpw r30,r31` against `GetUserSelection()` picks `r1+0xc` when the index equals the selection
// and `r1+8` otherwise. That is `i == selection ? light : dark`, word for word. Nothing in this
// tree claims `fn_8024426C`, so the DOL link takes it from dtk's own object of the surrounding
// run; for the host it gets the announced stand-in at the end of this file.
//
// **No argument of `fn_80244990` is read**, which the bytes settle rather than suggest: there is
// no register move between the frame setup and `bl fn_8024426C`, so `r3` is still the receiver
// `this` that `SetColors` takes, and that is why the declaration below takes the same three
// parameters retail's member function does and passes only the first on.
//
// **The unit is a `.cpp` with `extern "C"`, not the `.c` the seed named**, and that is forced by
// `fn_8024492C`. Retail's `addi r12,r1,8 / bl __ptmf_scall` is what mwcceppc emits for
// dereferencing a pointer-to-member-function, and `__ptmf_scall` reads its argument out of **r12**
// (`src/Runtime/ptmf.c:31-44`), a register plain C has no way to set. Only C++ has the member
// pointer that produces those bytes. `extern "C"` is what keeps the `fn_` names verbatim and
// unmangled, which is the seed's actual concern, and it is the arrangement
// `src/MetroidPrime/Carve801EF730.cpp:135` already uses for a Matching carve.
// `fn_80244990` needs C++ for its own sake too - `CSfxManager::SfxStart` is a class member.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `tools/unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1. Only
// `tools/flip_test.sh` catches that. Check it with
// `python3 tools/check_decl_order.py --unit MetroidPrime/Carve8024492C.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section. The claim sits inside `auto_03_80243ED4_text`
// (0x80243ED4..0x8024569C): the function below is `fn_80244870` (0x80244870, 0xBC) and the one
// above is `fn_802449D4` (0x802449D4, 0x94). Both are unsourced and outside this claim.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `MetroidPrime/ScriptLoader.cpp` (`.text` 0x80242894..0x80243ED4) and above is
// `WorldFormat/CAreaOctTree_Tests.cpp` (0x8024569C..0x802471B0), so this address is in the
// `MetroidPrime/` neighbourhood - the same reasoning `src/Dolphin/Carve8038A7DC.c:24-27` records.
// The carve's own `block` is `.text` and nothing else: no data, no vtable, no string.

#include "GuiSys/CGuiTableGroup.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"

#include <string.h>

extern "C" {

/** Retail's receiver class for `fn_8024492C` is not named by `symbols.txt`, so only the two
 *  things the bytes depend on are modelled: that the pointer-to-member stored in the callback
 *  record targets *some* class, and that the class has the two-argument member function the
 *  record is a pointer to. The body is never defined - the call goes through the copied pointer
 *  value, so nothing here is odr-used, and `sizeof` of the member pointer (12 bytes, the
 *  `li r5,0xc` at 0x80244948) is the same for every CodeWarrior pointer-to-member-function. */
class CCarveCallbackScreen {
public:
  void OnSelectionChange(CGuiTableGroup* caller, int oldSelection);
};

typedef void (CCarveCallbackScreen::*MethodPtr)(CGuiTableGroup*, int);

/** 0x8024426C, `symbols.txt:10251`, size 0xA8: this copy's `SetColors()` - see the header.
 *  Declared, never defined here: nothing in the tree claims it, so the DOL link takes it from
 *  dtk's own object of the run this claim was carved out of. */
extern void fn_8024426C(void* self);

/** `fn_80244990` - retail `.text:0x80244990`, 0x44 = 68 bytes, 17 instructions: a frame, one
 *  call, the `SfxStart` argument block and the epilogue. Twin of
 *  `DoSelectionChange__15CQuitGameScreenFP14CGuiTableGroupi` (0x80221DE4, 0x44), so the body is
 *  `src/MetroidPrime/CQuitGameScreen.cpp:85-91` with this copy's own `SetColors` in place of
 *  `SetColors__15CQuitGameScreenFv`. The sound is 1505 (`li r4,0x5e1`), volume 127, pan 64, and
 *  both bools false - the same move sound the twin plays, which is the evidence that the two are
 *  the same handler on two screens. `caller` and `oldSelection` are unused because retail reads
 *  neither. */
void fn_80244990(void* self, CGuiTableGroup* caller, int oldSelection) {
  fn_8024426C(self);
  CSfxManager::SfxStart(1505, 127, 64, CSfxManager::kAllAreas, false, false,
                        CSfxManager::kMedPriority);
}

/** `fn_8024492C` - retail `.text:0x8024492C`, 0x64 = 100 bytes, 25 instructions: a frame, the
 *  three argument registers spilled into r29/r30/r31, the 12-byte `memcpy` of the method pointer
 *  to `r1+8`, then `r3`/`r4`/`r5` restored and `__ptmf_scall`.  Twin of
 *  `TNonStaticCallback2<CSaveGameScreen, CGuiTableGroup* const, const int>::Function`
 *  (0x8017D124, 0x64), i.e. `include/Kyoto/TFunctor.hpp:123-126` exactly. The callee-saved
 *  triple is not decoration: `memcpy` is a call, so all three arguments have to survive it. */
void fn_8024492C(void* object, const void* method, CGuiTableGroup* p1, int p2) {
  const MethodPtr* m = static_cast< const MethodPtr* >(method);
  MethodPtr _m;
  memcpy(&_m, m, sizeof(_m));
  (static_cast< CCarveCallbackScreen* >(object)->*_m)(p1, p2);
}

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one. `fn_8024426C`'s own 0xA8 retail bytes are a
// spelling job of their own and nothing in this tree has claimed them; this file needs the call
// to resolve, and it is the same trade `src/MetroidPrime/Carve801EF730.cpp:197-215` makes for
// `fn_801EF7B0`. The guard is `__MWERKS__`, not `TARGET_PC`, to match those files: the matching
// build must take the symbol from dtk's own object of the surrounding run, and a second
// definition there would be the duplicate `tools/gate.sh`'s `port link dups` step exists to
// catch.
void fn_8024426C(void* self) { (void)self; }
#endif

} // extern "C"