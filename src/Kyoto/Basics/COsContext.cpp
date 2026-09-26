// COsContext - the port's implementation of the SDK's OS context.
//
// Retail's COsContext is a thin wrapper over the Dolphin OS: it owns the
// console type, the arena bounds the allocator carves MEM1 out of, and the two
// external framebuffers the video interface presents. On a PC none of the
// hardware exists, so every method below answers the question "what is the PC
// equivalent, and is it already in Aurora?" rather than "what did retail do":
//
//   - the MEM1 arena, the VI and the render mode *are* real, because Aurora
//     emulates them (`OSInit`/`OSAllocFromArenaLo`, `VIConfigure`);
//   - the window itself is not, because `aurora_initialize` has already created
//     it from `AuroraConfig` before the game is entered, so `OpenWindow` is an
//     adapter over Aurora's VI rather than a window setup;
//   - the DVD is Aurora's `aurora_dvd_open`, which the entry point has already
//     called by the time anything here runs;
//   - the keyboard and the memory card have no PC equivalent at all, so the
//     methods that would query them are deliberate no-ops and say so.
//
// Reference implementation: ../MetroidPrimePort `src/Kyoto/Basics/
// COsContextDolphin.cpp`, which reaches the same conclusions for the same
// engine. Differences from it are called out below.

#include "Kyoto/Basics/COsContext.hpp"

#include "Kyoto/Basics/CStopwatch.hpp"

#include "dolphin/gx/GXFrameBuffer.h"
#include "dolphin/os.h"
#include "dolphin/vi.h"

#include <string.h>

// Retail keeps this flag in the OS context and hands it to the video interface
// here; the game reaches it through CGraphics::SetProgressiveMode /
// GetProgressiveMode (include/Kyoto/Graphics/CGraphics.hpp:366), which have no
// definition in this tree yet. Nothing references the symbol today, so this is
// here to give the class its static storage.
//
// Note for whoever decompiles `MetroidPrime/main.cpp`: retail defines the symbol
// in the DOL's main object, and the Metroid Prime port does the same at
// `src/MetroidPrime/main.cpp:146`. Defining it in both files would be a
// duplicate symbol in a real link, so it belongs in exactly one - this one,
// because that is where the rest of the class's definitions live. If upstream
// ends up claiming it, delete this line rather than that one.
bool COsContext::mProgressiveMode;

