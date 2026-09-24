// In-game debug overlay. The port draws it with Aurora's ImGui backend, which
// is already initialized and rendered every presented frame, so this only has
// to build the windows between aurora_begin_frame and aurora_end_frame.

#include "port_debug.h"
#include "port_prompts.h"
#include "port_mouse.h"
#include "port_textures.h"
#include "port_build_info.h"

#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"

#include <aurora/gfx.h>
#include <dolphin/pad.h>
#include <dolphin/vi.h>
#include <imgui.h>
#include <musyx/port_voices.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
#include <SDL3/SDL_joystick.h>
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace aurora {
void request_screenshot() noexcept;
}

// Implemented by the AI and MusyX audio backends.
extern "C" void AIPortSetOutputEnabled(int enabled);
extern "C" int AIPortOutputEnabled(void);
extern "C" void salSetMuted(int muted);

namespace {
bool sInitialized = false;
bool sFastBoot = false;
bool sSkipCutscenes = false;
float sCutsceneSpeed = 8.f;
unsigned sSimRate = 60;
bool sSimAdaptive = false;
float sTickPeriod = 1.f / 60.f;
bool sFrameLimitEnabled = true;
bool sTraceTiming = false;
uint64_t sTimingNs = 0;
unsigned sTimingFrames = 0, sTimingTicks = 0;
double sActualFps = 0.0, sActualTps = 0.0;
bool sVsyncEnabled = false;
float sRenderScale = 1.f;
PortDebug::EAspectMode sAspectMode = PortDebug::kAspect_4_3;
bool sHudWide = false;
bool sMouseAim = false;
bool sTwinStick = false;
float sStickAimRate = 900.f;
// Gyro aiming: off / hold / always, auto / controller / phone, and how fast a
// rotation turns into aim travel.
int sGyroMode = 0;
int sGyroSource = 0;
float sGyroRate = 600.f;
bool sMouseCaptured = false;
bool sMouseGameplayActive = false;
bool sMouseInvertX = false;
bool sMouseInvertY = false;
bool sMouseButtons = true;
bool sMouseCrosshair = true;
PortMouse::AimState sMouseAimState;
PortMouse::ButtonGate sMouseButtonGate;
float sMouseSensitivity = 0.0035f;
float sMousePendingX = 0.f;
float sMousePendingY = 0.f;
float sMouseFrameX = 0.f;
float sMouseFrameY = 0.f;
bool sAiAudioEnabled = true;
bool sMusyxAudioEnabled = true;
bool sResetRequested = false;
std::atomic< bool > sToggleRequested{false};
// Mirrors sVisible for readers on other threads, so they never touch the lazy
// initialization or the ImGui state owned by the game thread.
std::atomic< bool > sOverlayVisible{false};
// Same idea for the twin-stick setting, which the Android touch overlay uses to
// pick a controller layout.
std::atomic< bool > sTwinStickFlag{false};
// Gyro state: the pad's gyro sensor is enabled once, and the phone's sensor is
// looked up once, so neither is touched on every tick.
bool sControllerGyroEnabled = false;
bool sPhoneGyroSearched = false;
SDL_Sensor* sPhoneGyro = nullptr;
const char* sGyroStatus = "off";
bool sVisible = false;
bool sSettingsDirty = false;
bool sAudioSettingsApplied = false;
bool sPresentationSettingsApplied = false;
CStateManager* sStateManager = nullptr;
int sPendingTeleport = -1;
bool sHasWorldTeleport = false;
std::string sDiscPath;
uint32_t sWorldTeleportWorld = 0;
uint32_t sWorldTeleportArea = 0;

std::string SettingsFilePath() {
  std::string dir;
  if (const char* env = std::getenv("MP_USER_PATH")) {
    if (env[0] != '\0') {
      dir = env;
    }
  }
  if (dir.empty()) {
    if (char* pref = SDL_GetPrefPath(nullptr, "Metroid Prime")) {
      dir = pref;
      SDL_free(pref);
    } else {
      dir = ".";
    }
  }
  if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
    dir += '/';
  }
  return dir + "port_settings.ini";
}

bool ParseBool(const std::string& value) {
  return value == "1" || value == "true" || value == "on" || value == "yes";
}

std::string Trim(const std::string& text) {
  const size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

void MarkDirty() { sSettingsDirty = true; }

void ApplySetting(const std::string& key, const std::string& value) {
  if (key == "frame_limit") {
    sFrameLimitEnabled = ParseBool(value);
  } else if (key == "vsync") {
    sVsyncEnabled = ParseBool(value);
  } else if (key == "disc_path") {
    sDiscPath = value;
  } else if (key == "render_scale") {
    const float f = static_cast< float >(std::atof(value.c_str()));
    if (std::isfinite(f) && f >= 0.f && f <= 4.f) {
      sRenderScale = f;
    }
  } else if (key == "aspect") {
    if (value == "16:9") {
      sAspectMode = PortDebug::kAspect_16_9;
    } else if (value == "window") {
      sAspectMode = PortDebug::kAspect_Window;
    } else if (value == "4:3") {
      sAspectMode = PortDebug::kAspect_4_3;
    }
  } else if (key == "hud_wide") {
    sHudWide = ParseBool(value);
  } else if (key == "mouse_aim") {
    sMouseAim = ParseBool(value);
  } else if (key == "twin_stick") {
    sTwinStick = ParseBool(value);
  } else if (key == "stick_aim_rate") {
    const float f = static_cast< float >(std::atof(value.c_str()));
    if (std::isfinite(f) && f >= 50.f && f <= 4000.f) {
      sStickAimRate = f;
    }
  } else if (key == "gyro_mode") {
    const long v = std::strtol(value.c_str(), nullptr, 10);
    if (v >= 0 && v <= 2) {
      sGyroMode = static_cast< int >(v);
    }
  } else if (key == "gyro_source") {
    const long v = std::strtol(value.c_str(), nullptr, 10);
    if (v >= 0 && v <= 2) {
      sGyroSource = static_cast< int >(v);
    }
  } else if (key == "gyro_rate") {
    const float f = static_cast< float >(std::atof(value.c_str()));
    if (std::isfinite(f) && f >= 20.f && f <= 5000.f) {
      sGyroRate = f;
    }
  } else if (key == "mouse_invert_x") {
    sMouseInvertX = ParseBool(value);
  } else if (key == "mouse_invert_y") {
    sMouseInvertY = ParseBool(value);
  } else if (key == "mouse_buttons") {
    sMouseButtons = ParseBool(value);
  } else if (key == "mouse_crosshair") {
    sMouseCrosshair = ParseBool(value);
  } else if (key == "mouse_sensitivity") {
    const float f = static_cast< float >(std::atof(value.c_str()));
    if (std::isfinite(f) && f > 0.f) {
      sMouseSensitivity = f;
    }
  } else if (key == "skip_cutscenes") {
    sSkipCutscenes = ParseBool(value);
  } else if (key == "cutscene_speed") {
    const float f = static_cast< float >(std::atof(value.c_str()));
    if (std::isfinite(f) && f >= 1.f && f <= 32.f) {
      sCutsceneSpeed = f;
    }
  } else if (key == "sim_rate") {
    const long rate = std::strtol(value.c_str(), nullptr, 10);
    if (rate >= 30 && rate <= 480) {
      sSimRate = static_cast< unsigned >(rate);
    }
  } else if (key == "sim_adaptive") {
    sSimAdaptive = ParseBool(value);
  } else if (key == "ai_audio") {
    sAiAudioEnabled = ParseBool(value);
  } else if (key == "musyx_audio") {
    sMusyxAudioEnabled = ParseBool(value);
  } else if (key == "voices_muted") {
    MusyxPortClearSampleMutes();
    const char* cursor = value.c_str();
    while (*cursor != '\0') {
      char* end = nullptr;
      const unsigned long id = std::strtoul(cursor, &end, 10);
      if (end == cursor) {
        break;
      }
      MusyxPortSetSampleMuted(static_cast< unsigned >(id), 1);
      cursor = end;
      while (*cursor == ',' || *cursor == ' ') {
        ++cursor;
      }
    }
  }
}

void LoadSettings() {
  const std::string path = SettingsFilePath();
  std::ifstream file(path);
  if (!file.is_open()) {
    return;
  }
  std::fprintf(stderr, "metroid_prime_port: loaded settings from %s\n", path.c_str());
  std::string line;
  while (std::getline(file, line)) {
    const size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line.erase(comment);
    }
    const size_t separator = line.find('=');
    if (separator == std::string::npos) {
      continue;
    }
    const std::string key = Trim(line.substr(0, separator));
    const std::string value = Trim(line.substr(separator + 1));
    if (!key.empty()) {
      ApplySetting(key, value);
    }
  }
}

