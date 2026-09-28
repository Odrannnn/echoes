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
#include "MetroidPrime/Factories/CCharacterFactoryBuilder.hpp"
#include "Kyoto/TOneStatic.hpp"
#include "Kyoto/Text/CRasterFont.hpp"

class IRenderer;
class CStringTable;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

// **Retail's three unnamed member constructors, declared and called, not stubbed.** The
// constructor at 0x8000848C makes four member-constructor calls and `CGameGlobalObjects` has
// three members whose class this tree does not model. Each of the three is given the shape
// retail has - a member of the right size whose *inlined* constructor body is the one call - and
// that is what puts the `bl` at retail's address. Written the other way round (a declared-only
// constructor, defined in a port-only TU) the object gains a call to a `_ZN..C1Ev` symbol the
// DOL does not define, and a declared-only destructor additionally needs a vtable the retail
// object has no symbol for. `tools/unit_fit.sh` lists both failures when they happen.
//
//   0x800084A0  bl fn_803096C4   r3 = this + 0x00, 0x48 bytes, writes nothing to `*this`
//   0x800084B8  bl fn_80032008   r3 = this + 0x108, 0x54 bytes, writes a vptr twice and calls
//                                fn_80301008 on `this+4` - **no longer one of these**: the class
//                                is written (`MetroidPrime/Factories/CCharacterFactoryBuilder.hpp`),
//                                its constructor is declared-only, and the symbol is renamed to
//                                `__ct__24CCharacterFactoryBuilderFv`, the way `CResFactory`'s is
//   0x80008524  bl fn_801F0A44   r3 = this + 0x150, 0x30 bytes, two bytes then four words
//
// All three are **unclaimed** ranges, so `dtk dol split` leaves them in the `auto_*` objects and
// the relocations land on retail's own addresses - which is what `MetroidPrime/Player/
// CGameStateCtor.cpp` already relies on for `fn_801449C8`.
extern "C" {
void fn_803096C4(void* self);
void fn_801F0A44(void* self);
}

// +0x00, **4 bytes**. Retail's `fn_803096C4` is 0x48 bytes of frame around a one-shot `CARDInit`
// behind two `.sbss` flag bytes at 0x80419B88/0x80419B89, and it stores neither a vptr nor
// anything else into `*this`; the class is modelled as four uninitialised bytes and the call is
// the whole of the constructor. The header used to spell this `char pad0[4]`, which is why the
// constructor could not be written at all until now - and `grep pad0` still finds the member.
class CGameGlobalObjectsCardInit {
public:
  char x0_pad[4];
  CGameGlobalObjectsCardInit() { fn_803096C4(this); }
};

// +0x108, **0x28 bytes**, `CCharacterFactoryBuilder`: `MetroidPrime/Factories/
// CCharacterFactoryBuilder.hpp`. Its `addi r3,r31,264` and the two stores of `this+0x108` into
// `gpCharacterFactoryBuilder` (0x80008530/0x80008544) are what identify it, and 0xE4 + 0x24 =
// 0x108 is the second measurement of `CResFactory`'s extent. The size is load-bearing: it is the
// 0x28 that puts `gameState` at +0x130 and every member after it where retail has it. It was a
// 0x28-byte stand-in with an inline `fn_80032008(this)` constructor until the class was written;
// the constructor is now declared-only and `fn_80032008` is renamed in `symbols.txt`.

// +0x150, the last member: retail's destructor destroys it last (0x80006538) and the constructor
// publishes `this+0x150` into `lbl_80418EC8` (0x80008538/0x80008558). `fn_801F0A44` is 0x30
// bytes and writes bytes at +0/+1 and words at +4/+8/+0xC/+0x10, and `CMain::RsMain` allocates
// the whole object with `li r3,0x164` at 0x80005CC4 - so this is **0x14**, and 0x150 + 0x14 =
// 0x164 closes the layout. No name, because `symbols.txt` has none.
//
// It is retail's module manager: an `rstl::map<rstl::string, record*>` (two bytes, a count at +4,
// the three-pointer tree header at +8), walked by `fn_801F05D0` - see
// `src/MetroidPrime/PortModuleManager.cpp`. On the host the header's pointers are eight bytes,
// so the map is **0x20** there, and the tail has to be that size or the walk reads past the
// object. `PortModuleManager.cpp` checks the two sizes agree.
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

  // Declared, and never defined in this tree (retail's is 0x80006518, 264 bytes). **Declaring
  // it does not suppress the weak member-destructor copies**, whatever an earlier version of this
  // comment said: `CGameGlobalObjectsCtor.o` still carries five `W` copies -
  // `__dt__Q24rstl24single_ptr<10CGameState>Fv` and four siblings, 476 bytes - which
  // `tools/unit_fit.sh` lists as extra. They are COMDAT weak, mwldeppc discards them, and
  // `tools/flip_test.sh` PASSes with them in the object (measured, lane `cgo`), which is the CAi
  // precedent in `docs/RUNNING_THE_DECOMP.md` holding for a second unit.
  ~CGameGlobalObjects();

  void PostInitialize(COsContext&, CMemorySys&);
  void AddPaksAndFactories();
  void LoadStringTable();

  rstl::single_ptr< CGameState >& GameState() { return gameState; }

  static CRasterFont* LoadDefaultFont();

private:
  // A real 4-byte member, not a phantom pad - it has a constructor and a destructor, and its
  // constructor is what 0x800084A0 calls. (The name `pad0` is historical and kept so that
  // `grep pad0` still finds this.) It was recorded as spurious by three sessions in a row, and
  // each of them was wrong, because all three read only the *order* of the constructor's calls
  // and never the value it stores:
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
  // in front of it. The class is `CGameGlobalObjectsCardInit`, declared above.
  //
  // What *is* wrong, and what made every offset measured from `CGameGlobalObjects` read 4 too
  // high, is `CResFactory`'s own size: it was 0xE4 when retail's is **0xE0**, so `CSimplePool`
  // compiled to `this+0xE8` against retail's `this+0xE4`. `CHECK_SIZEOF(CResFactory, 0xe0)` in
  // `Kyoto/CResFactory.hpp` has the derivation. `CGameGlobalObjects::PostInitialize` is the
  // proof by score: with the factory at 0xE4 it reads 99.88%, and with the factory at 0xE0 the
  // `addi r3, r29, 228` matches retail exactly.
  CGameGlobalObjectsCardInit pad0; // CGameGlobalObjects+0x00, ctor fn_803096C4 (0x803096C4)
  CResFactory resFactory; // +0x04, 0xE0
  CSimplePool simplePool; // +0xE4, 0x24
  // The next member retail constructs is at +0x108 (`bl 80032008` at 0x800084B8) and its
  // destructor runs on +0x108 (0x800065C4); the constructor publishes it as
  // `gpCharacterFactoryBuilder` (0x80008530/0x80008544). 0xE4 + 0x24 = 0x108, which is the
  // second measurement of `CResFactory`'s extent. **This was a comment until
  // 2026-09-26**, and while it was, every member below it read 0x28 lower than retail - which
  // is why the constructor could not be written: the stores at 0x80008534-0x80008558 are at
  // +0x130/+0x134/+0x144/+0x148/+0x14C, not at the offsets the layout gave.
  CCharacterFactoryBuilder characterFactoryBuilder; // +0x108, 0x28
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
  CGameGlobalObjectsTail x150_tail; // +0x150, published into lbl_80418EC8
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
