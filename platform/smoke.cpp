// Opt-in lifecycle driver for real-disc regression runs (MP_ENABLE_SMOKE_DRIVER).
#include "compat.h"
#include "port_debug.h"
#include "port_mouse.h"
#include "port_smoke.h"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "MetroidPrime/CGameArea.hpp"
#include "Kyoto/CARAMToken.hpp"
#include "MetroidPrime/Cameras/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerGun.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include <dolphin/pad.h>
#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace aurora {
void request_screenshot() noexcept;
}

namespace {
unsigned sMouseTicks = 0;
unsigned sMouseShots = 0, sMouseChargedShots = 0, sMouseMissiles = 0, sMouseLocks = 0;
unsigned sGunViewChecks = 0;
bool sMouseJumped = false;
bool sMouseMorphed = false, sMouseResumed = false;
bool sStrafedBeforeUI = false, sStrafedAfterUI = false;
bool sPowerProjectileSeen = false, sMissileProjectileSeen = false;
bool sMouseComplete = false;
unsigned sAreaReloads = 0;
bool AreaReloadEnabled() {
  static const bool enabled = std::getenv("MP_SMOKE_AREA_RELOAD") != nullptr;
  return enabled;
}
bool sWasLocked = false;
CVector3f sLastLockedDirection(0.f, 1.f, 0.f);
CVector3f sInitialDirection(0.f, 1.f, 0.f);
void MouseCheck(bool valid, const char* message) {
  if (!valid) {
    std::fprintf(stderr, "[mouse-smoke] failed at tick %u: %s\n", sMouseTicks, message);
    std::abort();
  }
}
}

void PortSmokeAreaReload(CStateManager& mgr) {
  static const unsigned pauseAfterTicks = [] {
    const char* value = std::getenv("MP_SMOKE_PAUSE");
    return value != nullptr ? static_cast<unsigned>(std::strtoul(value, nullptr, 10)) : 0;
  }();
  static unsigned sPauseTicks = 0;
  if (pauseAfterTicks != 0 && mgr.GetGameState() == CStateManager::kGS_Running &&
      mgr.GetCameraManager()->IsInFPCamera() &&
      !mgr.GetCameraManager()->IsInCinematicCamera()) {
    if (++sPauseTicks == pauseAfterTicks) {
      std::fputs("[smoke] entering pause screen\n", stderr);
      mgr.EnterPauseScreen();
    }
  }
  // MP_SMOKE_MAP=<ticks>: press Z to open the map screen. Z opens the map from
  // gameplay; in the pause screen it does nothing.
  static const unsigned mapAfterTicks = [] {
    const char* value = std::getenv("MP_SMOKE_MAP");
    return value != nullptr ? static_cast<unsigned>(std::strtoul(value, nullptr, 10)) : 0;
  }();
  if (mapAfterTicks != 0 && mgr.GetGameState() == CStateManager::kGS_Running &&
      mgr.GetCameraManager()->IsInFPCamera() &&
      !mgr.GetCameraManager()->IsInCinematicCamera()) {
    static unsigned sMapTicks = 0;
    if (++sMapTicks == mapAfterTicks) {
      std::fputs("[smoke] pressing Z for the map screen\n", stderr);
    }
    if (sMapTicks >= mapAfterTicks && sMapTicks < mapAfterTicks + 20) {
      PADStatus status{};
      status.err = PAD_ERR_NONE;
      status.button = PAD_TRIGGER_Z;
      PADSetVirtualStatus(0, &status);
    }
  }
  static const bool morphEnabled = std::getenv("MP_SMOKE_MORPH") != nullptr;
  static unsigned sMorphTicks = 0;
  static bool sMorphDone = false;
  if (morphEnabled && !sMorphDone && mgr.GetGameState() == CStateManager::kGS_Running &&
      mgr.GetCameraManager()->IsInFPCamera() &&
      !mgr.GetCameraManager()->IsInCinematicCamera()) {
    ++sMorphTicks;
    PADStatus status{};
    status.err = PAD_ERR_NONE;
    if ((sMorphTicks >= 120 && sMorphTicks < 126) || (sMorphTicks >= 360 && sMorphTicks < 366)) {
      status.button = PAD_BUTTON_X;
    }
    PADSetVirtualStatus(0, &status);
    if (sMorphTicks == 1) {
      std::fputs("[morph-smoke] begin\n", stderr);
    }
    if (sMorphTicks > 480) {
      sMorphDone = true;
      PADClearVirtualStatus(0);
      std::fputs("[morph-smoke] done\n", stderr);
    }
  }
  if (!AreaReloadEnabled() || sAreaReloads >= 3 || mgr.GetGameState() != CStateManager::kGS_Running ||
      mgr.GetCameraManager()->IsInCinematicCamera() || !mgr.GetCameraManager()->IsInFPCamera()) return;
  CWorld* world = mgr.World();
  if (!world || !world->DoesAreaExist(world->GetCurrentAreaId())) return;
  CGameArea* area = world->Area(world->GetCurrentAreaId());
  if (!area->IsLoaded() || area->GetOcclusionState() != CGameArea::kOS_Visible) return;
  GXDrawDone();
  std::fprintf(stderr, "[area-smoke] evict/restore area %d cycle %u\n", area->GetId().Value(), sAreaReloads + 1);
  area->SetOcclusionState(CGameArea::kOS_Occluded);
  const uint64_t deadline = SDL_GetTicksNS() + 5000000000ull;
  while (!area->TransferTokensToARAM()) {
    CARAMToken::UpdateAllDMAs();
    MouseCheck(SDL_GetTicksNS() < deadline, "area eviction stalled");
  }
  area->SetOcclusionState(CGameArea::kOS_Visible);
  ++sAreaReloads;
  if (sAreaReloads == 3) std::fputs("[area-smoke] passed: three geometry eviction/reload cycles\n", stderr);
}