void SaveSettings() {
  if (!sInitialized || !sSettingsDirty) {
    return;
  }
  const std::string path = SettingsFilePath();
  std::ofstream file(path, std::ios::trunc);
  if (!file.is_open()) {
    std::fprintf(stderr, "metroid_prime_port: could not write settings to %s\n", path.c_str());
    return;
  }
  const char* aspect = sAspectMode == PortDebug::kAspect_16_9  ? "16:9"
                       : sAspectMode == PortDebug::kAspect_Window ? "window"
                                                                  : "4:3";
  file << "# Metroid Prime native port settings. Written by the F1 debug overlay.\n";
  file << "# Environment variables (MP_*) override these for a single run.\n";
  file << "aspect=" << aspect << '\n';
  file << "hud_wide=" << (sHudWide ? 1 : 0) << '\n';
  file << "vsync=" << (sVsyncEnabled ? 1 : 0) << '\n';
  file << "render_scale=" << sRenderScale << '\n';
  file << "frame_limit=" << (sFrameLimitEnabled ? 1 : 0) << '\n';
  file << "skip_cutscenes=" << (sSkipCutscenes ? 1 : 0) << '\n';
  file << "cutscene_speed=" << sCutsceneSpeed << '\n';
  file << "sim_rate=" << sSimRate << '\n';
  file << "sim_adaptive=" << (sSimAdaptive ? 1 : 0) << '\n';
  file << "mouse_aim=" << (sMouseAim ? 1 : 0) << '\n';
  file << "twin_stick=" << (sTwinStick ? 1 : 0) << '\n';
  file << "stick_aim_rate=" << sStickAimRate << '\n';
  file << "gyro_mode=" << sGyroMode << '\n';
  file << "gyro_source=" << sGyroSource << '\n';
  file << "gyro_rate=" << sGyroRate << '\n';
  file << "mouse_invert_x=" << (sMouseInvertX ? 1 : 0) << '\n';
  file << "mouse_invert_y=" << (sMouseInvertY ? 1 : 0) << '\n';
  file << "mouse_buttons=" << (sMouseButtons ? 1 : 0) << '\n';
  file << "mouse_crosshair=" << (sMouseCrosshair ? 1 : 0) << '\n';
  file << "mouse_sensitivity=" << sMouseSensitivity << '\n';
  if (!sDiscPath.empty()) {
    file << "disc_path=" << sDiscPath << '\n';
  }
  file << "ai_audio=" << (sAiAudioEnabled ? 1 : 0) << '\n';
  file << "musyx_audio=" << (sMusyxAudioEnabled ? 1 : 0) << '\n';
  unsigned muted[64];
  const int mutedCount = MusyxPortGetMutedSamples(muted, 64);
  if (mutedCount > 0) {
    file << "voices_muted=";
    for (int i = 0; i < mutedCount; ++i) {
      file << (i == 0 ? "" : ",") << muted[i];
    }
    file << '\n';
  }
  file.flush();
  std::fprintf(stderr, "metroid_prime_port: saved settings to %s\n", path.c_str());
  sSettingsDirty = false;
}

bool SDLCALL debug_event_watch(void*, SDL_Event* event) {
  // F1 toggles the overlay. Watch the event rather than polling the key state:
  // a short tap can begin and end between two frames, so polling misses it.
  if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat &&
      event->key.scancode == SDL_SCANCODE_F1) {
    PortDebug::RequestToggle();
  }
  return true;
}

void EnsureInitialized() {
  if (sInitialized) {
    return;
  }
  sInitialized = true;
  LoadSettings();

  // Environment variables are explicit per-run overrides and win over the file.
  if (std::getenv("MP_TRACE_TIMING") != nullptr) {
    sTraceTiming = true;
  }
  if (std::getenv("MP_FAST_BOOT") != nullptr) {
    sFastBoot = true;
  }
  if (std::getenv("MP_SKIP_CUTSCENES") != nullptr) {
    sSkipCutscenes = true;
  }
  if (std::getenv("MP_SHOW_DEBUG_UI") != nullptr) {
    sVisible = true;
  }
  if (const char* aspect = std::getenv("MP_ASPECT")) {
    if (std::strcmp(aspect, "16:9") == 0) {
      sAspectMode = PortDebug::kAspect_16_9;
    } else if (std::strcmp(aspect, "window") == 0) {
      sAspectMode = PortDebug::kAspect_Window;
    } else if (std::strcmp(aspect, "4:3") == 0) {
      sAspectMode = PortDebug::kAspect_4_3;
    }
  } else if (std::getenv("MP_WIDESCREEN") != nullptr) {
    sAspectMode = PortDebug::kAspect_16_9;
  }
  if (std::getenv("MP_HUD_WIDE") != nullptr) {
    sHudWide = true;
  }
  if (std::getenv("MP_MOUSE_AIM") != nullptr) {
    sMouseAim = true;
  }
  if (std::getenv("MP_TWIN_STICK") != nullptr) {
    sTwinStick = true;
  }
  if (std::getenv("MP_MOUSE_INVERT_X") != nullptr) {
    sMouseInvertX = true;
  }
  if (std::getenv("MP_MOUSE_INVERT_Y") != nullptr) {
    sMouseInvertY = true;
  }
  if (std::getenv("MP_DISABLE_MOUSE_BUTTONS") != nullptr) {
    sMouseButtons = false;
  }
  if (std::getenv("MP_DISABLE_MOUSE_CROSSHAIR") != nullptr) {
    sMouseCrosshair = false;
  }
  if (const char* sens = std::getenv("MP_MOUSE_SENS")) {
    const float value = static_cast< float >(std::atof(sens));
    if (std::isfinite(value) && value > 0.f) {
      sMouseSensitivity = value;
    }
  }
  if (std::getenv("MP_DISABLE_AI_AUDIO") != nullptr) {
    sAiAudioEnabled = false;
  }
  if (const char* speed = std::getenv("MP_CUTSCENE_SPEED")) {
    const float value = static_cast< float >(std::atof(speed));
    if (std::isfinite(value) && value >= 1.f && value <= 32.f) {
      sCutsceneSpeed = value;
    }
  }
  if (const char* rate = std::getenv("MP_SIM_RATE")) {
    const long value = std::strtol(rate, nullptr, 10);
    if (value >= 30 && value <= 480) {
      sSimRate = static_cast< unsigned >(value);
    }
  }
  if (std::getenv("MP_SIM_ADAPTIVE") != nullptr) {
    sSimAdaptive = true;
  }

  std::atexit(SaveSettings);
}
} // namespace

