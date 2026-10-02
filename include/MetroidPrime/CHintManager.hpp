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
  // Retail's `SHint`: 0x70 bytes per entry, with the hint's `TUniqueId` at **+4**. Both are
  // measured, from three functions that walk the table identically -
  // `fn_801BA3EC` (0x801BA3EC), `fn_801BA428` (0x801BA428) and `fn_8022A5B4` (0x8022A5B4) all do
  // `lwz count,24(self)` / `lwz data,32(self)` / `mulli rX,count,112` / `add end,data,rX` and then
  // `addi rX,rX,112` per step, reading `lhz rX,4(entry)`. 112 = 0x70, and +4 is where the id is.
  // The hint's own runtime state fills the rest and nothing in the tree reads it, so it is left
  // as declared padding rather than guessed members.
  struct SHint {
    int x0_;         // x00, unknown: the entry's priority in the header's earlier reading
    TUniqueId mId;  // x04, the id `fn_8022A5B4` resolves and `fn_801BA3EC` searches for
    uchar x6_[0x6a]; // x06..x6f, the hint's runtime state; unnamed, unread by this tree
  };
  typedef rstl::pair< TUniqueId, TUniqueId > THintSender;

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
