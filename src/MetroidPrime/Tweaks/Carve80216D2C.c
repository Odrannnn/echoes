// Carved out of an unclaimed dtk `auto_*` range by lane `12`.  Every number here is
// measured: the addresses and sizes come from `symbols.txt`, the instructions are the ones
// dtk itself emitted into `build/G2ME01/asm/auto_03_80216CC4_text.s`, and the body below is
// the C those bytes are the compilation of.
//
// .text 0x80216D2C..0x80216D5C, 0x30 = 48 bytes, 4 functions:
//
//   fn_80216D50    0x80216D50  0xC    lwz r3, 0x0(r3); lfs f1, 0x30(r3); blr
//   fn_80216D44    0x80216D44  0xC    lwz r3, 0x0(r3); lfs f1, 0x34(r3); blr
//   fn_80216D38    0x80216D38  0xC    lwz r3, 0x0(r3); lfs f1, 0x54(r3); blr
//   fn_80216D2C    0x80216D2C  0xC    lwz r3, 0x0(r3); lfs f1, 0x58(r3); blr
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits
// function definitions in *reverse* source order and mwldeppc keeps the object `.text`
// verbatim, so an ascending file is a permuted `.text` - 100.00% per function and a broken
// DOL.  Only `tools/flip_test.sh` catches that.
//
// Retail names none of these.  `symbols.txt` carries the `fn_<addr>` placeholder and this
// file reproduces that symbol verbatim, so the definitions have to stay C: a C++ one would
// mangle to `_Z<len>fn_<addr>v` and objdiff would pair nothing.  That is also why the unit is
// a `.c` rather than a `.cpp`.
//
// **What the two loads are.**  Each function is `lwz r3, 0x0(r3)` - the block behind the
// pointer the caller passes in r3 - then one float out of that block.  The four callers
// settle it: `CWorldTransManager::GetCameraFov` calls 0x80216D50 with `gpTweakGame.get()`
// and `src/MetroidPrime/CWorldTransManager.cpp:29-33` records the same two instructions as
// "the float at +0x30 of the block `*gpTweakGame` points at", and
// `CGameState::GetHardModeDamageMultiplier` (0x80142498) calls 0x80216D38 the same way.  The
// matched `CTweakGui` accessors in `src/MetroidPrime/Tweaks/CTweakGui.cpp` are this shape
// exactly (`lwz` the member pointer, `lfs` the float at a constant offset), which is the
// idiom the body below uses.  The tree declares no `CTweakGame` block layout, so the fields
// are named by their offsets and nothing is asserted about the words between them.
//
// **The one hand-written duplicate is removed with this unit.**
// `src/MetroidPrime/Tweaks/CTweakGameHardModeDamageMultiplier.cpp` was a port-only unit that
// defined the same plain symbol `fn_80216D38` for the host link.  Both builds cannot define
// it: this unit is listed in `files.cmake`, so the host sees two definitions and
// `tools/link_check.sh` counts a duplicate.  The port-only unit was also dead - `nm` over the
// port's objects shows no reference to `fn_80216D38`, because the carve that used to call it
// (`CGameStateGetHardModeDamageMultiplier.cpp`) was superseded by upstream's `CGameState.cpp`,
// which calls the mangled `CTweakGame::GetHardModeDamageMultiplier` instead - and its single
// dereference did not reproduce retail's two loads.  With it gone, this file is the one
// definition of the symbol in both builds, and the port's undefined count is unchanged.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section
// (dtk `dol split` fails with "Cyclic dependency ... link order"), and because the
// functions on either side of this run are not trivial: `fn_80216D0C` above it is a
// three-instruction indexed load and `GetPakFile` below it opens a frame and calls a
// `rstl::basic_string` copy constructor.
//
// The directory is retail's own, taken from the nearest claimed range below: this address is
// 0x68 bytes past the end of `MetroidPrime/Tweaks/CTweakGui.cpp` (0x80215F80..0x80216CC4),
// and the two retail-named functions that bracket this run inside the same dtk range are
// `GetTotalPercentage__10CTweakGameFv` (0x80216D20) and `GetPakFile__10CTweakGameFv`
// (0x80216D5C), so the code is that unit neighbourhood.
typedef struct {
  unsigned char unk_00[0x30];
  float word_30;
  float word_34;
  unsigned char unk_38[0x54 - 0x38];
  float word_54;
  float word_58;
} STweakGameBlock;

float fn_80216D50(void* self) {
  return ((const STweakGameBlock*)*(const void* const*)self)->word_30;
}

float fn_80216D44(void* self) {
  return ((const STweakGameBlock*)*(const void* const*)self)->word_34;
}

float fn_80216D38(void* self) {
  return ((const STweakGameBlock*)*(const void* const*)self)->word_54;
}

float fn_80216D2C(void* self) {
  return ((const STweakGameBlock*)*(const void* const*)self)->word_58;
}
