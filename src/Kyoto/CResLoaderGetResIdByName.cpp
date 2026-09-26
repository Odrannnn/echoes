/**
 * `fn_802FCC44` - retail `.text:0x802FCC44`, `size:0xA0` = 160 bytes, 32-byte frame.
 * `CResLoader::GetResIdByName(const char*)`: the name lookup, and it is the **same two-list walk
 * as `fn_802FCDE8`** with `CPakFile::GetResIdByName` where that one calls `GetResInfo`:
 *
 * ```
 * 802fcc44:  stwu r1,-32(r1) ; mflr r0 ; stw r0,36(r1)
 * 802fcc50:  stw  r31,28(r1) ; stw r30,24(r1)
 * 802fcc58:  mr   r30,r4                        <- name
 * 802fcc5c:  stw  r29,20(r1) ; mr r29,r3         <- self
 * 802fcc64:  lwz  r31,28(r3)                    <- this+0x1C = x18_aramFileList.x4_start
 * 802fcc68:  b    <the test>
 * 802fcc6c:  lwz  r3,12(r31)                    <- node->x8_item.x4_pak
 * 802fcc70:  mr   r4,r30 ; bl GetResIdByName__8CPakFileCFPCc
 * 802fcc78:  cmplwi r3,0 ; beq <next> ; b <done>
 * 802fcc84:  lwz  r31,4(r31)                    <- node->x4_next
 * 802fcc88:  lwz  r0,32(r29) ; cmplw r31,r0 ; bne <the body>   <- this+0x20 = x8_end
 * 802fcc94:  lwz  r31,52(r29)                    <- this+0x34 = x30_pakList.x4_start
 * 802fcc98:  b    <the second test>
 *   ... the same six instructions again, with this+0x38 = x38_end ...
 * 802fccc4:  li   r3,0
 * 802fccc8:  <the epilogue>
 * ```
 *
 * Three things are the whole of it:
 *
 *  * **The two lists are `x18_aramFileList` and `x30_pakList`**, the same two `fn_802FCDE8`
 *    walks, read at `+0x1C`/`+0x20` and `+0x34`/`+0x38` - their `x4_start` and `x8_end`.
 *  * **The step is `node->x4_next` at `+4`, the value is `node->x8_item.x4_pak` at `+0xC`**, and
 *    both are raw `rstl::list< SPakLoadEntry >::node` fields. `node` is 16 bytes (two pointers
 *    then the 8-byte item) so the item's second word lands at `+0xC`; an `iterator` walk reaches
 *    the same offsets through `get_value()`, and it is what the two loops below are written as -
 *    `operator->` on the iterator compiles to the bare `lwz rX,12(node)`.
 *  * **The loop is rotated**: `b` into the test, the body first, the test last. That is MWCC's
 *    shape for a `while` whose test reads a member of `self` - it is the same shape
 *    `fn_802FCDE8` and the five accessors have, and it comes out of an ordinary
 *    `for (it = l.begin(); it != l.end(); ++it)` body.
 *
 * **The return type is `const SObjectTag*` and not a `CAssetId`**: the found path branches to the
 * epilogue with the callee's r3 untouched (`b 0x84` over the `li r3,0` at 0x802fcc84), so no field
 * is extracted from it. The header never declared a `CResLoader::GetResIdByName`, so this is a free
 * function; the two members it *does* declare for the accessors stay left undefined.
 *
 * The function is **not referenced by any other object** (`nm -u` over `build/G2ME01/obj` finds no
 * referrer), so nothing constrains the name; it is left as the dtk name because retail's symbol
 * for it is unnamed and objdiff pairs by name.
 */
#include "types.h"

#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResLoader.hpp"

extern "C" const SObjectTag* fn_802FCC44(void* resLoader, const char* name) {
  CResLoader* const self = static_cast< CResLoader* >(resLoader);
  typedef rstl::list< SPakLoadEntry > list_t;

  for (list_t::iterator it = self->x18_aramFileList.begin(); it != self->x18_aramFileList.end();
       ++it) {
    const SObjectTag* const tag = it->x4_pak->GetResIdByName(name);
    if (tag != nullptr) {
      return tag;
    }
  }

  for (list_t::iterator it = self->x30_pakList.begin(); it != self->x30_pakList.end(); ++it) {
    const SObjectTag* const tag = it->x4_pak->GetResIdByName(name);
    if (tag != nullptr) {
      return tag;
    }
  }
  return nullptr;
}