void PortSmokeWorldTeleport(CStateManager& mgr) {
  static uint32_t sTargetWorld = 0;
  static bool sTargetResolved = false;
  if (!sTargetResolved) {
    const char* value = std::getenv("MP_SMOKE_WORLD");
    if (value == nullptr) {
      sTargetResolved = true;
      return;
    }
    if (std::strcmp(value, "auto") == 0) {
      if (gpGameState == nullptr || gpMemoryCard == nullptr) return;
      const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
      const uint32_t current = gpGameState->CurrentWorldAssetId();
      for (int i = 0; i < worlds.size(); ++i) {
        if (worlds[i].first != current) {
          sTargetWorld = static_cast< uint32_t >(worlds[i].first);
          break;
        }
      }
      if (sTargetWorld == 0) return;
    } else {
      sTargetWorld = static_cast< uint32_t >(std::strtoul(value, nullptr, 16));
    }
    sTargetResolved = true;
  }
  if (sTargetWorld == 0) return;
  const uint32_t targetWorld = sTargetWorld;

  static CStateManager* sRequestMgr = nullptr;
  static CWorld* sRequestWorld = nullptr;
  static unsigned sWarmupTicks = 0;
  static bool sPassed = false;

  if (sRequestMgr == nullptr) {
    if (mgr.GetGameState() != CStateManager::kGS_Running ||
        !mgr.GetCameraManager()->IsInFPCamera() ||
        mgr.GetCameraManager()->IsInCinematicCamera()) {
      return;
    }
    if (++sWarmupTicks < 120) return;
    std::fprintf(stderr, "[world-smoke] requesting world %08X\n", targetWorld);
    PortDebug::RequestWorldTeleport(targetWorld, 0u);
    sRequestMgr = &mgr;
    sRequestWorld = mgr.World();
    return;
  }

  if (sPassed) return;
  static uint32_t sLastWorld = 0;
  if (gpGameState != nullptr && gpGameState->CurrentWorldAssetId() != sLastWorld) {
    sLastWorld = gpGameState->CurrentWorldAssetId();
    std::fprintf(stderr, "[world-smoke] current world %08X (mgr=%p world=%p loaded=%08X)\n",
                 sLastWorld, static_cast< void* >(&mgr), static_cast< void* >(mgr.World()),
                 mgr.World() != nullptr ? static_cast< uint32_t >(mgr.World()->IGetWorldAssetId())
                                        : 0u);
  }
  if (mgr.World() == sRequestWorld) return;
  if (gpGameState == nullptr || gpGameState->CurrentWorldAssetId() != targetWorld) return;
  if (mgr.GetGameState() != CStateManager::kGS_Running || mgr.World() == nullptr) return;
  if (mgr.World()->IGetWorldAssetId() != targetWorld) return;
  sPassed = true;
  std::fprintf(stderr, "[world-smoke] passed: world %08X area %d\n", targetWorld,
               mgr.World()->GetCurrentAreaId().Value());
}

