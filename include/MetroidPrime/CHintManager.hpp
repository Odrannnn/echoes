#ifndef _CHINTMANAGER
#define _CHINTMANAGER

#include "MetroidPrime/CHintState.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

class CGameHint;
class CStateManager;

// Guessed interface names, recovered from the shared hint runtime and its specializations.
class CHintManager {
public:
  CHintManager(int playerIndex, const rstl::string& name);
  virtual ~CHintManager();
  virtual void RefreshHint(CStateManager& mgr);
  virtual void Reset();
  virtual bool SetHint(CHintState* hint, CStateManager& mgr, bool areaChanged, bool force);
  virtual void ClearHint(CStateManager& mgr, bool areaChanged);
  virtual bool SelectHintFromStack(CHintState* hint, CStateManager& mgr, bool areaChanged);
  virtual void OnHintRemoved(CStateManager& mgr);

  // Guessed names. These queue changes for the manager's next update.
  void AddHint(TUniqueId hint, TUniqueId sender, CStateManager& mgr);
  void RemoveHint(TUniqueId hint, TUniqueId sender, CStateManager& mgr);
  void ForceRemoveHint(TUniqueId hint, CStateManager& mgr, TUniqueId sender);
  void Update(float dt, CStateManager& mgr);
  bool HasHint(const CStateManager& mgr) const;
  const CGameHint* GetCurrentHint(const CStateManager& mgr) const;
  CHintState* GetHintState(TUniqueId hint);
  const CHintState* GetHintState(TUniqueId hint) const;
  CHintState* GetBestHintState();

protected:
  int GetPlayerIndex() const { return mPlayerIndex; }
  int GetCurrentPriority() const { return mPriority; }
  void ClearCurrentHint(int priority) {
    mCurrentHintId = kInvalidUniqueId;
    mPriority = priority;
  }

private:
  // Guessed name. Sorting compares the priority only, not the record's other fields.
  struct SHint {
    SHint(const int& priority, const CHintState& state) : mPriority(priority), mState(state) {}
    bool operator<(const SHint& other) const { return mPriority < other.mPriority; }

    int mPriority;
    CHintState mState;
  };
  typedef rstl::pair< TUniqueId, TUniqueId > THintSender;

  static bool ContainsHint(const rstl::vector< SHint >& hints, TUniqueId hint);
  static bool ContainsHint(const rstl::vector< THintSender >& hints, TUniqueId hint);

  bool ProcessAddedHints(CStateManager& mgr);
  bool ProcessRemovedHints(CStateManager& mgr);
  bool AddHintState(int priority, const CHintState& hint);

  int mPlayerIndex;
  TAreaId mAreaId;
  TUniqueId mCurrentHintId;
  int mPriority;
  rstl::vector< SHint > mHints;
  rstl::vector< THintSender > mRemovedHints;
  rstl::vector< THintSender > mAddedHints;

public:
  // `fn_8022A5B4` (retail 0x8022A5B4) walks `mHints` and nothing else, so this is the whole of
  // the manager's interface that a caller needs.
  int GetNumHints() const { return static_cast< int >(mHints.size()); }
  const SHint& GetHint(int i) const { return mHints[i]; }
};
NESTED_CHECK_SIZEOF(CHintManager, SHint, 0x70)
CHECK_SIZEOF(CHintManager, 0x44)

// Retail's `fn_801B9480` (`0x801B9480`, 0x34 bytes): this manager's active hint, cast to
// `CGameHint`. It reads the hint id at `+0xC` of itself, which this tree's guessed layout does
// not model, so it cannot be a member yet. It sits in an unclaimed `.text` gap - between
// `Carve801B9420` and `Carve801B94B4` in `config/G2ME01/splits.txt` - so dtk fills it from
// retail and the declaration alone is what a caller needs, as for `fn_800D042C`.
extern "C" CGameHint* fn_801B9480(const CHintManager* self, CStateManager& mgr);

#endif // _CHINTMANAGER
