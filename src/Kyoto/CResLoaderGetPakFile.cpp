/**
 * `CResLoader::GetPakFile` - retail `.text:0x802FBA68`, `size:0xFC` = 252 bytes.
 *
 * **80.13% fuzzy, 0/1 functions at 100%, unit deliberately left `NonMatching`.** Not landed, and
 * the reason is one shape rather than twenty: MWCC unrolls the walk by eight and **peels the
 * first eight iterations**, and this build unrolls without the peel.
 *
 * ```
 * 802fba68: lwz  r0,44(r3)      ; 0x2C, the +0x18 list's count
 * 802fba6c: cmpw r4,r0
 * 802fba70: bge  0x802fbaec    ; idx >= count18 -> search +0x30 from idx - count18
 * 802fba74: cmpwi r4,0
 * 802fba78: lwz  r5,28(r3)      ; 0x1C, the +0x18 list's x4_start
 * 802fba7c: li   r6,0
 * 802fba80: ble  0x802fbae4    ; idx <= 0 -> return
 * 802fba84: cmpwi r4,8
 * 802fba88: addi r3,r4,-8       ; <- the peel: the trip count is idx - 8, not idx
 * 802fba8c: ble  0x802fbacc
 * 802fba90: addi r0,r3,7
 * 802fba94: srwi r0,r0,3        ; chunks = ceil((idx-8)/8)
 * 802fba98: mtctr r0
 * 802fbaa4: <eight `lwz rX,4(rX)`, r6 += 8, bdnz>
 * 802fbacc: subf r0,r6,r4 ; mtctr r0 ; cmpw r6,r4 ; bge   ; the remainder, one link each
 * 802fbae4: lwz  r3,12(r5)      ; the item's CPakFile*
 * ```
 *
 * This build emits the same eight-link body and the same remainder loop, but computes the chunk
 * count as `(idx - count18) >> 3` with no `-8` and no `cmpwi r4,8`, and its object is 0xE0 = 224
 * bytes against retail's 0xFC = 252. Getting the peel means writing a loop whose body MWCC peels,
 * which is a control-flow experiment rather than a naming one, so it is left for a lane that wants
 * it rather than half-landed here. **The `.text` is claimed and the unit is `NonMatching`, so the
 * DOL's sha1 is unaffected** - the range keeps retail's bytes.
 *
 * Two searches over the two finished pak lists, `+0x18` first and `+0x30` second. The list at
 * `+0x48` - the one `AddPakFileAsync` pushes into and `AsyncIdlePakLoading` drains - is not
 * searched, and that is the same split `fn_802FCFF4` and `GetPakCount` make.
 */

#include "types.h"

#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CResLoader.hpp"

CPakFile& CResLoader::GetPakFile(int idx) const {
  if (idx >= x18_aramFileList.size()) {
    rstl::list< SPakLoadEntry >::const_iterator it = x30_pakList.begin();
    for (int i = x18_aramFileList.size(); i < idx; ++i) {
      ++it;
    }
    return *it->x4_pak;
  }

  rstl::list< SPakLoadEntry >::const_iterator it = x18_aramFileList.begin();
  for (int i = 0; i < idx; ++i) {
    ++it;
  }
  return *it->x4_pak;
}
