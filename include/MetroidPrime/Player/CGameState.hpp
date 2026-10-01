#ifndef _CGAMESTATE
#define _CGAMESTATE

#include "types.h"

#include "Kyoto/TToken.hpp"
#include "MetroidPrime/CControlMapper.hpp"
#include "MetroidPrime/TGameTypes.hpp"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameStateBlocks.hpp"
#include "MetroidPrime/Player/CHintOptions.hpp"
#include "MetroidPrime/Player/CPersistentOptions.hpp"
#include "MetroidPrime/Player/CWorldState.hpp"

#include "rstl/auto_ptr.hpp"
#include "rstl/pair.hpp"
#include "rstl/rc_ptr.hpp"
#include "rstl/reserved_vector.hpp"
#include "rstl/vector.hpp"

class CBitStreamReader;
class CBitStreamWriter;
class CGameMode;
class CWorldTransManager;
class CWorldTransManagerView;
class CPlayerState;
class CMain;
class CInputStream;
class COutputStream;
class CGameState;

// The units that fill `CGameState` by retail offset under retail's unnamed symbols,
// declared at namespace scope with C linkage and friended below: GCC rejects a friend declaration
// whose linkage does not match the definition.
extern "C" CGameState* fn_801449C8(CGameState* self);
extern "C" void fn_801440C0(CGameState* self);
extern "C" void fn_80142DD4(CGameState* self, int idx);
extern "C" void fn_80142CF8(CGameState* self);
extern "C" void StreamNewGameState__5CMainFR12CInputStreami(CMain* self, CInputStream& in);
#ifdef __MWERKS__
extern "C" void fn_80144140(CGameState* self, CInputStream& in, int saveIdx);
#endif

// The 0xEC bytes at `CGameState+0x204`: the `CControlMapper` and then the one flag byte, and the
// three bytes the flag byte's own object has after it. It is a view, not a member - upstream's
// `CControlMapper` is at the same offset with the same 0xE8 size, and its own trailing four bytes
// are the `bool mHardMode : 1` plus `uchar x2ed_[3]`. It exists so that the two units that write
// that byte - `fn_801449C8` and `CGameState::SetIsDarkWorld` - can name the byte and the three
// one-bit fields in it without an offset in the source.
//
// **The three bits are in mwcceppc's declaration order, which is measured**: the retail encoding
// for a one-bit field is `rlwimi r0,r4,sh,mb,mb` with `mb` walking `24,25,26,...` from the byte's
// most significant bit, so a field declared first is bit 7, the second is bit 6 and the third is
// bit 5. `CGameState::SetIsDarkWorld` is retail's `rlwimi r0,r4,5,26,26` (0x801424C0), i.e. the
// **third** field, and the same third field the save-game reader writes at 0x80144390. Upstream
// declares one bitfield for this byte, `mHardMode : 1`, so **upstream's `mHardMode` is bit 7 and
// not the one `SetIsDarkWorld` writes** - `CGameStateSetIsDarkWorld.cpp` says so where it goes
// through this view. Neither declaration may move: 0xE8 + 1 + 3 = 0xEC ends the object at
// `CHECK_SIZEOF(CGameState, 0x2f0)`, and `CControlMapper` is 0xE8 either way.
struct SGameStateTail {
  CControlMapper mapper; //!< +0x204 .. +0x2EB, 0xE8
  struct SFlags {
    bool b7 : 1; //!< the save-game reader's first `ReadBits(1)`, `rlwimi r0,rX,7,24,24`
    bool b6 : 1; //!< second, `,6,25,25`. `CGameStateCtor.cpp` sets this one true
    bool b5 : 1; //!< third, `,5,26,26` - and the one `SetIsDarkWorld` writes
    u8 rest : 5;
  } flags;   //!< +0x2EC
  u8 x2ed_pad[3]; //!< 0x2ED..0x2EF
};
CHECK_SIZEOF(SGameStateTail, 0xec)
class CAudioGrpSetLoc;

class CGameState {
public:
  // Guessed name
  struct SPlayerResult {
    SPlayerResult()
    : mPlayerSelection(0), mScore(0), mDeaths(0), xc_(false), mRumbleEnabled(false) {}
    explicit SPlayerResult(CBitStreamReader& in);
    void PutTo(CBitStreamWriter& out) const;

    uint mPlayerSelection;
    int mScore;
    int mDeaths;
    bool xc_; // The second per-player controller option; meaning unresolved.
    bool mRumbleEnabled;
  };

  // Guessed name
  struct SPreviousGameResults {
    SPreviousGameResults()
    : mGameMode(0), mShowResults(false), x8_(0), mPlayerCount(0), mPlayers(4, SPlayerResult()) {}
    explicit SPreviousGameResults(CBitStreamReader& in);
    void PutTo(CBitStreamWriter& out) const;

    uint mGameMode;
    bool mShowResults;
    int x8_; // Result of the game mode's unresolved v14 query.
    int mPlayerCount;
    rstl::reserved_vector< SPlayerResult, 4 > mPlayers;
  };