// MP_SMOKE_STICK=1: hold the right stick and report the aim yaw change, to
// verify twin-stick aiming (run with MP_TWIN_STICK=1).
void PortSmokeStick(CStateManager& mgr) {
  static const bool enabled = std::getenv("MP_SMOKE_STICK") != nullptr;
  if (!enabled) return;
  static unsigned sTicks = 0;
  static float sStartYaw = 0.f;
  static bool sDone = false;
  if (sDone || mgr.GetGameState() != CStateManager::kGS_Running ||
      !mgr.GetCameraManager()->IsInFPCamera() || mgr.GetCameraManager()->IsInCinematicCamera()) {
    return;
  }
  ++sTicks;
  if (sTicks == 120) {
    sStartYaw = PortDebug::AimYaw();
  }
  if (sTicks >= 120 && sTicks < 240) {
    PADStatus status{};
    status.err = PAD_ERR_NONE;
    status.substickX = 127;
    PADSetVirtualStatus(0, &status);
    return;
  }
  PADClearVirtualStatus(0);
  sDone = true;
  std::fprintf(stderr, "[stick-smoke] passed: yaw %.4f -> %.4f\n", sStartYaw, PortDebug::AimYaw());
}

void PortSmokeVisor(CStateManager& mgr) {  static const bool enabled = std::getenv("MP_SMOKE_VISOR") != nullptr;
  if (!enabled) return;
  static unsigned sTicks = 0;
  static bool sRequested = false;
  static bool sPassed = false;
  if (sPassed) return;
  if (mgr.GetGameState() != CStateManager::kGS_Running ||
      !mgr.GetCameraManager()->IsInFPCamera() || mgr.GetCameraManager()->IsInCinematicCamera()) {
    return;
  }
  CPlayerState* ps = mgr.PlayerState();
  if (ps == nullptr) return;
  if (!sRequested) {
    if (++sTicks < 120) return;
    ps->SetPowerUp(CPlayerState::kIT_ThermalVisor, 1);
    ps->SetPickup(CPlayerState::kIT_ThermalVisor, 1);
    std::fputs("[visor-smoke] switching to the thermal visor\n", stderr);
    ps->StartTransitionToVisor(CPlayerState::kPV_Thermal);
    sRequested = true;
    return;
  }
  if (++sTicks > 900) {
    sPassed = true;
    std::fputs("[visor-smoke] passed: thermal visor stable\n", stderr);
  }
}

bool PortSmokeMouseEnabled() {
  static const bool enabled = std::getenv("MP_SMOKE_MOUSE") != nullptr;
  return enabled;
}

