#ifndef _CHINTOPTIONS
#define _CHINTOPTIONS

#include "types.h"

#include "rstl/string.hpp"
#include "rstl/vector.hpp"

enum EHintState { kHS_Zero, kHS_Waiting, kHS_Displaying, kHS_Delayed };

class CStateManager;
class CBitStreamReader;
class CBitStreamWriter;

// Retail's copy assignment, 0x801447C4, claimed by
// `src/MetroidPrime/Player/CGameState.cpp`. Declared here, with C linkage and **before** the
// class, so that the `friend` below names this entity: befriending it first would declare it
// with C++ linkage, and mwcceppc then emits `fn_801447C4__F...` instead of `fn_801447C4`, which
// leaves retail's 84 bytes unclaimed. Both parameters are untyped pointers for the same reason.
extern "C" void* fn_801447C4(void* self, const void* src);

class CHintOptions {
public:
  struct SHintState {
    SHintState();
    SHintState(EHintState state, float time);

    EHintState mState;
    float mTime;
    float mDismissalTimer; // Guessed name

    bool CanContinue() const;
    bool IsDismissed() const { return mDismissalTimer > 0.f; } // Guessed name
  };

  friend void* fn_801447C4(void*, const void*);

  CHintOptions();
  explicit CHintOptions(CBitStreamReader& in);
  void PutTo(CBitStreamWriter& out) const;
  void InitializeMemoryState();

  void EnsureHintNextTime(); // Prime PAL name; Echoes only has this max() variant
  void Update(float dt, CStateManager& mgr);

  void DelayHint(const rstl::string& name);
  void ActivateImmediateHintTimer(const rstl::string& name);
  void ActivateContinueDelayHintTimer(const rstl::string& name);
  void DismissDisplayedHint();

  const SHintState* GetCurrentDisplayedHint() const;
  int GetNextHintIdx();
  const rstl::vector< SHintState >& GetHintStates() const { return mHintStates; }

private:
  static uint GetBitCount(uint value);

  rstl::vector< SHintState > mHintStates;
  int mNextHintIdx;
  bool mInRezbitState;     // Guessed name
  bool mScanDisplayActive; // Guessed name
};
NESTED_CHECK_SIZEOF(CHintOptions, SHintState, 0xc)
CHECK_SIZEOF(CHintOptions, 0x18)

#endif // _CHINTOPTIONS