  struct GameFileStateInfo {
    double mPlayTime;
    CAssetId mMlvlId;
    float mHealth;
    uint mEnergyTanks;
    uint mTimestamp;
    uint mItemPercent;
    float mScanPercent;
    bool mHardMode;
    bool x21_;
  };

  static GameFileStateInfo LoadGameFileState(const void* data);
  static void SerializeNewForCleanSlot(CBitStreamWriter& out, bool hardMode); // Guessed name

  CGameState();
  explicit CGameState(CBitStreamReader& in);
  ~CGameState();

  void ReadSystemOptions(CInputStream& in);
  void PutTo(CBitStreamWriter& out);
  void WriteSystemOptions(COutputStream& out);
  void SetSystemOptions(const CPersistentOptions& options);
  void ExportPersistentOptions(CPersistentOptions& options);
  void WriteBackupBuf();
  void InitializeMemoryStates();
  void SetCurrentWorldId(CAssetId worldId);
  void SetDesiredWorldId(CAssetId worldId);
  void SetTotalPlayTime(double time);
  void SetEscapeTime(float time);
  void SetHardMode(bool hardMode);
  void SetDeferPowerupInit(bool defer);

  // Guessed names for the compressed-buffer copy and reset operations.
  void CopyCompressedGameState(int slot, const void* data);
  void ClearCompressedGameState(int slot);
  void RecordCompressedGameState(int slot, CGameState& state);
  void CopyCompressedGameOptions(int slot, const void* data);
  void RecordCompressedGameOptions(int slot);
  void CopyCompressedMultiplayerOptions(const void* data);
  void RecordCompressedMultiplayerOptions();
  void LoadCompressedGameOptions(int slot);
  void LoadCompressedMultiplayerOptions();
  void SetCompressedGameStates(const rstl::reserved_vector< rstl::vector< uchar >, 3 >& states);
  void SetCompressedGameOptions(const rstl::reserved_vector< rstl::vector< uchar >, 3 >& options);
  void SetCompressedMultiplayerOptions(const rstl::vector< uchar >& options);
  void RecordCheckpoint();
  void ClearCheckpoint();
  const rstl::reserved_vector< rstl::vector< uchar >, 3 >& GetCompressedGameStates() const {
    return mCompressedGameStates;
  }
  const rstl::reserved_vector< rstl::vector< uchar >, 3 >& GetCompressedGameOptions() const {
    return mCompressedGameOptions;
  }
  const rstl::vector< uchar >& GetCompressedMultiplayerOptions() const {
    return mCompressedMultiplayerOptions;
  }
  const rstl::vector< uchar >& GetCheckpointGameState() const { return mCheckpointGameState; }
  void ClearAudioGroups() { mAudioGroups.clear(); }

  void SetIsDarkWorld(bool);
  CGameMode& GetGameMode();
  const CGameMode& GetGameMode() const;
  void SetGameMode(CGameMode* mode);                                           // name inferred
  SPreviousGameResults& PreviousGameResults() { return mPreviousGameResults; } // Guessed name
  int GetGameModeType() const { return mPreviousGameResults.mGameMode; }       // name inferred
  CWorldState& StateForWorld(CAssetId worldId);
  CWorldState& CurrentWorldState();
  rstl::rc_ptr< CWorldTransManager >& WorldTransitionManager();
  CAssetId CurrentWorldAssetId() const;

  CControlMapper& ControlMapper() { return mControlMapper; }

  CPersistentOptions& SystemOptions() { return mSystemOptions; }

  // `fn_80143E88` (0x80143E88) passes `this + 500` to `fn_800068F4`, which walks its `+0x04` as
  // an element count and its `+0x0C` as a base pointer over 12-byte elements - so the object at
  // `+0x1F4` is the `TCachedToken<CAudioGrpSetLoc>` vector, `mAudioGroups`. The argument's type
  // is not ours to name, since `fn_800068F4` is retail code this port does not have; see the
  // port's `AudioGroups()` above for the same name against the port's layout.
  void* AudioGroups() { return &mAudioGroups; }

  CGameOptions& GameOptions() { return mGameOptions; }
  CGameStateEnvVarManager& PersistentOptions() { return mPersistentOptions; }

  CHintOptions& HintOptions() { return mHintOptions; }

  u32 GetCardSerialA() const { return mCardSerial >> 32; }
  u32 GetCardSerialB() const { return mCardSerial; }
  u64 GetCardSerial() const { return mCardSerial; }
  void SetCardSerial(u64 serial) { mCardSerial = serial; }

  // `CMemoryCardDriver::BuildExistingFileSlot` (0x8017A708) reads `this + 0x118 + i*16` as each
  // `mCompressedGameStates[i]`'s element count and `this + 0x120 + i*16` as its data pointer, and
  // `CMemoryCardDriver::ExportGameOptions` (0x8017A45C) reads the same two words at `+0x14C` /
  // `+0x154` of `mCompressedGameOptions`, which pins the two vectors at `+0x110` and `+0x144`.
  rstl::reserved_vector< rstl::vector< uchar >, 3 >& CompressedGameStates() {
    return mCompressedGameStates;
  }

