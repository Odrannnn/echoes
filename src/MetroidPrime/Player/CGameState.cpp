#include "MetroidPrime/Player/CGameState.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"
#include "Kyoto/Streams/CMemoryStreamOut.hpp"
#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CRelayTracker.hpp"
#include "MetroidPrime/CWorldLayerState.hpp"
#include "MetroidPrime/Player/CGMCoin.hpp"
#include "MetroidPrime/Player/CGMDeathMatch.hpp"
#include "MetroidPrime/Player/CGMFrontEnd.hpp"
#include "MetroidPrime/Player/CGMSinglePlayer.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
#include "MetroidPrime/Player/CGameMode.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Player/CWorldTransManager.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"

#include "dolphin/os.h"
#include "rstl/math.hpp"

#include <stdio.h>
#include <string.h>

// The 16-byte SGameStateBlock helpers (see CGameStateBlocks.hpp). Defined below in retail order,
// ported from the pre-sync carves (63aba15); fn_801465EC (reserve) and fn_80004D5C (copy) live
// elsewhere.
extern "C" void fn_80004D5C(SGameStateBlock* self, const SGameStateBlock* src);
extern "C" void fn_80142914(SGameStateBlock* self);
extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src);
extern "C" void fn_801465EC(SGameStateBlock* self, int size);

// Guessed name. Layer-name prefixes select which game mode owns each layer.
//
// **Declared here, immediately before its only user, and not at the top of the file.** The three
// literals land in this unit's own `.rodata` pool, and a literal's offset is part of the
// instruction that loads it, so the pool order is part of the match. Retail's pool puts
// `"InitialWorld"` at +0x07 and `"Samus01"`/`"Coins"` at +0x1B8/+0x1C0, so `fn_80143E88` is
// retail's *first* user of a string literal and this table is nearly its last. Moving the table
// down the file is what reproduces that: retail's `__sinit_CGameState_cpp` (0x80146874) writes
// the table at runtime out of `lbl_803A9208 + 42 / +440 / +448` - the same three offsets, and
// +42 is shared with `fn_80143E88`'s own `"Deathmatch"`.

uint CEnvironmentVariable::GetBitCount(uint value) {
  uint count = 0;
  for (; value != 0; value >>= 1) {
    ++count;
  }
  return count;
}

CEnvironmentVariable::CEnvironmentVariable(int minimum, int maximum, int value)
: mMin(minimum), mMax(maximum), mValue(value) {
  ClampToMinMax();
}

CEnvironmentVariable::CEnvironmentVariable(int minimum, int maximum, CBitStreamReader& in)
: mMin(minimum), mMax(maximum), mValue(minimum + in.ReadBits(GetBitCount(maximum - minimum))) {
  ClampToMinMax();
}

void CEnvironmentVariable::PutTo(CBitStreamWriter& out) const {
  out.WriteBits(mValue - mMin, GetBitCount(mMax - mMin));
}

void CEnvironmentVariable::Set(int value) {
  mValue = value;
  ClampToMinMax();
}

void CEnvironmentVariable::ClampToMinMax() {
  if (mValue < mMin || mValue > mMax) {
    mValue = CMath::Clamp(mMin, mValue, mMax);
  }
}

CGameStateEnvVarManager::CGameStateEnvVarManager(EVariableScope scope) : mScope(scope) {
  LoadFields();
}

CGameStateEnvVarManager::CGameStateEnvVarManager(EVariableScope scope, CBitStreamReader& in)
: mScope(scope) {
  LoadFields();
  InitializeMemoryState();
  for (rstl::map< rstl::string, CEnvironmentVariable >::iterator it = mVariables.begin();
       it != mVariables.end(); ++it) {
    it->second = CEnvironmentVariable(it->second.GetMinimum(), it->second.GetMaximum(), in);
  }
}

CEnvironmentVariable* CGameStateEnvVarManager::FindEnvironmentVariable(const char* name) {
  rstl::map< rstl::string, CEnvironmentVariable >::iterator it =
      mVariables.find(rstl::string_l(name));
  return it == mVariables.end() ? nullptr : &it->second;
}

void CGameStateEnvVarManager::AddVariable(const rstl::string& name,
                                          const CEnvironmentVariable& variable) {
  if (mVariables.find(name) == mVariables.end()) {
    mVariables.insert(rstl::pair< rstl::string, CEnvironmentVariable >(name, variable));
  }
}

