#include "MetroidPrime/Enemies/CStateMachine.hpp"

#include "Kyoto/Streams/CInputStream.hpp"

/**
 * Retail's own merged `.rodata` string pool entry `lbl_803AA230` (0x10 bytes, `symbols.txt:17242`);
 * the `"Default"` this compares against is the `+7` inside it, which is what both `Setup`
 * overloads address: `lis r4,0x803B ; addi r4,r4,-24016 ; addi r4,r4,7`.
 *
 * **Declared, never defined**: a literal of our own goes through mwcceppc's per-translation-unit
 * `@stringBase0` pool, which emits a `.rodata` section of its own (8 bytes here) that the linker
 * appends to the global pool and shifts every later entry - the same trap that keeps
 * `MetroidPrime/Factories/CStateMachineFactory.cpp` from flipping. Naming retail's entry resolves
 * the address without adding a section, and it is where retail's bytes already point.
 */
extern "C" const char lbl_803AA230[];

void CTrigger::Setup(const char* name, bool lnot, float arg, CTrigger* andTrigger) {
  if (name != nullptr) {
    strncpy(mName, name, sizeof(mName));
    mDefault = strncmp(mName, lbl_803AA230 + 7, 7) == 0 || mName[0] == '\0';
  }
  mArg = arg;
  mAndTrigger = andTrigger;
  mLNot = lnot;
}

void CTrigger::Setup(const char* name, bool lnot, float arg, CState* state) {
  if (name != nullptr) {
    strncpy(mName, name, sizeof(mName));
    mDefault = strncmp(mName, lbl_803AA230 + 7, 7) == 0 || mName[0] == '\0';
  }
  mArg = arg;
  mState = state;
  mLNot = lnot;
}

CState::CState(const char* name)
: mIndex(0), mNumTriggers(0), mFirstTrigger(nullptr), mComment(false) {
  strncpy(mName, name, sizeof(mName));
  if (strlen(mName) >= 2) {
    mComment = mName[0] == '/' && mName[1] == '/';
  }
}

CStateMachine::CStateMachine(CInputStream& in) {
  CTrigger* lastTrigger = nullptr;
  const int stateCount = in.ReadInt32();
  mStates.reserve(stateCount);

  char name[32];
  for (int i = 0; i < stateCount; ++i) {
    for (int j = 0; j < sizeof(name); ++j) {
      name[j] = in.ReadInt8();
      if (name[j] == '\0') {
        break;
      }
    }
    mStates.push_back_unsafe(CState(name));
  }

  mTriggers.reserve(in.ReadInt32());
  for (int i = 0; i < stateCount; ++i) {
    CState& state = mStates[i];
    const int firstTrigger = mTriggers.size();
    state.SetNumTriggers(in.ReadInt32());
    if (state.GetNumTriggers() == 0) {
      continue;
    }
    for (int j = 0; j < state.GetNumTriggers(); ++j) {
      mTriggers.push_back_unsafe(CTrigger());
    }
    state.SetTriggers(&mTriggers[firstTrigger]);

    for (int j = 0; j < state.GetNumTriggers(); ++j) {
      const int triggerCount = in.ReadInt32();
      for (int k = 0; k < triggerCount; ++k) {
        for (int n = 0; n < sizeof(name); ++n) {
          name[n] = in.ReadInt8();
          if (name[n] == '\0') {
            break;
          }
        }
        const float arg = in.ReadFloat();
        const bool lnot = name[0] == '!';
        const char* triggerName = lnot ? name + 1 : name;
        CTrigger* trigger;
        if (k < triggerCount - 1) {
          mTriggers.push_back_unsafe(CTrigger());
          trigger = &mTriggers.back();
        } else {
          trigger = state.GetTrig(j);
        }
        if (k == 0) {
          trigger->Setup(triggerName, lnot, arg, &mStates[in.ReadInt32()]);
        } else {
          trigger->Setup(triggerName, lnot, arg, lastTrigger);
        }
        lastTrigger = trigger;
      }
    }
  }

  for (int i = 0; i < mStates.size(); ++i) {
    mStates[i].SetIndex(i);
  }
  for (int i = 0; i < mTriggers.size(); ++i) {
    mTriggers[i].SetIndex(i);
  }
}

/**
 * `.text 0x801956B4` (eight bytes, `li r3,0` / `blr`) and `0x80194BF0` (four bytes, a bare `blr`).
 * They were `src/MetroidPrime/Carve801956B4.c` and `src/MetroidPrime/Carve80194BF0.c` on master;
 * upstream's `config/G2ME01/splits.txt` gives both ranges to this unit, so the bodies move here
 * and those files keep only their notes.
 *
 * Neither is called by a `bl` anywhere in the DOL, and `0x801956B4` is immediately followed by
 * `fn_801956BC`, so these are the out-of-line copies MWCC emits for empty inlines at file scope -
 * the same shape as `fn_80025E08` in `MetroidPrime/CAnimData.cpp` - and not members of
 * `CStateMachine` or `CStateFPC`. `config/G2ME01/symbols.txt` carries the `fn_<addr>` placeholder
 * for both, so the spellings are retail's own, and they are `extern "C"`: a C++ one would mangle
 * and objdiff would pair nothing.
 */
extern "C" int fn_801956B4() { return 0; }

extern "C" void fn_80194BF0() {}
