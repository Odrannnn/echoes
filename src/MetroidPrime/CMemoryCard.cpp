#include "MetroidPrime/CMemoryCard.hpp"

#include "MetroidPrime/CDummyWorld.hpp"
#include "MetroidPrime/CWorldLayerState.hpp"
#include "MetroidPrime/IGameArea.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CWorldState.hpp"

#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Text/CStringTable.hpp"
#include "rstl/algorithm.hpp"

// **The other 43 functions in this unit's retail range are named `fn_8017*` in
// `config/G2ME01/symbols.txt`, and 43 of them are already declared here in this file's own
// translation unit - they are this object's weak `rstl` template instantiations and free helper
// templates, which `dtk` cannot name.** objdiff pairs functions by symbol name, so they scored
// 0.00% each even where our body is byte-identical to retail's. Renaming the retail symbol to the
// mangled name mwcceppc emits - read out of `build/G2ME01/src/MetroidPrime/CMemoryCard.o` with
// `powerpc-eabi-nm`, never guessed - is what lets objdiff pair them; see "Pairing a function the
// retail symbol table has no name for" in `docs/RUNNING_THE_DECOMP.md`. It enables pairing, not
// matching: `build/report.json` reports 53 of 56, and every one of those 43 is 100.00%, with the
// DOL sha1 and all 86 RELs unchanged.
//
// The constructor's `worlds.push_back_unsafe(...)` is load-bearing too. Retail's 56-byte
// `fn_80178270` is `vector<CSaveWorldIntermediate>::push_back_unsafe` verbatim - no capacity check,
// `construct(mItems + mCount++, x)` and nothing else - and `worlds.reserve(40)` above already
// guarantees the room. Spelled `push_back`, this file instead emitted an out-of-line 124-byte
// `push_back` that calls `reserve`, which is 124 bytes retail's unit object does not have.
//
// Still unmatched, and what stops each:
//   `fn_80178AD0` (0x7C) - no body in this object at all: it builds a `rstl::string` from a
//     `CInputStream&` into a frame temporary and then reads three `uint` into +0x10/+0x14/+0x18
//     of `this`, and nothing in this file needs such a function.
//   `GetAreaAndWorldIdForSaveId` (90.45%) and `MergeEnvironmentVariables` (86.20%) - the diffs are
//     register allocation and slot assignment inside the loops, nothing structural.
//
// This unit is still not a `Matching` candidate: `tools/unit_fit.sh` reports `.rodata` 18 bytes
// against retail's 24 and `.sdata2` 0 against 8 (the `kInvalidAssetId` word retail materialises
// here), and ~4.5 KB of template instantiations this TU emits weakly but retail resolves elsewhere.
//
// **Do not declare these in an `extern "C"` block under the `fn_` name**: mwcceppc emits its own
// copy of the instantiation under the mangled name and the wrapper beside it, and objdiff then
// pairs the wrapper (which is 32 or 56 bytes of thunk) against retail's real body.
//
// The four `areas*` locals in `GetAreaAndWorldIdForSaveId` below are load-bearing: hoisting
// `areas.end()` into one **before** the `rstl::find` call is 196 bytes and 90.45%, the same
// search written as a hand-rolled `for` loop is 61.37%, and taking `end()` inline at the
// comparison instead of before the search drops it to 87.08%.

CMemoryCard::CMemoryCard() : mHints(gpSimplePool->GetObj("HINT_Hints")) {
  mHints.Lock();
  mWorldInter = rs_new rstl::vector< CSaveWorldIntermediate >;
  rstl::vector< CSaveWorldIntermediate >& worlds = *mWorldInter;
  mMemoryWorlds.reserve(40);
  worlds.reserve(40);

  const rstl::vector< rstl::pair< rstl::string, SObjectTag > > resources =
      gpResourceFactory->GetResourceIdToNameList();
  for (rstl::vector< rstl::pair< rstl::string, SObjectTag > >::const_iterator it =
           resources.begin();
       it != resources.end(); ++it) {
    const CAssetId worldId = it->second.id;
    if (gpResourceFactory->GetResourceTypeById(worldId) != 'MLVL') {
      continue;
    }

    rstl::vector< MemoryWorld >::iterator existing =
        rstl::lower_bound(mMemoryWorlds.begin(), mMemoryWorlds.end(), worldId,
                          rstl::default_pair_sorter_finder< rstl::vector< MemoryWorld > >());
    if (existing == mMemoryWorlds.end() || existing->first != worldId) {
      mMemoryWorlds.insert(existing, MemoryWorld(worldId, CSaveWorldMemory()));
      worlds.push_back_unsafe(CSaveWorldIntermediate(worldId, kInvalidAssetId));
    }
  }
}

