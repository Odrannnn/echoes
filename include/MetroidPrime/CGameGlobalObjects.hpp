#ifndef _CGAMEGLOBALOBJECTS
#define _CGAMEGLOBALOBJECTS

#include "types.h"

#include "rstl/optional_object.hpp"
#include "rstl/single_ptr.hpp"

#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CMemoryCardSys.hpp"
#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/CToken.hpp"
#include "Kyoto/Graphics/CGraphicsSys.hpp"
#include "Kyoto/TOneStatic.hpp"
#include "Kyoto/Text/CRasterFont.hpp"

class IRenderer;
class CStringTable;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

class CGameGlobalObjects {
public:
  CGameGlobalObjects(COsContext&, CMemorySys&);

  void PostInitialize(COsContext&, CMemorySys&);
  void AddPaksAndFactories();
  void LoadStringTable();

  rstl::single_ptr< CGameState >& GameState() { return gameState; }

  static CRasterFont* LoadDefaultFont();

private:
  // A real 4-byte member, not a phantom pad - it has a constructor and a destructor. (The name
  // `pad0` is historical and kept so that `grep pad0` still finds this.) It was recorded as
  // spurious by three sessions in a row, and each of them was wrong, because all three read only
  // the *order* of the constructor's calls and never the value it stores:
  //
  //   800084a0:  bl     803096c4 <fn_803096C4>     ; r3 = this + 0x00  <- the member
  //   800084a4:  addi   r3,r31,4
  //   800084a8:  bl     802fb154 <fn_802FB154>     ; r3 = this + 0x04  <- CResFactory
  //   80008528:  addi   r0,r31,4
  //   80008534:  stw    r0,-28380(r13)            ; gpResourceFactory = this + 0x04
  //
  // `gpResourceFactory` is the `CResFactory*` (36 registrations address `CFactoryMgr` as
  // `gpResourceFactory`+0x74, and `fn_802FB154` builds `+0x04` and `+0x74` in itself), and
  // retail stores it at **`this+0x04`**. So `CResFactory` is at +0x04 and there are four bytes
  // in front of it. `fn_803096C4` never writes `*this` - it is a one-shot `CARDInit` behind two
  // `.sbss` flag bytes - so the class is a vptr and nothing else, and it has no name here.
  //
  // What *is* wrong, and what made every offset measured from `CGameGlobalObjects` read 4 too
  // high, is `CResFactory`'s own size: it was 0xE4 when retail's is **0xE0**, so `CSimplePool`
  // compiled to `this+0xE8` against retail's `this+0xE4`. `CHECK_SIZEOF(CResFactory, 0xe0)` in
  // `Kyoto/CResFactory.hpp` has the derivation. `CGameGlobalObjects::PostInitialize` is the
  // proof by score: with the factory at 0xE4 it reads 99.88%, and with the factory at 0xE0 the
  // `addi r3, r29, 228` matches retail exactly.
  char pad0[4]; // CGameGlobalObjects+0x00, ctor fn_803096C4 (0x803096C4), dtor fn_80309660
  CResFactory resFactory; // +0x04, 0xE0
  CSimplePool simplePool; // +0xE4, 0x24
  // The next member retail constructs is at +0x108 (`bl 80032008` at 0x800084B8) and its
  // destructor runs on +0x108 (0x800065C4); the constructor publishes it as
  // `gpCharacterFactoryBuilder` (0x80008530/0x80008544). 0xE4 + 0x24 = 0x108, which is the
  // second measurement of `CResFactory`'s extent. This tree does not model the class, so
  // everything below it reads 0x28 lower than retail until it does.
  // CCharacterFactoryBuilder characterFactoryBuilder; // +0x108, 0x28
  rstl::single_ptr< CGameState > gameState; // +0x130
  rstl::single_ptr< CMemoryCard > memoryCard; // +0x134
  // +0x138..0x148, **0x10** bytes: the destructor reads the "engaged" flag at `this+0x144`
  // (`lbz r0,324(r30)` at 0x80006580) before destroying the token at +0x138, and the
  // constructor clears that same byte (0x80008500). Retail's `renderer` is therefore at +0x148,
  // not +0x144 - `PostInitialize` stores `AllocateRenderer`'s return there (0x80008460) and
  // `lwz r0,328(r29)` reads it back.
  rstl::optional_object< TLockedToken< CStringTable > > stringTable; // +0x138, 0x10
  rstl::single_ptr< IRenderer > renderer; // +0x148
  rstl::single_ptr< CInGameTweakManager > inGameTweakManager; // +0x14C
  // Retail has at least one more member: the destructor destroys +0x150 (0x80006538) last, and
  // the constructor publishes `this+0x150` into `lbl_80418EC8` (0x80008538/0x80008558).
};
// `resFactory` is 0xE0 and `simplePool` is 0x24, so with the four bytes of `pad0` in front of it
// `simplePool` lands at +0xE4 - which is retail's, three ways over (see `Kyoto/CResFactory.hpp`).
//
// **There is deliberately no `CHECK_OFFSETOF` or `NESTED_CHECK_SIZEOF` here, and that is a
// toolchain limit worth knowing.** mwcceppc 2.7 refuses to name a *private* member outside the
// class: `offsetof` gives "illegal access to protected/private member" and a nested
// `check_sizeof<CGameGlobalObjects::resFactory, ...>` gives "declaration syntax error". Every
// live `NESTED_CHECK_SIZEOF` in the tree names a **public** member (`CTweakValue::Audio`,
// `CPakFile::SResInfo`, `CStringTable::SReloadData`), and `MetroidPrime/CStateManager.hpp`'s two
// `CHECK_OFFSETOF`s are commented out for the same reason. So **a class whose members are all
// `private:` cannot carry a compile-time size or offset check in this project at all** - which is
// a large part of why the defects in this file went unnoticed for three sessions.
//
// The gate's per-function report is the instrument instead, and it is a one-run measurement:
// with the pad deleted, `CGameGlobalObjects::PostInitialize` drops 99.88 -> 98.37 because the
// fourth argument to `AllocateRenderer` becomes `mr r6,r29` against retail's `addi r6,r29,4`.
//
// Left commented rather than written, because writing either does not compile:
//
// NESTED_CHECK_SIZEOF(CGameGlobalObjects, resFactory, 0xe0);
// NESTED_CHECK_SIZEOF(CGameGlobalObjects, simplePool, 0x24);
// CHECK_OFFSETOF(CGameGlobalObjects, resFactory, 0x4);
// CHECK_OFFSETOF(CGameGlobalObjects, simplePool, 0xe4);

#endif // _CGAMEGLOBALOBJECTS
