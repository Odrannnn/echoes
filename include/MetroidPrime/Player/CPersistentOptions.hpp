#ifndef _CPERSISTENTOPTIONS
#define _CPERSISTENTOPTIONS

#include "Kyoto/SObjectTag.hpp"
#include "MetroidPrime/Player/CGameStateEnvVarManager.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

#ifdef TARGET_PC
class CEnvironmentVariable;
class CGameState;

// See `CGameState.hpp` - retail's unnamed `CGameState` constructor writes three words of this.
extern "C" CGameState* fn_801449C8(CGameState* self);

class CPersistentOptions {
#else
// Guessed name. The system-wide options include cinematics and the selected save slot.
class CPersistentOptions : public CGameStateEnvVarManager {
#endif
public:
  CPersistentOptions();
  explicit CPersistentOptions(CBitStreamReader& in);
  void InitializeMemoryState();
  void PutTo(CBitStreamWriter& out) const;
  bool GetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId) const;
  void SetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId, bool state);
  void SetSaveIdx(int idx) { mSaveIdx = idx; } // Guessed name
  int GetSaveIdx() const { return mSaveIdx; }

#ifdef TARGET_PC
  // Upstream inherits this from `CGameStateEnvVarManager`; the port's layout keeps it a member.
  CEnvironmentVariable* FindEnvironmentVariable(const char* name); // Guessed name

  // `fn_801449C8` (`CGameStateCtor.cpp`) is `CGameState`'s default constructor
  // under retail's unnamed symbol; it writes the words at +0x1C/+0x20/+0x24 of this member
  // (0x80144204/0x80144210/0x80144214). `fn_80144140` (the stream constructor) is not friended:
  // `PortStreamNewGameState.cpp` declares it with a `CBitStreamReader&`, and it reaches the same
  // words through the `SGameStateCardOpts` overlay in `CGameStateBlocks.hpp`.
  friend CGameState* fn_801449C8(CGameState*);
#endif

private:
#ifdef TARGET_PC
  // Upstream's `char x0_[0x28]`, split so the port's units can name the
  // three words `CGameState`'s constructor zeroes; 0x1C + 4 + 4 + 4 == 0x28.
  char x0_[0x1C];
  u32 x1c;
  u32 x20;
  u32 x24;
#else
  rstl::vector< rstl::pair< CAssetId, TEditorId > > mCinematicStates;
#endif
  int mSaveIdx;
};
CHECK_SIZEOF(CPersistentOptions, 0x2c)

#endif // _CPERSISTENTOPTIONS