unsigned PortSmokeMouseButtons(unsigned realButtons) {
  if (!PortSmokeMouseEnabled()) return realButtons;
  if (sMouseComplete) return 0;
  if ((sMouseTicks >= 10 && sMouseTicks < 15) ||
      (sMouseTicks >= 25 && sMouseTicks < 115) ||
      (sMouseTicks >= 335 && sMouseTicks < 380)) return SDL_BUTTON_LMASK;
  if ((sMouseTicks >= 135 && sMouseTicks < 140) ||
      (sMouseTicks >= 155 && sMouseTicks < 160) ||
      (sMouseTicks >= 175 && sMouseTicks < 180)) return SDL_BUTTON_MMASK;
  if (sMouseTicks >= 210 && sMouseTicks < 270) return SDL_BUTTON_RMASK;
  return 0;
}

void PortSmokeMouseBeforeUpdate(CStateManager& mgr) {
  if (!PortSmokeMouseEnabled() || sMouseComplete) return;
  // Real scripted cameras can interrupt the sequence (e.g. frigate tutorials).
  // Do not spend the planned morph/unmorph button presses while input is disabled.
  if (mgr.GetCameraManager()->IsInCinematicCamera() ||
      mgr.GetPlayer()->GetCameraState() == CPlayer::kCS_Spawned ||
      mgr.GetPlayer()->GetDisableInput() || mgr.GetGameState() != CStateManager::kGS_Running) {
    PADClearVirtualStatus(0);
    PortDebug::SetMouseCaptured(false);
    return;
  }
  if (sMouseTicks == 0 && !mgr.GetPlayer()->MouseControlsAllowed(mgr)) return;
  ++sMouseTicks;
  PortDebug::SetMouseCaptured(true);
  float dx = 0.f, dy = 0.f;
  if (sMouseTicks == 1) {
    PortDebug::ResetMouseAim();
    sInitialDirection = mgr.GetCameraManager()->GetFirstPersonCamera()->GetGunFollowTransform().GetForward();
  } else if (sMouseTicks == 2) {
    dx = 30.f; dy = -100.f;
  } else if (sMouseTicks == 3) {
    dx = -60.f; dy = 200.f;
  } else if (sMouseTicks == 4) {
    dy = -2000.f;
  } else if (sMouseTicks == 5) {
    dy = 4000.f;
  } else if (sMouseTicks == 7) {
    PortDebug::SynchronizeMouseAim(sInitialDirection.GetX(), sInitialDirection.GetY(), sInitialDirection.GetZ());
  }
  if (sMouseTicks == 80) PortDebug::SetFrameLimitEnabled(false);
  if (sMouseTicks == 200) PortDebug::SetFrameLimitEnabled(true);
  // Exercise moving aim while uncapped, then deliberately move the mouse while locked.
  if (sMouseTicks >= 85 && sMouseTicks < 100) { dx = 20.f; dy = -3.f; }
  if (sMouseTicks >= 100 && sMouseTicks < 115) { dx = -20.f; dy = 3.f; }
  if (sMouseTicks >= 220 && sMouseTicks < 260) { dx = 60.f; dy = 30.f; }
  if (sMouseTicks == 295) dy = -40.f;
  if (sMouseTicks >= 430 && sMouseTicks < 495) { dx = 100.f; dy = -100.f; }
  if (sMouseTicks == 365 || sMouseTicks == 370) PortDebug::Toggle();
  PADStatus jump{};
  jump.err = PAD_ERR_NONE;
  if (sMouseTicks >= 300 && sMouseTicks < 310) jump.button = PAD_BUTTON_B;
  if ((sMouseTicks >= 420 && sMouseTicks < 425) ||
      (sMouseTicks >= 500 && sMouseTicks < 505)) jump.button = PAD_BUTTON_X;
  if ((sMouseTicks >= 20 && sMouseTicks < 25) ||
      (sMouseTicks >= 390 && sMouseTicks < 395)) jump.stickX = 80;
  if ((sMouseTicks >= 25 && sMouseTicks < 30) ||
      (sMouseTicks >= 395 && sMouseTicks < 400)) jump.stickX = -80;
  PADSetVirtualStatus(0, &jump);
  PortDebug::AddMouseDelta(dx, dy);
  PortDebug::BeginFrameMouse();
}

