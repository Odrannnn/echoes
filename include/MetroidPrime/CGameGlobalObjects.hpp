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
  // MEASURED BUG, deliberately unfixed. retail's CGameGlobalObjects ctor calls into
  // r31+0 FIRST and r31+4 second, so CResFactory is at +0 and CResLoader at +4 -
  // there is no pad. This line puts CResFactory at +4, which makes every offset
  // measured from it 4 too high; it is why two lanes measured CFactoryMgr 4 bytes
  // apart. Deleting it shifts every member of CGameGlobalObjects, CResFactory and
  // CResLoader, so it needs a unit-movement report like f1's rc_ptr change, not a
  // drive-by. See the adjudication at the end of docs/research/paks.md.
  char pad0[4];
  CResFactory resFactory;
  CSimplePool simplePool;
  // CCharacterFactoryBuilder characterFactoryBuilder;
  rstl::single_ptr< CGameState > gameState;
  rstl::single_ptr< CMemoryCard > memoryCard;
  rstl::optional_object< TLockedToken< CStringTable > > stringTable;
  rstl::single_ptr< IRenderer > renderer;
  rstl::single_ptr< CInGameTweakManager > inGameTweakManager;
};

#endif // _CGAMEGLOBALOBJECTS
