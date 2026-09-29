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

  void SetIsDarkWorld(bool);
  CGameMode& GetGameMode();
  const CGameMode& GetGameMode() const;
  void SetGameMode(CGameMode* mode);                                     // name inferred
  CWorldState& StateForWorld(CAssetId worldId);
  CWorldState& CurrentWorldState();
  rstl::rc_ptr< CWorldTransManager >& WorldTransitionManager();
  CAssetId CurrentWorldAssetId() const;

  CControlMapper& ControlMapper() { return mControlMapper; }

  CPersistentOptions& SystemOptions() { return mSystemOptions; }

#ifdef TARGET_PC
  // Port: the accessors over the port's layout (below). Upstream's, under `#else`, read members
  // that layout does not have (`mPreviousGameResults`, `mGameOptions`, a single `mCardSerial`).
  int GetGameModeType() const { return mGameModeType; } // name inferred

  // `fn_80143E88` (0x80143E88) passes `this + 500` to `fn_800068F4`, which walks its `+0x04` as
  // an element count and its `+0x0C` as a base pointer over 12-byte elements, so the object is
  // the twelve-byte-element container. Both layouts put one at `+0x1F4` - upstream's
  // `mAudioGroups`, and the port's `x1f4` - and the argument's type is not ours to name, since
  // `fn_800068F4` is retail code this port does not have.
  void* AudioGroups() { return &x1f4; }

  CGameOptions& GameOptions() { return gameOptions; }
  CPersistentOptions& PersistentOptions() { return persistentOptions; }

  CHintOptions& HintOptions() { return hintOptions; }

  u32 GetCardSerialA() const { return cardSerialA; }
  u32 GetCardSerialB() const { return cardSerialB; }
  u64 GetCardSerial() const { return (u64(cardSerialA) << 32) | cardSerialB; }
  void SetCardSerial(u64 serial) {
    cardSerialA = serial >> 32;
    cardSerialB = serial;
  }
#else
  SPreviousGameResults& PreviousGameResults() { return mPreviousGameResults; } // Guessed name
  int GetGameModeType() const { return mPreviousGameResults.mGameMode; } // name inferred

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
#endif
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

#ifdef TARGET_PC

  // Port: the five functions declared at the top of this header.
  friend CGameState* fn_801449C8(CGameState*);
  friend void fn_801440C0(CGameState*);
  friend void fn_80142DD4(CGameState*, int);
  friend void fn_80142CF8(CGameState*);
  friend void StreamNewGameState__5CMainFR12CInputStreami(CMain*, CInputStream&);
  // `CMain::ResetGameState` (`CMainResetGameState.cpp`) copies four of the blocks below out and
  // back, and `fn_80144140` (`CGameStateStreamCtor.cpp`, retail's unnamed stream constructor) fills
  // every one of them. The host declares `fn_80144140` with a different reader type
  // (`PortStreamNewGameState.cpp`), so that friend is the matching build's only.
  friend class CMain;
#ifdef __MWERKS__
  friend void fn_80144140(CGameState*, CInputStream&, int);
#endif

