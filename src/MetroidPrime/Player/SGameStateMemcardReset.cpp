/**
 * `fn_80009898` - retail `.text:0x80009898`, `size:0x34` = 52 bytes, 0x80009898..0x800098CC.
 * The next symbol, `fn_800098CC` (`SGameStateMemcardFill.cpp`, 0x164 bytes), starts at 0x800098CC,
 * so that is the exact end of the range this unit claims.
 *
 *     800098ac  bl    80009ac0   fn_80009AC0(self)
 *     800098b0  mr    r3,r31      reload `self` - r3 is call-clobbered
 *     800098b4  bl    800098cc   fn_800098CC(self)
 *     800098b8  lwz   r0,20(r1)
 *     800098bc  lwz   r31,12(r1)
 *     800098c0  mtlr  r0
 *     800098c4  addi  r1,r1,16
 *     800098c8  blr
 *
 * **It is a `void` function**, and that is read off the epilogue rather than assumed: there is no
 * `mr r3,r31` between the last `bl` and the `blr`, so nothing is moved into `r3` on the way out.
 * The `mr r3,r31` at 0x800098B0 is the reload for the second call, not a return. Both callers -
 * `fn_80009DBC` at 0x80009E60, which discards the result, and nothing else - agree.
 *
 * ## Why this pair is on the boot path
 *
 * `fn_80009DBC` (`CGameStateMemcardCtor.cpp`, Matching) is the constructor of the 0xE8-byte
 * `SGameStateMemcard` at `CGameState+0x204`. It fills `+0x50` with 76 and `+0x54..+0x9B` with
 * `lbl_80417D91`, and then calls this function as its **last statement**. `fn_800098CC` - the
 * other half of the pair, in `SGameStateMemcardFill.cpp` - **overwrites** `+0x50` with 0 and
 * refills `+0x54` with 76 bytes of `lbl_80417D93`, so the 76 the constructor put there does not
 * survive its own last call. That is a fact about the pair and not a bug in the port;
 * `include/MetroidPrime/Player/CGameState.hpp` says so on `SGameStateMemcard`.
 *
 * ## Why this is its own unit and the fill is another
 *
 * The two functions are adjacent, so one unit *can* claim both, and it did: at 99.31% with
 * `fn_80009898` at 100% and `fn_800098CC` at 99.19% (99.55% once the pair is split, because
 * the reference frame changes). `fn_800098CC` is 7 register-allocation
 * instructions away from Matching and those 7 are documented in that file - they are an r4/r5
 * swap inside a loop retail's own bytes leave without a body, and about thirty spellings of it
 * were measured with `tools/try_batch.py` without moving them. **A Matching unit is the project's
 * only currency, and a Matching `fn_80009898` is worth more than one unit at 99.31%**, so the
 * pair is split at the symbol boundary: this file claims 0x80009898..0x800098CC and is
 * `Matching`, `SGameStateMemcardFill.cpp` claims 0x800098CC..0x80009A30 and is `NonMatching`.
 * This is the same prefix/matching/tail arrangement `ScriptRiftPortal` uses. The call from here
 * into `fn_800098CC` resolves against that unit's `dtk`-filled object, which carries retail's own
 * bytes, so the DOL still reproduces.
 *
 * `extern "C"` for the reason every retail-named function here has one: retail's symbol table
 * calls this `fn_80009898`, and a C++ spelling would mangle to a name objdiff has nothing to pair
 * against. Both callees are declared, not defined, here; `fn_80009AC0` is unclaimed and
 * `fn_800098CC` is the other half of this split.
 *
 * **Not in `files.cmake`, measured** - see this file's entry in `tools/check_files_cmake.py`.
 *
 * **After the merge to upstream PrimeDecomp/echoes** the two callees are upstream's
 * `CControlMapper` member functions on the same 0xE8 bytes at `CGameState+0x204` -
 * `fn_80009898` is `CControlMapper::Reset` and `fn_800098CC` its second half - and the
 * `SGameStateMemcard*` parameter survives as the named overlay in
 * `include/MetroidPrime/Player/CGameStateBlocks.hpp`. The retail names and this two-call body are
 * kept: the symbol is what `config/G2ME01/symbols.txt` calls it, and renaming it into the class
 * would change the symbol the `Matching` build pairs.
 */
#include "types.h"

#include "MetroidPrime/Player/CGameState.hpp"

// `fn_80009AC0`, retail 0x80009ac0, 0x130: the half of `SGameStateMemcard`'s default state that
// fills `+0x00..+0x4F`. Unclaimed, so `dtk` fills it with retail's bytes.
extern "C" void fn_80009AC0(SGameStateMemcard* self);

// The other half - `SGameStateMemcardFill.cpp`, NonMatching at 99.55%.
extern "C" void fn_800098CC(SGameStateMemcard* self);

extern "C" void fn_80009898(SGameStateMemcard* self) {
  fn_80009AC0(self);
  fn_800098CC(self);
}
