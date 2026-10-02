#ifndef _CGAMEHINT
#define _CGAMEHINT

#include "MetroidPrime/CActor.hpp"

#include <string.h>

class CHintState; // Guessed name: the separate runtime hint record, not an actor.

// Guessed name: the Wii CGameHint::EBreakHintType export suggests this common hint base.
class CGameHint : public CActor {
public:
  // Guessed enumerator names; EBreakHintType is exported by the Echoes Wii build.
  enum EBreakHintType {
    kBHT_None,
    kBHT_Fire,
    kBHT_Turn,
    kBHT_Jump,
    kBHT_TriggersAndB,
    kBHT_Unknown5
  };

  // Guessed name. Type-erased callback copied into the runtime hint state.
  struct SCallback {
    typedef void (*FInvoke)(void*, const void*, CStateManager&, CHintState&);

    SCallback() : mInvoke(nullptr), mContext(nullptr) { memset(mCallable, 0, sizeof(mCallable)); }

    bool IsNull() const {
      for (int i = 0; i < sizeof(mCallable); ++i) {
        if (mCallable[i] != 0) {
          return false;
        }
      }
      return true;
    }

    void operator()(CStateManager& mgr, CHintState& state) const {
      if (!IsNull()) {
        mInvoke(mContext, mCallable, mgr, state);
      }
    }

    FInvoke mInvoke;
    void* mContext;
    char mCallable[16]; // Original callable representation remains unresolved.
  };

  CGameHint(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
            const CTransform4f& xf, int priority, float timer, int acrossAreas,
            EBreakHintType breakType, uint deleteOnRemoval, uint requiredPresses, float unknown16c,
            SCallback onExpire, SCallback onBreak, float breakDelay);

  // CEntity
  ~CGameHint() override = 0;
  CEntity* TypesMatch(int typeId) const override;
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) override;

  int GetPriority() const { return mPriority; }
  float GetTimer() const { return mTimer; }
  EBreakHintType GetBreakType() const { return mBreakType; }
  bool GetDeleteOnRemoval() const { return mDeleteOnRemoval == 1; }
  uint GetRequiredPresses() const { return mRequiredPresses; }
  float GetUnknown16c() const { return x16c_; }
  const SCallback& GetOnExpire() const { return mOnExpire; }
  const SCallback& GetOnBreak() const { return mOnBreak; }
  float GetBreakDelay() const { return mBreakDelay; }
  bool GetAcrossAreas() const { return mAcrossAreas == 1; }

private:
  int mPriority;
  float mTimer;
  EBreakHintType mBreakType;
  uint mDeleteOnRemoval;
  uint mRequiredPresses;
  float x16c_;
  SCallback mOnExpire;
  SCallback mOnBreak;
  float mBreakDelay;
  int mAcrossAreas;
};
CHECK_SIZEOF(CGameHint, 0x1a8)
NESTED_CHECK_SIZEOF(CGameHint, SCallback, 0x18)

// Retail's control-hint actor, `EEntityType` 46 (`docs/research/missing_classes.md`: the
// `LoadControlHint` / `CTLH` loader is `10CUnknown46`, `fn_8022CEFC`, 752 bytes, and its
// `operator new` argument is 0x2F0). It is the class `fn_8022A5B4` `TCastToPtr`s to, and
// `CGameHint` is 0x1A8, so its **first member is at 0x1A8** - the word
// `CPlayerGunBase::ProcessInput` tests against the caller's mask. Both facts are measured:
// `fn_8022CEFC` writes it as its second thing (`stw r30,424(r29)`, immediately after the base
// class constructor call, with `li r0,19` in r30) and `fn_8022A5B4` reads it as
// `lwz r0,424(r3)`.
//
// Only the first member is modelled. The remaining 0x148 is the rest of the hint's own state,
// which no function in the tree reads, and `CHECK_SIZEOF` is deliberately omitted rather than
// guessed: a wrong total would be a claim this tree cannot back.
class CUnknown46 : public CGameHint {
public:
  // `CEntity`
  ~CUnknown46();
  CEntity* TypesMatch(int typeId) const;

  // The control flags `fn_8022A5B4` masks against. Guessed name; the value is retail's 19 in
  // the `CTLH` constructor and 0 in `fn_8022CEFC`'s own initialisation path.
  int GetControlFlags() const { return mControlFlags; }

private:
  int mControlFlags; // x1a8
};

#endif // _CGAMEHINT
