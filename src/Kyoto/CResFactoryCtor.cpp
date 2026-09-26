#include "Kyoto/CResFactory.hpp"

#include "dolphin/card.h"

// `CResFactory::CResFactory` - retail `fn_803096C4`, `.text:0x803096C4`, `size:0x4C` = 76 bytes,
// and **unnamed in `config/G2ME01/symbols.txt`**. It is 0x4C bytes of prologue and epilogue
// around five instructions, and it does not write one byte of `*this`:
//
//   0x803096D4  mr   r31,r3                  ; this is live across the body
//   0x803096D8  lbz  r0,lbl_80419B88(r13)    ; a static "already done" flag
//   0x803096DC  cmplwi r0,0
//   0x803096E0  bne  0x803096F0
//   0x803096E4  bl   CARDInit                ; 0x803581D4, size 0xAC
//   0x803096E8  li   r0,1
//   0x803096EC  stb  r0,lbl_80419B88(r13)
//   0x803096F0  li   r0,1
//   0x803096F4  mr   r3,r31                  ; dead, and retail keeps it
//   0x803096F8  stb  r0,lbl_80419B89(r13)
//
// **Why a resource factory calls `CARDInit` is not explained by anything in the DOL**, and the
// two flags are not referenced from anywhere else - a whole-DOL scan finds exactly this one
// writer and no reader of either. The name is the port's, not retail's: the symbol is unnamed
// there, and it is the constructor the port has to have because `CGameGlobalObjects` holds a
// `CResFactory` by value and `CMain::AsyncIdle` calls through the global. So this is what
// `CResFactory::CResFactory` is, on the evidence of where retail calls it
// (`CGameGlobalObjects::CGameGlobalObjects`, 0x800084A0, with `this+0`).
//
// **A layout defect found on the way, and what it actually was (lane j4, 2026-09-26).** Retail's
// `CGameGlobalObjects::CGameGlobalObjects` (0x8000848C) calls `fn_803096C4` on `this+0x00`,
// `fn_802FB154` - *this* constructor - on `this+0x04`, `fn_80301008` on `this+0xE4` and
// `fn_80032008` on `this+0x108`, and it stores `gpResourceFactory = this+0x04` at
// 0x80008534. So `CResFactory` is at `CGameGlobalObjects`+0x04 (the four bytes in front of it are
// a real member, with a ctor and a dtor) and `CResLoader` is at +0x08. `CResLoader` is **0x70**
// bytes, `CFactoryMgr` is at **`CResFactory`+0x74** - the 36 registrations' `addi r3, r31, 116`
// with `r31` = `gpResourceFactory` - and `CResFactory` is **0xE0**, so `CSimplePool` is at +0xE4.
// `include/MetroidPrime/CGameGlobalObjects.hpp`'s `char pad0[4]` is right; what was wrong, and is
// now fixed, is that this file's header modelled the factory as 0xE4, which put `CSimplePool` at
// +0xE8 and every later member 4 too high. `CHECK_SIZEOF(CResFactory, 0xe0)` and
// `uchar xac_[0x34]` are the landed state.
//
// Two things this file got wrong before that, both recorded so they are not repeated: it called
// `fn_803096C4` this constructor, and it called the 0xE0 size here a
// `CHECK_SIZEOF` *confirmation*. **`CHECK_SIZEOF` never measures a size** - it only checks that
// a model agrees with itself, so `0xe4` passed and so would `0xd0`.
//
// Two retail data objects come with it, both one byte, both adjacent:
// `.sbss:0x80419B88` (`lbl_80419B88`) and `.sbss:0x80419B89` (`lbl_80419B89`). They are left
// unclaimed on purpose: dtk gives a claimed range's symbols to *this* object and takes them away
// from the `auto_*` unit that has them, and `lbl_80419B8A` at 0x80419B8A is referenced from
// `CStateManager.o` and `auto_03_801724CC_text.o`, so claiming 0x80419B88..0x80419B8A breaks the
// link with `undefined: 'lbl_80419B8A'`. Referring to them without claiming is the arrangement
// `src/Kyoto/CResLoaderAddPakFileAsync.cpp` already uses for `lbl_803AFAA0`, and the relocation
// still lands on retail's address, so the bytes would match.
//
// **This file is port-only.** `configure.py` does not declare it, so mwcceppc never sees it and it
// is not a decompilation unit - the same arrangement as `src/MetroidPrime/PortGlobals.cpp`. Two
// attempts to make it a `Matching` unit were measured and both fail, for reasons worth keeping:
///
///  * Claiming `.text 0x803096C4-0x80309710` makes dtk's retail `main.o` - `main.cpp` is
///    **NonMatching**, so *its* object is what the DOL links - reference the symbol by its
///    unnamed name `fn_803096C4`, and this object exports `__ct__11CResFactoryFv`. Making them
///    meet needs `fn_803096C4` renamed in `config/G2ME01/symbols.txt`, a DOL-wide change this
///    file should not make on its own. (The direction is already the one the tree is going:
///    `AsyncIdle__11CResFactoryFUib` beside it is a rename.)
///  * Even with the rename, the object emits `__dt__11CResFactoryFv`, `__dt__8IFactoryFv`,
///    `__dt__Q24rstl…map<Ui, …>::Fv` and the CResFactory vtable, because `CResFactory` has a
///    virtual destructor and this is the first unit to define its constructor. Those are
///    unclaimed bytes that would land in the DOL and break the hash.
///
/// It is still worth having: it closes `_ZN11CResFactoryC1Ev` from the port's link gap, and the
/// net is **-1** rather than 0 because `CARDInit` is used in Aurora's own `lib/dolphin/card.cpp`
/// and so is never counted as missing.
extern bool lbl_80419B88;
extern bool lbl_80419B89;

// The port's definitions of the two flags. This translation unit is never compiled by
// mwcceppc, so a definition here cannot collide with the DOL's own: the retail objects come
// from dtk. Without them the two references are undefined and the ctor is a net **+1** on the
// link gap, because `link_gap.py` counts a referenced-but-undefined retail global as missing - a
// second instance of the rule `tools/check_files_cmake.py` spells out, that adding a file is not
// automatically a win. Measured: 491 -> 492 with the references and no definitions, 490 with
// both.
bool lbl_80419B88 = false;
bool lbl_80419B89 = false;

CResFactory::CResFactory() {
  if (!lbl_80419B88) {
#if defined(__MWERKS__)
    CARDInit();
#else
    // Port: there is no memory card. The only `CARDInit` the platform's
    // <dolphin/card.h> declares is Aurora's two-argument `CARDInit(const char* game,
    // const char* maker)`, which passes both to `CardChannel::InitCard` and dereferences
    // them; the retail no-argument one is `src/Dolphin/card/CARDBios.c:633`, and
    // `files.cmake` deliberately leaves `src/Dolphin/*.c` out. Calling Aurora's with
    // nulls would be a null dereference, not a translation, so the call is dropped and
    // the one-shot guard still flips - which is the part with behaviour.
#endif
    lbl_80419B88 = true;
  }
  lbl_80419B89 = true;
}
