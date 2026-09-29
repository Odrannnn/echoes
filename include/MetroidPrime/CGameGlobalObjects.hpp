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

#include "MetroidPrime/Factories/CCharacterFactoryBuilder.hpp"

class IRenderer;
class CStringTable;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

// +0x00, **4 bytes**. Retail's constructor (0x8000848C) makes four member-constructor calls and
// publishes `this+0x150` into `lbl_80418EC8` (0x80008538/0x80008558); `CMain::RsMain` allocates
// the whole object with `li r3,0x164` and `fn_801F05D0` walks `lbl_80418EC8` every frame, so the
// last member is retail's module map and cannot be dropped. Upstream has neither this member nor
// the function. `fn_801F0A44` is 0x30 bytes and is unnamed in the map; the definition is in
// `src/MetroidPrime/CGameGlobalObjectsTailCtor.cpp`.
//
// Retail's tail is **0x14** bytes (a count byte, a byte, and the three-pointer tree header). On the
// host the pointers are eight bytes, so the map is **0x20** there and the tail has to be that size
// or `fn_801F05D0` reads past the object; `src/MetroidPrime/PortModuleManager.cpp` static_asserts
// the two agree.
extern "C" void fn_801F0A44(void* self);

// The four bytes in front of `CResFactory`, and **not** a phantom pad: it has a constructor and a
// destructor, and its constructor is what 0x800084A0 calls.
//
//   800084a0:  bl     803096c4 <fn_803096C4>     ; r3 = this + 0x00  <- the member
//   800084a4:  addi   r3,r31,4
//   800084a8:  bl     802fb154 <fn_802fb154>     ; r3 = this + 0x04  <- CResFactory
//   80008528:  addi   r0,r31,4
//   80008534:  stw    r0,-28380(r13)            ; gpResourceFactory = this + 0x04
//
// `gpResourceFactory` is the `CResFactory*` and retail stores it at `this+0x04`, so there are four
// bytes in front of it. `fn_803096C4` is 0x48 bytes of frame around a one-shot `CARDInit` and
// stores neither a vptr nor anything else into `*this`; the class is modelled as four
// uninitialised bytes and the call is the whole of the constructor. The name `pad0` is kept so
// `grep pad0` still finds the member.
extern "C" void fn_803096C4(void* self);
class CGameGlobalObjectsCardInit {
public:
  char x0_pad[4];
  CGameGlobalObjectsCardInit() { fn_803096C4(this); }
};

class CGameGlobalObjectsTail {
public:
#ifdef TARGET_PC
  void* x0_pad[4];
#else
  uchar x0_pad[0x14];
#endif
  CGameGlobalObjectsTail() { fn_801F0A44(this); }
};

class CGameGlobalObjects {
public:
  CGameGlobalObjects(COsContext&, CMemorySys&);

  void PostInitialize(COsContext&, CMemorySys&);
  void AddPaksAndFactories();
  void LoadStringTable();

  rstl::single_ptr< CGameState >& GameState() { return gameState; }
  /// Prime 1 spells this `MemoryCard()` and `CMain::MemoryCardInitializePump` is its only reader
  /// in both games. Returning the member by reference is what makes retail's
  /// `single_ptr::operator=` out of line at 0x80007B94 rather than a `delete`/`store` pair.
  rstl::single_ptr< CMemoryCard >& MemoryCard() { return memoryCard; }

  static CRasterFont* LoadDefaultFont();

private:
  // +0x00. `CGameGlobalObjectsCardInit`, not `char pad0[4]`: the four bytes are a member with a
  // constructor, and the constructor's `bl` is one of retail's four. See the class above.
  CGameGlobalObjectsCardInit pad0;
  CResFactory resFactory;  // +0x04, 0xE0
  CSimplePool simplePool;  // +0xE4, 0x24
  // +0x108, 0x28. Upstream comments this member out. Retail constructs it at +0x108
  // (`bl 80032008` at 0x800084B8), destroys it at 0x800065C4, and publishes it as
  // `gpCharacterFactoryBuilder` (0x80008530/0x80008544); its 0x28 bytes are what put `gameState` at
  // +0x130 and every member after it where retail has it. **Not `TARGET_PC`-only**: with it inside
  // that block the matching build put `gameState` at +0x108 and `stringTable` at +0x110, so
  // `CGameGlobalObjects::LoadStringTable` and `PostInitialize` read 28 bytes below retail's and
  // the constructor had nowhere to put its five stores.
  CCharacterFactoryBuilder characterFactoryBuilder;
  rstl::single_ptr< CGameState > gameState;   // +0x130
  rstl::single_ptr< CMemoryCard > memoryCard; // +0x134
  // +0x138..0x148, 0x10 bytes: the destructor reads the "engaged" flag at `this+0x144` before
  // destroying the token at +0x138, so retail's `renderer` is at +0x148, not +0x144.
  rstl::optional_object< TLockedToken< CStringTable > > stringTable; // +0x138
  rstl::single_ptr< IRenderer > renderer;                           // +0x148
  rstl::single_ptr< CInGameTweakManager > inGameTweakManager;       // +0x14C
  CGameGlobalObjectsTail x150_tail; // +0x150 on retail, published into lbl_80418EC8
};

class IController;

extern const TToken< CRasterFont >* gpDefaultFont;
extern IController* gpController;

#endif // _CGAMEGLOBALOBJECTS
