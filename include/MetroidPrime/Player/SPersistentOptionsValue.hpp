#ifndef _SPERSISTENTOPTIONSVALUE
#define _SPERSISTENTOPTIONSVALUE

#include "types.h"

/**
 * `SPersistentOptionsValue`, retail 0x801462DC - the 12-byte mapped value of
 * `CPersistentOptions`' option map, and the `this` of its clamp at 0x801461AC.
 *
 * **The class is retail's, not a header's: the two units that need it each spell it out, so
 * this header exists only to stop the two copies drifting.** It is included by
 * `src/MetroidPrime/Player/SPersistentOptionsInit.cpp` (which only *calls* the constructor) and
 * by `SPersistentOptionsValueCtor.cpp` (which defines it); nothing else in the tree needs the
 * name, so the header is deliberately not wired into `include/MetroidPrime/Player/CPersistentOptions.hpp`
 * and the two units are the only includers.
 *
 * **0xC, and the three words are the constructor's three arguments verbatim** - `stw r4,0(r3)`,
 * `stw r5,4(r3)`, `stw r6,8(r3)` at 0x801462F0/0x801462F4/0x801462F8, in that order, with no
 * initialisation of its own. The clamp at 0x801461AC reads exactly +0x00, +0x04 and +0x08, and
 * the eleven rows `fn_80145C98` builds all have `+0x00 == 0`.
 *
 * The constructor has to be a **real C++ constructor, not an `extern "C"` helper**: retail's
 * caller forwards the address straight out of the call - `bl __ct__23SPersistentOptionsValueFiii ;
 * mr r5,r3` - and mwcceppc only does that for a constructor it compiled itself. Spelled
 * `extern "C" SPersistentOptionsValue fn_801462DC(int,int,int)` instead, the frame is
 * byte-identical and all eleven `addi r5,r1,N` come out re-derived - 99.98%, and not `Matching`.
 * Measured; the spellings are in `SPersistentOptionsValueCtor.cpp`'s header.
 */
class SPersistentOptionsValue {
public:
  SPersistentOptionsValue(int lo, int hi, int value);

  int x00_lo;
  int x04_hi;
  int x08_value;
};
CHECK_SIZEOF(SPersistentOptionsValue, 0xc)

#endif // _SPERSISTENTOPTIONSVALUE
