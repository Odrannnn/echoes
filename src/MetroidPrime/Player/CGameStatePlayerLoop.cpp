/**
 * `fn_801440C0` - retail `.text:0x801440C0`, `size:0x80` = 128 bytes, 0x801440C0..0x80144140.
 * The next symbol is `fn_80144140` (`CGameState::CGameState(CInputStream&, int)`, 0x684, in
 * `CGameStateStreamCtor.cpp`) and starts at 0x80144140, so that is the exact end of the range
 * this unit claims. The two functions are adjacent and unrelated.
 *
 * `CGameState::CGameState()` calls it once, at 0x80144C50, under `if (gpMemoryCard)`, so it is
 * retail's memory-card hook. The body is a loop over the four player states and then four calls
 * with no condition between them:
 *
 *     801440d4  li    r30,0           the loop index, hoisted above the prologue's last save
 *     801440dc  mr    r29,r3          `this`
 *     801440e0  addi  r31,r29,28      the walk pointer: `this + 0x1C`
 *     801440e4  b     0x801440f8      jump to the condition - a bottom-tested loop
 *     801440e8  lwz   r3,0(r31)      the CPlayerState*
 *     801440ec  bl    0x800850f8      CPlayerState::InitializeScanTimes
 *     801440f0  addi  r31,r31,8       the pointer steps; it is never re-indexed
 *     801440f4  addi  r30,r30,1
 *     801440f8  lwz   r0,24(r29)      `this->x18_playerStates`
 *     801440fc  cmpw  r30,r0
 *     80144100  blt   0x801440e8
 *     80144104  addi  r3,r29,196      `this + 0xC4` = hintOptions
 *     80144108  bl    0x80180430
 *     8014410c  addi  r3,r29,220      `this + 0xDC` = persistentOptions
 *     80144110  bl    0x80145a2c
 *     80144114  mr    r3,r29
 *     80144118  bl    0x801437dc
 *     8014411c  mr    r3,r29
 *     80144120  bl    0x8014306c
 *
 * ## The loop is walked with a *stepped pointer*, and that is load-bearing
 *
 * `x01c_players` is declared in `include/MetroidPrime/Player/CGameState.hpp` as
 * `CPlayerState* x01c_players[4][2]`, so `x01c_players[i][0]` is a **scaled index** and mwcceppc
 * emits `rlwimi`/`add` on the index rather than the two one-instruction bumps retail has. The
 * pointer has to be a separate variable stepped by 8 for `addi r31,r31,8` and `addi r30,r30,1` to
 * be the whole loop control - and the index has to survive as its own register, because the
 * condition reads the count back out of `this` (`lwz r0,24(r29)`) instead of keeping it. That is
 * mwcceppc's own idiom for `i < this->x18_playerStates` with `i` a signed `int`: `cmpw`/`blt`, not
 * `cmpw`/`bge` on an unsigned compare.
 *
 * `CPlayerState::InitializeScanTimes` is at retail 0x800850F8 and is **not** in this unit's
 * range. It is defined by `MetroidPrime/Player/CPlayerState.cpp`, which is NonMatching, and
 * `dtk`'s filled object for that unit is what the DOL link uses - the same arrangement
 * `configure.py` relies on for `CInputGeneratorUpdate.cpp`'s call to a `main.cpp` symbol.
 *
 * `fn_80180430` is on `hintOptions` and `fn_80145A2C` on `persistentOptions`, both named after the
 * members in `CGameState.hpp`; both are declared `extern "C"` here rather than called as methods
 * because retail's symbol table has no name for either and a C++ spelling would mangle to
 * something objdiff cannot pair. `fn_801437DC` and `fn_8014306C` take the whole `CGameState*`.
 *
 * **Not in `files.cmake`, measured** - see this file's entry in `tools/check_files_cmake.py`.
 */
#include "types.h"

#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"

// Upstream: `CHintOptions::InitializeMemoryState`. The host keeps the port's address name.
#if defined(__MWERKS__)
#define fn_80180430 InitializeMemoryState__12CHintOptionsFv
#endif

extern "C" {
// `fn_80180430`, on `this + 0xC4` (`hintOptions`); `fn_80145A2C`, on `this + 0xDC`
// (`persistentOptions`); the last two on the whole `CGameState`. None of the four is named in
// retail's symbol table.
void fn_80180430(CHintOptions* self);
void fn_80145A2C(CPersistentOptions* self);
void fn_801437DC(CGameState* self);
void fn_8014306C(CGameState* self);
} // extern "C"

// `x01c_players[0][0]` is the first `CPlayerState*`; stepping that `CPlayerState**` by 2 walks the
// 8-byte slots without a scaled index. See the header comment.
extern "C" void fn_801440C0(CGameState* self) {
  CPlayerState** player = &self->x01c_players[0][0];
  // **The comma operands are in this order because retail's two bumps are**: `addi r31,r31,8`
  // (the pointer) at 0x801440F0 comes *before* `addi r30,r30,1` (the index) at 0x801440F4, so the
  // step-then-increment spelling is the one that matches and the other one does not.
  for (int i = 0; i < self->x18_playerStates; player += 2, ++i) {
    (*player)->InitializeScanTimes();
  }

  fn_80180430(&self->hintOptions);
  fn_80145A2C(&self->persistentOptions);
  fn_801437DC(self);
  fn_8014306C(self);
}