void PortSmokeMouseAfterUpdate(CStateManager& mgr) {
  if (!PortSmokeMouseEnabled() || sMouseComplete || sMouseTicks == 0) return;
  const CPlayer& player = *mgr.GetPlayer();
  sPowerProjectileSeen |= mgr.GetWeaponIdCount(player.GetUniqueId(), kWT_Power) > 0;
  sMissileProjectileSeen |= mgr.GetWeaponIdCount(player.GetUniqueId(), kWT_Missile) > 0;
  if (player.GetMorphballTransitionState() == CPlayer::kMS_Morphed) sMouseMorphed = true;
  if (!player.MouseControlsAllowed(mgr)) {
    MouseCheck(!PortDebug::AimInitialized(), "inactive camera kept mouse ownership");
    MouseCheck(sMouseTicks < 620, "did not return from morph ball");
    sWasLocked = false;
    return;
  }
  if (sMouseMorphed) sMouseResumed = true;
  const float sidewaysSpeed = CVector3f::Dot(player.GetVelocityWR(), player.GetTransform().GetColumn(kDX));
  if (player.GetOrbitState() == CPlayer::kOS_NoOrbit && std::fabs(sidewaysSpeed) > 0.03f) {
    if (sMouseTicks >= 21 && sMouseTicks <= 31) sStrafedBeforeUI = true;
    if (sMouseTicks >= 391 && sMouseTicks <= 401) sStrafedAfterUI = true;
  }
  MouseCheck(!PortDebug::MouseCrosshair() || player.IsCrosshairsOpen(), "mouse-aim crosshair was not requested");
  const CVector3f forward = mgr.GetCameraManager()->GetFirstPersonCamera()->GetGunFollowTransform().GetForward();
  const float yaw = PortDebug::AimYaw(), pitch = PortDebug::AimPitch();
  const CVector3f aim(-std::sin(yaw) * std::cos(pitch), std::cos(yaw) * std::cos(pitch), std::sin(pitch));
  MouseCheck(CVector3f::Dot(forward, aim) > 0.9999f, "camera did not reach mouse/locked aim in this tick");
  MouseCheck(std::fabs(pitch) <= PortMouse::kMaxPitch + 0.0001f, "pitch limit");
  const bool locked = !player.MouseLookIsFree(mgr);
  if (locked) {
    ++sMouseLocks;
    sLastLockedDirection = forward;
  } else if (sWasLocked) {
    MouseCheck(CVector3f::Dot(forward, sLastLockedDirection) > 0.9999f, "lock release snapped to stale aim");
  }
  sWasLocked = locked;
  if (sMouseTicks >= 300 && sMouseTicks < 330 && player.GetPlayerMovementState() != NPlayer::kMS_OnGround)
    sMouseJumped = true;
  if (sMouseTicks >= 620) {
    MouseCheck(sMouseShots > 0 && sMouseChargedShots > 0 && sMouseMissiles > 0, "fire/charge/missile input was not delivered");
    MouseCheck(sPowerProjectileSeen && sMissileProjectileSeen, "weapon inputs did not create live projectiles");
    MouseCheck(sMouseLocks > 0 && sMouseJumped && sGunViewChecks > 50, "lock/jump/viewmodel coverage incomplete");
    MouseCheck(sMouseMorphed && sMouseResumed, "morph-ball handoff coverage incomplete");
    MouseCheck(sStrafedBeforeUI && sStrafedAfterUI, "free strafe failed before/after F1");
    PADClearVirtualStatus(0);
    sMouseComplete = true;
    std::fprintf(stderr, "[mouse-smoke] passed: shots=%u charged=%u missiles=%u lockedTicks=%u gunViews=%u jump=1 morph=1 strafe-before/after-F1=1\n",
                 sMouseShots, sMouseChargedShots, sMouseMissiles, sMouseLocks, sGunViewChecks);
  }
}