  // `CMemoryCardDriver::ExportGameOptions` (0x8017A45C) `Put`s element `i`'s data pointer and
  // element count for i = 0..2, then the same two words of the multiplayer buffer.
  const rstl::reserved_vector< rstl::vector< uchar >, 3 >& CompressedGameOptions() const {
    return mCompressedGameOptions;
  }

  // Per-element accessors, used in place of `CompressedGameStates()[i]` /
  // `CompressedGameOptions()[i]`.
  //
  // Retail keeps `gpGameState` as the address base and folds the member offset into each load's
  // displacement - `lwz r0,gpGameState ; add r5,r0,r30 ; lwz r4,340(r5) ; lwz r5,332(r5)` at
  // 0x8017A4A0..0x8017A4B0 (`ExportGameOptions`), and the same shape at `+0x118` / `+0x120` in
  // `BuildExistingFileSlot` (0x8017A730..0x8017A744). Indexing a **reference to the whole vector**
  // instead makes mwcceppc materialise `gpGameState + 0x144` in a register of its own and use the
  // small displacements `+0x10` / `+0x8`, which costs an extra `addi` per function - measured on
  // this tree: `ExportGameOptions` 97.98%, `CopyFileSlot` 99.16%, `BuildExistingFileSlot` 90.25%.
  // An accessor that indexes the member itself and hands back the element keeps `gpGameState` as
  // the base. Purely additive, and used only by `CMemoryCardDriver.cpp`.
  const rstl::vector< uchar >& CompressedGameStatesAt(int idx) const {
    return mCompressedGameStates[idx];
  }
  const rstl::vector< uchar >& CompressedGameOptionsAt(int idx) const {
    return mCompressedGameOptions[idx];
  }

  const rstl::vector< uchar >& CompressedMultiplayerOptions() const {
    return mCompressedMultiplayerOptions;
  }
  float GetHardModeDamageMultiplier() const;
  float GetHardModeWeaponMultiplier() const;
  bool GetHardModeEnabled() const { return mHardMode; }
  double GetTotalPlayTime() const { return mTotalPlayTime; }
  float GetEscapeTime() const { return mEscapeTime; }
  rstl::rc_ptr< CPlayerState > GetPlayerState() const;
  rstl::rc_ptr< CPlayerState > GetPlayerState(int player) const;
  rstl::rc_ptr< CPlayerState >& PlayerState(int player);

  // Port: retail 0x80142520 (8 bytes, `+0x3C`), which upstream does not declare; defined in
  // `src/MetroidPrime/mainMid.cpp` for `CGameArchitectureSupport::Update`.
  CWorldTransManagerView*& GetWorldState();

  // Port: `CMain::ResetGameState` and `StreamNewGameState` copy the compressed blocks out and back.
  friend class CMain;
  friend void ::StreamNewGameState__5CMainFR12CInputStreami(CMain* self, CInputStream& in);

private:
  void InitializeMemoryWorlds();

  CAssetId mWorldId;
  CAssetId mDesiredWorldId;
  rstl::vector< CWorldState > mWorldStates;
  rstl::reserved_vector< rstl::rc_ptr< CPlayerState >, 4 > mPlayerStates;
  rstl::rc_ptr< CWorldTransManager > mTransManager;
  double mTotalPlayTime;
  float mEscapeTime;
  CPersistentOptions mSystemOptions;
  CGameOptions mGameOptions;
  CHintOptions mHintOptions;
  CGameStateEnvVarManager mPersistentOptions;
  // Guessed element type: shares the system cinematic vector's native destructor.
  // Its separate purpose in CGameState remains unresolved.
  rstl::vector< rstl::pair< CAssetId, TEditorId > > xf4_;
  u64 mCardSerial;

  rstl::reserved_vector< rstl::vector< uchar >, 3 > mCompressedGameStates;
  rstl::reserved_vector< rstl::vector< uchar >, 3 > mCompressedGameOptions;
  rstl::vector< uchar > mCompressedMultiplayerOptions;
  rstl::vector< uchar > mCheckpointGameState;
  rstl::auto_ptr< CGameMode > mGameMode;
  SPreviousGameResults mPreviousGameResults;
  rstl::vector< TCachedToken< CAudioGrpSetLoc > > mAudioGroups;
  CControlMapper mControlMapper;
  bool mHardMode : 1;
  bool mInitPowerupsAtFirstSpawn : 1;
  bool mIsDarkWorld : 1;
  uchar x2ed_[3];
};

CHECK_SIZEOF(CGameState, 0x2f0)
NESTED_CHECK_SIZEOF(CGameState, GameFileStateInfo, 0x28)
NESTED_CHECK_SIZEOF(CGameState, SPlayerResult, 0x10)
NESTED_CHECK_SIZEOF(CGameState, SPreviousGameResults, 0x54)

extern CGameState* gpGameState;

// Unidentified game-flow helpers in the CGameState text range.
void StartGameFromFrontEnd();   // Guessed name
void ConfigureGameModeLayers(); // Guessed name
void fn_80143E88();

#endif // _CGAMESTATE
