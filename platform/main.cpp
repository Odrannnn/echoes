// Port entry point for Metroid Prime 2: Echoes.
//
// Aurora owns the real process entry, so this is what it calls. The order below
// is the one the Metroid Prime port proves: bring Aurora up, mount the disc,
// verify it is the disc this port is built for, then hand control to the game
// and shut the platform down afterwards.
//
// The game's entry is `InvokeCMain` (src/MetroidPrime/main.cpp:79), which builds
// `CMain` and calls `RsMain`. It takes the OS context, the memory system and the
// graphics system from its caller, and that caller is not decompiled - so the three
// objects are built here in the shape the caller will need. `COsContext`'s and
// `CMemorySys`'s constructors are host work (`src/Kyoto/Basics/COsContext.cpp` and
// the SDK), and `CGraphicsSys`'s is `src/Kyoto/Graphics/CGraphicsHostStartup.cpp`,
// which carries retail's `CGraphics::Startup` chain - so the VI bring-up this
// translation unit used to leave to a `CMain::OpenWindow` stand-in inside
// `RsMain` now happens here, where retail does it. Everything else here (Aurora
// setup, disc mounting and checking, teardown) is settled. See PORT_NOTES.md.

#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <dolphin/gx/GXAurora.h>

#include <cstdio>
#include <csignal>
#include <execinfo.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "Kyoto/Alloc/CMemorySys.hpp"
#include "Kyoto/Basics/COsContext.hpp"
#include "Kyoto/Graphics/CGraphicsSys.hpp"
#include "compiled_modules.h"
#include "port_entry.h"
#include "port_tweaks.h"

// The decompilation's exported entry point; see src/MetroidPrime/main.cpp.
extern "C" void InvokeCMain(int argc, char** argv, COsContext* context, void* unk1,
                            CMemorySys* memorySys, void* unk2);

namespace {

const char* kAppName = "Metroid Prime 2: Echoes";

std::string ExecutableDir(const char* argv0) {
  if (argv0 == nullptr || argv0[0] == '\0') {
    return {};
  }
  std::error_code error;
  const std::filesystem::path path = std::filesystem::absolute(argv0, error);
  if (error || !path.has_parent_path()) {
    return {};
  }
  return path.parent_path().string();
}

void ReportNoDisc(const char* argv0) {
  std::fprintf(stderr,
               "metroid_prime2_port: no disc image given.\n"
               "  usage: %s <path to Metroid Prime 2: Echoes (USA) (v1.00).iso>\n"
               "  or set MP2_DISC, or place the image next to the executable.\n",
               argv0 != nullptr ? argv0 : "metroid_prime2_port");
}

} // namespace

