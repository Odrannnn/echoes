#ifndef _CGUITABLEGROUP
#define _CGUITABLEGROUP

#include "GuiSys/CGuiWidget.hpp"
#include "Kyoto/Graphics/CColor.hpp"

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
  void SetColors(const CColor& selected, const CColor& unselected);
};

#endif // _CGUITABLEGROUP
