#ifndef _CGUITABLEGROUP
#define _CGUITABLEGROUP

#include "GuiSys/CGuiWidget.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/TFunctor.hpp"

// Echoes' GuiSys has a CGuiTableGroup widget - the choice list inside the save-game screen's
// FRME_GenericMenu. No unit in this tree decompiles it yet, so this header declares only what
// MetroidPrime/CSaveGameScreen.cpp needs. Every method below is defined in a GuiSys unit that is
// still an auto/* split (SetColors is retail's fn_80279260), so they are declared and never
// defined here. Do not add members without evidence for their offsets: callers read fields
// straight out of the object, and a guessed layout reads the wrong bytes.
//
// The CGuiWidget base is evidence, not a guess: PumpLoad's FindWidget returns a CGuiWidget* that
// is stored in mTablegroupChoices, and SetUIText passes it straight to SetIsActive, which retail
// defines on CGuiWidget (fn_8027D84C). Nothing in CSaveGameScreen reads through the base, so the
// inherited layout is never depended on here.
class CGuiTableGroup : public CGuiWidget {
public:
  // fn_802794D4 and fn_802794A0: each copies the 24-byte callback record wholesale out of the
  // argument into its own member, which is why they are declared but never written here.
  void SetMenuAdvanceCallback(const TFunctor1< CGuiTableGroup* const >& func);
  void SetMenuSelectionChangeCallback(const TFunctor2< CGuiTableGroup* const, const int >& func);
  void SetColors(const CColor& selected, const CColor& unselected);

  // Retail inlines this one too: 0x8017DD14 is `lwz r0,200(r3); stw r0,204(r3); stw 0,200(r3)`.
  void SetUserSelection(int sel) {
    mPrevUserSelection = mUserSelection;
    mUserSelection = sel;
  }

  // CSaveGameScreen::DoAdvance (0x8017C620) reads `lwz r5,200(r4)` off the caller once and
  // dispatches on it in every arm.
  int GetUserSelection() const { return mUserSelection; }

  // The layout below is measured from retail, not guessed: SetUIText (0x8017DD14) is
  // `lwz r0,200(r3); stw r0,204(r3); stw 0,200(r3)` on mTablegroupChoices, and fn_802794D4 /
  // fn_802794A0 copy the two 24-byte callback records to 212..232 and 260..284. Everything in
  // between is this unit's own CGuiTableGroup body, which nothing here reads, so the gaps are
  // padding rather than invented. Do not add CHECK_SIZEOF: retail's CGuiTableGroup is 0x11c in
  // Prime 1 and this header stays a facade until a GuiSys unit declares the real one.
private:
  char mUnknown_0xBC[12];
  int mUserSelection;     // 0xc8
  int mPrevUserSelection; // 0xcc
  char mUnknown_0xD0[4];
  TFunctor1< CGuiTableGroup* const > mDoMenuAdvance; // 0xd4
  char mUnknown_0xEC[24];
  TFunctor2< CGuiTableGroup* const, const int > mDoMenuSelChange; // 0x104
};

#endif // _CGUITABLEGROUP
