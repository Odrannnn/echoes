/**
 * `fn_801461AC` - retail 0x801461AC, `size:0x44` = 68 bytes, 0x801461AC..0x801461F0: the clamp
 * `SPersistentOptionsValue`'s constructor calls as its last statement, and the only member
 * function of the class.
 *
 *     0x801461AC  lwz  r4,8(r3)     v = +0x08
 *     0x801461B0  lwz  r5,0(r3)     lo = +0x00
 *     0x801461B4  cmpw r4,r5
 *     0x801461B8  blt  801461c8     v < lo  -> the body
 *     0x801461BC  lwz  r0,4(r3)     hi = +0x04
 *     0x801461C0  cmpw r4,r0
 *     0x801461C4  blelr             v <= hi  -> RETURN, no store
 *     0x801461C8  cmpw r5,r4        lo vs v
 *     0x801461CC  lwz  r0,4(r3)
 *     0x801461D0  ble  801461d8
 *     0x801461D4  b    801461e8     store r5, which is still lo
 *     0x801461D8  cmpw r0,r4        hi vs v
 *     0x801461DC  bge  801461e4
 *     0x801461E0  mr   r4,r0
 *     0x801461E4  mr   r5,r4
 *     0x801461E8  stw  r5,8(r3)
 *     0x801461EC  blr
 *
 * **No frame at all** - it is a leaf that calls nothing - and **one store**, at 0x801461E8, which
 * both arms of the body reach. So the body is a single guarded assignment, not an
 * `if (v < lo) ... else if (v > hi) ...`: that spelling needs two stores and mwcceppc does not
 * merge them. Measured - `if/else if` spelling 14 differing instructions, the guarded one 0.
 *
 * The guard is `v < lo || v > hi` with the second test written `blelr`: MWCC inverts `>` into
 * `<=` and, because the fall-through there *is* the function's return, the branch-to-return
 * collapses into a conditional return.
 *
 * ## The two spellings that decide the last four instructions
 *
 * The body re-tests the same two comparisons, and both re-tests are *inverted* on retail's bytes,
 * which is the whole of the 4-instruction gap this unit had at 99.71%:
 *
 *  * the outer test is `lo > v`, not `lo >= v` - retail branches on `ble`, i.e. "lo <= v goes to
 *    the hi arm", so the source's test is `lo > v` and MWCC inverted it. `lo >= v` compiles to
 *    `blt` and is 1 instruction out (measured, and it is the *only* difference).
 *  * the two values are read into locals `lo`, `hi`, `v` **inside** the guard, in that order, and
 *    not off the members. Without them the `lwz r0,4(r3)` for `hi` is not hoisted above the
 *    branch at 0x801461D0 and the block layout changes: 4 instructions out instead of 1
 *    (measured, `tern-loGE-hilt-hi1` and `tern-loGE-hilt-all` in the sweep).
 *
 * Both spellings were found with `tools/try_batch.py`, ranked by differing instructions; the full
 * sweep is 12 guard/body combinations and the winner is 0.
 */

#define _CMEMORY

#include "types.h"

#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"

extern "C" {
// `SPersistentOptionsValue::Clamp()`. `void`, and it returns through `this`, which is why there
// is no value in r3 at the `blr`. Retail's symbol table calls it `fn_801461AC` and a C++ spelling
// would mangle to something objdiff has nothing to pair against.
void fn_801461AC(SPersistentOptionsValue* self);
} // extern "C"

extern "C" void fn_801461AC(SPersistentOptionsValue* self) {
  SPersistentOptionsValue* p = self;
  if (p->x08_value < p->x00_lo || p->x08_value > p->x04_hi) {
    int lo = p->x00_lo;
    int hi = p->x04_hi;
    int v = p->x08_value;
    p->x08_value = (lo > v) ? lo : ((hi < v) ? hi : v);
  }
}
