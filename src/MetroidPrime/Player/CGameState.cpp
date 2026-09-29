#include "MetroidPrime/Player/CGameState.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Basics/CBasics.hpp"
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
#include "MetroidPrime/Player/SPersistentOptionsValue.hpp"
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

// The 36-byte-element block helpers, and the two loops that walk one. `SGameStateBlock`'s
// `x0c_data` is the base pointer and `x04_count` the count for this instance, because
// `fn_801426E0` (0x801426E0) indexes it as `data + count * 36`.
//
// `fn_80004458` (0x80004458) and `fn_80142760` (0x80142760) are retail code no port unit claims,
// so they are called through declarations rather than inlined copies. `fn_80142760` is the
// 36-byte element's copy: nine words with a refcount increment after the 4th, 6th and 9th.
extern "C" void fn_80004458(void* elem);
extern "C" void fn_80142718(void* elem, const void* src);
extern "C" void fn_80142760(void* elem, const void* src);

extern "C" void fn_801435D4(void* elem);
extern "C" void fn_801467C0(uchar* begin, uchar* end);

// The move half of `fn_801466F4`'s grow: it copy-constructs each 36-byte element from the old
// range into the new buffer and returns the new end. `begin` and `end` arrive by address -
// `fn_801466F4` builds both as locals of four words each (0x80146734-0x80146758).
extern "C" void* fn_8014680C(void* const* begin, void* const* end, void* dst) {
  uchar* out = static_cast< uchar* >(dst);
  for (uchar* in = static_cast< uchar* >( *begin ); in != static_cast< uchar* >( *end );
       in += 36, out += 36) {
    fn_80142718(out, in);
  }
  return out;
}

extern "C" void fn_801467C0(uchar* begin, uchar* end) {
  for (uchar* p = begin; p != end; p += 36) {
    fn_801435D4(p);
  }
}

extern "C" void fn_801467A0(uchar* begin, uchar* end) { fn_801467C0(begin, end); }

// The 36-byte block's `reserve` (retail 0x801466F4). The new buffer is filled by the
// `fn_8014680C` above from the four-word range this function builds on its own stack
// (0x80146734-0x80146758: the old end stored twice, the old begin stored twice, with
// `&range[3]` and `&range[1]` as the first two arguments), the old elements are then destroyed
// with `fn_801467A0` and the old block handed back to `CMemory::Free`. The guard is a **signed**
// `cmpw` - `n <= x08_cap` skips the grow entirely, with no allocation.
extern "C" void fn_801466F4(SGameStateBlock* self, int capacity) {
  if (capacity <= static_cast< int >(self->x08_cap)) {
    return;
  }

  uchar* const buffer =
      static_cast< uchar* >(rstl::rmemory_allocator::allocate(capacity * 36));
  uchar* const oldBegin = static_cast< uchar* >(self->x0c_data);
  uchar* const oldEnd = oldBegin + self->x04_count * 36;
  void* range[4];
  range[1] = oldEnd;
  range[0] = oldEnd;
  range[2] = oldBegin;
  range[3] = oldBegin;
  fn_8014680C(&range[3], &range[1], buffer);
  fn_801467A0(oldBegin, oldEnd);
  CMemory::Free(self->x0c_data);
  self->x0c_data = buffer;
  self->x08_cap = static_cast< u32 >(capacity);
}

// The 12-byte block's element copy (retail 0x801465A8). One word and two floats per element.
// `begin` and `end` are loaded once, before the loop (0x801465A8 / 0x801465AC), and `dst` is
// tested inside the loop body (0x801465B4), so a null destination still walks the range and
// returns the new end.
extern "C" void* fn_801465A8(void* const* begin, void* const* end, void* dst) {
  uchar* out = static_cast< uchar* >(dst);
  for (uchar* in = static_cast< uchar* >( *begin ); in != static_cast< uchar* >( *end );
       in += 12, out += 12) {
    if (out != nullptr) {
      u32* to = reinterpret_cast< u32* >(out);
      const u32* from = reinterpret_cast< const u32* >(in);
      to[0] = from[0];
      reinterpret_cast< float* >(to)[1] = reinterpret_cast< const float* >(from)[1];
      reinterpret_cast< float* >(to)[2] = reinterpret_cast< const float* >(from)[2];
    }
  }
  return out;
}

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

