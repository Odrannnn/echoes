/**
 * `fn_802FCEEC` - retail `.text:0x802FCEEC`, `size:0x24` = 36 bytes, six instructions:
 *
 * ```
 * 802fceec:  stwu r1,-16(r1)
 * 802fcef0:  mflr r0
 * 802fcef4:  lwz  r4,4(r4)          ; tag.id
 * 802fcef8:  stw  r0,20(r1)
 * 802fcefc:  bl   fn_802FCDE8
 * 802fcf00:  lwz  r0,20(r1)
 * 802fcf04:  mtlr r0
 * 802fcf08:  addi r1,r1,16
 * 802fcf0c:  blr
 * ```
 *
 * It is `fn_802FCDE8` - the three-list search `src/Kyoto/CResLoaderResAccessors.cpp` calls -
 * with the tag's `id` hoisted into r4 and the return value passed straight through, and
 * **nothing else**. There is no `stw r31` / `mr r31,r3`, which is the proof that `this` is
 * never live across the call here: the search takes it in r3 and returns the `CPakFile*` in
 * r3, and the seven callers that go on to use the `CPakFile::SResInfo*` the search left in
 * `this->x68_curRes` re-read it themselves.
 *
 * It is its own unit only because of where the ranges are: `fn_802FCDE8` is 0x802FCDE8, so
 * this function is the next thing in retail's `.text` and 0x802FCEEC is 0x24 bytes after the
 * accessors' range ends at 0x802FCC44 - **a unit may claim several contiguous ranges but not
 * two discontiguous ones**, and 0x802FCC44..0x802FCEEC is `fn_802FCEEC`'s own neighbour
 * `fn_802FCDE8`.
 *
 * `fn_802FCDE8` itself is retail's bytes here: it is unnamed in `config/G2ME01/symbols.txt` and
 * it is 0x104 bytes of three list walks whose two per-pak probes (`fn_802FCF98`, 0x5C, and
 * `fn_802FCF10`, 0x88) are themselves unwritten. `docs/research/paks.md` has the block map.
 */
#include "types.h"

#include "Kyoto/CResLoader.hpp"

extern "C" void* fn_802FCEEC(void* resLoader, const SObjectTag& tag) {
  return fn_802FCDE8(resLoader, tag.id);
}
