// Port entry point. Aurora owns the real process entry (aurora_main) and the
// window/GPU/input/audio backend; this file initializes it, mounts the user's
// disc, and hands control to the game.
//
// Disc path resolution: first non-flag argument, then $MP_DISC.

#include <aurora/aurora.h>
#include <aurora/dvd.h>
#include <aurora/gfx.h>
#include <aurora/event.h>
#include <aurora/main.h>
#include <dolphin/gx.h>
#include <dolphin/vi.h>
#include <dolphin/dvd.h>

#include "port_debug.h"
#include "port_randomizer.h"
#include "port_textures.h"
#include "port_prompts.h"
#include "port_build_info.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_timer.h>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

extern "C" int metroid_main(int argc, char** argv);
extern "C" void AIPortShutdown(void);

namespace {
#if defined(__ANDROID__)
// Aurora logs to stderr, which Android discards. Send it to logcat instead so
// the Vulkan/audio/disc diagnostics are actually reachable on a device.
void AndroidLogCallback(AuroraLogLevel level, const char* module, const char* message,
                        unsigned int len) {
    int priority = ANDROID_LOG_INFO;
    switch (level) {
    case LOG_DEBUG:
        priority = ANDROID_LOG_DEBUG;
        break;
    case LOG_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
    case LOG_ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
    case LOG_FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
    case LOG_INFO:
    default:
        break;
    }
    __android_log_print(priority, "aurora", "[%s] %.*s", module != nullptr ? module : "",
                        static_cast< int >(len), message);
}
#endif

std::string LowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

bool IsDiscImage(const std::filesystem::path& path) {
    static const char* const kExtensions[] = {".iso", ".gcm", ".rvz", ".wbfs", ".ciso", ".nkit"};
    const std::string ext = LowerExtension(path);
    for (const char* candidate : kExtensions) {
        if (ext == candidate) {
            return true;
        }
    }
    return false;
}

// Looks for a disc image next to the executable (and in its immediate
// subdirectories) so a copied build is self-contained.
std::string FindDiscNextToExecutable() {
    const char* base = SDL_GetBasePath();
    if (base == nullptr) {
        return {};
    }
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path baseDir(base);
    std::vector<fs::path> dirs{baseDir};
    for (fs::directory_iterator it(baseDir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_directory(ec)) {
            dirs.push_back(it->path());
        }
    }
    for (const fs::path& dir : dirs) {
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && IsDiscImage(it->path())) {
                return it->path().string();
            }
        }
    }
    return {};
}

const char* ResolveDiscPath(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] != '-' && argv[i][0] != '\0') {
            return argv[i];
        }
    }
    if (const char* env = std::getenv("MP_DISC"); env != nullptr && env[0] != '\0') {
        return env;
    }
    if (const char* saved = PortDebug::DiscPath(); saved != nullptr) {
#if defined(__ANDROID__)
        if (std::strncmp(saved, "content://", 10) == 0) {
            return saved;
        }
#endif
        if (std::filesystem::exists(saved)) {
            return saved;
        }
    }
    static const std::string sFound = FindDiscNextToExecutable();
    return sFound.empty() ? nullptr : sFound.c_str();
}

// Asks for the disc image with the platform's file dialog and remembers the
// choice. SDL delivers the result on another thread, so this pumps events until
// it arrives; the callback also fires with an empty list if the dialog fails.
std::string AskForDiscImage() {
    static std::atomic< bool > answered{false};
    static std::string chosen;
    const SDL_DialogFileFilter filters[] = {
        {"GameCube disc image", "iso;gcm;rvz;wbfs;ciso;nkit"},
        {"All files", "*"},
    };
    int windowCount = 0;
    SDL_Window** windows = SDL_GetWindows(&windowCount);
    SDL_Window* window = windows != nullptr && windowCount > 0 ? windows[0] : nullptr;
    SDL_free(windows);
    if (window == nullptr) {
        // Headless, as on a build runner: nothing to show a dialog on, so say
        // no disc was given rather than waiting for an answer that cannot come.
        std::fprintf(stderr, "metroid_prime_port: no window to ask for a disc image on\n");
        return {};
    }
    std::fprintf(stderr, "metroid_prime_port: no disc image found; asking for one\n");
    SDL_ShowOpenFileDialog(
        [](void*, const char* const* files, int) {
            if (files != nullptr && files[0] != nullptr) {
                chosen = files[0];
            }
            answered.store(true);
        },
        nullptr, window, filters, 2, nullptr, false);
    // Wait for the answer, but not forever: a dialog that never calls back
    // would otherwise hang a scripted or headless run.
    const Uint64 deadline = SDL_GetTicks() + 5 * 60 * 1000;
    while (!answered.load()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                std::fprintf(stderr, "metroid_prime_port: disc selection cancelled\n");
                return {};
            }
        }
        if (SDL_GetTicks() > deadline) {
            std::fprintf(stderr, "metroid_prime_port: disc selection timed out\n");
            return {};
        }
        SDL_Delay(10);
    }
    if (!chosen.empty()) {
        PortDebug::SetDiscPath(chosen.c_str());
        // Persist immediately: the settings are otherwise only written from the
        // overlay's draw path, which never runs if the game cannot frame.
        PortDebug::SaveSettingsNow();
        std::fprintf(stderr, "metroid_prime_port: disc image set to %s\n", chosen.c_str());
    }
    return chosen;
}

