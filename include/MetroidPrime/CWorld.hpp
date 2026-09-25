#ifndef _CWORLD
#define _CWORLD

#include "MetroidPrime/TGameTypes.hpp"
#include "MetroidPrime/CGameArea.hpp"

#include "rstl/auto_ptr.hpp"
#include "rstl/vector.hpp"


class CGameArea;

class CWorld {
public:
  const CGameArea& GetAreaAlways(TAreaId id) const { return *m_areas[id.Value()]; }
  CGameArea* Area(TAreaId id) { return m_areas[id.Value()].get(); }
  const CGameArea* GetArea(TAreaId id) const { return m_areas[id.Value()].get(); }
  
  bool IsAreaValid(TAreaId id) const { return m_areas[id.Value()]->IsLoaded(); }

  void SetLoadPauseState(bool);

  CGameArea::CChainIterator ChainHead() const { return CGameArea::CChainIterator(x4c_chainHead); }
  CGameArea::CConstChainIterator GetChainHead() const {
    return CGameArea::CConstChainIterator(x4c_chainHead);
  }
  static CGameArea::CConstChainIterator GetAliveAreasEnd() { return skGlobalEnd; }
  static CGameArea::CChainIterator AliveAreasEnd() { return skGlobalNonConstEnd; }
  static CGameArea::CConstChainIterator skGlobalEnd;
  static CGameArea::CChainIterator skGlobalNonConstEnd;

  static void PropogateAreaChain(CGameArea::EOcclusionState occlusionState, CGameArea* area,
                                 CWorld* world);

private:
  char pad1[0x18];
  rstl::vector< rstl::auto_ptr< CGameArea > > m_areas; // x18
  char pad28[0x24];
  CGameArea* x4c_chainHead; // the area chain CStateManager walks
};

#endif // _CWORLD
