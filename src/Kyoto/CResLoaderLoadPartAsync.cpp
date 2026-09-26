/**
 * `fn_802FC81C` - retail `.text:0x802FC81C`, `size:0x7C` = 124 bytes, 32-byte frame.
 *
 * ```
 * 802fc81c:  stwu r1,-32(r1) ; mflr r0 ; stw r0,36(r1)
 * 802fc828:  stw  r31,28(r1) ; mr   r31,r3        <- self
 * 802fc830:  stw  r30,24(r1) ; mr   r30,r7        <- buf
 * 802fc838:  stw  r29,20(r1) ; mr   r29,r6        <- len
 * 802fc840:  stw  r28,16(r1) ; mr   r28,r5        <- base offset
 * 802fc848:  bl   fn_802FCEEC
 * 802fc850:  lwz  r3,104(r31)                     <- this->x68_curRes, off the *old* r31
 * 802fc854:  mr   r31,r3                          <- ... then r31 is reused for the pak
 * 802fc858:  bl   GetOffset__Q28CPakFile8SResInfoCFv
 * 802fc864:  mr   r3,r31 ; mr r4,r30 ; mr r5,r29
 * 802fc870:  add  r7,r28,r0
 * 802fc874:  li   r6,0
 * 802fc878:  bl   AsyncSeekRead__8CDvdFileFPvUi11ESeekOrigini
 * ```
 *
 * It is `fn_802FCEEC` (the current-resource search) followed by
 * `CPakFile::DvdFile().AsyncSeekRead(buf, len, kSO_Set, base + GetOffset())`. Two things in that
 * listing are load-bearing and neither is guesswork:
 *
 *  * **`this` is live across the `fn_802FCEEC` call**, because the `+0x68` read comes *after* it -
 *    the search does not return the `SResInfo*`, it leaves it in the loader. That is the same
 *    reason the five accessors in `Kyoto/CResLoaderResAccessors.cpp` re-read `x68_curRes` rather
 *    than using the search's return value.
 *  * **r31 is the search's return value**, assigned *after* the `+0x68` read, so the compiler
 *    emitted the read against the register `self` still occupies and then overwrote it. Writing
 *    the two statements in that order is what reproduces it: hoisting `x68_curRes` into its own
 *    local *before* the search and using it after moves the `lwz` above the call and the unit drops
 *    to 86.77% (measured, both ways, same object). This is the same rule as `fn_802FCA68`'s
 *    `lwz r29,104(r29)` - `self` dies at the read, and the register is reused immediately.
 *  * **The call's arguments are `buf` in r4, the caller's length in r5, `kSO_Set` in r6 and
 *    `base + GetOffset()` in r7**, and the last two are *added*, not substituted. So the caller's
 *    third argument is an offset *within* the resource and the resource's own offset is the base -
 *    which is what makes this the "part" loader, and it is the only reading under which the `add`
 *    at 0x802fc86c is right.
 *
 * Retail's symbols for it are unnamed, so it is emitted under its dtk name with C linkage;
 * `auto_03_80052880` references `fn_802FC81C` by that name, so the name may not be renamed.
 */
#include "types.h"

#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CResLoader.hpp"

extern "C" void* fn_802FC81C(void* resLoader, const SObjectTag& tag, int baseOffset, int len,
                             void* buf) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  CPakFile* const pak = static_cast< CPakFile* >(fn_802FCEEC(resLoader, tag));
  const uint resOffset = self->x68_curRes->GetOffset();
  return pak->DvdFile().AsyncSeekRead(buf, len, kSO_Set, baseOffset + resOffset);
}