namespace PortDebug {

bool FastBoot() {
  EnsureInitialized();
  return sFastBoot;
}

bool SkipCutscenes() {
  EnsureInitialized();
  return sSkipCutscenes;
}

float CutsceneSpeed() {
  EnsureInitialized();
  return sCutsceneSpeed;
}

unsigned SimRate() {
  EnsureInitialized();
  return sSimRate;
}

void SetSimRate(unsigned hz) {
  EnsureInitialized();
  if (hz < 30u || hz > 480u) {
    return;
  }
  sSimRate = hz;
  MarkDirty();
}

float SimPeriod() { return 1.f / static_cast< float >(SimRate()); }

bool SimAdaptive() {
  EnsureInitialized();
  return sSimAdaptive;
}

void SetSimAdaptive(bool enabled) {
  EnsureInitialized();
  sSimAdaptive = enabled;
  MarkDirty();
}

float TickPeriod() {
  EnsureInitialized();
  return sTickPeriod;
}

void SetTickPeriod(float dt) {
  if (std::isfinite(dt) && dt > 0.f) {
    sTickPeriod = dt;
  }
}

float TickFrames() { return sTickPeriod * 60.f; }

bool FrameLimitEnabled() {
  EnsureInitialized();
  return sFrameLimitEnabled;
}

void RecordFrame(uint64_t durationNs, unsigned ticks, bool presented) {
  sTimingNs += durationNs;
  sTimingTicks += ticks;
  if (presented) ++sTimingFrames;
  if (sTimingNs >= 1000000000ull) {
    const double seconds = static_cast<double>(sTimingNs) / 1000000000.0;
    sActualFps = sTimingFrames / seconds;
    sActualTps = sTimingTicks / seconds;
    if (sTraceTiming) {
      std::fprintf(stderr, "[timing] render=%.1f FPS simulation=%.1f ticks/s cap=%s\n",
                   sActualFps, sActualTps, sFrameLimitEnabled ? "60" : "off");
    }
    sTimingNs = 0;
    sTimingFrames = sTimingTicks = 0;
  }
}

void SetFrameLimitEnabled(bool enabled) {
  EnsureInitialized();
  if (sFrameLimitEnabled != enabled) {
    sFrameLimitEnabled = enabled;
    MarkDirty();
  }
}

bool VsyncEnabled() {
  EnsureInitialized();
  return sVsyncEnabled;
}

void SetVsyncEnabled(bool enabled) {
  EnsureInitialized();
  // Always re-apply: the stored value can match while the surface still has the
  // previous present mode (e.g. the persisted value applied before the first
  // frame), which made the first toggle a no-op.
  sVsyncEnabled = enabled;
  aurora_enable_vsync(enabled);
}

float RenderScale() {
  EnsureInitialized();
  return sRenderScale;
}

void SetRenderScale(float scale) {
  EnsureInitialized();
  if (sRenderScale == scale) {
    return;
  }
  sRenderScale = scale;
  VISetFrameBufferScale(scale);
}

EAspectMode AspectMode() {
  EnsureInitialized();
  return sAspectMode;
}

void SetAspectMode(EAspectMode mode) {
  EnsureInitialized();
  sAspectMode = mode;
}

bool HudWide() {
  EnsureInitialized();
  return sHudWide;
}

void SetHudWide(bool enabled) {
  EnsureInitialized();
  sHudWide = enabled;
  MarkDirty();
}

bool MouseAim() {
  EnsureInitialized();
  return sMouseAim;
}

void SetMouseAim(bool enabled) {
  EnsureInitialized();
  sMouseAim = enabled;
  ResetMouseAim();
}

bool TwinStick() {
  EnsureInitialized();
  return sTwinStick;
}

void SetTwinStick(bool enabled) {
  EnsureInitialized();
  sTwinStick = enabled;
  MarkDirty();
}

float StickAimRate() {
  EnsureInitialized();
  return sStickAimRate;
}

void SetStickAimRate(float pixelsPerSecond) {
  EnsureInitialized();
  if (std::isfinite(pixelsPerSecond) && pixelsPerSecond >= 50.f && pixelsPerSecond <= 4000.f) {
    sStickAimRate = pixelsPerSecond;
    MarkDirty();
  }
}

void AddStickAim(float x, float y, float dt) {
  EnsureInitialized();
  if (!sTwinStick || Visible() || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(dt) ||
      dt <= 0.f) {
    return;
  }
  // x right / y up; the aim state expects SDL-style right/down positive.
  sMouseFrameX += x * sStickAimRate * dt;
  sMouseFrameY -= y * sStickAimRate * dt;
}

int GyroMode() {
  EnsureInitialized();
  return sGyroMode;
}

void SetGyroMode(int mode) {
  EnsureInitialized();
  if (mode < 0 || mode > 2) {
    return;
  }
  sGyroMode = mode;
  MarkDirty();
}

int GyroSource() {
  EnsureInitialized();
  return sGyroSource;
}

void SetGyroSource(int source) {
  EnsureInitialized();
  if (source < 0 || source > 2) {
    return;
  }
  sGyroSource = source;
  MarkDirty();
}

float GyroRate() {
  EnsureInitialized();
  return sGyroRate;
}

void SetGyroRate(float pixelsPerSecondPerRad) {
  EnsureInitialized();
  if (std::isfinite(pixelsPerSecondPerRad) && pixelsPerSecondPerRad >= 20.f &&
      pixelsPerSecondPerRad <= 5000.f) {
    sGyroRate = pixelsPerSecondPerRad;
    MarkDirty();
  }
}

const char* GyroStatus() { return sGyroStatus; }

void PollGyro() {
  EnsureInitialized();
  if (sGyroMode == 0) {
    sGyroStatus = "off";
    return;
  }
  // Gyro feeds the same aim state the mouse and twin stick use, so it only has
  // an effect where that is driving the camera.
  if (!sMouseAim && !sTwinStick) {
    sGyroStatus = "needs mouse aim or twin stick";
    return;
  }

  const bool wantController = sGyroSource == 0 || sGyroSource == 1;
  const bool wantPhone = sGyroSource == 0 || sGyroSource == 2;
  float yaw = 0.f;
  float pitch = 0.f;
  bool haveRates = false;

  if (wantController) {
    if (SDL_Gamepad* pad = PADGetSDLGamepadForIndex(0)) {
      if (SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO)) {
        if (!sControllerGyroEnabled) {
          SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_GYRO, true);
          sControllerGyroEnabled = true;
        }
        float data[3];
        if (SDL_GetGamepadSensorData(pad, SDL_SENSOR_GYRO, data, 3)) {
          // Radians per second; x is pitch, y is yaw.
          yaw = data[1];
          pitch = data[0];
          haveRates = true;
          sGyroStatus = "controller";
        }
      }
    }
  }

  if (!haveRates && wantPhone) {
    if (sPhoneGyro == nullptr && !sPhoneGyroSearched) {
      sPhoneGyroSearched = true;
      int count = 0;
      if (SDL_SensorID* ids = SDL_GetSensors(&count)) {
        for (int i = 0; i < count; ++i) {
          if (SDL_GetSensorTypeForID(ids[i]) == SDL_SENSOR_GYRO) {
            sPhoneGyro = SDL_OpenSensor(ids[i]);
            break;
          }
        }
        SDL_free(ids);
      }
    }
    if (sPhoneGyro != nullptr) {
      float data[3];
      if (SDL_GetSensorData(sPhoneGyro, data, 3)) {
        yaw = data[1];
        pitch = data[0];
        haveRates = true;
        sGyroStatus = "phone";
      }
    }
  }

  if (!haveRates) {
    sGyroStatus = "no gyro found";
    return;
  }

  bool active = sGyroMode == 2;
  if (!active) {
    // Hold to aim: right stick click on a pad, left ctrl on a keyboard.
    if (SDL_Gamepad* pad = PADGetSDLGamepadForIndex(0)) {
      active = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    }
    if (!active) {
      const bool* keys = SDL_GetKeyboardState(nullptr);
      active = keys != nullptr && keys[SDL_SCANCODE_LCTRL] != 0;
    }
  }
  if (!active) {
    sGyroStatus = "held off";
    return;
  }

  const float dt = TickPeriod();
  if (!std::isfinite(dt) || dt <= 0.f) {
    return;
  }
  // x right / y up, the same shape AddStickAim takes; the aim state expects
  // right/down positive.
  sMouseFrameX += yaw * sGyroRate * dt;
  sMouseFrameY -= pitch * sGyroRate * dt;
}

void ResetMouseAim() {
  sMouseAimState.Reset();
  sMouseGameplayActive = false;
  sMouseButtonGate.Reset();
  sMousePendingX = sMousePendingY = sMouseFrameX = sMouseFrameY = 0.f;
}

void SetMouseCaptured(bool captured) {
  sMouseCaptured = captured;
  if (!captured) {
    sMouseButtonGate.Reset();
    sMousePendingX = sMousePendingY = sMouseFrameX = sMouseFrameY = 0.f;
  }
}

bool MouseCaptured() { return sMouseCaptured; }
bool MouseGameplayActive() { return sMouseGameplayActive; }
void SetMouseGameplayActive(bool active) {
  sMouseGameplayActive = active;
  if (!active) ResetMouseAim();
}
bool MouseInvertX() { EnsureInitialized(); return sMouseInvertX; }
bool MouseInvertY() { EnsureInitialized(); return sMouseInvertY; }
bool MouseButtons() { EnsureInitialized(); return sMouseButtons; }
bool MouseCrosshair() { EnsureInitialized(); return sMouseCrosshair; }
unsigned MouseWeaponButtons(unsigned held) {
  return sMouseButtonGate.Poll(MouseAim() && MouseButtons() && MouseGameplayActive() &&
                               MouseCaptured() && !Visible(), held);
}

bool UpdateMouseAim(bool active, bool locked, float x, float y, float z) {
  SetMouseGameplayActive(active);
  const bool applied = sMouseAimState.Update(active, locked, x, y, z, sMouseFrameX, sMouseFrameY,
                                            MouseSensitivity(), MouseInvertX(), MouseInvertY());
  if (applied) sMouseFrameX = sMouseFrameY = 0.f;
  return applied;
}
void SynchronizeMouseAim(float x, float y, float z) { sMouseAimState.Synchronize(x, y, z); }

float MouseSensitivity() {
  EnsureInitialized();
  return sMouseSensitivity;
}

void SetMouseSensitivity(float radiansPerPixel) {
  EnsureInitialized();
  if (std::isfinite(radiansPerPixel) && radiansPerPixel > 0.f) {
    sMouseSensitivity = radiansPerPixel;
  }
}

void AddMouseDelta(float dx, float dy) {
  if (!sMouseCaptured || !MouseAim() || Visible() || !std::isfinite(dx) || !std::isfinite(dy)) {
    return;
  }
  sMousePendingX += dx;
  sMousePendingY += dy;
}

