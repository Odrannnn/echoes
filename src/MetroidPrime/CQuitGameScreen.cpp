#include "MetroidPrime/CQuitGameScreen.hpp"

#include "GuiSys/CGuiFrame.hpp"
#include "GuiSys/CGuiTableGroup.hpp"
#include "GuiSys/CGuiWidget.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Input/CFinalInput.hpp"

// Retail plays its navigation sounds from plain `li` immediates rather than a load out of
// .sdata2 (0x80221E00 `li r4,1505`, 0x80221E54/0x80221E84 `li r4,1507`, 0x80222080
// `li r4,3068`), so the source spelled the ids as literals rather than named constants.

static const char* const skFrameNames[] = {"FRME_QuitScreen1", "FRME_QuitScreen2",
                                           "FRME_QuitScreen4"};

CQuitGameScreen::CQuitGameScreen(EQuitType type, int layout)
: mType(type)
, mFrame(gpSimplePool->GetObj(skFrameNames[layout]))
, mLoadedFrame(nullptr)
, mChoiceTable(nullptr)
, mAction(kQA_None)
, mTitle(nullptr)
, mYesChoice(nullptr)
, mNoChoice(nullptr) {
  mFrame.Lock();
}

CQuitGameScreen::~CQuitGameScreen() {}

void CQuitGameScreen::ProcessUserInput(const CFinalInput& input) {
  // Retail (0x80222028): forward to the loaded frame when there is one, then act on a button -
  // `lbz r0,42(r31); rlwinm. r0,r0,26,31,31`, i.e. bit 5 of btns3 - but only when mType is not
  // kQT_ContinueFromLastSave (`lwz r0,0(r30); cmpwi r0,1`). A cancel on the resume screen would
  // drop the player back into the game behind the menu, so retail skips it.
  //
  // Which accessor that bit belongs to is measured, not assumed: `rlwinm rA,rS,SH,31,31` tests
  // input bit 31-SH, and compiling one caller per accessor gives btns3 PA -> `rlwinm 25` (bit 6),
  // PB -> 26 (5), PX -> 27 (4), PY -> 28 (3), PZ -> 29 (2), PL -> 30 (1), PR -> 31 (0); DA -> 29
  // and DB -> 30 in btns1; PStart -> 28 (3) in btns4. So MWCC allocates these `uchar x : 1` fields
  // counting *down* from bit 6 rather than up from bit 0, and it does so per anonymous struct - the
  // rule holds across all three groups measured, but only 9 of the 32 accessors were checked, so
  // treat the rule as a guide and the shifts as the fact. CCredits is Matching at 49/49 and its
  // `input.PA()` emits `rlwinm 25`, which pins the packing against retail and not against this
  // probe alone. Retail's 26 is therefore PB(). The header's btns3 field names are guesses; what is
  // pinned here is the bit.
  if (mLoadedFrame == nullptr) {
    return;
  }
  mLoadedFrame->ProcessUserInput(input);
  if (input.PB() && mType != kQT_ContinueFromLastSave) {
    mAction = kQA_No;
    CSfxManager::SfxStart(3068, 127, 63, CSfxManager::kAllAreas, false, false,
                          CSfxManager::kMedPriority);
  }
}

void CQuitGameScreen::Draw() const {
  // TODO: draw the type-specific backdrop and frame through shared GUI draw parameters.
}

EQuitAction CQuitGameScreen::Update(float dt) {
  if (mLoadedFrame == nullptr && mFrame.IsLoaded()) {
    FinishedLoading();
  }
  return mAction;
}

void CQuitGameScreen::DoAdvance(CGuiTableGroup* caller) {
  // Retail (0x80221E28) reads the *caller's* selection, `lwz r0,200(r4)`, and both arms play the
  // same sound (1507) before setting the action: row 0 is Yes, row 1 is No. The table group's own
  // callbacks are not consulted.
  if (caller->GetUserSelection() == 0) {
    CSfxManager::SfxStart(1507, 127, 64, CSfxManager::kAllAreas, false, false,
                          CSfxManager::kMedPriority);
    mAction = kQA_Yes;
  } else {
    CSfxManager::SfxStart(1507, 127, 64, CSfxManager::kAllAreas, false, false,
                          CSfxManager::kMedPriority);
    mAction = kQA_No;
  }
}

void CQuitGameScreen::DoSelectionChange(CGuiTableGroup* caller, int oldSelection) {
  // Retail (0x80221DE4) recolours and plays the move sound (1505), pan 64. Neither argument is read.
  SetColors();
  CSfxManager::SfxStart(1505, 127, 64, CSfxManager::kAllAreas, false, false,
                        CSfxManager::kMedPriority);
}

void CQuitGameScreen::FinishedLoading() {
  mLoadedFrame = mFrame.GetObject();
  // TODO: bind the table/text panes, localize the choices, install callbacks and set defaults.
}

void CQuitGameScreen::SetColors() {
  // Retail (0x802219EC) builds both colours up front and then walks the choice table's two worker
  // widgets, recolouring the selected row light grey and the other dark grey. Two differences from
  // CGuiTableGroup::SetColors (fn_80279260, which CSaveGameScreen calls), both visible in the
  // disassembly: this inlines the loop instead of calling out, and it does *not* null-check the
  // widget the virtual returns - there is no `cmplwi r3,0; beq` between the `bctrl` and the
  // `bl SetColor`. Both choice rows are always present in FRME_QuitScreen*, so retail does not
  // guard.
  const CColor light(uchar(200), uchar(200), uchar(200), uchar(255));
  const CColor dark(uchar(50), uchar(50), uchar(50), uchar(255));
  const int selection = mChoiceTable->GetUserSelection();
  for (int i = 0; i < 2; i++) {
    CGuiWidget* widget = mChoiceTable->GetWorkerWidget(i);
    widget->SetColor(i == selection ? light : dark);
  }
}