void CGameStateEnvVarManager::InitializeMemoryState() {
  const rstl::vector< CMemoryCard::EnvironmentVariable >& variables =
      mScope == kVS_System ? gpMemoryCard->GetSystemVariables() : gpMemoryCard->GetGameVariables();
  for (rstl::vector< CMemoryCard::EnvironmentVariable >::const_iterator it = variables.begin();
       it != variables.end(); ++it) {
    AddVariable(it->mName, CEnvironmentVariable(it->mMinimum, it->mMaximum, it->mDefaultValue));
  }
}

void CGameStateEnvVarManager::PutTo(CBitStreamWriter& out) const {
  for (rstl::map< rstl::string, CEnvironmentVariable >::const_iterator it = mVariables.begin();
       it != mVariables.end(); ++it) {
    it->second.PutTo(out);
  }
}

CPersistentOptions::CPersistentOptions() : CGameStateEnvVarManager(kVS_System), mSaveIdx(0) {
  if (gpMemoryCard != nullptr) {
    InitializeMemoryState();
  }
}

CPersistentOptions::CPersistentOptions(CBitStreamReader& in)
: CGameStateEnvVarManager(kVS_Game), mSaveIdx(0) {
  in.ReadBits(32); // SYST
  mSaveIdx = in.ReadBits(2);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  int cinematicCount = 0;
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    cinematicCount += saveWorld->GetCinematicCount();
  }

  rstl::vector< bool > cinematicStates(cinematicCount, false);
  for (int i = 0; i < cinematicCount; ++i) {
    cinematicStates[i] = in.ReadPackedBool();
  }

  int stateIdx = 0;
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    const rstl::vector< TEditorId >& cinematics = saveWorld->GetCinematics();
    for (int i = 0; i < cinematics.size(); ++i) {
      if (cinematicStates[stateIdx]) {
        SetCinematicState(rstl::pair< CAssetId, TEditorId >(it->first, cinematics[i]), true);
      }
      ++stateIdx;
    }
  }

  InitializeMemoryState();
  CGameStateEnvVarManager::operator=(CGameStateEnvVarManager(kVS_System, in));
  in.ReadBits(32); // SYND
}

void CPersistentOptions::InitializeMemoryState() {
  CGameStateEnvVarManager::InitializeMemoryState();
}

void CPersistentOptions::PutTo(CBitStreamWriter& out) const {
  out.WriteBits('SYST', 32);
  out.WriteBits(mSaveIdx, 2);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  int cinematicCount = 0;
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    cinematicCount += saveWorld->GetCinematicCount();
  }

  rstl::vector< bool > cinematicStates;
  cinematicStates.reserve(cinematicCount);
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    for (int i = 0; i < saveWorld->GetCinematicCount(); ++i) {
      cinematicStates.push_back(GetCinematicState(
          rstl::pair< CAssetId, TEditorId >(it->first, saveWorld->GetCinematics()[i])));
    }
  }
  for (int i = 0; i < cinematicCount; ++i) {
    out.WriteBits(cinematicStates[i] ? 1 : 0, 1);
  }

  CGameStateEnvVarManager::PutTo(out);
  out.WriteBits('SYND', 32);
}

CWorldState::CWorldState(CAssetId worldId)
: mWorldId(worldId)
, mAreaId(0)
, mRelayTracker(rs_new CRelayTracker)
, mMapWorldInfo(rs_new CMapWorldInfo)
, mDesiredAreaAssetId(kInvalidAssetId)
, mLayerState(rs_new CWorldLayerState) {}

CWorldState::CWorldState(CBitStreamReader& in, CAssetId worldId,
                         const CWorldSaveGameInfo& saveWorld)
: mWorldId(worldId)
, mAreaId(kInvalidAreaId)
, mRelayTracker(nullptr)
, mMapWorldInfo(nullptr)
, mDesiredAreaAssetId(kInvalidAssetId)
, mLayerState(nullptr) {
  mAreaId = TAreaId(in.ReadBits(32));
  mDesiredAreaAssetId = in.ReadBits(32);
  mRelayTracker = rs_new CRelayTracker(in, saveWorld);
  mMapWorldInfo = rs_new CMapWorldInfo(in, saveWorld, mWorldId);
  mLayerState = rs_new CWorldLayerState(in);
}

