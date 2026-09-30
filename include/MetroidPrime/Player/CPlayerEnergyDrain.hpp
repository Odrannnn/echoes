#ifndef _CPLAYERENERGYDRAIN
#define _CPLAYERENERGYDRAIN

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "rstl/vector.hpp"

class CStateManager;

class CEnergyDrainSource {
public:
  CEnergyDrainSource(TUniqueId src, float intensity) : mSource(src), mIntensity(intensity) {}
  bool operator<(const CEnergyDrainSource& other) const { return mSource < other.mSource; }
  TUniqueId GetEnergyDrainSourceId() const { return mSource; }
  void SetEnergyDrainIntensity(float in) { mIntensity = in; }
  float GetEnergyDrainIntensity() const { return mIntensity; }

private:
  TUniqueId mSource;
  float mIntensity;
};
CHECK_SIZEOF(CEnergyDrainSource, 0x8)

// CEnergyDrainSource is trivially copyable, and retail's code for every rstl loop that
// copies one is a bare lhz/sth/lfs/stfs pair. Leaving it on the generic `new (dest) T(src)`
// path makes mwcceppc emit the placement-new null check on the destination (`cmplwi r,0` /
// `beq` around the store) in vector::insert_into and vector::erase, which retail never has.
namespace rstl {
RSTL_DECLARE_TRIVIALLY_CONSTRUCTIBLE(CEnergyDrainSource)
}

class CPlayerEnergyDrain {
public:
  CPlayerEnergyDrain(uint numSources);

  // Guessed name: clears the sources and elapsed drain time.
  void Clear();
  bool AddEnergyDrainSource(TUniqueId id, float intensity);
  void RemoveEnergyDrainSource(TUniqueId id);
  const rstl::vector< CEnergyDrainSource >& GetEnergyDrainSources() const { return mSources; }
  float GetEnergyDrainTime() const { return mEnergyDrainTime; }
  void ProcessEnergyDrain(const CStateManager& mgr, float dt);

private:
  rstl::vector< CEnergyDrainSource > mSources;
  float mEnergyDrainTime;
};
CHECK_SIZEOF(CPlayerEnergyDrain, 0x14)

#endif // _CPLAYERENERGYDRAIN
