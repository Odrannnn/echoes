/**
 * `CPreFrontEnd`, the "Pre front-end window": retail 0x80192708..0x80192864, 0x15C bytes, four
 * functions and `vtable for CPreFrontEnd` at 0x803B5BA0. Written from the disassembly
 * (build/G2ME01/asm/auto_03_80192708_text.s); nothing here is guessed.
 *
 *   0x80192708  0x60  fn_80192708               ~CPreFrontEnd()          vtable slot 0
 *   0x80192768  0x24  fn_80192768               Draw() const             vtable slot 3
 *   0x8019278C  0x7C  fn_8019278C               OnMessage(...)           vtable slot 1
 *   0x80192808  0x5C  __ct__12CPreFrontEndFv    CPreFrontEnd()
 *
 * The vtable is `{ 0, 0, fn_80192708, fn_8019278C, GetIsContinueDraw__6CIOWinCFv, fn_80192768,
 * PreDraw__6CIOWinCFv }`, so `GetIsContinueDraw` and `PreDraw` are `CIOWin`'s and slot 3 is `Draw`.
 *
 * `CMainFlow::SetGameState` creates it for `kCFS_PreFrontEnd` when a restart mode is set, and on
 * the port that is the first frame. **Port-only**: no split covers the range (it is dtk's auto
 * gap `auto_03_80192708_text`), so there is no configure.py unit for it and main.dol cannot see
 * this file. `CMainFlowDtor.cpp` calls the constructor by its old placeholder name through
 * `extern "C"`, which is why `fn_80192808` below forwards to the real one.
 */
#include "MetroidPrime/CPreFrontEnd.hpp"

#include "MetroidPrime/CArchitectureMessage.hpp"
#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CMemoryCard.hpp"

#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"

#include <new>

// `lis r4,lbl_803AA210@ha ; bl string_l__4rstlFPCc ; bl __ct__6CIOWinF...`, then the vtable store.
CPreFrontEnd::CPreFrontEnd() : CIOWin(rstl::string_l("Pre front-end window")) {}

// Retail's is the compiler's: vtable store, `bl __dt__6CIOWinFv`, `Free` when the flag says so.
CPreFrontEnd::~CPreFrontEnd() {}

// Only the timer tick is handled. Until every pak has loaded the window stops the pump
// (`li r3,1`); after that it idles the resource factory for 1000000 (`lis r4,0xf ; addi
// r4,r4,0x4240`), pumps the memory card, and removes itself once `gpMemoryCard` exists.
CIOWin::EMessageReturn CPreFrontEnd::OnMessage(const CArchitectureMessage& msg,
                                               CArchitectureQueue&) {
  if (msg.GetType() != kAM_TimerTick) {
    return kMR_Normal;
  }
  if (!gpResourceFactory->GetResLoader().AreAllPaksLoaded()) {
    return kMR_Exit;
  }
  gpResourceFactory->AsyncIdle(1000000, false);
  gpMain->MemoryCardInitializePump();
  if (gpMemoryCard == nullptr) {
    return kMR_Exit;
  }
  return kMR_RemoveIOWinAndExit;
}

void CPreFrontEnd::Draw() const { CGraphics::SetIsBeginSceneClearFb(true); }

// `CMainFlowDtor.cpp` constructs through `extern "C" void* fn_80192808(void* self)` into storage
// from `__nw__FUlPCcPCc(20, ...)`; the constructor returns `this` in r3, as retail's does.
extern "C" void* fn_80192808(void* self) { return new (self) CPreFrontEnd(); }