CMemoryCard::~CMemoryCard() {}

CSaveWorldIntermediate::CSaveWorldIntermediate(CAssetId mlvlId, CAssetId savwId) {
  if (savwId == kInvalidAssetId) {
    mDummyWorld = rs_new CDummyWorld(mlvlId, false);
  } else {
    mSaveWorld =
        rs_new TCachedToken< CWorldSaveGameInfo >(gpSimplePool->GetObj(SObjectTag('SAVW', savwId)));
    mSaveWorld->Lock();
  }

  mMlvlId = mlvlId;
  mWorldNameId = kInvalidAssetId;
  mDarkWorldNameId = kInvalidAssetId;
  mSaveWorldId = savwId;
}

bool CSaveWorldIntermediate::InitializePump() {
  if (!mDummyWorld.null()) {
    if (mDummyWorld->ICheckWorldComplete()) {
      CDummyWorld* dummyWorld = mDummyWorld.get();
      IWorld& world = *dummyWorld;
      mWorldNameId = dummyWorld->IGetStringTableAssetId();
      mDarkWorldNameId = world.IGetDarkStringTableAssetId();
      mSaveWorldId = world.IGetSaveWorldAssetId();
      int areaCount = world.IGetAreaCount();
      mAreaIds.reserve(areaCount);
      for (int i = 0; i < areaCount; ++i) {
        mAreaIds.push_back_unsafe(world.IGetAreaAlways(TAreaId(i))->IGetAreaSaveId());
      }
      CWorldState& state = gpGameState->StateForWorld(world.IGetWorldAssetId());
      CWorldLayerState& layerState = *state.GetLayerState();
      mDefaultLayerStates = layerState.GetAreaLayers();
      mLayerNames = layerState.GetLayerNames();
      mAreaLayerNameOffsets = layerState.GetLayerNameOffsets();
      if (mSaveWorldId != kInvalidAssetId) {
        mSaveWorld = rs_new TCachedToken< CWorldSaveGameInfo >(
            gpSimplePool->GetObj(SObjectTag('SAVW', mSaveWorldId)));
        mSaveWorld->Lock();
      }
      mDummyWorld = nullptr;
    }
  } else {
    if (!mSaveWorld.null()) {
      if (mSaveWorld->IsLoaded()) {
        return true;
      }
    } else {
      return true;
    }
  }
  return false;
}