void CWorldState::PutTo(CBitStreamWriter& out, const CWorldSaveGameInfo& saveWorld) const {
  out.WriteBits(mAreaId.Value(), 32);
  out.WriteBits(mDesiredAreaAssetId, 32);
  mRelayTracker->PutTo(out, saveWorld);
  mMapWorldInfo->PutTo(out, saveWorld, mWorldId);
  mLayerState->PutTo(out);
}

CAssetId CWorldState::GetWorldAssetId() const { return mWorldId; }

rstl::ncrc_ptr< CRelayTracker >& CWorldState::RelayTracker() { return mRelayTracker; }

rstl::ncrc_ptr< CMapWorldInfo >& CWorldState::MapWorldInfo() { return mMapWorldInfo; }

rstl::rc_ptr< CMapWorldInfo > CWorldState::GetMapWorldInfo() const { return mMapWorldInfo; }

TAreaId CWorldState::GetCurrentArea() const { return mAreaId; }

void CWorldState::SetAreaId(TAreaId areaId) { mAreaId = areaId; }

CAssetId CWorldState::GetDesiredAreaAssetId() const { return mDesiredAreaAssetId; }

void CWorldState::SetDesiredAreaAssetId(CAssetId areaId) { mDesiredAreaAssetId = areaId; }

rstl::ncrc_ptr< CWorldLayerState >& CWorldState::GetLayerState() { return mLayerState; }

CGameState::SPlayerResult::SPlayerResult(CBitStreamReader& in)
: mPlayerSelection(in.ReadBits(2))
, mScore(int(in.ReadBits(16)) - 0x8000)
, mDeaths(int(in.ReadBits(16)) - 0x8000) {
  // The controller options are not serialized or initialized by this constructor.
}

void CGameState::SPlayerResult::PutTo(CBitStreamWriter& out) const {
  out.WriteBits(mPlayerSelection, 2);
  out.WriteBits(mScore + 0x8000, 16);
  out.WriteBits(mDeaths + 0x8000, 16);
}

CGameState::SPreviousGameResults::SPreviousGameResults(CBitStreamReader& in) {
  in.ReadBits(32); // PREV
  mGameMode = in.ReadBits(32);
  mShowResults = in.ReadPackedBool();
  x8_ = int(in.ReadBits(8)) - 0x80;
  mPlayerCount = in.ReadBits(3);
  for (int i = 0; i < 4; ++i) {
    mPlayers.push_back(SPlayerResult(in));
  }
}

void CGameState::SPreviousGameResults::PutTo(CBitStreamWriter& out) const {
  out.WriteBits('PREV', 32);
  out.WriteBits(mGameMode, 32);
  out.WriteBits(mShowResults ? 1 : 0, 1);
  out.WriteBits(x8_ + 0x80, 8);
  out.WriteBits(mPlayerCount, 3);
  for (int i = 0; i < 4; ++i) {
    mPlayers[i].PutTo(out);
  }
}

CGameState::CGameState()
: mWorldId(kInvalidAssetId)
, mDesiredWorldId(kInvalidAssetId)
, mTransManager(rs_new CWorldTransManager)
, mTotalPlayTime(0.0)
, mEscapeTime(0.f)
, mPersistentOptions(CGameStateEnvVarManager::kVS_Game)
, mCardSerial(0)
, mCompressedGameStates(3, rstl::vector< uchar >())
, mCompressedGameOptions(3, rstl::vector< uchar >())
, mGameMode(rs_new CGMSinglePlayer)
, mControlMapper(0)
, mHardMode(false)
, mInitPowerupsAtFirstSpawn(true)
, mIsDarkWorld(false) {
  for (int player = 0; player < 4; ++player) {
    mPlayerStates.push_back(rstl::rc_ptr< CPlayerState >(rs_new CPlayerState(player, nullptr)));
  }
  if (gpMemoryCard != nullptr) {
    InitializeMemoryStates();
  }
  for (int slot = 0; slot < 3; ++slot) {
    RecordCompressedGameOptions(slot);
  }
  RecordCompressedMultiplayerOptions();
}

extern "C" void fn_8014495C(SGameStateBlock* elems, int n, const SGameStateBlock* src) {
  SGameStateBlock* p = elems;
  for (int i = 0; i < n; i++, p++) {
    fn_80142A10(p, src);
  }
}