void BeginFrameMouse() {
  sMouseFrameX = sMousePendingX;
  sMouseFrameY = sMousePendingY;
  sMousePendingX = 0.f;
  sMousePendingY = 0.f;
}

void GetFrameMouseDelta(float& dx, float& dy) {
  dx = sMouseFrameX;
  dy = sMouseFrameY;
}

float AimYaw() { return sMouseAimState.yaw; }
float AimPitch() { return sMouseAimState.pitch; }
bool AimInitialized() { return sMouseAimState.initialized; }

bool AiAudioEnabled() {
  EnsureInitialized();
  return AIPortOutputEnabled() != 0;
}

void SetAiAudioEnabled(bool enabled) {
  EnsureInitialized();
  if (sAiAudioEnabled == enabled && AiAudioEnabled() == enabled) {
    return;
  }
  sAiAudioEnabled = enabled;
  AIPortSetOutputEnabled(enabled ? 1 : 0);
}

bool MusyxAudioEnabled() {
  EnsureInitialized();
  return sMusyxAudioEnabled;
}

void SetMusyxAudioEnabled(bool enabled) {
  EnsureInitialized();
  if (sMusyxAudioEnabled == enabled) {
    return;
  }
  sMusyxAudioEnabled = enabled;
  salSetMuted(enabled ? 0 : 1);
}

void RequestReset() { sResetRequested = true; }

bool ConsumeResetRequest() {
  const bool requested = sResetRequested;
  sResetRequested = false;
  return requested;
}

void SetStateManager(CStateManager* mgr) { sStateManager = mgr; }
CStateManager* StateManager() { return sStateManager; }
void RequestTeleport(int areaId) { sPendingTeleport = areaId; }
bool ConsumeTeleportRequest(int& areaId) {
  if (sPendingTeleport < 0) {
    return false;
  }
  areaId = sPendingTeleport;
  sPendingTeleport = -1;
  return true;
}
void RequestWorldTeleport(uint32_t worldId, uint32_t areaAssetId) {
  sWorldTeleportWorld = worldId;
  sWorldTeleportArea = areaAssetId;
  sHasWorldTeleport = true;
}
bool ConsumeWorldTeleportRequest(uint32_t& worldId, uint32_t& areaAssetId) {
  if (!sHasWorldTeleport) {
    return false;
  }
  worldId = sWorldTeleportWorld;
  areaAssetId = sWorldTeleportArea;
  sHasWorldTeleport = false;
  return true;
}

bool Visible() {
  EnsureInitialized();
  return sVisible;
}

bool OverlayVisible() { return sOverlayVisible.load(std::memory_order_acquire); }

bool TwinStickFlag() { return sTwinStickFlag.load(std::memory_order_acquire); }

void SaveSettingsNow() {
  EnsureInitialized();
  SaveSettings();
}

// Phones report a density of roughly 3, which leaves ImGui's default 13px font
// unreadably small, so scale the overlay to match the display. The scale is not
// known on the first frame, so keep watching for it instead of latching once.
void UpdateUiScale() {
  if (ImGui::GetCurrentContext() == nullptr) {
    return;
  }
  static bool sInitialized = false;
  static float sAppliedScale = 1.f;
  static SDL_Window* sWindow = nullptr;
  if (!sInitialized) {
    sInitialized = true;
    // The overlay has to size itself to the scaled font, so do not restore a
    // window size remembered from a previous, smaller run.
    ImGui::GetIO().IniFilename = nullptr;
  }
  if (sWindow == nullptr) {
    int windowCount = 0;
    if (SDL_Window** windows = SDL_GetWindows(&windowCount)) {
      if (windowCount > 0) {
        sWindow = windows[0];
      }
      SDL_free(windows);
    }
    if (sWindow == nullptr) {
      return;
    }
  }
  const float displayScale = SDL_GetWindowDisplayScale(sWindow);
  const float uiScale = std::clamp(displayScale, 1.f, 4.f);
  if (uiScale == sAppliedScale) {
    return;
  }
  const float ratio = uiScale / sAppliedScale;
  sAppliedScale = uiScale;
  ImGui::GetStyle().ScaleAllSizes(ratio);
  ImGui::GetIO().FontGlobalScale *= ratio;
}

void RequestToggle() { sToggleRequested.store(true, std::memory_order_release); }