void PortSmokeMouseShot(bool charged, bool secondary) {
  if (!PortSmokeMouseEnabled() || sMouseComplete) return;
  MouseCheck(sMouseTicks < 365 || sMouseTicks > 385, "UI/capture transition fired a held charge");
  if (secondary) ++sMouseMissiles;
  else if (charged) ++sMouseChargedShots;
  else ++sMouseShots;
}

void PortSmokeMouseGunView(const CStateManager& mgr, const CPlayerGun& gun, const CTransform4f& worldView) {
  if (!PortSmokeMouseEnabled() || !mgr.GetPlayer()->MouseControlsAllowed(mgr) ||
      mgr.GetPlayer()->GetGunHolsterState() != CPlayer::kGH_Drawn) return;
  const CVector3f gunForward = gun.GetTransform().GetForward();
  MouseCheck(CVector3f::Dot(CGraphics::GetViewMatrix().GetForward(), gunForward) > 0.9999f,
             "held cannon and its render camera use different orientations");
  if (mgr.GetPlayer()->MouseLookIsFree(mgr))
    MouseCheck(CVector3f::Dot(worldView.GetForward(), gunForward) > 0.9999f, "visible aim lags simulation aim");
  ++sGunViewChecks;
}

bool PortSmokeFrame(unsigned frame) {
  if (PortSmokeMouseEnabled() && frame == 1) PortDebug::SetMouseAim(true);
  static const unsigned limit = [] {
    const char* value = std::getenv("MP_SMOKE_FRAMES");
    return value != nullptr ? static_cast<unsigned>(std::strtoul(value, nullptr, 10)) : 0;
  }();
  static const char* resizeSpec = std::getenv("MP_SMOKE_RESIZE");
  if (resizeSpec != nullptr && frame == 300) {
    int rw = 0;
    int rh = 0;
    if (std::sscanf(resizeSpec, "%dx%d", &rw, &rh) == 2 && rw > 0 && rh > 0) {
      int count = 0;
      SDL_Window** windows = SDL_GetWindows(&count);
      if (windows != nullptr && count > 0) {
        SDL_SetWindowSize(windows[0], rw, rh);
        std::fprintf(stderr, "[smoke] resized window to %dx%d\n", rw, rh);
      }
      SDL_free(windows);
    }
  }
  static const char* shotList = std::getenv("MP_SMOKE_SHOT");
  if (shotList != nullptr) {
    for (const char* p = shotList; *p != '\0';) {
      char* end = nullptr;
      const unsigned shotFrame = static_cast<unsigned>(std::strtoul(p, &end, 10));
      if (end == p) {
        break;
      }
      if (frame == shotFrame) {
        aurora::request_screenshot();
        std::fprintf(stderr, "[smoke] screenshot requested at frame %u\n", frame);
      }
      p = *end == ',' ? end + 1 : end;
    }
  }
  // MP_SMOKE_FRONTEND=<frame>: tap Start every 300 frames from that frame so
  // the front-end screens (and their button prompts) are reached without a
  // player. Start alone leaves dialogs such as the save check on screen.
  static const unsigned frontEndFrame = [] {
    const char* value = std::getenv("MP_SMOKE_FRONTEND");
    return value != nullptr ? static_cast<unsigned>(std::strtoul(value, nullptr, 10)) : 0;
  }();
  if (frontEndFrame != 0 && frame >= frontEndFrame) {
    PADStatus status{};
    status.err = PAD_ERR_NONE;
    if ((frame - frontEndFrame) % 300 < 8) {
      status.button = PAD_BUTTON_START;
    }
    PADSetVirtualStatus(0, &status);
  }
  // MP_SMOKE_BIND_A=<scancode>: rebind the A action once, so the prompt's
  // binding-aware icon can be checked without going through the Controls tab.
  static const bool bindApplied = [] {
    const char* value = std::getenv("MP_SMOKE_BIND_A");
    if (value == nullptr || value[0] == '\0') {
      return false;
    }
    PADKeyButtonBinding binding{};
    binding.scancode = static_cast< s32 >(std::strtol(value, nullptr, 10));
    binding.padButton = PAD_BUTTON_A;
    const bool ok = PADSetKeyButtonBinding(PAD_CHAN0, binding) != FALSE;
    std::fprintf(stderr, "[smoke] rebind A to scancode %d: %s\n", binding.scancode, ok ? "ok" : "failed");
    return ok;
  }();
  (void)bindApplied;
  if (limit == 0) return false;
  static SDL_Window* window = nullptr;
  if (window == nullptr) {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    if (windows != nullptr && count > 0) window = windows[0];
    SDL_free(windows);
  }
  if (std::getenv("MP_SMOKE_LIFECYCLE") != nullptr && limit >= 240) {
    if (frame == limit / 4) {
      if (window != nullptr) SDL_HideWindow(window);
      PortDebug::SetAiAudioEnabled(false);
      PortDebug::SetMusyxAudioEnabled(false);
      std::fputs("[smoke] hide and mute\n", stderr);
    } else if (frame == limit / 4 + 30) {
      if (window != nullptr) SDL_ShowWindow(window);
      PortDebug::SetAiAudioEnabled(true);
      PortDebug::SetMusyxAudioEnabled(true);
      PortDebug::SetFrameLimitEnabled(false);
      std::fputs("[smoke] restore, unmute and uncap\n", stderr);
    } else if (frame == limit / 2) {
      PortDebug::SetFrameLimitEnabled(true);
      PortDebug::RequestReset();
      PortDebug::ResetMouseAim();
      std::fputs("[smoke] reset to menu\n", stderr);
    }
  }
  if (frame < limit || (PortSmokeMouseEnabled() && !sMouseComplete) ||
      (AreaReloadEnabled() && sAreaReloads < 3)) return false;
  std::fputs("[smoke] clean exit requested\n", stderr);
  return true;
}

