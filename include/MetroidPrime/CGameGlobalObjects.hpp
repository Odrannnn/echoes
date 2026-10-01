#ifndef _CGAMEGLOBALOBJECTS
#define _CGAMEGLOBALOBJECTS

#include "types.h"

#include "rstl/optional_object.hpp"
#include "rstl/single_ptr.hpp"

#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/CMemoryCardSys.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/CToken.hpp"
#include "Kyoto/TOneStatic.hpp"
#include "Kyoto/Text/CRasterFont.hpp"
#include "MetroidPrime/CRELFileManager.hpp"
#include "MetroidPrime/Factories/CCharacterFactoryBuilder.hpp"

#include "MetroidPrime/Factories/CCharacterFactoryBuilder.hpp"

class IRenderer;
class CStringTable;
class CGameState;
class CMemoryCard;
class CInGameTweakManager;

// **`TOneStatic<CGameGlobalObjects>` is a base, and retail says so twice**: `CMain::RsMain` calls
// `0x80008AD4` (`TOneStatic<CGameGlobalObjects>::operator new`) with `li r3,356` and then the
// constructor with no allocation in between, and `~CGameGlobalObjects` (`0x80006518`) ends in a
// call to `0x80008B04`, the matching `operator delete`.
//
// The members at +0x00 and +0x150 are upstream's `CMemoryCardSys` (constructor `0x803096C4`, a
// one-shot `CARDInit`) and `CRELFileManager` (constructor `0x801F0A44`, published as
// `gpRelFileManager`) since the eighth upstream sync. Before it this tree modelled them as
// `CGameGlobalObjectsCardInit pad0` and `CGameGlobalObjectsTail x150_tail`.
class CGameGlobalObjects : public TOneStatic< CGameGlobalObjects > {
public:
  CGameGlobalObjects(COsContext&, CMemorySys&);
  ~CGameGlobalObjects();

  void PostInitialize(COsContext&, CMemorySys&);
  void AddPaksAndFactories(COsContext& context);
  void LoadStringTable();

  rstl::single_ptr< CGameState >& GameState() { return mGameState; }
  /// Prime 1 spells this `MemoryCard()` and `CMain::MemoryCardInitializePump` is its only reader
  /// in both games. Returning the member by reference is what makes retail's
  /// `single_ptr::operator=` out of line at 0x80007B94 rather than a `delete`/`store` pair.
  rstl::single_ptr< CMemoryCard >& MemoryCard() { return mMemoryCard; }

  static CRasterFont* LoadDefaultFont();

  // **`public` rather than upstream's `private`, and only so `src/MetroidPrime/main.cpp` can
  // write retail's own teardown.** Retail's `~CGameGlobalObjects` (0x80006518) destroys all ten
  // members one at a time from outside the class, which no C++ can spell for a class with private
  // members, and the spelling that reproduces its bytes is in that file under the block for
  // 0x800064D0. A `friend` is not the alternative: mwcceppc appends the parameter encoding to any
  // function a class declares as a friend, so objdiff would not pair the symbol (measured).
public:
  CMemoryCardSys mMemoryCardSys;                      // +0x00
  CResFactory mResFactory;                            // +0x04, 0xE0
  CSimplePool mSimplePool;                            // +0xE4, 0x24
  CCharacterFactoryBuilder mCharacterFactoryBuilder;  // +0x108, 0x28
  rstl::single_ptr< CGameState > mGameState;          // +0x130
  rstl::single_ptr< CMemoryCard > mMemoryCard;        // +0x134
  // +0x138..0x148, 0x10 bytes: the destructor reads the "engaged" flag at `this+0x144` before
  // destroying the token at +0x138, so retail's `mRenderer` is at +0x148, not +0x144.
  rstl::optional_object< TLockedToken< CStringTable > > mStringTable; // +0x138
  rstl::single_ptr< IRenderer > mRenderer;                           // +0x148
  rstl::single_ptr< CInGameTweakManager > mInGameTweakManager;       // +0x14C
  CRELFileManager mRelFileManager; // +0x150 on retail, published as gpRelFileManager
};
CHECK_SIZEOF(CGameGlobalObjects, 0x164)

class IController;

extern const TToken< CRasterFont >* gpDefaultFont;
extern IController* gpController;

#endif // _CGAMEGLOBALOBJECTS
