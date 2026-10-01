#ifndef _CPERSISTENTOPTIONS
#define _CPERSISTENTOPTIONS

#include "Kyoto/SObjectTag.hpp"
#include "MetroidPrime/Player/CGameStateEnvVarManager.hpp"
#include "MetroidPrime/TGameTypes.hpp"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

// Guessed name. The system-wide options include cinematics and the selected save slot.
class CPersistentOptions : public CGameStateEnvVarManager {
public:
  CPersistentOptions();
  explicit CPersistentOptions(CBitStreamReader& in);
  void InitializeMemoryState();
  void PutTo(CBitStreamWriter& out) const;
  bool GetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId) const;
  void SetCinematicState(rstl::pair< CAssetId, TEditorId > cinematicId, bool state);
  void SetSaveIdx(int idx) { mSaveIdx = idx; } // Guessed name
  int GetSaveIdx() const { return mSaveIdx; }


private:
  rstl::vector< rstl::pair< CAssetId, TEditorId > > mCinematicStates;
  int mSaveIdx;
};

// `rstl/pair.hpp` has the trivial-assignment overload of `construct_impl` for `pair<uint, uint>`
// only. `pair<CAssetId, TEditorId>` is the same two trivially copyable words, and without this
// overload `rstl::construct` falls to placement `new`, which mwcceppc guards with a null test on
// the destination - retail's `CPersistentOptions::SetCinematicState` (0x80142250) stores the new
// element with no such test. Declared here, not in `rstl/pair.hpp`, so `rstl` keeps no
// MetroidPrime type. Only `construct_impl` is added: an `is_trivially_destructible` specialisation
// would change every `destroy` loop over this pair, and `~CPersistentOptions` matches today.
namespace rstl {
inline void construct_impl(void* dest, const pair< CAssetId, TEditorId >& src) {
  *static_cast< pair< CAssetId, TEditorId >* >(dest) = src;
}
} // namespace rstl

CHECK_SIZEOF(CPersistentOptions, 0x2c)

#endif // _CPERSISTENTOPTIONS