// MP_SMOKE_WALK=<ticks>: hold the stick full forward and report the ground
// speed, so a tick-rate change can be checked to stay real-time.
void PortSmokeWalk(CStateManager& mgr) {
  static const unsigned walkTicks = [] {
    const char* value = std::getenv("MP_SMOKE_WALK");
    return value != nullptr ? static_cast< unsigned >(std::strtoul(value, nullptr, 10)) : 0u;
  }();
  if (walkTicks == 0) return;
  static unsigned sTicks = 0;
  static bool sStarted = false;
  static CVector3f sStart;
  static bool sDone = false;
  static float sMaxSpeed = 0.f;
  if (sDone || mgr.GetGameState() != CStateManager::kGS_Running ||
      !mgr.GetCameraManager()->IsInFPCamera() || mgr.GetCameraManager()->IsInCinematicCamera()) {
    return;
  }
  const CPlayer* player = mgr.GetPlayer();
  if (player == nullptr) return;
  if (!sStarted) {
    sStarted = true;
    sStart = player->GetTranslation();
    std::fprintf(stderr, "[walk-smoke] begin\n");
  }
  {
    const CVector3f vel = player->GetVelocityWR();
    const CVector3f flat(vel.GetX(), vel.GetY(), 0.f);
    sMaxSpeed = rstl::max_val(sMaxSpeed, flat.Magnitude());
  }
  PADStatus status{};
  status.err = PAD_ERR_NONE;
  status.stickY = 127;
  PADSetVirtualStatus(0, &status);
  if (++sTicks < walkTicks) return;
  PADClearVirtualStatus(0);
  sDone = true;
  const float dist = (player->GetTranslation() - sStart).Magnitude();
  const double seconds = static_cast< double >(sTicks) * PortDebug::TickPeriod();
  std::fprintf(stderr,
               "[walk-smoke] passed: ticks=%u seconds=%.3f dist=%.3f maxFlatSpeed=%.4f speed=%.4f/s\n",
               sTicks, seconds, dist, sMaxSpeed, seconds > 0.0 ? dist / seconds : 0.0);
}