extern "C" SGameStateSlots* fn_80144924(SGameStateSlots* self, int n, const SGameStateBlock* src) {
  self->x00_count = n;
  fn_8014495C(self->x04_blk, n, src);
  return self;
}

CGameState::CGameState(CBitStreamReader& in)
: mWorldId(kInvalidAssetId)
, mDesiredWorldId(kInvalidAssetId)
, mTransManager(rs_new CWorldTransManager)
, mTotalPlayTime(0.0)
, mEscapeTime(0.f)
, mPersistentOptions(CGameStateEnvVarManager::kVS_Game)
, mCardSerial(0)
, mCompressedGameStates(3, rstl::vector< uchar >())
, mCompressedGameOptions(3, rstl::vector< uchar >())
, mGameMode(rs_new CGMSinglePlayer)
, mControlMapper(0)
, mHardMode(false)
, mInitPowerupsAtFirstSpawn(true)
, mIsDarkWorld(false) {
  in.ReadBits(32); // GMST
  in.ReadBits(32); // Timestamp
  mHardMode = in.ReadPackedBool();
  mInitPowerupsAtFirstSpawn = in.ReadPackedBool();
  mIsDarkWorld = in.ReadPackedBool();
  mWorldId = in.ReadBits(32);
  mDesiredWorldId = mWorldId;
  CMain::EnsureWorldPakReady(mWorldId);

  union {
    double value;
    u64 bits;
  } playTime;
  playTime.bits = u64(in.ReadBits(32)) << 32;
  playTime.bits |= in.ReadBits(32);
  mTotalPlayTime = playTime.value;
  for (int player = 0; player < 4; ++player) {
    mPlayerStates.push_back(rstl::rc_ptr< CPlayerState >(rs_new CPlayerState(player, in)));
  }
  mHintOptions = CHintOptions(in);
  mPreviousGameResults = SPreviousGameResults(in);
  mEscapeTime = in.GetInputStream().ReadFloat();
  mPersistentOptions = CGameStateEnvVarManager(CGameStateEnvVarManager::kVS_Game, in);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  mWorldStates.reserve(worlds.size());
  const uchar worldCount = in.GetInputStream().ReadUint8();
  for (uchar i = 0; i < worldCount; ++i) {
    const CAssetId worldId = in.GetInputStream().ReadInt32();
    int bitCount = in.GetInputStream().ReadUint16();
    if (!gpMemoryCard->HasSaveWorldMemory(worldId)) {
      // The original also constructs an unused diagnostic string for the missing world.
      while (bitCount > 0) {
        in.ReadBits(rstl::min_val(bitCount, 32));
        bitCount -= 32;
      }
    } else {
      TLockedToken< CWorldSaveGameInfo > saveWorld = gpSimplePool->GetObj(
          SObjectTag('SAVW', gpMemoryCard->GetSaveWorldMemory(worldId).GetSaveWorldAssetId()));
      mWorldStates.push_back(CWorldState(in, worldId, **saveWorld));
    }
  }
  in.GetInputStream().ReadInt32(); // GMND

  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    // StateForWorld creates defaults for worlds absent from the save.
    StateForWorld(it->first);
  }
  InitializeMemoryWorlds();
  WriteBackupBuf();
  for (int slot = 0; slot < 3; ++slot) {
    RecordCompressedGameOptions(slot);
  }
  RecordCompressedMultiplayerOptions();
}

void CGameState::InitializeMemoryStates() {
  for (int i = 0; i < mPlayerStates.size(); ++i) {
    mPlayerStates[i]->InitializeScanTimes();
  }
  mHintOptions.InitializeMemoryState();
  mPersistentOptions.InitializeMemoryState();
  InitializeMemoryWorlds();
  WriteBackupBuf();
}

// The 64 zero bytes `fn_80143E88` copy-constructs its local out of: `.rodata:0x803A91C8`, the
// 0x40 bytes immediately below this unit's own pool at `lbl_803A9208` (0x803A9208). It is retail
// data in a retail object, not something this unit may claim - the claim starts at 0x803A9208
// (`config/G2ME01/splits.txt`) - so it is referenced by name, the way `lbl_803A9208` is.
extern "C" const char lbl_803A91C8[];

// `fn_800068F4` walks its argument as a twelve-byte-element container: `+0x04` the element count,
// `+0x0C` the base pointer, `count * 12` the end (0x800068F4, 0x80146900-0x80146920). Retail
// code the port does not have, so it is called through an untyped pointer.
extern "C" void fn_800068F4(void* self);

