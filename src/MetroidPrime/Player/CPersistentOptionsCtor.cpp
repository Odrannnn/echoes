/**
 * `fn_80146154` - retail 0x80146154, `size:0x58` = 88 bytes: the constructor of
 * `CGameState+0xDC`, `CPersistentOptions`. The **second** of the nested constructors
 * `CGameState::CGameState()` (retail `fn_801449C8`, `CGameStateCtor.cpp`, Matching at 100%) needs;
 * the first, `fn_80145950`, is `CGameStateCardOptsCtor.cpp` and calls this one with `flag = 0`.
 *
 *     0x80146170  stw  r4,0(r3)      +0x00 = the int argument, whole word
 *     0x80146178  stb  r5,4(r3)      +0x04
 *     0x8014617C  stb  r4,5(r3)      +0x05
 *     0x80146180  stw  r0,8(r3)      +0x08 .. +0x14, four zero words
 *     ...
 *     0x80146190  bl   80145c98      one call, `r3` still `this`
 *     0x801461A8  blr
 *
 * ## +0x04 and +0x05 are *not* the argument
 *
 * Both come from `lbz r5,8(r1)` and `lbz r4,12(r1)` (0x80146164, 0x80146174) - the function's
 * own frame, 8 and 12 bytes above the stack pointer the prologue just allocated with
 * `stwu r1,-32(r1)`. The callee saves LR at 36(r1) and r31 at 28(r1), so 0..31 is this frame's
 * local+outgoing area and the caller's parameter save area starts at 32(r1). The two bytes are
 * whatever the last call through this frame left there; `fn_80146154` is called with `r3` and
 * `r4` only (`fn_80146154(this+0xDC, 1)` from `fn_801449C8`, `fn_80146154(self, 0)` from
 * `fn_80145950`), so nothing ever writes them. **The header's claim that this "cannot be written
 * as source" is wrong and is superseded by this unit** - see the body for the spelling and the
 * measurements behind it.
 *
 * `LoadFields` (retail 0x80145C98, 0x2F4, `fn_80145C98` until 2026-10-01) is the one callee: it
 * branches on `*(this+0x00)` - `lwz r0,0(r3) ; cmpwi r0,0 ; bne` - which is the `flag` this
 * constructor stores, so this is the class's own initialiser and the flag is a real "which game"
 * selector, not a bool.
 *
 * `CPersistentOptions` keeps +0x00..+0x1B as one opaque `u8[0x1C]` (the header says why: the
 * three words at +0x1C..+0x28 are zeroed by the *callers*, so they are the class's default member
 * initialisation and have nowhere to go in a `char pad[0x2c]`). The shapes this function writes
 * are overlaid here instead of named there, so no header changes: `SFirst1C` is 0x1C, measured by
 * `CHECK_SIZEOF` below.
 *
 * `extern "C"` for the same reason as every other retail-named function here: retail's symbol
 * table calls it `fn_80146154`, and a C++ `CPersistentOptions::CPersistentOptions(int)` would
 * mangle to `__ct__17CPersistentOptionsFi` and leave objdiff with nothing to pair it against. The
 * name is also what `CGameStateCtor.cpp` and `CGameStateStreamCtor.cpp` already declare, so no
 * rename touches `config/G2ME01/symbols.txt` or the REL modules that reference it. Those two
 * declare it `void` and discard the result; this returns the pointer because retail's epilogue is
 * `mr r3,r31`, and a return type only has to agree where two declarations meet.
 */

#include "types.h"

#include "MetroidPrime/Player/CPersistentOptions.hpp"

// This file is in `files.cmake` but not a `configure.py` unit - the 0x80142188..0x801468C4 range
// it duplicates is claimed by `CGameState.cpp` - so it is compiled only by the port, by the host
// compiler. It therefore calls the initialiser as an ordinary C++ member, through the header:
// `CGameStateEnvVarManager::LoadFields`, retail 0x80145C98, `fn_80145C98` until 2026-10-01.
// Declaring it `extern "C"` under a retail spelling instead - which is what this did - names a
// symbol nothing defines on a host link, so the call went unresolved.

// +0x00..+0x1B, the part of the class `CPersistentOptions` keeps as one opaque array.
struct SFirst1C {
  int x00;        //!< +0x00 - the constructor's int argument, stored whole
  u8 x04;         //!< +0x04
  u8 x05;         //!< +0x05
  u8 x06_unk[2];  //!< +0x06
  u32 x08;        //!< +0x08
  u32 x0c;        //!< +0x0C
  u32 x10;        //!< +0x10
  u32 x14;        //!< +0x14
  u8 x18_unk[4];  //!< +0x18..+0x1B - nothing in the DOL writes these
};
CHECK_SIZEOF(SFirst1C, 0x1c)

extern "C" CPersistentOptions* fn_80146154(CPersistentOptions* self, int flag) {
  SFirst1C* p = reinterpret_cast< SFirst1C* >(self);

  // +0x04 and +0x05 are two uninitialised bytes of this frame, four bytes apart, and nothing in
  // C++ says "an uninitialised byte". These two 4-byte slots are never written; their first
  // bytes are what retail loads. Ranked with `tools/try_batch.py` by differing instructions:
  // two `volatile bool`s land at 8(r1) and 9(r1) - 2 instructions out; two `volatile u32`s read
  // through a byte pointer land at 12(r1) and 8(r1), the two slots the wrong way round - also 2
  // out; a padded struct of two `volatile bool`s, the same; one 8-byte local read as two bytes
  // four apart, which is `w` below, is the one that reproduces all twenty-two instructions.
  // `volatile` is load-bearing: without it there is no stack slot at all and mwcceppc folds both
  // reads away into `stb r3,4(r3)`.
  volatile u32 w[2];

  p->x00 = flag;
  p->x04 = *reinterpret_cast< volatile u8* >(&w[0]);
  p->x05 = *reinterpret_cast< volatile u8* >(&w[1]);
  p->x08 = 0;
  p->x0c = 0;
  p->x10 = 0;
  p->x14 = 0;

  self->LoadFields();
  return self;
}