// A SIGSEGV that prints where it happened. `tools/boot_probe.sh` reports "died on signal 11"
// and nothing else, which is enough to know the boot died and not enough to fix it: the last
// `printf` before the marker says which *step*, and everything inside that step is a guess.
// A backtrace turns the guess into a measurement, and it costs one signal handler.
//
// Deliberately not a substitute for a debugger: this has to work in the exact binary
// `boot_probe.sh` builds, with the reach stubs linked and no debugger attached, because that
// is the configuration that reproduces the fault. `-rdynamic` is already in the link line, so
// `backtrace_symbols` can name our own functions rather than only addresses.
static void PortFaultHandler(int sig) {
  const char* name = (sig == SIGSEGV) ? "SIGSEGV" : (sig == SIGBUS ? "SIGBUS" : "signal");
  std::fprintf(stderr, "\n[port] caught %s (%d) - backtrace follows\n", name, sig);
  std::fflush(stderr);
  void* frames[64];
  const int n = ::backtrace(frames, 64);
  ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
  std::fflush(stderr);
  // Re-raise with the default handler so the exit status still says "died on signal 11",
  // which is what boot_probe.sh greps for. A handler that swallows it would make the probe
  // report a clean exit, and a clean exit from a segfaulting boot is the one lie worth
  // being unable to tell.
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

int main(int argc, char** argv) {
  std::signal(SIGSEGV, PortFaultHandler);
  std::signal(SIGBUS, PortFaultHandler);
  std::signal(SIGILL, PortFaultHandler);
  std::signal(SIGFPE, PortFaultHandler);

  // 16:9 at the game's logical height. The render mode is widened to match when
  // the widescreen work lands; Aurora upscales to the window either way.
  const AuroraConfig config = {
      .appName = kAppName,
      .userPath = std::getenv("MP2_USER_PATH"),
      .cachePath = std::getenv("MP2_CACHE_PATH"),
      .resourcesPath = nullptr,
      .desiredBackend = BACKEND_AUTO,
      .vsync = false,
      .allowTextureDumps = false,
      .windowWidth = 854u,
      .windowHeight = 480u,
      .mem1Size = MEM1_DEFAULT_SIZE,
      .mem2Size = ARAM_DEFAULT_SIZE,
  };

  aurora_initialize(argc, argv, &config);
  // Keep the internal framebuffer at the game's aspect rather than the window's,
  // so a mismatched window shape letterboxes instead of stretching.
  AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);

  const std::string discImage = port::entry::ResolveDiscPath(
      argc, argv, std::getenv("MP2_DISC"), ExecutableDir(argc > 0 ? argv[0] : nullptr));
  if (discImage.empty()) {
    ReportNoDisc(argc > 0 ? argv[0] : nullptr);
    aurora_shutdown();
    return 1;
  }

  if (!aurora_dvd_open(discImage.c_str())) {
    std::fprintf(stderr, "metroid_prime2_port: failed to open disc image: %s\n", discImage.c_str());
    aurora_shutdown();
    return 1;
  }

  const DVDDiskID* discId = DVDGetCurrentDiskID();
  if (discId == nullptr || !port::entry::IsSupportedDisc(*discId)) {
    std::fprintf(stderr,
                 "metroid_prime2_port: unsupported disc; expected Metroid Prime 2: Echoes "
                 "(USA) (v1.00), G2ME01 revision 0.\n");
    aurora_dvd_close();
    aurora_shutdown();
    return 1;
  }
  std::printf("metroid_prime2_port: disc mounted: %s\n", discImage.c_str());

  // Prime the window and event state so the game's first frame can succeed: the
  // console build submits GX during early initialization, before its main loop.
  aurora_update();

  // The game's entry owns the OS context and memory system. It returns void, so
  // the exit code is the platform's: reaching the end means the game returned.
  COsContext osContext(true, true);
  CMemorySys memorySys(osContext, CMemorySys::GetGameAllocator());

  // Retail builds a `CGraphicsSys` here too, in `main` (0x801EFB00), and passes it to
  // `InvokeCMain` as the sixth argument - an 8-byte object built with `fn_802BE85C`,
  // which is `CGraphicsSys::CGraphicsSys` and whose whole body is
  // `CGraphics::Startup(osContext, progressive)`. That constructor is where retail's VI
  // bring-up happens (`fn_802C329C` -> `fn_802C2FD4` = `ConfigureVideo`:
  // `VIGetTvFormat` -> `GXAdjustForOverscan` -> two framebuffers -> `VIConfigure` ->
  // `VIFlush` -> `GXInit` -> `GXSetCopyFilter`), and it has to run *before* the game
  // starts: without `GXInit` there is no GX, and without `mRenderModeObj` filled there is
  // no EFB shape for Aurora's presenter.
  //
  // The `false` is the progressive flag. Retail reads it from the console's saved region -
  // `fn_801EFC68` / `fn_801EFE6C` read the region `lbl_80419304` points at, and the byte
  // they return is the flag. The host has no saved region: there is no console to have
  // set it, and `OSGetSavedRegion` in platform/sdk_stubs.cpp answers null/null, so
  // progressive is unconditionally off. That is also the value retail's own PAL build
  // uses, and the one `ConfigureVideo`'s NTSC branch takes `GXNtsc480IntDf` for.
  //
  // This is a real object with a real destructor, not a leaked temporary: `~CGraphicsSys`
  // calls `CGraphics::Shutdown`, which restores the texture-region callback and stalls the
  // frame-delayed allocator, and both are retail's.
  CGraphicsSys graphicsSys(osContext, memorySys, false);

  // Three REL modules are compiled into the game library instead of being read
  // off the disc - Tweaks, CannonBall and ForgottenObject. On the cube each is a
  // separate module whose prolog and epilog run when it loads, which is what
  // publishes their function-pointer tables. Nothing on the host loads them, so
  // the port runs their entry points here, before the game's own entry, or those
  // tables stay null and the loaders behind them are never reached.
  port::modules::InitAll();

  // Tweaks.rel's `REL_CreateTweakGlobals` is the only writer of `gpTweakPlayerA`
  // (0x80418F44), and `CGameArchitectureSupport`'s constructor dereferences it at
  // 0x80007F38 with no null test - so this is the first of the two globals the
  // boot path needs and the only one that can be stood in for. The stand-in
  // allocates real 4-byte cells over a zeroed `SLdrTweakPlayer`, which makes the
  // five `CTweakPlayer` accessors answer 0.0f; it carries no tweak data, because
  // the data lives in `Standard.NTWK` inside a pak and the paks are boot-path
  // step 13. `STweaks_FuncPtrs::CreateGlobals` is the retail route and it is
  // assigned by `TweaksInit` and called by nothing. See
  // src/MetroidPrime/PortTweakGlobals.cpp and docs/research/tweak_globals.md.
  port::tweaks::CreateStandInTweakPlayers();

  InvokeCMain(argc, argv, &osContext, nullptr, &memorySys, nullptr);

  port::modules::ShutdownAll();

  aurora_dvd_close();
  aurora_shutdown();
  return 0;
}
