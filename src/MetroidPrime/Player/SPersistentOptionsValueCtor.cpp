/**
 * `__ct__23SPersistentOptionsValueFiii` - retail 0x801462DC, `size:0x3C` = 60 bytes,
 * 0x801462DC..0x80146318: the constructor of the 12-byte value of `CPersistentOptions`' option map.
 *
 *     0x801462DC  stwu r1,-16(r1)   LR at 20(r1), r31 at 12(r1)
 *     0x801462EC  mr   r31,r3       `this`, live across the call
 *     0x801462F0  stw  r4,0(r3)     +0x00 = the first argument
 *     0x801462F4  stw  r5,4(r3)     +0x04
 *     0x801462F8  stw  r6,8(r3)     +0x08
 *     0x801462FC  bl   801461ac     the clamp, `r3` still `this`
 *     0x80146304  mr   r3,r31       return the address
 *
 * Three stores and one call, so the whole body is the member initialisation list followed by the
 * clamp, and the only thing that can go wrong is the return.
 *
 * ## Why it has to be a constructor, measured
 *
 * Retail's own caller, `fn_80145C98` (0x80145C98, `CPersistentOptionsInit.cpp`), forwards the
 * address **out of the call**: `bl __ct__23SPersistentOptionsValueFiii ; mr r5,r3`. mwcceppc emits
 * that forwarding only for a constructor it compiled itself, whose contract is `this` in and
 * `this` out. Spelled `extern "C" SPersistentOptionsValue fn_801462DC(int,int,int)` - by-value
 * return, which is the ABI-legal way to say the same thing for a C function - the *frame* comes
 * out byte-identical (`stwu -16`, LR at 20, r31 at 12, the same three `stw`s, the same `bl`, the
 * same `mr r3,r31`) and **all eleven `addi r5,r1,N` in `fn_80145C98` are re-derived instead of
 * forwarded**: `mr r5,r3` becomes `addi r5,r1,N` for the temporary the by-value return needs.
 * That is 11 instructions out of 189 in that unit - **99.98%, and not `Matching`**.
 *
 * So the finding is confirmed rather than merely plausible, and the direction of the fix is the
 * opposite of the obvious one: the constructor's *return type* is load-bearing, and
 * `config/G2ME01/symbols.txt` has to carry the mangled name `__ct__23SPersistentOptionsValueFiii`
 * for 0x801462DC. It already does. Only `fn_80145A2C` and `fn_80145C98` call it and neither is
 * written as source, so the rename touches no other unit.
 *
 * The class itself lives in `include/MetroidPrime/Player/SPersistentOptionsValue.hpp`, because
 * `CPersistentOptionsInit.cpp` has to call this constructor and the two copies of the class must
 * not drift.
 */

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out of this
// translation unit, and it has to come before *any* include. See `CPersistentOptionsInit.cpp`.
#define _CMEMORY

#include "types.h"

#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"

// The clamp, retail 0x801461AC, `SPersistentOptionsValueClamp.cpp`. A free function rather than a
// member because retail's symbol table has no mangled name for it, so objdiff has nothing to pair
// an invented `SPersistentOptionsValue::Clamp()` against.
extern "C" void fn_801461AC(SPersistentOptionsValue* self);

SPersistentOptionsValue::SPersistentOptionsValue(int lo, int hi, int value)
    : x00_lo(lo), x04_hi(hi), x08_value(value) {
  fn_801461AC(this);
}