// Default texture-replacement folder next to the executable.
const char* DefaultTexturesPath() {
    static const std::string sPath = [] {
#if defined(__ANDROID__)
        char* pref = SDL_GetPrefPath(nullptr, "Metroid Prime");
        if (pref == nullptr) {
            return std::string();
        }
        const std::string dir = std::string(pref) + "textures";
        SDL_free(pref);
#else
        const char* base = SDL_GetBasePath();
        if (base == nullptr) {
            return std::string();
        }
        const std::string dir = std::string(base) + "textures";
#endif
        std::error_code ec;
        return std::filesystem::is_directory(dir, ec) ? dir : std::string();
    }();
    return sPath.empty() ? nullptr : sPath.c_str();
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("Metroid Prime native port %s\n", MP_BUILD_REVISION);
        return 0;
    }
    std::fprintf(stderr, "metroid_prime_port: build %s\n", MP_BUILD_REVISION);
    PortRandomizer::EnsureLoaded();
    // A 16:9 window when widescreen is requested; the game's render mode is
    // widened to match. Values are the default window size only.
    const bool widescreen = PortDebug::AspectMode() != PortDebug::kAspect_4_3;
    // MP_DUMP_TEXTURES=1 writes every source texture to
    // <cachePath>/texture_dumps as DDS, so replacement packs can be authored.
    const char* dumpEnv = std::getenv("MP_DUMP_TEXTURES");
    const bool dumpTextures = dumpEnv != nullptr && dumpEnv[0] != '\0' && std::strcmp(dumpEnv, "0") != 0;
    std::string resourcesPath;
#if defined(__ANDROID__)
    if (char* pref = SDL_GetPrefPath(nullptr, "Metroid Prime")) {
        resourcesPath = pref;
        SDL_free(pref);
    }
#endif
    AuroraConfig config = {
        .appName = "Metroid Prime",
        .userPath = std::getenv("MP_USER_PATH"),
        .cachePath = std::getenv("MP_CACHE_PATH"),
        .resourcesPath = resourcesPath.empty() ? nullptr : resourcesPath.c_str(),
        .desiredBackend = BACKEND_AUTO,
        .vsync = false,
        .allowTextureDumps = dumpTextures,
        // Keep the internal framebuffer at the game's logical size so its two
        // framebuffer allocations fit in MEM1; Aurora upscales to the window.
        .windowWidth = static_cast<uint32_t>(widescreen ? 854 : 640),
        .windowHeight = 480,
        .mem1Size = MEM1_DEFAULT_SIZE,
        .mem2Size = ARAM_DEFAULT_SIZE,
    };

#if defined(__ANDROID__)
    // SDL3 drops touch-derived mouse events by default, and ImGui's SDL3
    // backend only understands mouse events. The touch overlay in Java claims
    // gameplay touches, so whatever reaches SDL here is meant for ImGui.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    config.logCallback = AndroidLogCallback;
#endif
    aurora_initialize(argc, argv, &config);
    // Apply the persisted render scale. Vsync is applied on the first drawn
    // frame (once the swapchain surface exists) so it uses real capabilities.
    VISetFrameBufferScale(PortDebug::RenderScale());
    // Fit the internal EFB to the game's render-mode aspect rather than the
    // window aspect, so fixed 4:3/16:9 modes are never stretched when the window
    // shape differs; the present letterboxes instead.
    AuroraSetViewportPolicy(AURORA_VIEWPORT_FIT);

    // Optional HD texture replacements, in Aurora's naming convention
    // (tex1_<w>x<h>_<texhash>[_<tluthash>]_<format>.dds/.png); a per-device
    // subfolder is selected from the connected controller. Aurora also accepts
    // Dolphin format names such as CMPR and RGBA8.
    const char* textures = std::getenv("MP_TEXTURES");
    if (textures == nullptr || textures[0] == '\0') {
        textures = DefaultTexturesPath();
    }
    PortTextures::Initialize(textures);
    // Binding-aware prompt icons, served from <textures>/bindings.
    PortPrompts::Initialize(textures);

    // Disc image: an explicit argument or MP_DISC, else the path saved on a
    // previous launch, else a copy beside the executable, else ask for one.
    PortDebug::LoadDiscPath();
    std::string discImage;
    if (const char* resolved = ResolveDiscPath(argc, argv); resolved != nullptr) {
        discImage = resolved;
    } else {
        discImage = AskForDiscImage();
    }
    if (discImage.empty()) {
        std::fprintf(stderr,
                     "metroid_prime_port: no disc image given.\n"
                     "  usage: %s <path to Metroid Prime (USA) (v1.00).iso>\n"
                     "  or set MP_DISC, or place the image next to the executable.\n", argv[0]);
        aurora_shutdown();
        return 1;
    }
    const char* discPath = discImage.c_str();

    if (!aurora_dvd_open(discPath)) {
        std::fprintf(stderr, "metroid_prime_port: failed to open disc image: %s\n", discPath);
        aurora_shutdown();
        return 1;
    }
    std::printf("metroid_prime_port: disc mounted: %s\n", discPath);
    const DVDDiskID* discId = DVDGetCurrentDiskID();
    if (discId == nullptr || std::memcmp(discId->gameName, "GM8E", 4) != 0 ||
        std::memcmp(discId->company, "01", 2) != 0 || discId->diskNumber != 0 || discId->gameVersion != 0) {
        std::fprintf(stderr, "metroid_prime_port: unsupported disc; expected GM8E01 USA revision 0.\n");
        aurora_dvd_close();
        aurora_shutdown();
        return 1;
    }

    // Prime the window/event state so the game's first aurora_begin_frame can
    // succeed (the game submits GX during early init, before its main loop).
    aurora_update();

    int result = 1;
    try {
        result = metroid_main(argc, argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "metroid_prime_port: %s\n", error.what());
    }

    AIPortShutdown();
    aurora_dvd_close();
    aurora_shutdown();
    return result;
}
