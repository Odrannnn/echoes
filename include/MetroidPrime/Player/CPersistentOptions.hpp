#ifndef _CPERSISTENTOPTIONS
#define _CPERSISTENTOPTIONS

#include "types.h"

// **0x2C, and the last three words are named.** `CGameState::CGameState(CInputStream&, int)`
// (retail `fn_80144140`, 0x80144140) constructs this member at `CGameState+0xDC` with
// `fn_80146154` and then zeroes `+0x1C`, `+0x20` and `+0x24` of it - `stw r0,248(r30)`,
// `252`, `256` at 0x80144204, 0x80144210 and 0x80144214, which are `0xF8`, `0xFC` and `0x100`,
// and `0xDC + 0x1C == 0xF8`. The three stores are in the constructor's own body, not in
// `fn_80146154`, so they belong to the class's *default member initialisation*, and a
// `char pad[0x2c]` has nowhere to put them.
//
// The first 0x1C bytes are still unnamed, but **not because `fn_80146154` is unwritable - that
// claim is superseded (2026-09-26) and `src/MetroidPrime/Player/CPersistentOptionsCtor.cpp` is
// now a Matching unit at 100%.** It said: "`fn_80146154` (0x80146154, 0x58) writes +0x04 and
// +0x05 and cannot be written as source - it is called as `fn_80146154(this+0xDC, 1)` with `r3`
// and `r4` only, and it does `lbz r5,8(r1)` and `lbz r4,12(r1)`, which are its own outgoing
// parameter save area, never written by the caller, and stores both into the object. No C++ can
// express 'pass two garbage bytes on the stack'." The conclusion was wrong on the mechanism: with
// `stwu r1,-32(r1)`, LR at 36(r1) and r31 at 28(r1), offsets 8 and 12 are this frame's *own*
// local area, not the caller's parameter save area, so what is needed is not "garbage bytes passed
// in" but "two uninitialised bytes read out of my own frame" - which one 8-byte `volatile` local
// read as two bytes four apart reproduces exactly. The class stays unnamed because +0x00 is the
// constructor's int argument and +0x06..+0x1B are still unknown, not because the constructor is
// out of reach. What `fn_80146154` writes is: +0x00 whole word (the argument), +0x04, +0x05, and
// +0x08..+0x14 as four zero words.
class CPersistentOptions {
public:
  u8 x00[0x1C]; //!< +0x00..+0x1B - `fn_80146154` writes +0x00 (a word), +0x04 and +0x05, and
                //!< +0x08..+0x14; the file's header overlays those shapes on this array
  u32 x1c;      //!< +0x1C (`CGameState+0xF8`), zeroed by the constructor
  u32 x20;      //!< +0x20 (`CGameState+0xFC`), zeroed
  u32 x24;      //!< +0x24 (`CGameState+0x100`), zeroed
  u32 x28;      //!< +0x28 (`CGameState+0x104`) - the tail word. The constructor does **not** zero
                //!< it, so it is either padding or a member nothing in the DOL writes; 0x2C is
                //!< fixed by `0xDC + 0x2C == CGameState+0x108 == cardSerial`.
};
CHECK_SIZEOF(CPersistentOptions, 0x2c)

#endif // _CPERSISTENTOPTIONS