// Retail reads back the members the initialiser list has just stored, not the parameters, which
// keeps the frame at retail's 16 bytes (0x801462E4).
CEnvironmentVariable::CEnvironmentVariable(int minimum, int maximum, CBitStreamReader& in)
: mMin(minimum), mMax(maximum), mValue(mMin + in.ReadBits(GetBitCount(mMax - mMin))) {
  ClampToMinMax();
}

// Retail forms the difference into a local before calling GetBitCount, holding it in r31 across
// the call (0x80146118).
void CEnvironmentVariable::PutTo(CBitStreamWriter& out) const {
  const int value = mValue - mMin;
  out.WriteBits(value, GetBitCount(mMax - mMin));
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

// `fn_80145BDC` (0x80145BDC) - the out-of-line `rstl::map` lower_bound walk over
// `rstl::string -> CEnvironmentVariable`, unnamed in the symbol table and claimed by no unit,
// so it is called through a declaration. It returns the node, or null when the key is absent.
extern "C" void* fn_80145BDC(void* tree, const void* key);

// `fn_8014601C` (0x8014601C) - that walk reached through a two-word out-parameter. Its body is
// byte-for-byte the one at `fn_80145B90` (0x80145B90), the other copy of the same walk: the
// result word first, then `tree + 8` as the second word, which is the header the iterator is
// paired with.
extern "C" void fn_8014601C(void* out, void* tree, const void* key) {
  u32* words = static_cast< u32* >(out);
  words[0] = reinterpret_cast< u32 >(fn_80145BDC(tree, key));
  words[1] = reinterpret_cast< u32 >(tree) + 8;
}

CEnvironmentVariable* CGameStateEnvVarManager::FindEnvironmentVariable(const char* name) {
  rstl::map< rstl::string, CEnvironmentVariable >::iterator it =
      mVariables.find(rstl::string_l(name));
  // Retail tests the end iterator with `!=` and selects the second through a ternary
  // (0x80145C74). `end` is bound to a local declared *after* the find: binding it before puts
  // `addi rX,this,12` in the prologue and costs r31.
  rstl::map< rstl::string, CEnvironmentVariable >::iterator end = mVariables.end();
  return it != end ? &it->second : nullptr;
}

// **Declared between `FindEnvironmentVariable` (0x80145E24) and `AddVariable` (0x801442CC) because
// that is where retail's offset order puts it** - `check_decl_order.py` pairs by name, and the
// block landed after `PutTo` at first and put the whole tail of the unit 7 slots out of place.
extern "C" void fn_80145ACC(CPersistentOptions* self, const rstl::string& name,
                            const SPersistentOptionsValue& value);

// `fn_80145C98` - retail `.text:0x80145C98`, `size:0x2F4` = 756 bytes, 0x80145C98..0x80145F8C.
// `CPersistentOptions`' own default initialiser, called by `fn_80146154` (the constructor that
// stores the scope word this function branches on). Eleven straight-line statements, not a loop
// over a table: a loop gives mwcceppc a `ctr` and a body to unroll, and retail has neither.
//
// **The eleven names are literals here and were `lbl_803A9208 + K` in the carve.** This unit owns
// the pool (`splits.txt` claims `.rodata 0x803A9208..0x803A93D0`), so the names have to be its
// own literals for `lis/addi/addi K` to reproduce; that is also what puts them at +213..+438,
// which is where retail keeps them and where `__sinit_CGameState_cpp`'s `+440`/`+448` ("Samus01",
// "Coins") then land.
//
// The three numbers are the value's `{lo, hi, default}`; every row has `lo == 0`, so
// `SPersistentOptionsValue`'s clamp is a no-op for all of them, but retail passes them.
extern "C" void fn_80145C98(CPersistentOptions* self) {
  // +0x00 is the constructor's scope word: the system-wide object gets the table, the per-game
  // one (built from the bit stream) does not. Read through `int*` because it is the base class's
  // private member.
  if (reinterpret_cast< const int* >(self)[0] != 0) {
    return;
  }

  fn_80145ACC(self, rstl::string_l("FreezeInstructionsFirstPerson"), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l("FreezeInstructionsMorphBall"), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l("PowerbombPickupMessages"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("PercentScans"), SPersistentOptionsValue(0, 100, 0));
  fn_80145ACC(self, rstl::string_l("NormalModeCompleted"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("HardModeCompleted"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("AllPickupsFound"), SPersistentOptionsValue(0, 1, 0));
  fn_80145ACC(self, rstl::string_l("AutoMapperPaneMode"), SPersistentOptionsValue(0, 2, 1));
  fn_80145ACC(self, rstl::string_l("LogbookLegendVisible"), SPersistentOptionsValue(0, 1, 1));
  fn_80145ACC(self, rstl::string_l("IngAttachedWarningCount"), SPersistentOptionsValue(0, 3, 0));
  fn_80145ACC(self, rstl::string_l("SeenIntroText"), SPersistentOptionsValue(0, 1, 0));
}

void CGameStateEnvVarManager::AddVariable(const rstl::string& name,
                                          const CEnvironmentVariable& variable) {
  // Same hoist as `FindEnvironmentVariable` (the end iterator is materialised into a local
  // declared after the find, which is what puts `addi r0,r29,12` where retail has it at
  // 0x80145B08) but the opposite branch polarity: retail tests `it == end` and only reaches
  // the insert when they are equal (0x80145B10/0x80145B20).
  rstl::map< rstl::string, CEnvironmentVariable >::iterator it = mVariables.find(name);
  rstl::map< rstl::string, CEnvironmentVariable >::iterator end = mVariables.end();
  if (it == end) {
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
      // `stbx` right after `mCount++` (0x80145564..0x8014557C): the reserved vector's append is
      // unchecked, like retail's.
      cinematicStates.push_back_unsafe(GetCinematicState(
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

// `CHintOptions`'s copy assignment (retail 0x801447C4, unnamed in the symbol table, and so
// claimable only under an `extern "C"` name - see CHintOptions.hpp). The
// `rstl::vector< SHintState >::operator=` it calls is out of line and lands at 0x80144818,
// immediately after this.
extern "C" void* fn_801447C4(void* self, const void* src) {
  CHintOptions& to = *static_cast< CHintOptions* >(self);
  const CHintOptions& from = *static_cast< const CHintOptions* >(src);
  to.mHintStates = from.mHintStates;
  to.mNextHintIdx = from.mNextHintIdx;
  to.mInRezbitState = from.mInRezbitState;
  to.mScanDisplayActive = from.mScanDisplayActive;
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
      // **The string is built and thrown away, and retail builds it.** `Stringize` into an
      // `rstl::string`, skip the save data's bits, then let the string die (0x80144684-0x801446b0,
      // `~basic_string` at 0x801446d8). The format literal is not decoration either: it is
      // `lbl_803A9208 + 60`, the first of the two long messages this unit's pool keeps at +60
      // and +134, and the instruction pair that loads it is part of the match.
      rstl::string missing(CBasics::Stringize(
          "Cannot find World Asset(%x) to load save data.  Skipping save game info.\n", worldId));
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
    const uchar knownWorlds = mWorldStates.size();
    StateForWorld(it->first);
    if (mWorldStates.size() != knownWorlds) {
      // The second unused diagnostic string, `lbl_803A9208 + 134`: only when StateForWorld had
      // to invent a world, i.e. the save did not carry one (0x8014470c-0x80144758).
      rstl::string defaulted(CBasics::Stringize(
          "Save game did not contain World Asset(%x).  Creating default world save info.\n",
          it->first));
    }
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
    rstl::rc_ptr< CWorldLayerState > layersRc = gpGameState->CurrentWorldState().GetLayerState();
    CWorldLayerState& layers = *layersRc;
    int layerCount = layers.GetAreaLayerCount(TAreaId(area));
    for (int layer = 0; layer < layerCount; ++layer) {
      for (int i = 0; i < 3; ++i) {
        // Spelled as a difference compared to zero, not as `==`, and with the layer table's
        // value on the left. mwcceppc lowers `a == b` to `subf r0,r0,r3` (b - a) whatever the
        // source operand order, but lowers `a - b == 0` to `subf r0,r3,r0` (a - b) - which is
        // what retail emits at 0x80143DC8. `type - second == 0` is the one order that does NOT
        // work; only `second - type == 0` does. Unsigned subtraction, so it is exactly the
        // equality it replaces.
        bool active =
            (sGameModeLayers[i].second - gpGameState->GetGameMode().GetGameModeType()) == 0;
        const char* prefix = sGameModeLayers[i].first;
        const rstl::string& name = layers.GetLayerName(TAreaId(area), TLayerId(layer));
        if (strncmp(prefix, name.data(), strlen(prefix)) == 0) {
          layers.SetLayerActive(TAreaId(area), TLayerId(layer), active);
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
    // Retail materialises the three arguments in r4, r5, r6 in that order (0x80143818..0x80143820);
    // named locals make the compiler emit them in declaration order rather than last-first.
    const rstl::vector< CWorldLayers::Area >& defaultStates = it->second.GetDefaultLayerStates();
    const rstl::rc_ptr< rstl::vector< rstl::string > >& layerNames = it->second.GetLayerNames();
    const rstl::rc_ptr< rstl::vector< int > >& layerNameOffsets = it->second.GetLayerNameOffsets();
    layers->InitializeWorldLayers(defaultStates, layerNames, layerNameOffsets);
  }
}

// The per-element destructor `fn_801467C0` loops over.
extern "C" void fn_801435D4(void* elem) { fn_80004458(elem); }

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

// The 16-byte block's element copy (retail 0x801429AC), the same shape as the 12-byte block's
// `fn_801465A8` above: `begin` and `end` are the source range, `dst` the destination, the stride
// is the element size and the **return value is the advanced destination**, not `dst` itself
// (0x801429F4 is `mr r3,r31`, with `r31` the destination walked forward in the loop). Writing
// `return dst` costs a register - the original destination has to stay live across the loop, so
// the compiler adds `r28` and the function is 112 bytes against retail's 100. Spelling the
// element as `SGameStateBlock` is what gives the 16-byte stride.
extern "C" void* fn_801429AC(void* begin, void* end, void* dst) {
  SGameStateBlock* out = static_cast< SGameStateBlock* >(dst);
  for (SGameStateBlock* in = static_cast< SGameStateBlock* >(begin);
       in != static_cast< SGameStateBlock* >(end); ++in, ++out) {
    fn_80142A10(out, in);
  }
  return out;
}

void CGameState::SetCompressedGameOptions(
    const rstl::reserved_vector< rstl::vector< uchar >, 3 >& options) {
  mCompressedGameOptions = options;
}

extern "C" void fn_80142914(SGameStateBlock* self) { self->x04_count = 0; }

void CGameState::SetCompressedMultiplayerOptions(const rstl::vector< uchar >& options) {
  mCompressedMultiplayerOptions = options;
}

extern "C" void fn_80142738(void* elem, const void* src);

// The 36-byte element's "construct in place" pair. `fn_80142738` is the null test, `fn_80142718`
// the forwarder `fn_801426E0` and `fn_8014680C` both call.
extern "C" void fn_80142738(void* elem, const void* src) {
  if (elem != nullptr) {
    fn_80142760(elem, src);
  }
}

extern "C" void fn_80142718(void* elem, const void* src) { fn_80142738(elem, src); }

// The block's append: the element slot is `data + count * 36` and the count goes up before the
// element is built, not after (0x801426EC-0x80142704). The index is counted in **words**, not
// bytes: the element is 36 bytes, which is 9 `u32`s, and retail's `mulli r0,r5,36` at 0x801426F4
// is `mwcceppc`'s strength reduction of `words + n * 9`. Spelled as `n * 36` on a `uchar*` the
// multiply lands on the count's own register instead of a temporary and the function sits at
// 97.50%.
extern "C" void fn_801426E0(SGameStateBlock* self, const void* src) {
  u32* const words = static_cast< u32* >(self->x0c_data);
  const u32 n = self->x04_count;
  self->x04_count = n + 1;
  fn_80142718(words + n * 9, src);
}

CWorldState& CGameState::StateForWorld(CAssetId worldId) {
  // Both exits route through one end-test at +0x60, so the search *breaks* rather than
  // returning from inside the loop. `it` is hoisted because it is needed after the loop, but
  // `end` is not: retail re-reads mCount/mItems from the member at each test (0x8014260C).
  rstl::vector< CWorldState >::iterator it = mWorldStates.begin();
  while (it != mWorldStates.end()) {
    if (it->GetWorldAssetId() == worldId) {
      break;
    }
    ++it;
  }

  if (it != mWorldStates.end()) {
    return *it;
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
    // Retail's `reserve(count+1)` is followed by an inline store of the new last element
    // (0x80142244, then 0x80142248..0x8014226C) with no capacity test, so this is
    // `push_back_unsafe`, not `push_back`.
    mCinematicStates.push_back_unsafe(cinematicId);
  }
}