struct SGameStateName {
  char x00_name[0x40];
};

// Called from `CMainFlow::AdvanceGameState` (0x8001DE34) when the restart mode is neither
// `kRM_None` nor `kRM_StateSetter`, i.e. when the game is resuming into the world rather than
// resetting through the front end. `gpResourceFactory->GetResourceIdByName("InitialWorld")` is
// the probe: a non-null answer means the world is loaded, and the game resumes as a single-player
// game; a null answer means it is not, and the game resumes *in* the front end. The name it
// builds in the second case is the results-screen layer name for the mode that was played.
void fn_80143E88() {
  CMain::EnsureWorldPaksReady();
  fn_800068F4(gpGameState->AudioGroups());

  const SObjectTag* const world = gpResourceFactory->GetResourceIdByName("InitialWorld");
  if (world != nullptr) {
    gpGameState->SetCurrentWorldId(world->GetId());
    gpGameState->SetGameMode(rs_new CGMSinglePlayer());
  } else {
    gpGameState->SetCurrentWorldId(gpResourceFactory->GetResourceIdByName("FrontEnd")->GetId());
    gpGameState->SetGameMode(rs_new CGMFrontEnd());

    rstl::rc_ptr< CWorldLayerState > layers = gpGameState->CurrentWorldState().GetLayerState();
    layers->GetAreaLayerCount(TAreaId(0));

    // **Pool order, not order of use.** Retail loads the three addresses in one hoisted block as
    // `+29`, `+37`, `+42` - `Results`, `Coin`, `Deathmatch` - and the `"%s%s%d"` it passes to
    // both `sprintf`s is created last, by the first one, and lands at +53. Written inline at the
    // call sites the pool would order them by first *use* and every immediate would move. They
    // are declared before the two member reads for a second reason: the pool base they share with
    // the `new` operands has to land in `r4`, and the member read has to be pushed off it.
    const char* const kResults = "Results";
    const char* const kCoin = "Coin";
    const char* const kDeathmatch = "Deathmatch";

    // **These two are read before the 64-byte copy, and that is load-bearing.** Retail loads
    // them at 0x80143FB8/0x80143FC0 and `mShowResults` only at 0x80144050, so two values have to
    // survive sixteen stores. Reading all three before the copy is *also* wrong - it costs
    // `mShowResults` its live range and the frame comes out -128 bytes.
    const CGameState::SPreviousGameResults& results = gpGameState->PreviousGameResults();
    const int gameMode = results.mGameMode;
    const int playerCount = results.mPlayerCount;
    SGameStateName name = *reinterpret_cast< const SGameStateName* >(lbl_803A91C8);

    if (results.mShowResults && playerCount > 1) {
      // The two-sided tests are `== 'DTHM'` and `== 'COIN'`, spelled as `addis` against the
      // high half and `cmplwi` against the low (0x80144064, 0x80144084) - which is what mwcceppc
      // emits for a full-word compare against a constant that will not fit in one immediate.
      if (gameMode == 'DTHM') {
        sprintf(name.x00_name, "%s%s%d", kResults, kDeathmatch, playerCount);
      } else if (gameMode == 'COIN') {
        sprintf(name.x00_name, "%s%s%d", kResults, kCoin, playerCount);
      }
    }
  }
}

// Guessed name. Layer-name prefixes select which game mode owns each layer.
static rstl::pair< const char*, uint > sGameModeLayers[] = {
    rstl::pair< const char*, uint >("Deathmatch", 'DTHM'),
    rstl::pair< const char*, uint >("Samus01", 'SNGL'),
    rstl::pair< const char*, uint >("Coins", 'COIN'),
};

void ConfigureGameModeLayers() {
  for (int area = 0;
       area < gpMemoryCard->GetSaveWorldMemory(gpGameState->CurrentWorldAssetId()).GetAreaCount();
       ++area) {
    rstl::rc_ptr< CWorldLayerState > layers = gpGameState->CurrentWorldState().GetLayerState();
    int layerCount = layers->GetAreaLayerCount(TAreaId(area));
    for (int layer = 0; layer < layerCount; ++layer) {
      for (int i = 0; i < 3; ++i) {
        bool active = sGameModeLayers[i].second == gpGameState->GetGameMode().GetGameModeType();
        const char* prefix = sGameModeLayers[i].first;
        const rstl::string& name = layers->GetLayerName(TAreaId(area), TLayerId(layer));
        if (strncmp(prefix, name.data(), strlen(prefix)) == 0) {
          layers->SetLayerActive(TAreaId(area), TLayerId(layer), active);
        }
      }
    }
  }
}