void UpdateControllerNav() {
  EnsureInitialized();
  // Registered here rather than in EnsureInitialized so the event system is only
  // touched from the game thread; the Java visibility query can reach that
  // initialization from the UI thread.
  static bool sEventWatchRegistered = false;
  if (!sEventWatchRegistered) {
    sEventWatchRegistered = true;
    SDL_AddEventWatch(debug_event_watch, nullptr);
  }
  UpdateUiScale();
  if (sToggleRequested.exchange(false, std::memory_order_acq_rel)) {
    Toggle();
  }
  sOverlayVisible.store(sVisible, std::memory_order_release);
  sTwinStickFlag.store(sTwinStick, std::memory_order_release);

  ImGuiIO& io = ImGui::GetIO();
  io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  SDL_Gamepad* pad = PADGetSDLGamepadForIndex(0);
  if (pad == nullptr) {
    return;
  }
  const auto held = [pad](SDL_GamepadButton button) {
    return SDL_GetGamepadButton(pad, button);
  };
  const auto axis = [pad](SDL_GamepadAxis a) {
    return SDL_GetGamepadAxis(pad, a);
  };
  constexpr Sint16 kStickThreshold = 16000;
  io.AddKeyEvent(ImGuiKey_GamepadDpadUp,
                 held(SDL_GAMEPAD_BUTTON_DPAD_UP) || axis(SDL_GAMEPAD_AXIS_LEFTY) < -kStickThreshold);
  io.AddKeyEvent(ImGuiKey_GamepadDpadDown,
                 held(SDL_GAMEPAD_BUTTON_DPAD_DOWN) || axis(SDL_GAMEPAD_AXIS_LEFTY) > kStickThreshold);
  io.AddKeyEvent(ImGuiKey_GamepadDpadLeft,
                 held(SDL_GAMEPAD_BUTTON_DPAD_LEFT) || axis(SDL_GAMEPAD_AXIS_LEFTX) < -kStickThreshold);
  io.AddKeyEvent(ImGuiKey_GamepadDpadRight,
                 held(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || axis(SDL_GAMEPAD_AXIS_LEFTX) > kStickThreshold);
  io.AddKeyEvent(ImGuiKey_GamepadFaceDown, held(SDL_GAMEPAD_BUTTON_SOUTH));
  io.AddKeyEvent(ImGuiKey_GamepadFaceRight, held(SDL_GAMEPAD_BUTTON_EAST));
  io.AddKeyEvent(ImGuiKey_GamepadL1, held(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER));
  io.AddKeyEvent(ImGuiKey_GamepadR1, held(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));

  static bool sChordHeld = false;
  const bool chord = held(SDL_GAMEPAD_BUTTON_START) && held(SDL_GAMEPAD_BUTTON_BACK);
  if (chord && !sChordHeld) {
    Toggle();
  }
  sChordHeld = chord;
}

void Toggle() {
  EnsureInitialized();
  sVisible = !sVisible;
  SetMouseCaptured(false);
}

void DrawPerformanceTab() {
  ImGui::Text("Build: %s", MP_BUILD_REVISION);
  bool frameLimit = sFrameLimitEnabled;
  if (ImGui::Checkbox("60 FPS cap (target)", &frameLimit)) {
    SetFrameLimitEnabled(frameLimit);
    MarkDirty();
  }
  ImGui::Text("Measured render rate: %.1f FPS", sActualFps);
  if (sSimAdaptive) {
    ImGui::Text("Measured simulation: %.1f ticks/s (adaptive)", sActualTps);
  } else {
    ImGui::Text("Measured simulation: %.1f ticks/s (target %u)", sActualTps, sSimRate);
  }
  ImGui::Text("Frame time: %.2f ms", static_cast< double >(ImGui::GetIO().DeltaTime) * 1000.0);

  ImGui::Separator();
  ImGui::TextUnformatted("Experimental: simulation rate");
  bool adaptive = sSimAdaptive;
  if (ImGui::Checkbox("Adaptive (follow frame rate)", &adaptive)) {
    PortDebug::SetSimAdaptive(adaptive);
  }
  ImGui::BeginDisabled(adaptive);
  int simRate = static_cast< int >(sSimRate);
  if (ImGui::SliderInt("Sim Hz", &simRate, 30, 480)) {
    PortDebug::SetSimRate(static_cast< unsigned >(simRate));
  }
  ImGui::EndDisabled();
  if (adaptive) {
    ImGui::TextWrapped(
        "One step per frame with dt = the measured frame time (clamped 30-480 Hz), "
        "so a variable frame rate is matched exactly. Leave the FPS cap off.");
  } else if (simRate != 60) {
    ImGui::TextWrapped(
        "60 Hz is console-accurate. Higher values step the game logic at the display "
        "rate instead of interpolating the camera; leave the FPS cap off for it to "
        "matter.");
  }
}

void DrawCutscenesTab() {
  if (ImGui::Checkbox("Skip / fast-forward cutscenes", &sSkipCutscenes)) {
    MarkDirty();
  }
  if (ImGui::SliderFloat("Cutscene speed", &sCutsceneSpeed, 1.f, 32.f, "%.0fx")) {
    MarkDirty();
  }
  if (ImGui::Button("Reset cutscene speed")) {
    sCutsceneSpeed = 8.f;
    MarkDirty();
  }
}

void DrawRenderTab() {
  bool vsync = sVsyncEnabled;
  if (ImGui::Checkbox("Vsync", &vsync)) {
    SetVsyncEnabled(vsync);
    MarkDirty();
  }

  int aspect = static_cast< int >(sAspectMode);
  if (ImGui::Combo("Aspect ratio", &aspect, "4:3\0" "16:9\0" "Follow window\0")) {
    SetAspectMode(static_cast< EAspectMode >(aspect));
    MarkDirty();
  }

  bool hudWide = sHudWide;
  if (ImGui::Checkbox("Widescreen HUD (spread to edges)", &hudWide)) {
    SetHudWide(hudWide);
    MarkDirty();
  }
  ImGui::TextWrapped(
      "Keeps each HUD element's shape but spreads its position so edge elements "
      "reach the wide corners. Only affects the in-game HUD, not menus.");

  bool autoScale = sRenderScale <= 0.f;
  if (ImGui::Checkbox("Auto render scale (native)", &autoScale)) {
    SetRenderScale(autoScale ? 0.f : 1.f);
    MarkDirty();
  }
  if (!autoScale) {
    float scale = sRenderScale;
    if (ImGui::SliderFloat("EFB scale", &scale, 1.f, 2.f, "%.2fx")) {
      SetRenderScale(scale);
      MarkDirty();
    }
    ImGui::TextUnformatted("Scales the internal EFB; higher values use more GPU memory.");
  }

  ImGui::Text("HD texture set: %s", PortTextures::DeviceName());
  ImGui::TextWrapped(
      "Selected from the connected controller (xbox, playstation, switch, "
      "gamecube, standard, keyboard); set MP_TEXTURE_DEVICE to override.");
}

void DrawInputTab() {
  bool mouseAim = sMouseAim;
  if (ImGui::Checkbox("Mouse aim", &mouseAim)) {
    SetMouseAim(mouseAim);
    MarkDirty();
  }
  bool twinStick = sTwinStick;
  if (ImGui::Checkbox("Twin stick (right stick aims)", &twinStick)) {
    SetTwinStick(twinStick);
    MarkDirty();
  }
  ImGui::BeginDisabled(!sTwinStick);
  float stickRate = sStickAimRate;
  if (ImGui::SliderFloat("Stick aim speed", &stickRate, 100.f, 3000.f, "%.0f px/s",
                         ImGuiSliderFlags_Logarithmic)) {
    SetStickAimRate(stickRate);
  }
  ImGui::EndDisabled();
  ImGui::TextWrapped(
      "Twin stick uses the right stick as a direct camera aim (the same path as "
      "the mouse) and consumes it, so it no longer free-looks. Fire stays on "
      "whatever is bound to A; remap it in the Controls tab.");
  ImGui::SeparatorText("Gyro aim");
  const char* gyroModes[] = {"Off", "Hold to aim", "Always aim"};
  int gyroMode = sGyroMode;
  if (ImGui::Combo("Mode", &gyroMode, gyroModes, 3)) {
    SetGyroMode(gyroMode);
  }
  ImGui::BeginDisabled(sGyroMode == 0);
  const char* gyroSources[] = {"Auto", "Controller", "Phone"};
  int gyroSource = sGyroSource;
  if (ImGui::Combo("Source", &gyroSource, gyroSources, 3)) {
    SetGyroSource(gyroSource);
  }
  float gyroRate = sGyroRate;
  if (ImGui::SliderFloat("Sensitivity", &gyroRate, 50.f, 3000.f, "%.0f px/s per rad/s",
                         ImGuiSliderFlags_Logarithmic)) {
    SetGyroRate(gyroRate);
  }
  ImGui::Text("Gyro: %s", GyroStatus());
  ImGui::TextWrapped(
      "Tilt the pad or the phone to aim. Hold to aim uses right stick click or "
      "left ctrl. Needs mouse aim or twin stick, since the gyro feeds that same "
      "aim.");
  ImGui::EndDisabled();

  if (ImGui::Checkbox("Invert mouse X", &sMouseInvertX)) {
    MarkDirty();
  }
  if (ImGui::Checkbox("Invert mouse Y", &sMouseInvertY)) {
    MarkDirty();
  }
  if (ImGui::Checkbox("Mouse weapon buttons", &sMouseButtons)) {
    sMouseButtonGate.Reset();
    MarkDirty();
  }
  if (ImGui::Checkbox("Mouse-aim crosshair", &sMouseCrosshair)) {
    MarkDirty();
  }
  ImGui::TextUnformatted("Left: fire/charge   Right: lock-on   Middle: missile");
  ImGui::TextUnformatted("Existing keyboard/controller weapon bindings also work.");
  if (ImGui::SliderFloat("Sensitivity", &sMouseSensitivity, 0.0005f, 0.02f, "%.4f rad/px",
                         ImGuiSliderFlags_Logarithmic)) {
    MarkDirty();
  }
}

void DrawAudioTab() {
  bool ai = AiAudioEnabled();
  if (ImGui::Checkbox("Streamed audio (music/movies)", &ai)) {
    SetAiAudioEnabled(ai);
    MarkDirty();
  }
  bool musyx = sMusyxAudioEnabled;
  if (ImGui::Checkbox("MusyX audio (effects/streams)", &musyx)) {
    SetMusyxAudioEnabled(musyx);
    MarkDirty();
  }
}

// ---------------------------------------------------------------------------
// Controls: rebind keyboard/mouse and controller inputs to the emulated pad.
// The binding backend (matching, persistence, name helpers) lives in Aurora.
// ---------------------------------------------------------------------------

constexpr u32 kControlPort = PAD_CHAN0;

struct SControlPadButton {
  PADButton button;
  const char* label;
};
const SControlPadButton kControlPadButtons[] = {
    {PAD_BUTTON_A, "A"},             {PAD_BUTTON_B, "B"},
    {PAD_BUTTON_X, "X"},             {PAD_BUTTON_Y, "Y"},
    {PAD_TRIGGER_L, "L"},            {PAD_TRIGGER_R, "R"},
    {PAD_TRIGGER_Z, "Z"},            {PAD_BUTTON_START, "Start"},
    {PAD_BUTTON_UP, "D-pad Up"},     {PAD_BUTTON_DOWN, "D-pad Down"},
    {PAD_BUTTON_LEFT, "D-pad Left"}, {PAD_BUTTON_RIGHT, "D-pad Right"},
};

enum class EControlCapture { kNone, kKeyButton, kKeyAxis, kPadButton, kPadAxis };
EControlCapture sControlCapture = EControlCapture::kNone;
int sControlCaptureIndex = 0;

std::string ScancodeName(s32 scancode) {
  switch (scancode) {
  case PAD_KEY_INVALID:
    return "(unbound)";
  case PAD_KEY_MOUSE_LEFT:
    return "Mouse Left";
  case PAD_KEY_MOUSE_MIDDLE:
    return "Mouse Middle";
  case PAD_KEY_MOUSE_RIGHT:
    return "Mouse Right";
  case PAD_KEY_MOUSE_X1:
    return "Mouse X1";
  case PAD_KEY_MOUSE_X2:
    return "Mouse X2";
  default:
    break;
  }
  const char* name = SDL_GetScancodeName(static_cast< SDL_Scancode >(scancode));
  return name != nullptr && name[0] != '\0' ? name : "(unknown)";
}

std::string PadAxisName(PADAxis axis) {
  const char* name = PADGetAxisName(axis);
  const char* dir = PADGetAxisDirectionLabel(axis);
  if (name == nullptr) {
    return "(axis)";
  }
  return dir != nullptr ? std::string(name) + " " + dir : std::string(name);
}

s32 KeyForPadButton(const PADKeyButtonBinding* list, u32 count, PADButton button) {
  for (u32 i = 0; i < count; ++i) {
    if (list[i].padButton == button) {
      return list[i].scancode;
    }
  }
  return PAD_KEY_INVALID;
}

s32 KeyForPadAxis(const PADKeyAxisBinding* list, u32 count, PADAxis axis) {
  for (u32 i = 0; i < count; ++i) {
    if (list[i].padAxis == axis) {
      return list[i].scancode;
    }
  }
  return PAD_KEY_INVALID;
}

u32 NativeButtonForPadButton(const PADButtonMapping* list, u32 count, PADButton button) {
  for (u32 i = 0; i < count; ++i) {
    if (list[i].padButton == button) {
      return list[i].nativeButton;
    }
  }
  return PAD_NATIVE_BUTTON_INVALID;
}

// While a keyboard binding is being captured, bind the next newly pressed key or
// mouse button. Controller captures poll the pad directly.
void PollControlCapture() {
  if (sControlCapture == EControlCapture::kNone) {
    return;
  }

  if (sControlCapture == EControlCapture::kPadButton) {
    const s32 native = PADGetNativeButtonPressed(kControlPort);
    if (native >= 0) {
      PADButtonMapping mapping{};
      mapping.nativeButton = static_cast< u32 >(native);
      mapping.padButton = kControlPadButtons[sControlCaptureIndex].button;
      PADSetButtonMapping(kControlPort, mapping);
      PADSerializeMappings();
      sControlCapture = EControlCapture::kNone;
    }
    return;
  }
  if (sControlCapture == EControlCapture::kPadAxis) {
    const PADSignedNativeAxis pulled = PADGetNativeAxisPulled(kControlPort);
    if (pulled.nativeAxis >= 0) {
      PADAxisMapping mapping{};
      mapping.nativeAxis = pulled;
      mapping.nativeButton = static_cast< s32 >(PAD_NATIVE_BUTTON_INVALID);
      mapping.padAxis = static_cast< PADAxis >(sControlCaptureIndex);
      PADSetAxisMapping(kControlPort, mapping);
      PADSerializeMappings();
      sControlCapture = EControlCapture::kNone;
    }
    return;
  }

  static bool sPrevKeys[SDL_SCANCODE_COUNT] = {};
  static Uint32 sPrevMouse = 0;
  const bool* keys = SDL_GetKeyboardState(nullptr);
  float mx = 0.f;
  float my = 0.f;
  const Uint32 mouse = SDL_GetMouseState(&mx, &my);

  s32 scancode = PAD_KEY_INVALID;
  for (int i = 0; i < SDL_SCANCODE_COUNT; ++i) {
    if (keys[i] && !sPrevKeys[i]) {
      scancode = i;
      break;
    }
  }
  if (scancode == PAD_KEY_INVALID) {
    const Uint32 kMouseButtons[] = {SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT, SDL_BUTTON_X1,
                                    SDL_BUTTON_X2};
    const s32 kMouseCodes[] = {PAD_KEY_MOUSE_LEFT, PAD_KEY_MOUSE_MIDDLE, PAD_KEY_MOUSE_RIGHT,
                               PAD_KEY_MOUSE_X1, PAD_KEY_MOUSE_X2};
    for (int i = 0; i < 5; ++i) {
      const Uint32 mask = SDL_BUTTON_MASK(kMouseButtons[i]);
      if ((mouse & mask) != 0 && (sPrevMouse & mask) == 0) {
        scancode = kMouseCodes[i];
        break;
      }
    }
  }
  for (int i = 0; i < SDL_SCANCODE_COUNT; ++i) {
    sPrevKeys[i] = keys[i];
  }
  sPrevMouse = mouse;

  if (scancode == PAD_KEY_INVALID) {
    return;
  }
  if (sControlCapture == EControlCapture::kKeyButton) {
    PADKeyButtonBinding binding{};
    binding.scancode = scancode;
    binding.padButton = kControlPadButtons[sControlCaptureIndex].button;
    PADSetKeyButtonBinding(kControlPort, binding);
  } else {
    PADKeyAxisBinding binding{};
    binding.scancode = scancode;
    binding.padAxis = static_cast< PADAxis >(sControlCaptureIndex);
    binding.influence = 1;
    PADSetKeyAxisBinding(kControlPort, binding);
  }
  PADSerializeMappings();
  sControlCapture = EControlCapture::kNone;
}

void DrawControlsTab() {
  PollControlCapture();

  ImGui::TextUnformatted("Pad 1. Click Bind, then press the input to assign it.");
  if (ImGui::Button("Restore controller defaults")) {
    PADRestoreDefaultMapping(kControlPort);
    PADSerializeMappings();
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear keyboard bindings")) {
    PADClearKeyBindings(kControlPort);
    PADSerializeMappings();
  }
  if (sControlCapture != EControlCapture::kNone) {
    ImGui::SameLine();
    ImGui::TextUnformatted("... waiting for input");
  }

  u32 keyButtonCount = 0;
  PADKeyButtonBinding* keyButtons = PADGetKeyButtonBindings(kControlPort, &keyButtonCount);
  u32 keyAxisCount = 0;
  PADKeyAxisBinding* keyAxes = PADGetKeyAxisBindings(kControlPort, &keyAxisCount);
  u32 padButtonCount = 0;
  PADButtonMapping* padButtons = PADGetButtonMappings(kControlPort, &padButtonCount);
  u32 padAxisCount = 0;
  PADAxisMapping* padAxes = PADGetAxisMappings(kControlPort, &padAxisCount);

  if (ImGui::CollapsingHeader("Keyboard & mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (int i = 0; i < static_cast< int >(ARRAY_SIZE(kControlPadButtons)); ++i) {
      ImGui::PushID(i);
      const PADButton button = kControlPadButtons[i].button;
      const bool listening =
          sControlCapture == EControlCapture::kKeyButton && sControlCaptureIndex == i;
      if (ImGui::Button(listening ? "Press..." : "Bind", ImVec2(70.f, 0.f))) {
        sControlCapture = EControlCapture::kKeyButton;
        sControlCaptureIndex = i;
      }
      ImGui::SameLine();
      if (ImGui::Button("Clear", ImVec2(60.f, 0.f))) {
        PADKeyButtonBinding binding{};
        binding.scancode = PAD_KEY_INVALID;
        binding.padButton = button;
        PADSetKeyButtonBinding(kControlPort, binding);
        PADSerializeMappings();
      }
      ImGui::SameLine();
      ImGui::Text("%-12s %s", kControlPadButtons[i].label,
                  ScancodeName(KeyForPadButton(keyButtons, keyButtonCount, button)).c_str());
      ImGui::PopID();
    }
    for (int i = 0; i < PAD_AXIS_COUNT; ++i) {
      ImGui::PushID(100 + i);
      const bool listening = sControlCapture == EControlCapture::kKeyAxis && sControlCaptureIndex == i;
      if (ImGui::Button(listening ? "Press..." : "Bind", ImVec2(70.f, 0.f))) {
        sControlCapture = EControlCapture::kKeyAxis;
        sControlCaptureIndex = i;
      }
      ImGui::SameLine();
      if (ImGui::Button("Clear", ImVec2(60.f, 0.f))) {
        PADKeyAxisBinding binding{};
        binding.scancode = PAD_KEY_INVALID;
        binding.padAxis = static_cast< PADAxis >(i);
        binding.influence = 1;
        PADSetKeyAxisBinding(kControlPort, binding);
        PADSerializeMappings();
      }
      ImGui::SameLine();
      ImGui::Text("%-12s %s", PadAxisName(static_cast< PADAxis >(i)).c_str(),
                  ScancodeName(KeyForPadAxis(keyAxes, keyAxisCount, static_cast< PADAxis >(i))).c_str());
      ImGui::PopID();
    }
  }

  if (ImGui::CollapsingHeader("Controller", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (int i = 0; i < static_cast< int >(ARRAY_SIZE(kControlPadButtons)); ++i) {
      ImGui::PushID(200 + i);
      const PADButton button = kControlPadButtons[i].button;
      const bool listening =
          sControlCapture == EControlCapture::kPadButton && sControlCaptureIndex == i;
      if (ImGui::Button(listening ? "Press..." : "Bind", ImVec2(70.f, 0.f))) {
        sControlCapture = EControlCapture::kPadButton;
        sControlCaptureIndex = i;
      }
      ImGui::SameLine();
      const u32 native = NativeButtonForPadButton(padButtons, padButtonCount, button);
      const char* nativeName =
          native == PAD_NATIVE_BUTTON_INVALID ? "(unbound)" : PADGetNativeButtonName(native);
      ImGui::Text("%-12s %s", kControlPadButtons[i].label,
                  nativeName != nullptr ? nativeName : "(unknown)");
      ImGui::PopID();
    }
    for (int i = 0; i < PAD_AXIS_COUNT; ++i) {
      ImGui::PushID(300 + i);
      const bool listening = sControlCapture == EControlCapture::kPadAxis && sControlCaptureIndex == i;
      if (ImGui::Button(listening ? "Press..." : "Bind", ImVec2(70.f, 0.f))) {
        sControlCapture = EControlCapture::kPadAxis;
        sControlCaptureIndex = i;
      }
      ImGui::SameLine();
      const char* nativeName = "(unbound)";
      for (u32 j = 0; j < padAxisCount; ++j) {
        if (padAxes[j].padAxis == static_cast< PADAxis >(i)) {
          const char* axisName = PADGetNativeAxisName(padAxes[j].nativeAxis);
          nativeName = axisName != nullptr ? axisName : "(axis)";
          break;
        }
      }
      ImGui::Text("%-12s %s", PadAxisName(static_cast< PADAxis >(i)).c_str(), nativeName);
      ImGui::PopID();
    }
  }
}

void DrawVoicesTab() {  PortMusyxVoice voices[64];
  const int count = MusyxPortCopyVoices(voices, 64);
  if (count == 0) {
    ImGui::TextUnformatted("No active MusyX voices.");
    return;
  }

  struct Agg {
    PortMusyxVoice voice;
    int instances;
  };
  std::vector< Agg > aggs;
  for (int i = 0; i < count; ++i) {
    bool found = false;
    for (Agg& agg : aggs) {
      if (agg.voice.smpId == voices[i].smpId) {
        ++agg.instances;
        if (voices[i].rms > agg.voice.rms) {
          agg.voice = voices[i];
        }
        found = true;
        break;
      }
    }
    if (!found) {
      aggs.push_back(Agg{voices[i], 1});
    }
  }
  std::sort(aggs.begin(), aggs.end(),
            [](const Agg& a, const Agg& b) { return a.voice.rms > b.voice.rms; });

  ImGui::TextUnformatted("Active samples, loudest first. Mute one to isolate it.");
  for (const Agg& agg : aggs) {
    ImGui::PushID(static_cast< int >(agg.voice.smpId));
    bool muted = MusyxPortIsSampleMuted(agg.voice.smpId) != 0;
    if (ImGui::Checkbox("##mute", &muted)) {
      MusyxPortSetSampleMuted(agg.voice.smpId, muted ? 1 : 0);
      MarkDirty();
    }
    ImGui::SameLine();
    ImGui::Text("smp %u  %s  len %u  pitch %u  rms %d  vol %u/%u  x%d", agg.voice.smpId,
                agg.voice.looped ? "loop" : "one-shot", agg.voice.length, agg.voice.pitch,
                agg.voice.rms, agg.voice.volL, agg.voice.volR, agg.instances);
    ImGui::PopID();
  }
  if (ImGui::Button("Unmute all")) {
    MusyxPortClearSampleMutes();
    MarkDirty();
  }
}

void DrawSessionTab() {
  if (ImGui::Button("Restart to menu")) {
    RequestReset();
  }
  ImGui::SameLine();
  if (ImGui::Button("Screenshot (F12)")) {
    aurora::request_screenshot();
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Settings are saved automatically when changed.");
  const std::string path = SettingsFilePath();
  ImGui::TextWrapped("File: %s", path.c_str());
  if (ImGui::Button("Save settings now")) {
    sSettingsDirty = true;
    SaveSettings();
  }
  ImGui::SameLine();
  ImGui::TextUnformatted(sSettingsDirty ? "Unsaved changes" : "Saved");
}

void GrantItem(CPlayerState& ps, CPlayerState::EItemType type, int amount, int capacity) {
  ps.SetPowerUp(type, capacity);
  ps.SetPickup(type, amount);
}

void DrawDebugTab() {
  CStateManager* mgr = sStateManager;
  if (mgr == nullptr) {
    ImGui::TextUnformatted("Waiting for gameplay...");
    return;
  }
  CPlayerState* ps = mgr->PlayerState();
  if (ps == nullptr) {
    ImGui::TextUnformatted("No player state.");
    return;
  }

  ImGui::Text("Health: %.0f / %.0f", ps->HealthInfo()->GetHP(), ps->CalculateHealth());
  if (ImGui::Button("Full health")) {
    ps->HealthInfo()->SetHP(ps->CalculateHealth());
  }
  ImGui::SameLine();
  if (ImGui::Button("Grant everything")) {
    for (int i = CPlayerState::kIT_PowerBeam; i < CPlayerState::kIT_Max; ++i) {
      GrantItem(*ps, static_cast< CPlayerState::EItemType >(i), 1, 1);
    }
    GrantItem(*ps, CPlayerState::kIT_Missiles, 250, 250);
    GrantItem(*ps, CPlayerState::kIT_PowerBombs, 8, 8);
    GrantItem(*ps, CPlayerState::kIT_EnergyTanks, 14, 14);
    ps->HealthInfo()->SetHP(ps->CalculateHealth());
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Abilities");
  struct SItemToggle {
    const char* name;
    CPlayerState::EItemType type;
  };
  static const SItemToggle kItems[] = {
      {"Power Beam", CPlayerState::kIT_PowerBeam},
      {"Ice Beam", CPlayerState::kIT_IceBeam},
      {"Wave Beam", CPlayerState::kIT_WaveBeam},
      {"Plasma Beam", CPlayerState::kIT_PlasmaBeam},
      {"Charge Beam", CPlayerState::kIT_ChargeBeam},
      {"Super Missile", CPlayerState::kIT_SuperMissile},
      {"Ice Spreader", CPlayerState::kIT_IceSpreader},
      {"Wavebuster", CPlayerState::kIT_Wavebuster},
      {"Flamethrower", CPlayerState::kIT_Flamethrower},
      {"Combat Visor", CPlayerState::kIT_CombatVisor},
      {"Scan Visor", CPlayerState::kIT_ScanVisor},
      {"Thermal Visor", CPlayerState::kIT_ThermalVisor},
      {"X-Ray Visor", CPlayerState::kIT_XRayVisor},
      {"Morph Ball", CPlayerState::kIT_MorphBall},
      {"Morph Ball Bombs", CPlayerState::kIT_MorphBallBombs},
      {"Boost Ball", CPlayerState::kIT_BoostBall},
      {"Spider Ball", CPlayerState::kIT_SpiderBall},
      {"Space Jump Boots", CPlayerState::kIT_SpaceJumpBoots},
      {"Grapple Beam", CPlayerState::kIT_GrappleBeam},
      {"Gravity Suit", CPlayerState::kIT_GravitySuit},
      {"Varia Suit", CPlayerState::kIT_VariaSuit},
      {"Phazon Suit", CPlayerState::kIT_PhazonSuit},
  };
  for (const SItemToggle& item : kItems) {
    bool owned = ps->HasPowerUp(item.type);
    if (ImGui::Checkbox(item.name, &owned)) {
      if (owned) {
        GrantItem(*ps, item.type, 1, 1);
      } else {
        ps->SetPowerUp(item.type, 0);
        ps->SetPickup(item.type, 0);
      }
    }
  }

  int missiles = ps->GetItemAmount(CPlayerState::kIT_Missiles);
  if (ImGui::SliderInt("Missiles", &missiles, 0, 250)) {
    GrantItem(*ps, CPlayerState::kIT_Missiles, missiles, 250);
  }
  int powerBombs = ps->GetItemAmount(CPlayerState::kIT_PowerBombs);
  if (ImGui::SliderInt("Power Bombs", &powerBombs, 0, 8)) {
    GrantItem(*ps, CPlayerState::kIT_PowerBombs, powerBombs, 8);
  }
  int tanks = ps->GetItemAmount(CPlayerState::kIT_EnergyTanks);
  if (ImGui::SliderInt("Energy Tanks", &tanks, 0, 14)) {
    GrantItem(*ps, CPlayerState::kIT_EnergyTanks, tanks, 14);
    ps->HealthInfo()->SetHP(ps->CalculateHealth());
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Teleport");
  CWorld* world = mgr->World();
  if (world == nullptr) {
    ImGui::TextUnformatted("No world.");
    return;
  }
  if (mgr->GetGameState() != CStateManager::kGS_Running) {
    ImGui::TextUnformatted("(waiting for gameplay)");
  }
  const int areaCount = world->IGetAreaCount();
  const int current = world->GetCurrentAreaId().Value();
  ImGui::Text("Current area: %d of %d", current, areaCount);
  for (int i = 0; i < areaCount; ++i) {
    ImGui::PushID(i);
    if (ImGui::Button(i == current ? "Reload" : "Go")) {
      PortDebug::RequestTeleport(i);
    }
    ImGui::SameLine();
    ImGui::Text("Area %d", i);
    ImGui::PopID();
  }

  ImGui::Separator();
  ImGui::TextUnformatted("Worlds");
  if (gpMemoryCard == nullptr) {
    ImGui::TextUnformatted("(memory card not ready)");
    return;
  }
  static bool sWorldListBuilt = false;
  static std::vector< std::pair< uint32_t, std::string > > sWorldList;
  if (!sWorldListBuilt && !gpMemoryCard->GetMemoryWorlds().empty()) {
    sWorldListBuilt = true;
    const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
    for (int i = 0; i < worlds.size(); ++i) {
      const uint32_t id = static_cast< uint32_t >(worlds[i].first);
      std::string name;
      const wchar_t* wide = worlds[i].second.GetFrontEndName();
      if (wide != nullptr) {
        for (const wchar_t* p = wide; *p != 0; ++p) {
          name.push_back(static_cast< char >(*p));
        }
      }
      if (name.empty()) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "MLVL %08X", static_cast< unsigned >(id));
        name = buf;
      }
      sWorldList.emplace_back(id, name);
    }
  }
  if (!sWorldListBuilt) {
    ImGui::TextUnformatted("(loading worlds...)");
    return;
  }
  for (const std::pair< uint32_t, std::string >& entry : sWorldList) {
    ImGui::PushID(static_cast< int >(entry.first));
    const bool isCurrent =
        gpGameState != nullptr && gpGameState->CurrentWorldAssetId() == entry.first;
    if (ImGui::Button(isCurrent ? "Here" : "Go")) {
      PortDebug::RequestWorldTeleport(entry.first, 0u);
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(entry.second.c_str());
    ImGui::PopID();
  }
}

void DrawUI() {
  EnsureInitialized();
  if (!sAudioSettingsApplied) {
    // Apply persisted audio mutes once the backends are alive.
    sAudioSettingsApplied = true;
    SetAiAudioEnabled(sAiAudioEnabled);
    SetMusyxAudioEnabled(sMusyxAudioEnabled);
  }
  if (!sPresentationSettingsApplied) {
    // Apply persisted vsync once the swapchain surface exists (first drawn
    // frame), so the present mode is chosen from real surface capabilities.
    sPresentationSettingsApplied = true;
    aurora_enable_vsync(sVsyncEnabled);
  }
  if (!sVisible) {
    return;
  }

  ImGui::SetNextWindowPos(ImVec2(8.f, 8.f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(440.f, 200.f), ImGuiCond_FirstUseEver);
  bool open = true;
  if (ImGui::Begin("Metroid Prime Port", &open, ImGuiWindowFlags_MenuBar)) {
    if (ImGui::BeginMenuBar()) {
      ImGui::TextUnformatted("F1: hide   F10: frame limit   F12: screenshot");
      ImGui::EndMenuBar();
    }

    if (ImGui::BeginTabBar("##debug_tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
      if (ImGui::BeginTabItem("Performance")) {
        DrawPerformanceTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Cutscenes")) {
        DrawCutscenesTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Input")) {
        DrawInputTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Controls")) {
        DrawControlsTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Render")) {
        DrawRenderTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Audio")) {
        DrawAudioTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Voices")) {
        DrawVoicesTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Debug")) {
        DrawDebugTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Session")) {
        DrawSessionTab();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
  ImGui::End();

  if (!open) {
    sVisible = false;
  }

  if (sSettingsDirty) {
    SaveSettings();
  }
}

void LoadDiscPath() {
  const std::string path = SettingsFilePath();
  std::ifstream file(path);
  if (!file.is_open()) {
    return;
  }
  std::string line;
  while (std::getline(file, line)) {
    const size_t separator = line.find('=');
    if (separator == std::string::npos || Trim(line.substr(0, separator)) != "disc_path") {
      continue;
    }
    sDiscPath = Trim(line.substr(separator + 1));
    std::fprintf(stderr, "metroid_prime_port: saved disc image %s\n", sDiscPath.c_str());
    return;
  }
}

const char* DiscPath() {
  return sDiscPath.empty() ? nullptr : sDiscPath.c_str();
}

void SetDiscPath(const char* path) {
  sDiscPath = path != nullptr ? path : "";
  sSettingsDirty = true;
}

} // namespace PortDebug

#if defined(__ANDROID__)
// The touch overlay covers the display and consumes every touch before SDL
// sees it. While the debug overlay is open the game is paused and those touches
// belong to ImGui, so the Java side asks this and stops claiming them.
#if defined(__ANDROID__)
namespace {
// The touch overlay acts as a real gamepad rather than synthesising keyboard
// keys, so the game's own controller mapping, prompts and rebinding apply to it
// unchanged, and its sticks are sticks rather than four keys pretending to be
// one.
SDL_Joystick* g_virtualPad = nullptr;

SDL_Joystick* VirtualPad() {
  if (g_virtualPad != nullptr) {
    return g_virtualPad;
  }
  SDL_VirtualJoystickDesc desc;
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
  // Report as a common Xbox pad so the pad type, and with it the port's prompt
  // icons, match what the overlay draws.
  desc.vendor_id = 0x045e;
  desc.product_id = 0x02ea;
  desc.name = "Metroid Prime touch gamepad";
  const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
  if (id == 0) {
    __android_log_print(ANDROID_LOG_ERROR, "touchpad", "attach failed: %s", SDL_GetError());
    return nullptr;
  }
  // Install our own mapping even though SDL recognises those Xbox ids. The
  // overlay writes SDL_Gamepad* indices straight into
  // SDL_SetJoystickVirtualAxis/SDL_SetJoystickVirtualButton, which describe the
  // right controls only when the pad's mapping is this plain layout. SDL's
  // built-in Xbox mapping binds rightx:a3, righty:a4, start:b8 and the
  // shoulders to b4/b5, so the right stick, Start and the beams landed on the
  // wrong controls. An API mapping outranks the built-in table and is reloaded
  // onto the already-attached pad.
  {
    char guid[64];
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(id), guid, sizeof(guid));
    char mapping[512];
    SDL_snprintf(mapping, sizeof(mapping),
                 "%s,Metroid Prime touch gamepad,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,"
                 "leftshoulder:b9,rightshoulder:b10,dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
                 "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,",
                 guid);
    SDL_AddGamepadMapping(mapping);
  }
  g_virtualPad = SDL_OpenJoystick(id);
  __android_log_print(ANDROID_LOG_INFO, "touchpad", "attached id=%d gamepad=%d open=%d", id,
                      SDL_IsGamepad(id) ? 1 : 0, g_virtualPad != nullptr ? 1 : 0);
  return g_virtualPad;
  __android_log_print(ANDROID_LOG_INFO, "touchpad", "attached id=%d gamepad=%d open=%d", id,
                      SDL_IsGamepad(id) ? 1 : 0, g_virtualPad != nullptr ? 1 : 0);
  return g_virtualPad;
}
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_metroidprime_port_TouchControlsView_nativeVirtualButton(JNIEnv*, jclass, jint button,
                                                                 jboolean down) {
  if (SDL_Joystick* pad = VirtualPad()) {
    SDL_SetJoystickVirtualButton(pad, static_cast< int >(button), down == JNI_TRUE);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_org_metroidprime_port_TouchControlsView_nativeVirtualAxis(JNIEnv*, jclass, jint axis,
                                                               jfloat value) {
  if (SDL_Joystick* pad = VirtualPad()) {
    // The overlay sends a normalised -1..1 deflection, but the virtual joystick
    // API takes a Sint16 joystick-axis value. Passing the float straight through
    // truncated every partial deflection to 0, so the sticks and triggers read
    // as centred and the player could neither move nor aim.
    const float clamped = value < -1.f ? -1.f : (value > 1.f ? 1.f : value);
    SDL_SetJoystickVirtualAxis(pad, static_cast< int >(axis),
                               static_cast< Sint16 >(std::lround(clamped * 32767.f)));
  }
}
#endif

extern "C" JNIEXPORT jboolean JNICALL
Java_org_metroidprime_port_TouchControlsView_nativeDebugOverlayVisible(JNIEnv*, jclass) {
  return PortDebug::OverlayVisible() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_metroidprime_port_TouchControlsView_nativeSetTouchDevice(JNIEnv*, jclass, jboolean xbox) {
  PortPrompts::NoteTouchInput(xbox == JNI_TRUE);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_metroidprime_port_TouchControlsView_nativeTwinStick(JNIEnv*, jclass) {
  return PortDebug::TwinStickFlag() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_metroidprime_port_TouchControlsView_nativeToggleDebugOverlay(JNIEnv*, jclass) {
  PortDebug::RequestToggle();
}
#endif