COsContext::COsContext(bool, bool) :
    // Retail reads the console type out of the OS and leaves the rest for
    // OpenWindow. The two arguments are unused in retail too, and nothing in
    // the tree can say what they select.
    //
    // Everything else is zeroed here, and that is a port decision rather than
    // retail's: `platform/main.cpp` constructs the memory system immediately
    // after this object, so `CMemory::Startup` -> `CGameAllocator::Initialize`
    // -> `GetBaseFreeRam()` runs *before* anything ever calls OpenWindow. The
    // arena bounds therefore have to be real from the constructor, and the
    // accessors have to have defined values to return, rather than reading
    // uninitialised memory. GetFramebuf1/2 legitimately answer null until
    // OpenWindow runs, because until then there is no framebuffer.
    x0_right(0),
    x4_bottom(0), x8_left(0), xc_top(0),
    // **Both names changed with the measurement, and the two words were the wrong way round.**
    // +0x10 is the *console type* and +0x14 is a *language* - retail's constructor stores
    // `OSGetLanguage() & 0xF` at +0x14 *before* `CBasics::Init` and the EConsoleType at +0x10
    // after it. The header had `x10_format` (a TV-format code) and `x14_consoleType`, so each
    // word was named for the other one's contents. See `COsContextCtor.cpp` and the note on
    // `include/Kyoto/Basics/COsContext.hpp`.
    //
    // Both are initialised here as well as being set below, so that no path through this
    // constructor leaves either word unread - which is the same reason every other member
    // above is given a defined value.
    x10_consoleType(kCT_Retail), x14_language(0),
    x18_arenaLo1(nullptr), x1c_arenaHi(nullptr), x20_arenaLo2(nullptr),
    x24_frameBuffer1(nullptr), x28_frameBuffer2(nullptr), x2c_frameBufferSize(0),
    x30_renderMode() {
  // Retail's constructor calls CBasics::Init(), which is OSInit + OSInitFastCast
  // + DVDInit + CStopwatch::InitGlobalTimer. On PC those split three ways:
  //
  //   - OSInit is Aurora's, and it is the only thing that maps MEM1 and sets the
  //     arena bounds. Nothing else in the port calls it - not
  //     aurora_initialize, not platform/main.cpp - so without this line
  //     OSGetArenaLo() and OSGetArenaHi() are both null,
  //     GetBaseFreeRam() returns 0, and CGameAllocator::Initialize underflows
  //     its heap size and throws std::bad_alloc. This is the one call in the
  //     file that the port cannot do without.
  //   - OSInitFastCast has no PC meaning (compat.h's CBasics::SwapBytes is the
  //     identity on a little-endian host), and DVDInit is Aurora's
  //     aurora_dvd_open, which platform/main.cpp has already called by the time
  //     anything constructs a context.
  //   - CStopwatch::InitGlobalTimer is the game's own half and is called
  //     directly instead, because CBasics::Init() itself has no definition in
  //     this tree: it is a decompilation unit (an Object() line in
  //     configure.py), not port code.
  OSInit();
  CStopwatch::InitGlobalTimer();

  // The arena is live now, so the constructor can publish its bounds. Retail
  // gets these from OSInit directly; here they have to be captured, because
  // every later bump goes through AllocFromArena and OpenWindow, which update
  // them, and GetBaseFreeRam() reads them between those calls.
  x18_arenaLo1 = OSGetArenaLo();
  x1c_arenaHi = OSGetArenaHi();
  x20_arenaLo2 = OSGetArenaLo();

  // Retail's switch over OSGetConsoleType(). Aurora has no console at all, and
  // platform/sdk_stubs.cpp answers 0 - OS_CONSOLE_RETAIL - which the Metroid
  // port's copy of this switch does not name, so there it is left unassigned.
  // Naming it (and the retail revision case) is the whole port-side difference,
  // and it is also why the switch needs a default: the local has to be assigned
  // on every path, whatever the SDK reports.
  switch (OSGetConsoleType()) {
  case OS_CONSOLE_RETAIL:
  case OS_CONSOLE_RETAIL1:
    x10_consoleType = kCT_Retail;
    break;
  case OS_CONSOLE_DEVHW1:
    x10_consoleType = kCT_Development1;
    break;
  case OS_CONSOLE_DEVHW2:
  case OS_CONSOLE_DEVHW3:
    x10_consoleType = kCT_Development2Or3;
    break;
  case OS_CONSOLE_EMULATOR:
    x10_consoleType = kCT_Emulator;
    break;
  default:
    x10_consoleType = kCT_Retail;
    break;
  }
}

COsContext::~COsContext() {
  // Nothing to undo. The window belongs to Aurora, which `aurora_shutdown` in
  // platform/main.cpp destroys after the game returns, and the MEM1 arena is a
  // bump allocator that is never individually freed - retail has the same
  // property, and freeing a framebuffer here would only hand memory back to a
  // region nothing reads.
}

// Retail waits for the video retrace here and reports false when the program
// should stop. Its one caller is CInputGenerator::Update, which does
// `if (!x0_context->Update()) return false;` - so the return value is a
// "keep generating input" flag and nothing else.
//
// On PC the pump is the platform's, and it already lives in the game's main
// loop (`aurora_update()`, which drains Aurora's window and input events and is
// where the Metroid Prime port reads AURORA_EXIT to end the frame). Pumping it
// a second time from the input path would consume events the loop expects to
// see, and the input path runs more than once per frame. So: continue.
bool COsContext::Update() { return true; }

// Deliberate no-op. Retail reads the state of a Dolphin keyboard key code; the
// port's input is Aurora's controller layer, which CFinalInput and
// DolphinIController already read, and no PC equivalent of "is raw key N down"
// exists for the game to ask. Returning "nothing pressed" is the same answer
// the Metroid Prime port gives, and the bitfields the callers ask for
// (IsPressed/JustPressed) are both false.
COsKeyState COsContext::GetOsKeyState(int key) const {
  return COsKeyState(key, false, false, false, false);
}

// Aurora's arena is a real bump allocator over the MEM1 block that `OSInit`
// mapped from AuroraConfig::mem1Size, so this is retail's behaviour and it works
// unchanged. It is the same arena the game's own heap comes out of -
// CGameAllocator::Initialize calls OSAllocFromArenaLo directly - so the game
// heap, the two framebuffers below and whatever else reaches for this method all
// share one MEM1 budget. The three arena fields are refreshed after every bump
// because GetBaseFreeRam() is what the game's allocator sizes itself against.
void* COsContext::AllocFromArena(size_t sz) {
  void* ret = OSAllocFromArenaLo(static_cast< u32 >(sz), 32);

  x20_arenaLo2 = OSGetArenaLo();
  x18_arenaLo1 = OSGetArenaLo();
  x1c_arenaHi = OSGetArenaHi();
  return ret;
}

