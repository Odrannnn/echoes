/**
 * `fn_80145950` - retail 0x80145950, `size:0x5C` = 92 bytes: the constructor of
 * `CGameState+0x54`, `SGameStateCardOpts` (0x2C). The first of the nested constructors
 * `CGameState::CGameState()` (retail `fn_801449C8`, `CGameStateCtor.cpp`, Matching at 100%) needs,
 * and `CGameGlobalObjects`' constructor runs that on the port's boot path at 0x800084DC.
 *
 *     0x80145958  li   r4,0        the second argument to the call below
 *     0x80145968  bl   80146154    `fn_80146154(this, 0)` - `CPersistentOptionsCtor.cpp`
 *     0x8014596C  li   r0,0
 *     0x80145970  stw  r0,28(r31)  +0x1C
 *     0x80145974  stw  r0,32(r31)  +0x20
 *     0x80145978  stw  r0,36(r31)  +0x24
 *     0x8014597C  stw  r0,40(r31)  +0x28
 *     0x80145980  lwz  r0,-28356(r13)   `gpMemoryCard`
 *     0x8014598C  mr   r3,r31
 *     0x80145990  bl   80145628
 *     0x801459A8  blr
 *
 * The `lwz r0,-28356(r13) ; cmplwi r0,0 ; beq` is the same three instructions as
 * `if (gpMemoryCard)` in `fn_801449C8` at 0x80144C50, so the global is the same one. `r13` is
 * `_SDA_BASE_` = 0x8041FD80, and -28356 = -0x6EC4 lands on 0x80418EBC, which `symbols.txt` calls
 * `gpMemoryCard` - independently of the match, since `lwz r3,-28360(r13)` two instructions into
 * `CGameArchitectureSupport`'s constructor lands on 0x80418EB8 = `gpGameState`.
 *
 * So the block at +0x54 is the *same* shape as the one at +0xDC - `fn_80146154` is what builds
 * +0x00..+0x17 of it - and this wrapper adds the four zero words at +0x1C..+0x28 and the
 * memory-card hook. `CPersistentOptionsCtor.cpp` explains the two uninitialised bytes inside
 * `fn_80146154`; they are in the callee, so this function needs nothing unusual.
 *
 * It returns `this` (`mr r3,r31` at 0x80145998, after the epilogue's reload of r31), so it is
 * declared returning the pointer even though both callers - `fn_801449C8` at 0x80144A6C and
 * `fn_80144140` at 0x801441DC - discard it. Their declarations say `void`; that is a separate
 * translation unit, and a return type only has to agree where two declarations meet.
 * `extern "C"` for the same reason as every other retail-named function here: retail's symbol
 * table calls it `fn_80145950` and a C++ constructor would mangle to
 * `__ct__18SGameStateCardOptsFv`, which objdiff has nothing to pair against. The name is what
 * `CGameStateCtor.cpp` and `CGameStateStreamCtor.cpp` already declare, so nothing in
 * `config/G2ME01/symbols.txt` is renamed and the REL modules that reference it are untouched.
 */

#include "types.h"

#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

extern "C" {
  // The +0x54 block is the same 0x2C shape as the class at +0xDC and is built by its constructor.
  CPersistentOptions* fn_80146154(CPersistentOptions* self, int flag);
  // `fn_80145628`, retail 0x80145628: a frame and a tail call to `fn_80145A2C`, which walks
  // `gpMemoryCard`'s own two 28-byte-element vectors depending on `*(this+0x00)`.
  void fn_80145628(SGameStateCardOpts* self);
} // extern "C"

extern "C" SGameStateCardOpts* fn_80145950(SGameStateCardOpts* self) {
  // `SGameStateCardOpts` and `CPersistentOptions` are both 0x2C and share their first 0x18; the
  // header says so. The cast is what the retail call is: same pointer, different static type.
  fn_80146154(reinterpret_cast< CPersistentOptions* >(self), 0);

  self->x1c = 0;
  self->x20 = 0;
  self->x24 = 0;
  self->x28 = 0;

  if (gpMemoryCard) {
    fn_80145628(self);
  }
  return self;
}
