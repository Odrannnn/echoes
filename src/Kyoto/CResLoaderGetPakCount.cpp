/**
 * `CResLoader::GetPakCount` - retail `.text:0x802FBC60`, `size:0x10` = 16 bytes.
 *
 * ```
 * 802fbc60: lwz  r4,44(r3)      ; 0x2C = the list at +0x18's x14_count
 * 802fbc64: lwz  r0,68(r3)      ; 0x44 = the list at +0x30's x14_count
 * 802fbc68: add  r3,r4,r0
 * 802fbc6c: blr
 * ```
 *
 * So it counts the two **finished** pak lists and nothing else: the list at +0x48, the one
 * `AddPakFileAsync` pushes into and `AsyncIdlePakLoading` drains, is deliberately not counted -
 * a pak being loaded is not a pak you can read. The two it does count are the ARAM-file paks
 * (+0x18) and the ordinary ones (+0x30), which is the same split `fn_802FCFF4` makes and the
 * same split `GetPakFile` searches: `GetPakFile` reads `this+0x1C` while `idx < *(this+0x2C)`
 * and `this+0x34` after it (0x802fba78 / 0x802fbaf0).
 *
 * Its own unit rather than a third function in `Kyoto/CResLoaderPakPump.cpp`, because 0x802FBC60
 * and 0x802FCCE4 are not adjacent: a unit may claim several contiguous ranges but not two
 * discontiguous ones, and 0x1B4 of retail between them belongs to `CResLoader::GetPakFile`
 * (0x802FBA68, `size:0xFC`) and the code around it.
 */

#include "types.h"

#include "Kyoto/CResLoader.hpp"

int CResLoader::GetPakCount() const {
  return static_cast< int >(x18_aramFileList.size() + x30_pakList.size());
}