// Guessed name
void StartGameFromFrontEnd() {
  const CGMFrontEnd config = static_cast< const CGMFrontEnd& >(gpGameState->GetGameMode());
  CGameMode* mode = nullptr;
  switch (config.GetSelectedGameMode()) {
  case CGMFrontEnd::kSGM_SinglePlayer:
    mode = rs_new CGMSinglePlayer;
    break;
  case CGMFrontEnd::kSGM_DeathMatch: {
    CGMDeathMatch* deathMatch = rs_new CGMDeathMatch(config.GetPlayerCount(), config.GetFragLimit(),
                                                     config.GetTimeLimit(), true, false);
    deathMatch->SetMusicIndex(config.GetMusicIndex());
    mode = deathMatch;
    break;
  }
  case CGMFrontEnd::kSGM_Coin: {
    CGMCoin* coin =
        rs_new CGMCoin(config.GetPlayerCount(), config.GetCoinLimit(), config.GetTimeLimit(), true);
    coin->SetMusicIndex(config.GetMusicIndex());
    mode = coin;
    break;
  }
  case CGMFrontEnd::kSGM_FrontEnd:
    mode = rs_new CGMFrontEnd;
    break;
  }

  const CGameState::SPreviousGameResults results = gpGameState->PreviousGameResults();
  gpMain->StreamNewGameState(false);
  if (config.GetSelectedGameMode() == CGMFrontEnd::kSGM_Coin ||
      config.GetSelectedGameMode() == CGMFrontEnd::kSGM_DeathMatch) {
    gpGameState->LoadCompressedMultiplayerOptions();
  } else if (config.GetSelectedGameMode() == CGMFrontEnd::kSGM_SinglePlayer) {
    gpGameState->LoadCompressedGameOptions(gpGameState->SystemOptions().GetSaveIdx());
  }
  gpGameState->GameOptions().EnsureOptions();
  gpGameState->SetGameMode(mode);
  gpGameState->PreviousGameResults() = results;

  for (int i = 0; i < config.GetPlayerCount(); ++i) {
    const CGMFrontEnd::SPlayerConfig& player = config.GetPlayer(i);
    gpGameState->PlayerState(i)->FUN_80085c18(player.mPlayerSelection);
    rstl::pair< bool, bool >& options = gpGameState->GameOptions().PlayerOptions(i);
    options.first = player.mRumbleEnabled;
    options.second = player.x5_;
  }
  ConfigureGameModeLayers();
  gpGameState->WriteBackupBuf();
}

void CGameState::InitializeMemoryWorlds() {
  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    rstl::rc_ptr< CWorldLayerState > layers = StateForWorld(it->first).GetLayerState();
    layers->InitializeWorldLayers(it->second.GetDefaultLayerStates(), it->second.GetLayerNames(),
                                  it->second.GetLayerNameOffsets());
  }
}

void CGameState::SerializeNewForCleanSlot(CBitStreamWriter& out, bool hardMode) {
  CGameState state;
  state.SetHardMode(hardMode);
  state.SetDesiredWorldId(0x3bfa3eff);
  state.StateForWorld(0x3bfa3eff).SetDesiredAreaAssetId(0x62b0d67d);
  state.PutTo(out);
}

