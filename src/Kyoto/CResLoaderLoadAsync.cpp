/**
 * `fn_802FCA68` - retail `.text:0x802FCA68`, `size:0x80` = 128 bytes, 32-byte frame. The
 * **whole-resource** async load, the twin of `fn_802FC81C` in `Kyoto/CResLoaderLoadPartAsync.cpp`:
 *
 * ```
 * 802fca68:  stwu r1,-32(r1) ; mflr r0 ; stw r0,36(r1)
 * 802fca74:  stw  r31,28(r1) ; stw r30,24(r1) ; stw r29,20(r1)
 * 802fca7c:  mr   r29,r3                        <- self
 * 802fca84:  stw  r28,16(r1) ; mr r28,r5        <- buf
 * 802fca8c:  bl   fn_802FCEEC
 * 802fca90:  lwz  r29,104(r29)                  <- this->x68_curRes (r29 reused)
 * 802fca94:  mr   r30,r3                        <- the pak
 * 802fca98:  bl   GetSize__Q28CPakFile8SResInfoCFv
 * 802fca9c:  mr   r31,r3                        <- size
 * 802fcaa0:  bl   GetOffset__Q28CPakFile8SResInfoCFv
 * 802fcaac:  addi r0,r31,31
 * 802fcab0:  mr   r7,r3                         <- offset
 * 802fcab4:  mr   r3,r30 ; mr r4,r28
 * 802fcabc:  clrrwi r5,r0,5                     <- (size + 31) & ~31
 * 802fcac0:  li   r6,0
 * 802fcac4:  bl   AsyncSeekRead__8CDvdFileFPvUi11ESeekOrigini
 * ```
 *
 * So the body is `AsyncSeekRead(buf, (GetSize() + 31) & ~31, kSO_Set, GetOffset())` and the two
 * differences from the part loader are that the length is **the resource's own size rounded up
 * to 32** - `addi r0,r31,31` then `clrrwi r5,r0,5`, not a mask on the caller's argument - and
 * that `+0x68` is re-read through the register `self` already occupies (`lwz r29,104(r29)`), the
 * same self-then-reuse shape as `fn_802FC81C`.
 *
 * **`GetSize` is called before `GetOffset`**, and both are called through the *same* register
 * (`mr r3,r29` before each), so `x68_curRes` is a single local, not two reads of `self`. More
 * importantly **the length needs a named local**: written inline,
 * `AsyncSeekRead(buf, (res->GetSize() + 31) & ~31, kSO_Set, res->GetOffset())`, mwcceppc calls
 * `GetOffset` *first* and puts the `addi`/`clrrwi` in the wrong place - 87.03% - and hoisting
 * `const uint size = res->GetSize();` gives retail's exact order (`GetSize`, then `GetOffset`, then
 * `addi r0,r31,31`) and 100.00%. **The argument order of an expression is not the evaluation order
 * mwcceppc picks, and a named local is what fixes it.** Measured, both ways, same object.
 *
 * Unnamed in `config/G2ME01/symbols.txt` and referenced by **five** dtk objects
 * (`auto_03_8004E448`, `auto_03_8020EE18`, `auto_03_8021BFE0`, `auto_03_80273F60`,
 * `auto_03_802C3E88`), so it keeps its dtk name with C linkage.
 */
#include "types.h"

#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CDvdFile.hpp"
#include "Kyoto/CResLoader.hpp"

extern "C" void* fn_802FCA68(void* resLoader, const SObjectTag& tag, void* buf) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  CPakFile* const pak = static_cast< CPakFile* >(fn_802FCEEC(resLoader, tag));
  CPakFile::SResInfo* const res = self->x68_curRes;
  const uint size = res->GetSize();
  return pak->DvdFile().AsyncSeekRead(buf, (size + 31) & ~static_cast< uint >(31), kSO_Set,
                                     res->GetOffset());
}
