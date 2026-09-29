/**
 * Port-only: `CAudioStateWin` and `CConsoleOutputWindow` as real classes with real vtables.
 *
 * ## Why the configured units cannot serve the port
 *
 * `CAudioStateWinCtor.cpp` and `CConsoleOutputWindowCtor.cpp` are `Matching` DOL units, and they
 * match by declaring their own vtable-less `CIOWin` and storing retail's vtable *object*
 * (`lbl_803B3950`, `lbl_803B37F0`) into word 0. A host link has no retail object there - the
 * boot probe binds each to a zero-filled `reachdata_N` - so a window built that way has a null
 * vtable, and the first virtual call on it faults. That is exactly how the boot died at frame 1:
 * `fn_80049244`'s `PreDraw()` on the `CErrorOutputWindow` (priority 100000) jumped through heap
 * garbage, because its constructor was a reach stub. So the port builds these two from the real
 * headers here, and the DOL keeps its units.
 *
 * Bodies follow retail's own code, read from the vtables at `.data 0x803B3950` (CAudioStateWin:
 * dtor `fn_800E24D0`, OnMessage `fn_800E2530`, the rest `CIOWin`'s) and `.data 0x803B37F0`
 * (CConsoleOutputWindow: dtor `fn_800D6360`, OnMessage `fn_800D62F0`, Draw `fn_800D61A4`).
 * Where a body reads state the port has not named, it says so rather than guessing an offset.
 */
#include "MetroidPrime/CAudioStateWin.hpp"
#include "MetroidPrime/CConsoleOutputWindow.hpp"

#include "MetroidPrime/CArchitectureMessage.hpp"
#include "MetroidPrime/Decode.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"

#include <stdio.h>

// ---------------------------------------------------------------------------------------------
// CAudioStateWin, retail 0x800E24D0..0x800E2620
// ---------------------------------------------------------------------------------------------

CAudioStateWin::CAudioStateWin() : CIOWin(rstl::string_l("CAudioStateWin")) {}

CAudioStateWin::~CAudioStateWin() {}

CIOWin::EMessageReturn CAudioStateWin::OnMessage(const CArchitectureMessage& msg,
                                                 CArchitectureQueue&) {
  switch (msg.GetType()) {
  case kAM_SetGameState:
    // 0x800E2564-0x800E2570.
    CSfxManager::KillAll(CSfxManager::kSC_Game);
    CSfxManager::SetChannel(CSfxManager::kSC_Game);
    break;
  case kAM_QuitGameplay: {
    // 0x800E2578-0x800E25AC: `if (worldTransManager->xbc == 0 || gpMain->x58 != 0)
    // { SetChannel(kSC_Default); KillAll(kSC_Game); }`. Neither field has a name or a header in
    // this tree (there is no CWorldTransManager.hpp), and a guest offset is not a host offset,
    // so the test is not reproduced; the log line says so.
    static bool sWarned = false;
    if (!sWarned) {
      sWarned = true;
      printf("[CAudioStateWin] QuitGameplay: world-transition/restart test not ported - retail "
             "behaviour NOT reproduced, sfx channel left as is\n");
    }
    break;
  }
  default:
    break;
  }
  return kMR_Normal;
}

// ---------------------------------------------------------------------------------------------
// CConsoleOutputWindow, retail 0x800D61A4..0x800D65A4
// ---------------------------------------------------------------------------------------------

CConsoleOutputWindow* CConsoleOutputWindow::mInstance = nullptr;

// Retail 0x800D63F0. `x40_` is `632.f / fn_802BAD0C(&mFont, 48)`, the lines that fit on screen;
// `fn_802BAD0C` has no body in the port and neither does `CFont(float)` (see
// src/Kyoto/Text/CFontPortStub.cpp), so the count is left at 0 and each line holds only its
// terminator. Retail fills each line with `x40_ + 1` terminators; the content is the same.
CConsoleOutputWindow::CConsoleOutputWindow(int n, float a, float b)
: CIOWin(rstl::string_l("ConsoleOutputWindow"))
, mFont(b)
, mUnk(a)
, x40_(0)
, x44_(0)
, x48_(0) {
  mText.reserve(n);
  mUnkFloats.reserve(n);
  for (int i = 0; i < n; ++i) {
    mText.push_back(rstl::string_l(""));
    mUnkFloats.push_back(0.f);
  }
  mInstance = this;
}

// Retail 0x800D6360: clears the instance pointer, then the members and the base.
CConsoleOutputWindow::~CConsoleOutputWindow() { mInstance = nullptr; }

// Retail 0x800D62F0: only a timer tick does anything.
CIOWin::EMessageReturn CConsoleOutputWindow::OnMessage(const CArchitectureMessage& msg,
                                                       CArchitectureQueue&) {
  if (msg.GetType() == kAM_TimerTick) {
    Update(MakeMsg::GetParmTimerTick(msg).GetReal());
  }
  return kMR_Normal;
}

// Retail 0x800D62A8: every line's remaining display time counts down to zero.
void CConsoleOutputWindow::Update(float dt) {
  for (int i = 0; i < mUnkFloats.size(); ++i) {
    const float t = mUnkFloats[i] - dt;
    mUnkFloats[i] = t > 0.f ? t : 0.f;
  }
}

// Retail 0x800D61A4 sets the depth range, calls the renderer, then draws each line whose time is
// still positive, newest first. Nothing in the port writes a line (retail's writer is not
// decompiled), so every time is 0 and retail's loop would draw nothing; the text path, which
// needs the unwritten CFont, is not reproduced.
void CConsoleOutputWindow::Draw() const {
  for (int i = 0; i < mUnkFloats.size(); ++i) {
    if (mUnkFloats[i] > 0.f) {
      static bool sWarned = false;
      if (!sWarned) {
        sWarned = true;
        printf("[CConsoleOutputWindow] Draw: a live console line - text drawing not ported, "
               "retail behaviour NOT reproduced\n");
      }
      return;
    }
  }
}