// The window/VI bring-up, and the interesting one. Aurora has already created
// the window by the time the game is entered, so this is not a window setup:
// it is the point where the game tells Aurora how big its EFB is, which is all
// `VIConfigure` takes and all Aurora's presenter needs. Everything else in the
// signature is retail's own window management - a title, a position and a
// fullscreen flag for a window that does not exist yet - and the title in
// particular is set once, in AuroraConfig::appName, so honouring the argument
// here would fight the entry point. Aurora does expose VISetWindowTitle and
// friends under TARGET_PC; the Metroid Prime port declines them for the same
// reason and the port stays consistent with it.
//
// Returns -1, as the Metroid Prime port's does: the value is retail's and its
// only caller, CMain::OpenWindow, ignores it.
int COsContext::OpenWindow(const char* /*title*/, int /*x*/, int /*y*/, int w, int h,
                           bool /*fullscreen*/) {
  // VIInit and VIFlush are no-ops in Aurora (VIFlush has no meaning without a
  // real VI); they are kept because they are the retail sequence and because a
  // future Aurora with a real VI would want them.
  VIInit();

  // Aurora's GXAdjustForOverscan takes its template by non-const pointer, hence
  // the local's type.
  //
  // Aurora answers VI_NTSC unconditionally, so this lands on the NTSC case
  // today. The other two are kept because the switch is retail's and the enum
  // above the class names all three; the default is unreachable on PC and only
  // exists so the local is never read uninitialised.
  GXRenderModeObj* rModeObj;
  switch (VIGetTvFormat()) {
  case VI_NTSC:
    rModeObj = &GXNtsc480IntDf;
    break;
  case VI_PAL:
    rModeObj = &GXPal528IntDf;
    break;
  case VI_MPAL:
    rModeObj = &GXMpal480IntDf;
    break;
  default:
    rModeObj = &GXNtsc480IntDf;
    break;
  }

  // Retail writes w/h into the VI registers and then calls GXAdjustForOverscan,
  // which reads those registers back - so on hardware the request survives. On
  // PC Aurora's GXAdjustForOverscan takes the mode as an explicit template and
  // copies it whole into the out parameter, which is why the Metroid Prime
  // port's version of this function writes w/h into x30_renderMode first and
  // then has them overwritten: the arguments are dead there. Applying them to
  // the template instead is the same intent, and it is what lets whoever widens
  // the render mode (PORT_NOTES' widescreen work) do it through this argument.
  GXRenderModeObj mode = *rModeObj;
  if (w > 0) {
    mode.viWidth = static_cast< u16 >(w);
  }
  if (h > 0) {
    mode.viHeight = static_cast< u16 >(h);
  }

  // Retail hides the console's overscan border. Aurora's copy only insets the
  // visible VI rectangle and deliberately leaves the EFB and XFB at the game's
  // logical size, because those drive GXSetViewport/GXSetScissor.
  GXAdjustForOverscan(&mode, &x30_renderMode, 0, 16);

  x8_left = x30_renderMode.viXOrigin;
  xc_top = x30_renderMode.viYOrigin;
  x0_right = x30_renderMode.viWidth;
  x4_bottom = x30_renderMode.viHeight;

  // Two external framebuffers, 16-byte aligned rows, 2 bytes per pixel. They
  // come out of Aurora's MEM1 arena like everything else the game allocates.
  x2c_frameBufferSize = (static_cast< int >(x30_renderMode.fbWidth) + 15) & ~15;
  x2c_frameBufferSize *= x30_renderMode.xfbHeight;
  x2c_frameBufferSize *= 2;

  x24_frameBuffer1 = OSAllocFromArenaLo(x2c_frameBufferSize, 32);
  x28_frameBuffer2 = OSAllocFromArenaLo(x2c_frameBufferSize, 32);
  x20_arenaLo2 = OSGetArenaLo();
  x18_arenaLo1 = OSGetArenaLo();
  x1c_arenaHi = OSGetArenaHi();

  x30_renderMode.viWidth += 20;
  x30_renderMode.viXOrigin -= 10;

  if (mProgressiveMode) {
    x30_renderMode.viTVmode = VI_TVMODE_NTSC_PROG;
    x30_renderMode.xFBmode = VI_XFBMODE_SF;
    const uchar progressiveFilterPattern[7] = {4, 4, 16, 16, 16, 4, 4};
    memcpy(x30_renderMode.vfilter, progressiveFilterPattern, 7);
  }

  // The one call Aurora actually acts on: it publishes the EFB/XFB size the
  // game's GX work will produce, and the presenter scales it to the window.
  VIConfigure(&x30_renderMode);
  VIFlush();
  return -1;
}
