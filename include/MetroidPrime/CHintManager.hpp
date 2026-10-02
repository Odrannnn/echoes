#ifndef _CHINTMANAGER
#define _CHINTMANAGER

#include "MetroidPrime/TGameTypes.hpp"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

class CGameHint;
class CStateManager;

// Supporting declaration only: the manager's remaining interface is not yet reconstructed.
class CHintManager {
public:
  virtual ~CHintManager();

  // Guessed names. These queue changes for the manager's next update.
  void AddHint(TUniqueId hint, TUniqueId sender, CStateManager& mgr);
  void RemoveHint(TUniqueId hint, TUniqueId sender, CStateManager& mgr);
  void ForceRemoveHint(TUniqueId hint, CStateManager& mgr, TUniqueId sender);
  void Update(float dt);

private:
  struct SHint; // Guessed name: priority followed by a runtime CHintState, stride 0x70.
  typedef rstl::pair< TUniqueId, TUniqueId > THintSender;

  int mPlayerIndex;
  TAreaId mAreaId;
  TUniqueId mCurrentHintId;
  int mPriority;
  rstl::vector< SHint > mHints;
  rstl::vector< THintSender > mRemovedHints;
  rstl::vector< THintSender > mAddedHints;
};
CHECK_SIZEOF(CHintManager, 0x44)

// Retail's `fn_801B9480` (`0x801B9480`, 0x34 bytes): this manager's active hint, cast to
// `CGameHint`. It reads the hint id at `+0xC` of itself, which this tree's guessed layout does
// not model, so it cannot be a member yet. It sits in an unclaimed `.text` gap - between
// `Carve801B9420` and `Carve801B94B4` in `config/G2ME01/splits.txt` - so dtk fills it from
// retail and the declaration alone is what a caller needs, as for `fn_800D042C`.
extern "C" CGameHint* fn_801B9480(const CHintManager* self, CStateManager& mgr);

#endif // _CHINTMANAGER