private:
  // Port: upstream's layout (under `#else`) with its three padding arrays split into the shapes
  // the port's retail-named units need. The offsets below are upstream's; the three arrays have been replaced by the shapes the port's
  // retail-named units need. **That replacement changes no offset and no total size**, which is
  // what makes it safe:
  //
  //   `char pad1[0x48]` at +0x00  ->  +0x00..+0x47, the eight rows below
  //   `char x110_[0x88]` at +0x110 ->  +0x110..+0x197, the four rows below
  //   `char x1a4_[0x60]` at +0x1A4 ->  +0x1A4..+0x1F3 (`x1a4_`) and +0x1F4..+0x203 (`x1f4`)
  //
  // Each replacement is a *split of padding only*: no member upstream names is renamed, retyped,
  // reordered or removed, and the sum of the two halves is the array's own length. What each one
  // buys is a name for bytes that a port unit addresses:
  //
  //   `pad1`  +0x08 `x08_reserve` and +0x18 `x18_playerStates` are the `rstl::reserved_vector`
  //           and the count of the four-entry array at +0x1C that `fn_801449C8` fills
  //           (`CGameStateCtor.cpp`), and +0x3C/+0x40 are retail's `rstl::rc_ptr<CWorldTransManagerView>`
  //           pair - `operator new(0x4B0)`'d pointer and a separately `new(4)`'d refcount word
  //           set to 1 (0x801441A0/0x801441C4).
  //   `x110_` +0x110/+0x144 are the two 0x34 `SGameStateSlots` that `fn_80144924` builds with
  //           `n = 3`, and +0x178/+0x188 two 16-byte `SGameStateBlock`s. The second one is pinned
  //           by `fn_80142CF8` (0x80142CF8): `addi r3,r31,376 ; bl fn_80142BA4 ; lwz r4,388(r31)`
  //           is construct-at-0x178 and read-0x184, so the shape's `x0c_data` is at 0x184.
  //   `x1a4_` +0x1F4 is the fifth `SGameStateBlock`, immediately after the `+0x1A0` block: its
  //           three non-`x00_unk` words are the `stw r0,504/508/512(r30)` at 0x801442CC/D4/D8.
  //
  // The `+0x1A0` block itself is reached through `mGameModeType` - retail's
  // `CMainFlow::AdvanceGameState` reads `lwz r4,416(r4)`, which is the same word as
  // `SGameStateWorlds::x00` - and the `+0x204` block through `mControlMapper`; both are named in
  // `SGameStateTail` and in `MetroidPrime/Player/CGameStateBlocks.hpp`.
  int x00_unk;                    //!< +0x00, -1. `stw r5,0(r3)` 0x80144160
  int x04_unk;                    //!< +0x04, -1. `stw r5,4(r30)` 0x8014416C
  SGameStateBlock x08_reserve;    //!< +0x08, 0x10 - the 36-byte-element block `fn_801466F4` walks
  int x18_playerStates;           //!< +0x18, the count of `x01c_players`
  CPlayerState* x01c_players[4][2]; //!< +0x1C, four 8-byte `{CPlayerState*, int*}` pairs
  CWorldTransManagerView* x3c_worldState; //!< +0x3C, retail: x0_ptr of an rc_ptr
  uint* x40_refCount;             //!< +0x40, retail: x4_refCount of the same rc_ptr, `new(4)`, `= 1`
  uint x44_unk;                   //!< +0x44, 4 bytes nothing in the DOL reads or writes

  double mTotalPlayTime;          //!< +0x48, `lfd f1,-25112(r2)` 0x801441CC (359999.0)
  float mEscapeTime;              //!< +0x50, `lfs f0,-25096(r2)` 0x801441D0 (100.0f)
  CPersistentOptions mSystemOptions; //!< +0x54, 0x2C
  CGameOptions gameOptions;       //!< +0x80, 0x44
  CHintOptions hintOptions;       //!< +0xC4, 0x18
  CPersistentOptions persistentOptions; //!< +0xDC, 0x2C
  u32 cardSerialA;                //!< +0x108
  u32 cardSerialB;                //!< +0x10C

  SGameStateSlots x110;           //!< +0x110, 0x34
  SGameStateSlots x144;           //!< +0x144, 0x34
  SGameStateBlock x178;           //!< +0x178, 0x10
  SGameStateBlock x188;           //!< +0x188, 0x10

  rstl::auto_ptr< CGameMode > mGameMode; //!< +0x198, 0x8 - `mHas` is retail's `x198_ptrSet`
                                          //!< (0x801442A8) and `mItem` its `x19c_ptr` (0x80144278)
  int mGameModeType;              //!< +0x1A0 - the first word of the `+0x1A0` block
  char x1a4_[0x50];               //!< +0x1A4 .. +0x1F3
  SGameStateBlock x1f4;           //!< +0x1F4, 0x10

  CControlMapper mControlMapper;  //!< +0x204, 0xE8
#else
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
#endif
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
void StartGameFromFrontEnd(); // Guessed name
void ConfigureGameModeLayers(); // Guessed name
void fn_80143E88();

#endif // _CGAMESTATE
