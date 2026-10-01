/**
 * `CInGameTweakManager::CInGameTweakManager()` - retail `__ct__19CInGameTweakManagerFv`,
 * `.text:0x8016C230`, `size:0x14` = 20 bytes:
 *
 *   8016c230  li   r0,0
 *   8016c234  stw  r0,4(r3)
 *   8016c238  stw  r0,8(r3)
 *   8016c23c  stw  r0,12(r3)
 *   8016c240  blr
 *
 * The three stores are `rstl::vector<CTweakValue>`'s default constructor (`mValues`); +0x00 is
 * the vector's empty allocator and is left as the heap had it. The only caller is
 * `CGameGlobalObjects`'s constructor, `new(16)` at 0x80008508.
 */

#include "MetroidPrime/CInGameTweakManager.hpp"

CInGameTweakManager::CInGameTweakManager() {}
