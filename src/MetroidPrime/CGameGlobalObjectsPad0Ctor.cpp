/**
 * `fn_803096C4` - the constructor of `CGameGlobalObjects+0x00`, retail `.text:0x803096C4`,
 * `size:0x4C` = 76 bytes. **This body used to live in `src/Kyoto/CResFactoryCtor.cpp` and call
 * itself `CResFactory::CResFactory()`, which was the wrong function.** `CGameGlobalObjects`'s
 * constructor calls `fn_803096C4` on `this+0x00` and `fn_802FB154` on `this+0x04` (0x800084A0 /
 * 0x800084A8), and `gpResourceFactory` is stored as `this+0x04` at 0x80008534, so `CResFactory` is
 * at **+0x04** and `fn_802FB154` is its constructor. The four bytes in front of it are a real
 * member with a constructor and a destructor (`fn_80309660`), and this is that member's
 * constructor. `src/Kyoto/CResFactoryCtor.cpp` is now the real `CResFactory` constructor.
 *
 * The class is **unnamed and unnameable**: this function never writes `*this` - it is a one-shot
 * `CARDInit` behind two `.sbss` flag bytes - and nothing in the DOL reads those flags either, so
 * `include/MetroidPrime/CGameGlobalObjects.hpp` keeps the member as `char pad0[4]` with both
 * function names in the comment. This file is named for *where* the class is, not for what it is
 * called, because nothing knows the second.
 *
 *     0x803096D4  mr   r31,r3                  ; this is live across the body
 *     0x803096D8  lbz  r0,lbl_80419B88(r13)    ; a static "already done" flag
 *     0x803096DC  cmplwi r0,0
 *     0x803096E0  bne  0x803096F0
 *     0x803096E4  bl   CARDInit                ; 0x803581D4, size 0xAC
 *     0x803096E8  li   r0,1
 *     0x803096EC  stb  r0,lbl_80419B88(r13)
 *     0x803096F0  li   r0,1
 *     0x803096F4  mr   r3,r31                  ; dead, and retail keeps it
 *     0x803096F8  stb  r0,lbl_80419B89(r13)
 *
 * **Why a member holding nothing calls `CARDInit` is not explained by anything in the DOL**, and
 * the two flags are not referenced from anywhere else - a whole-DOL scan finds exactly this one
 * writer and no reader of either.
 *
 * Two retail data objects come with it, both one byte, both adjacent: `.sbss:0x80419B88`
 * (`lbl_80419B88`) and `.sbss:0x80419B89` (`lbl_80419B89`). They are left **unclaimed on purpose**:
 * dtk gives a claimed range's symbols to the claiming object and takes them away from the
 * `auto_*` unit that has them, and `lbl_80419B8A` at 0x80419B8A is referenced from
 * `CStateManager.o` and `auto_03_801724CC_text.o`, so claiming 0x80419B88..0x80419B8A breaks the
 * link with `undefined: 'lbl_80419B8A'`. Referring to them without claiming is the arrangement
 * `src/Kyoto/CResLoaderAddPakFileAsync.cpp` already uses for `lbl_803AFAA0`, and the relocation
 * still lands on retail's address, so the bytes would match.
 *
 * **This file is port-only.** `configure.py` does not declare it, so mwcceppc never sees it and it
 * is not a decompilation unit - the same arrangement as `src/Kyoto/CResFactoryPortVirtuals.cpp`.
 * It is **not** declared as `CResFactory::CResFactory()` any more, and that is the whole point of
 * the move: the port's real `CResFactory` constructor now lives in
 * `src/Kyoto/CResFactoryPortVirtuals.cpp` as a one-line forwarder to `fn_802FB154`, which is what
 * the two together used to be, wrongly.
 *
 * The function keeps **retail's own name** rather than a C++ one. That is not cosmetic: it is the
 * arrangement that would let a future lane make this a `Matching` unit without a DOL-wide rename,
 * because `main.cpp` is `NonMatching` and *its* object is what the DOL links, so it references
 * this function as `fn_803096C4` and a unit exporting `fn_803096C4` meets it. What still blocks
 * that is the two `.sbss` flags: a unit that claims `.text 0x803096C4-0x80309710` has to
 * reference them, and the only legal way to do that (not claiming the range) is also why the two
 * definitions below have to exist for the port.
 */
#include "types.h"

#include "dolphin/card.h"

extern bool lbl_80419B88;
extern bool lbl_80419B89;

// The port's definitions of the two flags. This translation unit is never compiled by mwcceppc, so
// a definition here cannot collide with the DOL's own: the retail objects come from dtk. Without
// them the two references are undefined and this file is a net **+1** on the link gap, because
// `tools/link_gap.py` counts a referenced-but-undefined retail global as missing - a second
// instance of the rule `tools/check_files_cmake.py` spells out, that adding a file is not
// automatically a win. Measured with the references and no definitions: 491 -> 492; with both:
// 490.
bool lbl_80419B88 = false;
bool lbl_80419B89 = false;

// The parameter is `void*` and not `CResFactory*` because the class this constructs is *not*
// `CResFactory` - it is the four bytes at `CGameGlobalObjects`+0x00 - and retail never writes it.
extern "C"
void fn_803096C4(void* self) {
  if (!lbl_80419B88) {
#if defined(__MWERKS__)
    CARDInit();
#else
    // Port: there is no memory card. The only `CARDInit` the platform's <dolphin/card.h>
    // declares is Aurora's two-argument `CARDInit(const char* game, const char* maker)`, which
    // passes both to `CardChannel::InitCard` and dereferences them; the retail no-argument one is
    // `src/Dolphin/card/CARDBios.c:633`, and `files.cmake` deliberately leaves `src/Dolphin/*.c`
    // out. Calling Aurora's with nulls would be a null dereference, not a translation, so the
    // call is dropped and the one-shot guard still flips - which is the part with behaviour.
#endif
    lbl_80419B88 = true;
  }
  lbl_80419B89 = true;
}