bool CMemoryCard::InitializePump() {
  if (mWorldInter.null()) {
    for (rstl::vector< MemoryWorld >::iterator it = mMemoryWorlds.begin();
         it != mMemoryWorlds.end(); ++it) {
      CSaveWorldMemory& memory = it->second;
      if (memory.mWorldName.valid() && !memory.mWorldName->IsLoaded()) {
        return false;
      }
      if (memory.mDarkWorldName.valid() && !memory.mDarkWorldName->IsLoaded()) {
        return false;
      }
    }
    return mHints.IsLoaded();
  }

  bool done = true;
  rstl::vector< CSaveWorldIntermediate >& worlds = *mWorldInter;
  for (rstl::vector< CSaveWorldIntermediate >::iterator it = worlds.begin(); it != worlds.end();
       ++it) {
    CSaveWorldIntermediate& world = *it;
    if (world.InitializePump()) {
      if (world.mSaveWorld.null()) {
        continue;
      }

      CSaveWorldMemory& memory = const_cast< CSaveWorldMemory& >(GetSaveWorldMemory(world.mMlvlId));
      if (memory.mSaveWorldId == kInvalidAssetId) {
        memory.mSaveWorldId = world.mSaveWorldId;
      }
      if (memory.mWorldNameId == kInvalidAssetId) {
        memory.mWorldNameId = world.mWorldNameId;
      }
      if (memory.mDarkWorldNameId == kInvalidAssetId) {
        memory.mDarkWorldNameId = world.mDarkWorldNameId;
      }
      memory.mAreaIds = world.mAreaIds;
      memory.mDefaultLayerStates = world.mDefaultLayerStates;
      memory.mLayerNames = world.mLayerNames;
      memory.mAreaLayerNameOffsets = world.mAreaLayerNameOffsets;

      const CWorldSaveGameInfo& saveInfo = *world.mSaveWorld->GetObject();
      memory.mAreaCount = saveInfo.GetAreaCount();
      mScanStates.reserve(mScanStates.size() + saveInfo.GetScans().size());
      for (rstl::vector< ScanState >::const_iterator scan = saveInfo.GetScans().begin();
           scan != saveInfo.GetScans().end(); ++scan) {
        if (rstl::find(mScanStates.begin(), mScanStates.end(), *scan) == mScanStates.end()) {
          mScanStates.push_back_unsafe(*scan);
        }
      }
      MergeEnvironmentVariables(saveInfo.GetSystemVariables(), mSystemVariables);
      MergeEnvironmentVariables(saveInfo.GetGameVariables(), mGameVariables);

      memory.mSaveWorld = *world.mSaveWorld;
      world.mSaveWorld = nullptr;

      const SObjectTag worldName('STRG', memory.mWorldNameId);
      if (gpResourceFactory->CanBuild(worldName)) {
        memory.mWorldName = TCachedToken< CStringTable >(gpSimplePool->GetObj(worldName));
        memory.mWorldName->Lock();
      }
      const SObjectTag darkWorldName('STRG', memory.mDarkWorldNameId);
      if (gpResourceFactory->CanBuild(darkWorldName)) {
        memory.mDarkWorldName = TCachedToken< CStringTable >(gpSimplePool->GetObj(darkWorldName));
        memory.mDarkWorldName->Lock();
      }
    } else {
      done = false;
    }
  }

  if (done) {
    mWorldInter = nullptr;
    rstl::sort_by_key(mScanStates);
  }
  return false;
}

bool CMemoryCard::HasSaveWorldMemory(CAssetId worldId) const {
  rstl::vector< MemoryWorld >::const_iterator it = rstl::find_by_key(mMemoryWorlds, worldId);
  return it != mMemoryWorlds.end();
}

const CSaveWorldMemory& CMemoryCard::GetSaveWorldMemory(CAssetId worldId) const {
  return rstl::find_by_key(mMemoryWorlds, worldId)->second;
}

const wchar_t* CSaveWorldMemory::GetFrontEndName() const {
  if (mWorldName.valid() && mWorldName->GetObject() != nullptr) {
    const CStringTable& names = *mWorldName->GetObject();
    if (names.GetStringCount() >= 4) {
      return names.GetString(3);
    }
    return names.GetString(0);
  }
  return nullptr;
}

const wchar_t* CSaveWorldMemory::GetDarkFrontEndName() const {
  if (mDarkWorldName.valid() && mDarkWorldName->GetObject() != nullptr) {
    const CStringTable& names = *mDarkWorldName->GetObject();
    if (names.GetStringCount() >= 4) {
      return names.GetString(3);
    }
    return names.GetString(0);
  }
  return nullptr;
}

rstl::pair< CAssetId, TAreaId > CMemoryCard::GetAreaAndWorldIdForSaveId(uint saveId) const {
  for (rstl::vector< MemoryWorld >::const_iterator it = mMemoryWorlds.begin();
       it != mMemoryWorlds.end(); ++it) {
    const rstl::vector< uint >& areas = it->second.mAreaIds;
    rstl::vector< uint >::const_iterator areasEnd = areas.end();
    rstl::vector< uint >::const_iterator area = rstl::find(areas.begin(), areasEnd, saveId);
    if (area != areasEnd) {
      return rstl::pair< CAssetId, TAreaId >(it->first, TAreaId(area - areas.begin()));
    }
  }
  return rstl::pair< CAssetId, TAreaId >(kInvalidAssetId, kInvalidAreaId);
}

void CMemoryCard::MergeEnvironmentVariables(const rstl::vector< EnvironmentVariable >& source,
                                            rstl::vector< EnvironmentVariable >& destination) {
  destination.reserve(destination.size() + source.size());
  for (rstl::vector< EnvironmentVariable >::const_iterator it = source.begin(); it != source.end();
       ++it) {
    if (rstl::find(destination.begin(), destination.end(), *it) == destination.end()) {
      destination.push_back_unsafe(*it);
    }
  }
}

bool CWorldSaveGameInfo::SEnvironmentVariable::operator==(const SEnvironmentVariable& other) const {
  return mName == other.mName;
}