void CGameState::PutTo(CBitStreamWriter& out) {
  out.WriteBits('GMST', 32);
  out.WriteBits(OSTicksToSeconds(OSGetTime()), 32);
  out.WriteBits(mHardMode ? 1 : 0, 1);
  out.WriteBits(mInitPowerupsAtFirstSpawn ? 1 : 0, 1);
  out.WriteBits(mIsDarkWorld ? 1 : 0, 1);
  out.WriteBits(mDesiredWorldId, 32);

  union {
    double value;
    u64 bits;
  } playTime;
  playTime.value = mTotalPlayTime;
  out.WriteBits(playTime.bits >> 32, 32);
  out.WriteBits(playTime.bits, 32);

  for (int i = 0; i < mPlayerStates.size(); ++i) {
    mPlayerStates[i]->PutTo(out);
  }
  mHintOptions.PutTo(out);
  mPreviousGameResults.PutTo(out);
  out.GetOutputStream().WriteReal32(mEscapeTime);
  mPersistentOptions.PutTo(out);

  const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
  out.GetOutputStream().WriteUint8(worlds.size());
  rstl::auto_ptr< uchar > buffer(rs_new uchar[0x400]);
  for (rstl::vector< CMemoryCard::MemoryWorld >::const_iterator it = worlds.begin();
       it != worlds.end(); ++it) {
    TLockedToken< CWorldSaveGameInfo > saveWorld =
        gpSimplePool->GetObj(SObjectTag('SAVW', it->second.GetSaveWorldAssetId()));
    CWorldState& state = StateForWorld(it->first);
    uint bitCount;
    {
      CMemoryStreamOut stream(buffer.get(), 0x400);
      CBitStreamWriter writer(stream);
      state.PutTo(writer, **saveWorld);
      stream.Flush();
      bitCount = writer.GetWrittenBits();
    }
    out.GetOutputStream().WriteUint32(it->first);
    out.GetOutputStream().WriteUint16(bitCount);
    state.PutTo(out, **saveWorld);
  }
  out.GetOutputStream().WriteUint32('GMND');
}

void CGameState::ReadSystemOptions(CInputStream& in) {
  CBitStreamReader reader(in);
  mSystemOptions = CPersistentOptions(reader);
}

void CGameState::WriteSystemOptions(COutputStream& out) {
  CBitStreamWriter writer(out);
  mSystemOptions.PutTo(writer);
}

void CGameState::SetSystemOptions(const CPersistentOptions& options) { mSystemOptions = options; }

void CGameState::ExportPersistentOptions(CPersistentOptions& options) {
  options.SetSaveIdx(mSystemOptions.GetSaveIdx());
}

void CGameState::WriteBackupBuf() {
  rstl::vector< uchar >& buffer = mCompressedGameStates[mSystemOptions.GetSaveIdx()];
  buffer.resize(0xa38);
  CMemoryStreamOut stream(buffer.data(), 0xa38);
  CBitStreamWriter out(stream);
  PutTo(out);
}

void CGameState::RecordCheckpoint() {
  mCheckpointGameState.resize(0xa38);
  CMemoryStreamOut stream(mCheckpointGameState.data(), 0xa38);
  CBitStreamWriter out(stream);
  PutTo(out);
}

void CGameState::ClearCheckpoint() { mCheckpointGameState.clear(); }

void CGameState::SetCompressedGameStates(
    const rstl::reserved_vector< rstl::vector< uchar >, 3 >& states) {
  mCompressedGameStates = states;
}

void CGameState::CopyCompressedGameState(int slot, const void* data) {
  mCompressedGameStates[slot].resize(0xa38);
  memcpy(mCompressedGameStates[slot].data(), data, 0xa38);
}

void CGameState::RecordCompressedGameState(int slot, CGameState& state) {
  mCompressedGameStates[slot].resize(0xa38);
  CMemoryStreamOut stream(mCompressedGameStates[slot].data(), 0xa38);
  CBitStreamWriter out(stream);
  state.PutTo(out);
}

void CGameState::ClearCompressedGameState(int slot) {
  mCompressedGameStates[slot] = rstl::vector< uchar >();
}

void CGameState::RecordCompressedGameOptions(int slot) {
  mCompressedGameOptions[slot].resize(0x20);
  CMemoryStreamOut stream(mCompressedGameOptions[slot].data(), 0x20);
  CBitStreamWriter out(stream);
  mGameOptions.PutTo(out);
}

void CGameState::CopyCompressedGameOptions(int slot, const void* data) {
  mCompressedGameOptions[slot].resize(0x20);
  memcpy(mCompressedGameOptions[slot].data(), data, 0x20);
}

void CGameState::RecordCompressedMultiplayerOptions() {
  mCompressedMultiplayerOptions.resize(0x20);
  CMemoryStreamOut stream(mCompressedMultiplayerOptions.data(), 0x20);
  CBitStreamWriter out(stream);
  mGameOptions.PutTo(out);
}

