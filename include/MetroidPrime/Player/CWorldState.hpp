#ifndef _CWORLDSTATE
#define _CWORLDSTATE

#include "MetroidPrime/CMapWorldInfo.hpp"
#include "rstl/rc_ptr.hpp"

class CRelayTracker;
class CWorldLayerState;
class CBitStreamReader;
class CBitStreamWriter;
class CWorldState;

// Retail's `CWorldState` copy, 0x80142760, unnamed in the symbol table and so claimable only
// under an `extern "C"` name - see `CHintOptions.hpp:15-20`, which records why the declaration has
// C linkage and sits **before** the class: befriending it first would declare it with C++ linkage
// and mwcceppc would emit `fn_80142760__F...` instead, leaving retail's 124 bytes unclaimed. The
// return type is `CWorldState*` because this is a copy constructor, which returns `this`, and
// `CGameState.cpp` records why returning it is also what the register allocation needs.
extern "C" CWorldState* fn_80142760(void* elem, const void* src);

class CWorldState {
public:
  explicit CWorldState(CAssetId worldId);
  CWorldState(CBitStreamReader& in, CAssetId worldId, const CWorldSaveGameInfo& saveWorld);
  ~CWorldState();

  void PutTo(CBitStreamWriter& out, const CWorldSaveGameInfo& saveWorld) const;
  CAssetId GetWorldAssetId() const;
  TAreaId GetCurrentArea() const;
  void SetAreaId(TAreaId areaId);
  CAssetId GetDesiredAreaAssetId() const;
  void SetDesiredAreaAssetId(CAssetId areaId);
  rstl::rc_ptr< CMapWorldInfo > GetMapWorldInfo() const;
  rstl::ncrc_ptr< CMapWorldInfo >& MapWorldInfo();
  rstl::ncrc_ptr< CWorldLayerState >& GetLayerState();
  rstl::ncrc_ptr< CRelayTracker >& RelayTracker(); // Guessed name

  friend CWorldState* fn_80142760(void* elem, const void* src);

private:
  CAssetId mWorldId;
  TAreaId mAreaId;
  rstl::ncrc_ptr< CRelayTracker > mRelayTracker;
  rstl::ncrc_ptr< CMapWorldInfo > mMapWorldInfo;
  CAssetId mDesiredAreaAssetId;
  rstl::ncrc_ptr< CWorldLayerState > mLayerState;
};
CHECK_SIZEOF(CWorldState, 0x24)

#endif // _CWORLDSTATE