// clear, reserve(count), then count unchecked appends of *src.
extern "C" void fn_80142BA4(SGameStateBlock* self, int count, const unsigned char* src) {
  fn_80142914(self);
  fn_801465EC(self, count);
  for (int i = 0; i < count; ++i) {
    unsigned char* p = static_cast< unsigned char* >(self->x0c_data) + self->x04_count++;
    *p = *src;
  }
}

void CGameState::CopyCompressedMultiplayerOptions(const void* data) {
  mCompressedMultiplayerOptions.resize(0x20);
  memcpy(mCompressedMultiplayerOptions.data(), data, 0x20);
}

extern "C" void fn_80142A10(SGameStateBlock* self, const SGameStateBlock* src) {
  fn_80004D5C(self, src);
}

void CGameState::SetCompressedGameOptions(
    const rstl::reserved_vector< rstl::vector< uchar >, 3 >& options) {
  mCompressedGameOptions = options;
}

extern "C" void fn_80142914(SGameStateBlock* self) { self->x04_count = 0; }

void CGameState::SetCompressedMultiplayerOptions(const rstl::vector< uchar >& options) {
  mCompressedMultiplayerOptions = options;
}

CWorldState& CGameState::StateForWorld(CAssetId worldId) {
  for (rstl::vector< CWorldState >::iterator it = mWorldStates.begin(); it != mWorldStates.end();
       ++it) {
    if (it->GetWorldAssetId() == worldId) {
      return *it;
    }
  }

  mWorldStates.reserve(mWorldStates.size() + 1);
  mWorldStates.push_back(CWorldState(worldId));
  return mWorldStates.back();
}

CAssetId CGameState::CurrentWorldAssetId() const { return mWorldId; }

CWorldState& CGameState::CurrentWorldState() { return StateForWorld(mWorldId); }

void CGameState::SetCurrentWorldId(CAssetId worldId) {
  StateForWorld(worldId);
  mWorldId = worldId;
  CMain::EnsureWorldPakReady(worldId);
  SetDesiredWorldId(worldId);
}

void CGameState::SetDesiredWorldId(CAssetId worldId) { mDesiredWorldId = worldId; }

rstl::rc_ptr< CPlayerState > CGameState::GetPlayerState() const { return mPlayerStates[0]; }

rstl::rc_ptr< CPlayerState >& CGameState::PlayerState(int player) { return mPlayerStates[player]; }

rstl::rc_ptr< CPlayerState > CGameState::GetPlayerState(int player) const {
  return mPlayerStates[player];
}

rstl::rc_ptr< CWorldTransManager >& CGameState::WorldTransitionManager() { return mTransManager; }

void CGameState::SetTotalPlayTime(double time) {
  mTotalPlayTime = CMath::Clamp(0.0, time, 359999.0);
}

void CGameState::SetEscapeTime(float time) { mEscapeTime = time; }

void CGameState::SetHardMode(bool hardMode) { mHardMode = hardMode; }

void CGameState::SetDeferPowerupInit(bool defer) { mInitPowerupsAtFirstSpawn = defer; }

void CGameState::SetIsDarkWorld(bool darkWorld) { mIsDarkWorld = darkWorld; }

float CGameState::GetHardModeDamageMultiplier() const {
  return gpTweakGame->GetHardModeDamageMultiplier();
}

float CGameState::GetHardModeWeaponMultiplier() const {
  return gpTweakGame->GetHardModeWeaponMultiplier();
}

const CGameMode& CGameState::GetGameMode() const { return *mGameMode; }

CGameMode& CGameState::GetGameMode() { return *mGameMode; }

void CGameState::SetGameMode(CGameMode* mode) { mGameMode = rstl::auto_ptr< CGameMode >(mode); }

bool CPersistentOptions::GetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId) const {
  for (rstl::vector< rstl::pair< CAssetId, TEditorId > >::const_iterator it =
           mCinematicStates.begin();
       it != mCinematicStates.end(); ++it) {
    if (*it == cinematicId) {
      return true;
    }
  }
  return false;
}

void CPersistentOptions::SetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId,
                                           bool state) {
  for (rstl::vector< rstl::pair< CAssetId, TEditorId > >::iterator it = mCinematicStates.begin();
       it != mCinematicStates.end(); ++it) {
    if (*it == cinematicId) {
      if (!state) {
        mCinematicStates.erase(it);
      }
      return;
    }
  }
  if (state) {
    mCinematicStates.reserve(mCinematicStates.size() + 1);
    mCinematicStates.push_back(cinematicId);
  }
}
