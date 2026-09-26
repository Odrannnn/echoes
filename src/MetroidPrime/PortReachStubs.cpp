/**
 * Port REACHABILITY STUBS - GENERATED, DIAGNOSTIC ONLY, do not hand-edit.
 *
 *   generator: tools/gen_link_stubs.py --reachable
 *   input:     docs/research/boot_path_reachable.tsv  (from tools/link_reach.py)
 *   built by:  -DMP_BOOT_STUBS=ON, which NOTHING but tools/boot_probe.sh passes
 *
 * **THIS IS NOT PART OF THE PORT.** Every symbol here is referenced by an object the boot path
 * *does* reach, so these definitions are lies: the program will run, and then behave wrongly.
 * `link_check.sh` never sees this file, and the real link still fails on all
 * 332 of them.
 *
 * **What it is for.** `link_reach.py` can say which symbols are reachable. It cannot say which
 * one the game asks for *first*, or in what order - and the order is what tells a lane what to
 * write next. So each stub logs its own name on entry and returns. One run prints the sequence
 * the boot path actually demands:
 *
 *   [0007] _ZN13CGameAllocator10InitializeER10COsContext
 *   [0008] _ZN9CGameStateC1ER11CInputStreami
 *
 * A non-void function stubbed as `void()` returns whatever was in the return register, so the
 * port may fault later than the first stub. That is expected; the log up to the fault is still
 * ordered evidence, and the fault is itself the next thing to find.
 *
 * Breakdown: 229 game method, 71 REL loader, 31 unmangled fn_/lbl_, 1 vtable/typeinfo.
 */

#include <cstdio>
#include <cstdlib>

namespace {

unsigned g_stubSeq = 0;

} // namespace

// One definition for all of them: the name is passed, not encoded in a symbol, so this stays
// readable and the cost is one PLT call per stub rather than 331 near-identical bodies.
extern "C" void mpReachStub(const char* mangled, const char* demangled) {
  std::fprintf(stderr, "[reach-stub %04u] %s   (%s)\n", ++g_stubSeq, mangled, demangled);
  std::fflush(stderr);
}

// AllocateRenderer(IObjectStore&, COsContext&, CMemorySys&, IFactory&)
extern "C" void reachstub_0() asm("_Z16AllocateRendererR12IObjectStoreR10COsContextR10CMemorySysR8IFactory");
extern "C" void reachstub_0() { mpReachStub("_Z16AllocateRendererR12IObjectStoreR10COsContextR10CMemorySysR8IFactory", "AllocateRenderer(IObjectStore&, COsContext&, CMemorySys&, IFactory&)"); }

// CARAMManager::Alloc(unsigned int, void*)
extern "C" void reachstub_1() asm("_ZN12CARAMManager5AllocEjPv");
extern "C" void reachstub_1() { mpReachStub("_ZN12CARAMManager5AllocEjPv", "CARAMManager::Alloc(unsigned int, void*)"); }

// CARAMManager::CancelDMA(unsigned int)
extern "C" void reachstub_2() asm("_ZN12CARAMManager9CancelDMAEj");
extern "C" void reachstub_2() { mpReachStub("_ZN12CARAMManager9CancelDMAEj", "CARAMManager::CancelDMA(unsigned int)"); }

// CARAMManager::DMAToARAM(void*, void*, unsigned int, CARAMManager::EDMAPriority)
extern "C" void reachstub_3() asm("_ZN12CARAMManager9DMAToARAMEPvS0_jNS_12EDMAPriorityE");
extern "C" void reachstub_3() { mpReachStub("_ZN12CARAMManager9DMAToARAMEPvS0_jNS_12EDMAPriorityE", "CARAMManager::DMAToARAM(void*, void*, unsigned int, CARAMManager::EDMAPriority)"); }

// CARAMManager::DMAToMRAM(void*, void*, unsigned int, CARAMManager::EDMAPriority)
extern "C" void reachstub_4() asm("_ZN12CARAMManager9DMAToMRAMEPvS0_jNS_12EDMAPriorityE");
extern "C" void reachstub_4() { mpReachStub("_ZN12CARAMManager9DMAToMRAMEPvS0_jNS_12EDMAPriorityE", "CARAMManager::DMAToMRAM(void*, void*, unsigned int, CARAMManager::EDMAPriority)"); }

// CARAMManager::Free(void const*, void*)
extern "C" void reachstub_5() asm("_ZN12CARAMManager4FreeEPKvPv");
extern "C" void reachstub_5() { mpReachStub("_ZN12CARAMManager4FreeEPKvPv", "CARAMManager::Free(void const*, void*)"); }

// CARAMManager::GetInvalidAlloc()
extern "C" void reachstub_6() asm("_ZN12CARAMManager15GetInvalidAllocEv");
extern "C" void reachstub_6() { mpReachStub("_ZN12CARAMManager15GetInvalidAllocEv", "CARAMManager::GetInvalidAlloc()"); }

// CARAMManager::IsAllocValid(void const*)
extern "C" void reachstub_7() asm("_ZN12CARAMManager12IsAllocValidEPKv");
extern "C" void reachstub_7() { mpReachStub("_ZN12CARAMManager12IsAllocValidEPKv", "CARAMManager::IsAllocValid(void const*)"); }

// CARAMManager::IsDMACompleted(unsigned int)
extern "C" void reachstub_8() asm("_ZN12CARAMManager14IsDMACompletedEj");
extern "C" void reachstub_8() { mpReachStub("_ZN12CARAMManager14IsDMACompletedEj", "CARAMManager::IsDMACompleted(unsigned int)"); }

// CARAMManager::WaitForDMACompletion(unsigned int)
extern "C" void reachstub_9() asm("_ZN12CARAMManager20WaitForDMACompletionEj");
extern "C" void reachstub_9() { mpReachStub("_ZN12CARAMManager20WaitForDMACompletionEj", "CARAMManager::WaitForDMACompletion(unsigned int)"); }

// CActor* TCastToPtr<CActor>(CEntity*)
extern "C" void reachstub_10() asm("_Z10TCastToPtrI6CActorEPT_P7CEntity");
extern "C" void reachstub_10() { mpReachStub("_Z10TCastToPtrI6CActorEPT_P7CEntity", "CActor* TCastToPtr<CActor>(CEntity*)"); }

// CActor::AddToRenderer(CFrustumPlanes const&, CStateManager const&) const
extern "C" void reachstub_11() asm("_ZNK6CActor13AddToRendererERK14CFrustumPlanesRK13CStateManager");
extern "C" void reachstub_11() { mpReachStub("_ZNK6CActor13AddToRendererERK14CFrustumPlanesRK13CStateManager", "CActor::AddToRenderer(CFrustumPlanes const&, CStateManager const&) const"); }

// CActor::SetDirtyFlags()
extern "C" void reachstub_12() asm("_ZN6CActor13SetDirtyFlagsEv");
extern "C" void reachstub_12() { mpReachStub("_ZN6CActor13SetDirtyFlagsEv", "CActor::SetDirtyFlags()"); }

// CActor::SetTransformAlt(CTransform4f const&)
extern "C" void reachstub_13() asm("_ZN6CActor15SetTransformAltERK12CTransform4f");
extern "C" void reachstub_13() { mpReachStub("_ZN6CActor15SetTransformAltERK12CTransform4f", "CActor::SetTransformAlt(CTransform4f const&)"); }

// CActor::Think(float, CStateManager&)
extern "C" void reachstub_14() asm("_ZN6CActor5ThinkEfR13CStateManager");
extern "C" void reachstub_14() { mpReachStub("_ZN6CActor5ThinkEfR13CStateManager", "CActor::Think(float, CStateManager&)"); }

// CActor::UpdateSfxEmitters()
extern "C" void reachstub_15() asm("_ZN6CActor17UpdateSfxEmittersEv");
extern "C" void reachstub_15() { mpReachStub("_ZN6CActor17UpdateSfxEmittersEv", "CActor::UpdateSfxEmitters()"); }

// CActorLights::BuildAreaLightList(CStateManager const&, CGameArea const&, CAABox const&)
extern "C" void reachstub_16() asm("_ZN12CActorLights18BuildAreaLightListERK13CStateManagerRK9CGameAreaRK6CAABox");
extern "C" void reachstub_16() { mpReachStub("_ZN12CActorLights18BuildAreaLightListERK13CStateManagerRK9CGameAreaRK6CAABox", "CActorLights::BuildAreaLightList(CStateManager const&, CGameArea const&, CAABox const&)"); }

// CActorLights::BuildDynamicLightList(CStateManager const&, CAABox const&)
extern "C" void reachstub_17() asm("_ZN12CActorLights21BuildDynamicLightListERK13CStateManagerRK6CAABox");
extern "C" void reachstub_17() { mpReachStub("_ZN12CActorLights21BuildDynamicLightListERK13CStateManagerRK6CAABox", "CActorLights::BuildDynamicLightList(CStateManager const&, CAABox const&)"); }

// CActorLights::CActorLights(unsigned int, CVector3f, int, int, float, bool, bool, bool, bool)
extern "C" void reachstub_18() asm("_ZN12CActorLightsC1Ej9CVector3fiifbbbb");
extern "C" void reachstub_18() { mpReachStub("_ZN12CActorLightsC1Ej9CVector3fiifbbbb", "CActorLights::CActorLights(unsigned int, CVector3f, int, int, float, bool, bool, bool, bool)"); }

// CActorLights::~CActorLights()
extern "C" void reachstub_19() asm("_ZN12CActorLightsD1Ev");
extern "C" void reachstub_19() { mpReachStub("_ZN12CActorLightsD1Ev", "CActorLights::~CActorLights()"); }

// CActorModelParticles::Render(CStateManager const&, CActor const&) const
extern "C" void reachstub_20() asm("_ZNK20CActorModelParticles6RenderERK13CStateManagerRK6CActor");
extern "C" void reachstub_20() { mpReachStub("_ZNK20CActorModelParticles6RenderERK13CStateManagerRK6CActor", "CActorModelParticles::Render(CStateManager const&, CActor const&) const"); }

// CActorModelParticles::SetupHook(TUniqueId)
extern "C" void reachstub_21() asm("_ZN20CActorModelParticles9SetupHookE9TUniqueId");
extern "C" void reachstub_21() { mpReachStub("_ZN20CActorModelParticles9SetupHookE9TUniqueId", "CActorModelParticles::SetupHook(TUniqueId)"); }

// CActorParameters::CActorParameters()
extern "C" void reachstub_22() asm("_ZN16CActorParametersC1Ev");
extern "C" void reachstub_22() { mpReachStub("_ZN16CActorParametersC1Ev", "CActorParameters::CActorParameters()"); }

// CAnimData::GetAnimTimeRemaining(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const
extern "C" void reachstub_23() asm("_ZNK9CAnimData20GetAnimTimeRemainingERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_23() { mpReachStub("_ZNK9CAnimData20GetAnimTimeRemainingERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CAnimData::GetAnimTimeRemaining(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const"); }

// CAnimData::GetAverageVelocity(int) const
extern "C" void reachstub_24() asm("_ZNK9CAnimData18GetAverageVelocityEi");
extern "C" void reachstub_24() { mpReachStub("_ZNK9CAnimData18GetAverageVelocityEi", "CAnimData::GetAverageVelocity(int) const"); }

// CAnimData::InitializeEffects(CStateManager&, TAreaId, CVector3f const&)
extern "C" void reachstub_25() asm("_ZN9CAnimData17InitializeEffectsER13CStateManager7TAreaIdRK9CVector3f");
extern "C" void reachstub_25() { mpReachStub("_ZN9CAnimData17InitializeEffectsER13CStateManager7TAreaIdRK9CVector3f", "CAnimData::InitializeEffects(CStateManager&, TAreaId, CVector3f const&)"); }

// CAnimData::IsAnimTimeRemaining(float, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const
extern "C" void reachstub_26() asm("_ZNK9CAnimData19IsAnimTimeRemainingEfRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_26() { mpReachStub("_ZNK9CAnimData19IsAnimTimeRemainingEfRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CAnimData::IsAnimTimeRemaining(float, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const"); }

// CAnimData::PreRender()
extern "C" void reachstub_27() asm("_ZN9CAnimData9PreRenderEv");
extern "C" void reachstub_27() { mpReachStub("_ZN9CAnimData9PreRenderEv", "CAnimData::PreRender()"); }

// CAudioStateWin::CAudioStateWin()
extern "C" void reachstub_28() asm("_ZN14CAudioStateWinC1Ev");
extern "C" void reachstub_28() { mpReachStub("_ZN14CAudioStateWinC1Ev", "CAudioStateWin::CAudioStateWin()"); }

// CBasics::Stringize(char const*, ...)
extern "C" void reachstub_29() asm("_ZN7CBasics9StringizeEPKcz");
extern "C" void reachstub_29() { mpReachStub("_ZN7CBasics9StringizeEPKcz", "CBasics::Stringize(char const*, ...)"); }

// CCallStack::CCallStack(unsigned int, char const*, char const*)
extern "C" void reachstub_30() asm("_ZN10CCallStackC1EjPKcS1_");
extern "C" void reachstub_30() { mpReachStub("_ZN10CCallStackC1EjPKcS1_", "CCallStack::CCallStack(unsigned int, char const*, char const*)"); }

// CCallStack::GetFileAndLineText() const
extern "C" void reachstub_31() asm("_ZNK10CCallStack18GetFileAndLineTextEv");
extern "C" void reachstub_31() { mpReachStub("_ZNK10CCallStack18GetFileAndLineTextEv", "CCallStack::GetFileAndLineText() const"); }

// CCallStack::GetTypeText() const
extern "C" void reachstub_32() asm("_ZNK10CCallStack11GetTypeTextEv");
extern "C" void reachstub_32() { mpReachStub("_ZNK10CCallStack11GetTypeTextEv", "CCallStack::GetTypeText() const"); }

// CCameraManager::CastGameCameratoFirstPersonCamera(CGameCamera const*)
extern "C" void reachstub_33() asm("_ZN14CCameraManager33CastGameCameratoFirstPersonCameraEPK11CGameCamera");
extern "C" void reachstub_33() { mpReachStub("_ZN14CCameraManager33CastGameCameratoFirstPersonCameraEPK11CGameCamera", "CCameraManager::CastGameCameratoFirstPersonCamera(CGameCamera const*)"); }

// CCameraManager::GetCurrentCamera(CStateManager const&, int) const
extern "C" void reachstub_34() asm("_ZNK14CCameraManager16GetCurrentCameraERK13CStateManageri");
extern "C" void reachstub_34() { mpReachStub("_ZNK14CCameraManager16GetCurrentCameraERK13CStateManageri", "CCameraManager::GetCurrentCamera(CStateManager const&, int) const"); }

// CCameraManager::IsInCinematicCamera() const
extern "C" void reachstub_35() asm("_ZNK14CCameraManager19IsInCinematicCameraEv");
extern "C" void reachstub_35() { mpReachStub("_ZNK14CCameraManager19IsInCinematicCameraEv", "CCameraManager::IsInCinematicCamera() const"); }

// CConsoleOutputWindow::CConsoleOutputWindow(int, float, float)
extern "C" void reachstub_36() asm("_ZN20CConsoleOutputWindowC1Eiff");
extern "C" void reachstub_36() { mpReachStub("_ZN20CConsoleOutputWindowC1Eiff", "CConsoleOutputWindow::CConsoleOutputWindow(int, float, float)"); }

// CDamageVulnerability::NormalVulnerabilty()
extern "C" void reachstub_37() asm("_ZN20CDamageVulnerability18NormalVulnerabiltyEv");
extern "C" void reachstub_37() { mpReachStub("_ZN20CDamageVulnerability18NormalVulnerabiltyEv", "CDamageVulnerability::NormalVulnerabilty()"); }

// CEntityInfo::~CEntityInfo()
extern "C" void reachstub_38() asm("_ZN11CEntityInfoD1Ev");
extern "C" void reachstub_38() { mpReachStub("_ZN11CEntityInfoD1Ev", "CEntityInfo::~CEntityInfo()"); }

// CEnvFxManager::Initialize()
extern "C" void reachstub_39() asm("_ZN13CEnvFxManager10InitializeEv");
extern "C" void reachstub_39() { mpReachStub("_ZN13CEnvFxManager10InitializeEv", "CEnvFxManager::Initialize()"); }

// CErrorOutputWindow::CErrorOutputWindow(bool)
extern "C" void reachstub_40() asm("_ZN18CErrorOutputWindowC1Eb");
extern "C" void reachstub_40() { mpReachStub("_ZN18CErrorOutputWindowC1Eb", "CErrorOutputWindow::CErrorOutputWindow(bool)"); }

// CFrustumPlanes::CFrustumPlanes()
extern "C" void reachstub_41() asm("_ZN14CFrustumPlanesC1Ev");
extern "C" void reachstub_41() { mpReachStub("_ZN14CFrustumPlanesC1Ev", "CFrustumPlanes::CFrustumPlanes()"); }

// CGameArchitectureSupport::UnloadAudio()
extern "C" void reachstub_42() asm("_ZN24CGameArchitectureSupport11UnloadAudioEv");
extern "C" void reachstub_42() { mpReachStub("_ZN24CGameArchitectureSupport11UnloadAudioEv", "CGameArchitectureSupport::UnloadAudio()"); }

// CGameArea::fn_80057550() const
extern "C" void reachstub_43() asm("_ZNK9CGameArea11fn_80057550Ev");
extern "C" void reachstub_43() { mpReachStub("_ZNK9CGameArea11fn_80057550Ev", "CGameArea::fn_80057550() const"); }

// CGameArea::fn_800575BC(CStateManager&)
extern "C" void reachstub_44() asm("_ZN9CGameArea11fn_800575BCER13CStateManager");
extern "C" void reachstub_44() { mpReachStub("_ZN9CGameArea11fn_800575BCER13CStateManager", "CGameArea::fn_800575BC(CStateManager&)"); }

// CGameCollision::RayStaticIntersection(CStateManager const&, CVector3f const&, CVector3f const&, float, CMaterialFilter const&)
extern "C" void reachstub_45() asm("_ZN14CGameCollision21RayStaticIntersectionERK13CStateManagerRK9CVector3fS5_fRK15CMaterialFilter");
extern "C" void reachstub_45() { mpReachStub("_ZN14CGameCollision21RayStaticIntersectionERK13CStateManagerRK9CVector3fS5_fRK15CMaterialFilter", "CGameCollision::RayStaticIntersection(CStateManager const&, CVector3f const&, CVector3f const&, float, CMaterialFilter const&)"); }

// CGameCollision::RayWorldIntersection(CStateManager const&, TUniqueId&, CVector3f const&, CVector3f const&, float, CMaterialFilter const&, rstl::reserved_vector<TUniqueId, 1024> const&)
extern "C" void reachstub_46() asm("_ZN14CGameCollision20RayWorldIntersectionERK13CStateManagerR9TUniqueIdRK9CVector3fS7_fRK15CMaterialFilterRKN4rstl15reserved_vectorIS3_Li1024EEE");
extern "C" void reachstub_46() { mpReachStub("_ZN14CGameCollision20RayWorldIntersectionERK13CStateManagerR9TUniqueIdRK9CVector3fS7_fRK15CMaterialFilterRKN4rstl15reserved_vectorIS3_Li1024EEE", "CGameCollision::RayWorldIntersection(CStateManager const&, TUniqueId&, CVector3f const&, CVector3f const&, float, CMaterialFilter const&, rstl::reserved_vector<TUniqueId, 1024> const&)"); }

// CGameOptions::~CGameOptions()
extern "C" void reachstub_47() asm("_ZN12CGameOptionsD1Ev");
extern "C" void reachstub_47() { mpReachStub("_ZN12CGameOptionsD1Ev", "CGameOptions::~CGameOptions()"); }

// CGameState::CGameState(CInputStream&, int)
extern "C" void reachstub_48() asm("_ZN10CGameStateC1ER12CInputStreami");
extern "C" void reachstub_48() { mpReachStub("_ZN10CGameStateC1ER12CInputStreami", "CGameState::CGameState(CInputStream&, int)"); }

// CGameState::GetGameMode()
extern "C" void reachstub_49() asm("_ZN10CGameState11GetGameModeEv");
extern "C" void reachstub_49() { mpReachStub("_ZN10CGameState11GetGameModeEv", "CGameState::GetGameMode()"); }

// CGameState::GetHardModeDamageMultiplier() const
extern "C" void reachstub_50() asm("_ZNK10CGameState27GetHardModeDamageMultiplierEv");
extern "C" void reachstub_50() { mpReachStub("_ZNK10CGameState27GetHardModeDamageMultiplierEv", "CGameState::GetHardModeDamageMultiplier() const"); }

// CGameState::GetHardModeEnabled() const
extern "C" void reachstub_51() asm("_ZNK10CGameState18GetHardModeEnabledEv");
extern "C" void reachstub_51() { mpReachStub("_ZNK10CGameState18GetHardModeEnabledEv", "CGameState::GetHardModeEnabled() const"); }

// CGameState::SetIsDarkWorld(bool)
extern "C" void reachstub_52() asm("_ZN10CGameState14SetIsDarkWorldEb");
extern "C" void reachstub_52() { mpReachStub("_ZN10CGameState14SetIsDarkWorldEb", "CGameState::SetIsDarkWorld(bool)"); }

// CGameState::SetUnk50(float)
extern "C" void reachstub_53() asm("_ZN10CGameState8SetUnk50Ef");
extern "C" void reachstub_53() { mpReachStub("_ZN10CGameState8SetUnk50Ef", "CGameState::SetUnk50(float)"); }

// CGraphics::SetModelMatrix(CTransform4f const&)
extern "C" void reachstub_54() asm("_ZN9CGraphics14SetModelMatrixERK12CTransform4f");
extern "C" void reachstub_54() { mpReachStub("_ZN9CGraphics14SetModelMatrixERK12CTransform4f", "CGraphics::SetModelMatrix(CTransform4f const&)"); }

// CGraphics::SetScreenPosition(int, int, int)
extern "C" void reachstub_55() asm("_ZN9CGraphics17SetScreenPositionEiii");
extern "C" void reachstub_55() { mpReachStub("_ZN9CGraphics17SetScreenPositionEiii", "CGraphics::SetScreenPosition(int, int, int)"); }

// CGraphics::SetUseVideoFilter(bool)
extern "C" void reachstub_56() asm("_ZN9CGraphics17SetUseVideoFilterEb");
extern "C" void reachstub_56() { mpReachStub("_ZN9CGraphics17SetUseVideoFilterEb", "CGraphics::SetUseVideoFilter(bool)"); }

// CGraphics::SetViewPointMatrix(CTransform4f const&)
extern "C" void reachstub_57() asm("_ZN9CGraphics18SetViewPointMatrixERK12CTransform4f");
extern "C" void reachstub_57() { mpReachStub("_ZN9CGraphics18SetViewPointMatrixERK12CTransform4f", "CGraphics::SetViewPointMatrix(CTransform4f const&)"); }

// CGrappleArm::fn_801C37BC(int)
extern "C" void reachstub_58() asm("_ZN11CGrappleArm11fn_801C37BCEi");
extern "C" void reachstub_58() { mpReachStub("_ZN11CGrappleArm11fn_801C37BCEi", "CGrappleArm::fn_801C37BC(int)"); }

// CGrappleArm::fn_801C3824(CStateManager&, float, bool)
extern "C" void reachstub_59() asm("_ZN11CGrappleArm11fn_801C3824ER13CStateManagerfb");
extern "C" void reachstub_59() { mpReachStub("_ZN11CGrappleArm11fn_801C3824ER13CStateManagerfb", "CGrappleArm::fn_801C3824(CStateManager&, float, bool)"); }

// CGunEffectUnk::fn_801DCFF0()
extern "C" void reachstub_60() asm("_ZN13CGunEffectUnk11fn_801DCFF0Ev");
extern "C" void reachstub_60() { mpReachStub("_ZN13CGunEffectUnk11fn_801DCFF0Ev", "CGunEffectUnk::fn_801DCFF0()"); }

// CGunEffectUnk::fn_801DD010()
extern "C" void reachstub_61() asm("_ZN13CGunEffectUnk11fn_801DD010Ev");
extern "C" void reachstub_61() { mpReachStub("_ZN13CGunEffectUnk11fn_801DD010Ev", "CGunEffectUnk::fn_801DD010()"); }

// CGunStateMachine::GetCurrentStateName() const
extern "C" void reachstub_62() asm("_ZNK16CGunStateMachine19GetCurrentStateNameEv");
extern "C" void reachstub_62() { mpReachStub("_ZNK16CGunStateMachine19GetCurrentStateNameEv", "CGunStateMachine::GetCurrentStateName() const"); }

// CGunStateMachine::SetState(CStateManager&, CPlayerGun*, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)
extern "C" void reachstub_63() asm("_ZN16CGunStateMachine8SetStateER13CStateManagerP10CPlayerGunRKN4rstl12basic_stringIcNS4_11char_traitsIcEENS4_17rmemory_allocatorEEE");
extern "C" void reachstub_63() { mpReachStub("_ZN16CGunStateMachine8SetStateER13CStateManagerP10CPlayerGunRKN4rstl12basic_stringIcNS4_11char_traitsIcEENS4_17rmemory_allocatorEEE", "CGunStateMachine::SetState(CStateManager&, CPlayerGun*, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)"); }

// CGunStateMachine::SetStateFuncs(SGunStateFunc const*, int)
extern "C" void reachstub_64() asm("_ZN16CGunStateMachine13SetStateFuncsEPK13SGunStateFunci");
extern "C" void reachstub_64() { mpReachStub("_ZN16CGunStateMachine13SetStateFuncsEPK13SGunStateFunci", "CGunStateMachine::SetStateFuncs(SGunStateFunc const*, int)"); }

// CGunStateMachine::SetStateMachine(CStateMachine const*)
extern "C" void reachstub_65() asm("_ZN16CGunStateMachine15SetStateMachineEPK13CStateMachine");
extern "C" void reachstub_65() { mpReachStub("_ZN16CGunStateMachine15SetStateMachineEPK13CStateMachine", "CGunStateMachine::SetStateMachine(CStateMachine const*)"); }

// CGunStateMachine::SetTriggerFuncs(SGunTriggerFunc const*, int)
extern "C" void reachstub_66() asm("_ZN16CGunStateMachine15SetTriggerFuncsEPK15SGunTriggerFunci");
extern "C" void reachstub_66() { mpReachStub("_ZN16CGunStateMachine15SetTriggerFuncsEPK15SGunTriggerFunci", "CGunStateMachine::SetTriggerFuncs(SGunTriggerFunc const*, int)"); }

// CGunWeapon::GetWeaponInfo() const
extern "C" void reachstub_67() asm("_ZNK10CGunWeapon13GetWeaponInfoEv");
extern "C" void reachstub_67() { mpReachStub("_ZNK10CGunWeapon13GetWeaponInfoEv", "CGunWeapon::GetWeaponInfo() const"); }

// CGunWeapon::IsChargeAnimOver() const
extern "C" void reachstub_68() asm("_ZNK10CGunWeapon16IsChargeAnimOverEv");
extern "C" void reachstub_68() { mpReachStub("_ZNK10CGunWeapon16IsChargeAnimOverEv", "CGunWeapon::IsChargeAnimOver() const"); }

// CGunWeapon::fn_801D8EC0()
extern "C" void reachstub_69() asm("_ZN10CGunWeapon11fn_801D8EC0Ev");
extern "C" void reachstub_69() { mpReachStub("_ZN10CGunWeapon11fn_801D8EC0Ev", "CGunWeapon::fn_801D8EC0()"); }

// CGunWeapon::fn_801D8F2C()
extern "C" void reachstub_70() asm("_ZN10CGunWeapon11fn_801D8F2CEv");
extern "C" void reachstub_70() { mpReachStub("_ZN10CGunWeapon11fn_801D8F2CEv", "CGunWeapon::fn_801D8F2C()"); }

// CGunWeapon::fn_801D8F64()
extern "C" void reachstub_71() asm("_ZN10CGunWeapon11fn_801D8F64Ev");
extern "C" void reachstub_71() { mpReachStub("_ZN10CGunWeapon11fn_801D8F64Ev", "CGunWeapon::fn_801D8F64()"); }

// CGunWeapon::fn_801DA364(CStateManager&, bool)
extern "C" void reachstub_72() asm("_ZN10CGunWeapon11fn_801DA364ER13CStateManagerb");
extern "C" void reachstub_72() { mpReachStub("_ZN10CGunWeapon11fn_801DA364ER13CStateManagerb", "CGunWeapon::fn_801DA364(CStateManager&, bool)"); }

// CIOWinManager::RemoveIOWin(rstl::rc_ptr<CIOWin> const&)
extern "C" void reachstub_73() asm("_ZN13CIOWinManager11RemoveIOWinERKN4rstl6rc_ptrI6CIOWinEE");
extern "C" void reachstub_73() { mpReachStub("_ZN13CIOWinManager11RemoveIOWinERKN4rstl6rc_ptrI6CIOWinEE", "CIOWinManager::RemoveIOWin(rstl::rc_ptr<CIOWin> const&)"); }

// CInGameTweakManager::GetIdentifierForMusicEvent(unsigned int, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)
extern "C" void reachstub_74() asm("_ZN19CInGameTweakManager26GetIdentifierForMusicEventEjRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_74() { mpReachStub("_ZN19CInGameTweakManager26GetIdentifierForMusicEventEjRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CInGameTweakManager::GetIdentifierForMusicEvent(unsigned int, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)"); }

// CInGameTweakManager::GetTweakValue(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const
extern "C" void reachstub_75() asm("_ZNK19CInGameTweakManager13GetTweakValueERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_75() { mpReachStub("_ZNK19CInGameTweakManager13GetTweakValueERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CInGameTweakManager::GetTweakValue(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const"); }

// CInGameTweakManager::HasTweakValue(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const
extern "C" void reachstub_76() asm("_ZNK19CInGameTweakManager13HasTweakValueERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_76() { mpReachStub("_ZNK19CInGameTweakManager13HasTweakValueERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CInGameTweakManager::HasTweakValue(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const"); }

// CInputGenerator::Update(float, CArchitectureQueue&)
extern "C" void reachstub_77() asm("_ZN15CInputGenerator6UpdateEfR18CArchitectureQueue");
extern "C" void reachstub_77() { mpReachStub("_ZN15CInputGenerator6UpdateEfR18CArchitectureQueue", "CInputGenerator::Update(float, CArchitectureQueue&)"); }

// CLightParameters::CLightParameters()
extern "C" void reachstub_78() asm("_ZN16CLightParametersC1Ev");
extern "C" void reachstub_78() { mpReachStub("_ZN16CLightParametersC1Ev", "CLightParameters::CLightParameters()"); }

// CLightParameters::MakeActorLights() const
extern "C" void reachstub_79() asm("_ZNK16CLightParameters15MakeActorLightsEv");
extern "C" void reachstub_79() { mpReachStub("_ZNK16CLightParameters15MakeActorLightsEv", "CLightParameters::MakeActorLights() const"); }

// CMain::ResetGameState()
extern "C" void reachstub_80() asm("_ZN5CMain14ResetGameStateEv");
extern "C" void reachstub_80() { mpReachStub("_ZN5CMain14ResetGameStateEv", "CMain::ResetGameState()"); }

// CModelData::AdvanceAnimation(float, CStateManager&, TAreaId, bool)
extern "C" void reachstub_81() asm("_ZN10CModelData16AdvanceAnimationEfR13CStateManager7TAreaIdb");
extern "C" void reachstub_81() { mpReachStub("_ZN10CModelData16AdvanceAnimationEfR13CStateManager7TAreaIdb", "CModelData::AdvanceAnimation(float, CStateManager&, TAreaId, bool)"); }

// CModelData::AdvanceParticles(CTransform4f const&, float, CStateManager&)
extern "C" void reachstub_82() asm("_ZN10CModelData16AdvanceParticlesERK12CTransform4ffR13CStateManager");
extern "C" void reachstub_82() { mpReachStub("_ZN10CModelData16AdvanceParticlesERK12CTransform4ffR13CStateManager", "CModelData::AdvanceParticles(CTransform4f const&, float, CStateManager&)"); }

// CModelData::GetBounds(CTransform4f const&) const
extern "C" void reachstub_83() asm("_ZNK10CModelData9GetBoundsERK12CTransform4f");
extern "C" void reachstub_83() { mpReachStub("_ZNK10CModelData9GetBoundsERK12CTransform4f", "CModelData::GetBounds(CTransform4f const&) const"); }

// CModelData::GetLocatorTransform(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const
extern "C" void reachstub_84() asm("_ZNK10CModelData19GetLocatorTransformERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_84() { mpReachStub("_ZNK10CModelData19GetLocatorTransformERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CModelData::GetLocatorTransform(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const"); }

// CModelData::GetRenderingModel(CStateManager const&)
extern "C" void reachstub_85() asm("_ZN10CModelData17GetRenderingModelERK13CStateManager");
extern "C" void reachstub_85() { mpReachStub("_ZN10CModelData17GetRenderingModelERK13CStateManager", "CModelData::GetRenderingModel(CStateManager const&)"); }

// CModelData::GetScaledLocatorTransform(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const
extern "C" void reachstub_86() asm("_ZNK10CModelData25GetScaledLocatorTransformERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_86() { mpReachStub("_ZNK10CModelData25GetScaledLocatorTransformERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CModelData::GetScaledLocatorTransform(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&) const"); }

// CModelData::GetScaledLocatorTransformDynamic(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, CCharAnimTime const*) const
extern "C" void reachstub_87() asm("_ZNK10CModelData32GetScaledLocatorTransformDynamicERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEPK13CCharAnimTime");
extern "C" void reachstub_87() { mpReachStub("_ZNK10CModelData32GetScaledLocatorTransformDynamicERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEPK13CCharAnimTime", "CModelData::GetScaledLocatorTransformDynamic(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, CCharAnimTime const*) const"); }

// CModelData::IsDefinitelyOpaque(CModelData::EWhichModel) const
extern "C" void reachstub_88() asm("_ZNK10CModelData18IsDefinitelyOpaqueENS_11EWhichModelE");
extern "C" void reachstub_88() { mpReachStub("_ZNK10CModelData18IsDefinitelyOpaqueENS_11EWhichModelE", "CModelData::IsDefinitelyOpaque(CModelData::EWhichModel) const"); }

// CModelData::Render(CModelData::EWhichModel, CTransform4f const&, CActorLights const*, CModelFlags const&) const
extern "C" void reachstub_89() asm("_ZNK10CModelData6RenderENS_11EWhichModelERK12CTransform4fPK12CActorLightsRK11CModelFlags");
extern "C" void reachstub_89() { mpReachStub("_ZNK10CModelData6RenderENS_11EWhichModelERK12CTransform4fPK12CActorLightsRK11CModelFlags", "CModelData::Render(CModelData::EWhichModel, CTransform4f const&, CActorLights const*, CModelFlags const&) const"); }

// CModelData::RenderParticles(CFrustumPlanes const&) const
extern "C" void reachstub_90() asm("_ZNK10CModelData15RenderParticlesERK14CFrustumPlanes");
extern "C" void reachstub_90() { mpReachStub("_ZNK10CModelData15RenderParticlesERK14CFrustumPlanes", "CModelData::RenderParticles(CFrustumPlanes const&) const"); }

// CModelData::RenderUnsortedParts(CModelData::EWhichModel, CTransform4f const&, CActorLights const*, CModelFlags const&) const
extern "C" void reachstub_91() asm("_ZNK10CModelData19RenderUnsortedPartsENS_11EWhichModelERK12CTransform4fPK12CActorLightsRK11CModelFlags");
extern "C" void reachstub_91() { mpReachStub("_ZNK10CModelData19RenderUnsortedPartsENS_11EWhichModelERK12CTransform4fPK12CActorLightsRK11CModelFlags", "CModelData::RenderUnsortedParts(CModelData::EWhichModel, CTransform4f const&, CActorLights const*, CModelFlags const&) const"); }

// CModelData::SetInfraModel(rstl::pair<unsigned int, unsigned int> const&)
extern "C" void reachstub_92() asm("_ZN10CModelData13SetInfraModelERKN4rstl4pairIjjEE");
extern "C" void reachstub_92() { mpReachStub("_ZN10CModelData13SetInfraModelERKN4rstl4pairIjjEE", "CModelData::SetInfraModel(rstl::pair<unsigned int, unsigned int> const&)"); }

// CModelData::SetXRayModel(rstl::pair<unsigned int, unsigned int> const&)
extern "C" void reachstub_93() asm("_ZN10CModelData12SetXRayModelERKN4rstl4pairIjjEE");
extern "C" void reachstub_93() { mpReachStub("_ZN10CModelData12SetXRayModelERKN4rstl4pairIjjEE", "CModelData::SetXRayModel(rstl::pair<unsigned int, unsigned int> const&)"); }

// CModelData::~CModelData()
extern "C" void reachstub_94() asm("_ZN10CModelDataD1Ev");
extern "C" void reachstub_94() { mpReachStub("_ZN10CModelDataD1Ev", "CModelData::~CModelData()"); }

// CMorphBall::SwitchToTire()
extern "C" void reachstub_95() asm("_ZN10CMorphBall12SwitchToTireEv");
extern "C" void reachstub_95() { mpReachStub("_ZN10CMorphBall12SwitchToTireEv", "CMorphBall::SwitchToTire()"); }

// CObjectList::fn_8000B538(TUniqueId) const
extern "C" void reachstub_96() asm("_ZNK11CObjectList11fn_8000B538E9TUniqueId");
extern "C" void reachstub_96() { mpReachStub("_ZNK11CObjectList11fn_8000B538E9TUniqueId", "CObjectList::fn_8000B538(TUniqueId) const"); }

// CObjectList::fn_8000B588(TUniqueId)
extern "C" void reachstub_97() asm("_ZN11CObjectList11fn_8000B588E9TUniqueId");
extern "C" void reachstub_97() { mpReachStub("_ZN11CObjectList11fn_8000B588E9TUniqueId", "CObjectList::fn_8000B588(TUniqueId)"); }

// CParticleDatabase::DeleteAllLights(CStateManager&)
extern "C" void reachstub_98() asm("_ZN17CParticleDatabase15DeleteAllLightsER13CStateManager");
extern "C" void reachstub_98() { mpReachStub("_ZN17CParticleDatabase15DeleteAllLightsER13CStateManager", "CParticleDatabase::DeleteAllLights(CStateManager&)"); }

// CParticleDatabase::GetBounds() const
extern "C" void reachstub_99() asm("_ZNK17CParticleDatabase9GetBoundsEv");
extern "C" void reachstub_99() { mpReachStub("_ZNK17CParticleDatabase9GetBoundsEv", "CParticleDatabase::GetBounds() const"); }

// CParticleDatabase::RenderSystemsToBeDrawnFirst() const
extern "C" void reachstub_100() asm("_ZNK17CParticleDatabase27RenderSystemsToBeDrawnFirstEv");
extern "C" void reachstub_100() { mpReachStub("_ZNK17CParticleDatabase27RenderSystemsToBeDrawnFirstEv", "CParticleDatabase::RenderSystemsToBeDrawnFirst() const"); }

// CParticleDatabase::RenderSystemsToBeDrawnLast() const
extern "C" void reachstub_101() asm("_ZNK17CParticleDatabase26RenderSystemsToBeDrawnLastEv");
extern "C" void reachstub_101() { mpReachStub("_ZNK17CParticleDatabase26RenderSystemsToBeDrawnLastEv", "CParticleDatabase::RenderSystemsToBeDrawnLast() const"); }

// CPlayer* TCastToPtr<CPlayer>(CEntity&)
extern "C" void reachstub_102() asm("_Z10TCastToPtrI7CPlayerEPT_R7CEntity");
extern "C" void reachstub_102() { mpReachStub("_Z10TCastToPtrI7CPlayerEPT_R7CEntity", "CPlayer* TCastToPtr<CPlayer>(CEntity&)"); }

// CPlayer* TCastToPtr<CPlayer>(CEntity*)
extern "C" void reachstub_103() asm("_Z10TCastToPtrI7CPlayerEPT_P7CEntity");
extern "C" void reachstub_103() { mpReachStub("_Z10TCastToPtrI7CPlayerEPT_P7CEntity", "CPlayer* TCastToPtr<CPlayer>(CEntity*)"); }

// CPlayer::PlaySfxForPlayer(unsigned int, short, int, bool, int)
extern "C" void reachstub_104() asm("_ZN7CPlayer16PlaySfxForPlayerEjsibi");
extern "C" void reachstub_104() { mpReachStub("_ZN7CPlayer16PlaySfxForPlayerEjsibi", "CPlayer::PlaySfxForPlayer(unsigned int, short, int, bool, int)"); }

// CPlayer::fn_8000BC44(CStateManager&)
extern "C" void reachstub_105() asm("_ZN7CPlayer11fn_8000BC44ER13CStateManager");
extern "C" void reachstub_105() { mpReachStub("_ZN7CPlayer11fn_8000BC44ER13CStateManager", "CPlayer::fn_8000BC44(CStateManager&)"); }

// CPlayer::fn_8000BE98() const
extern "C" void reachstub_106() asm("_ZNK7CPlayer11fn_8000BE98Ev");
extern "C" void reachstub_106() { mpReachStub("_ZNK7CPlayer11fn_8000BE98Ev", "CPlayer::fn_8000BE98() const"); }

// CPlayer::fn_8000d3ac(CVector3f const&, CStateManager&)
extern "C" void reachstub_107() asm("_ZN7CPlayer11fn_8000d3acERK9CVector3fR13CStateManager");
extern "C" void reachstub_107() { mpReachStub("_ZN7CPlayer11fn_8000d3acERK9CVector3fR13CStateManager", "CPlayer::fn_8000d3ac(CVector3f const&, CStateManager&)"); }

// CPlayer::fn_8000d40c(CVector3f const&, CStateManager&)
extern "C" void reachstub_108() asm("_ZN7CPlayer11fn_8000d40cERK9CVector3fR13CStateManager");
extern "C" void reachstub_108() { mpReachStub("_ZN7CPlayer11fn_8000d40cERK9CVector3fR13CStateManager", "CPlayer::fn_8000d40c(CVector3f const&, CStateManager&)"); }

// CPlayerGun::ComboActive(CStateManager&, EStateMsg, float)
extern "C" void reachstub_109() asm("_ZN10CPlayerGun11ComboActiveER13CStateManager9EStateMsgf");
extern "C" void reachstub_109() { mpReachStub("_ZN10CPlayerGun11ComboActiveER13CStateManager9EStateMsgf", "CPlayerGun::ComboActive(CStateManager&, EStateMsg, float)"); }

// CPlayerGun::EnableChargeFx(CStateManager&, bool)
extern "C" void reachstub_110() asm("_ZN10CPlayerGun14EnableChargeFxER13CStateManagerb");
extern "C" void reachstub_110() { mpReachStub("_ZN10CPlayerGun14EnableChargeFxER13CStateManagerb", "CPlayerGun::EnableChargeFx(CStateManager&, bool)"); }

// CPlayerGun::FidgetOver(CStateManager&, CTriggerData const&)
extern "C" void reachstub_111() asm("_ZN10CPlayerGun10FidgetOverER13CStateManagerRK12CTriggerData");
extern "C" void reachstub_111() { mpReachStub("_ZN10CPlayerGun10FidgetOverER13CStateManagerRK12CTriggerData", "CPlayerGun::FidgetOver(CStateManager&, CTriggerData const&)"); }

// CPlayerGun::Fidgeting(CStateManager&, EStateMsg, float)
extern "C" void reachstub_112() asm("_ZN10CPlayerGun9FidgetingER13CStateManager9EStateMsgf");
extern "C" void reachstub_112() { mpReachStub("_ZN10CPlayerGun9FidgetingER13CStateManager9EStateMsgf", "CPlayerGun::Fidgeting(CStateManager&, EStateMsg, float)"); }

// CPlayerGun::GetBeamAmmoTypeAndCosts(bool, CStateManager&, CPlayerState::EItemType&, CPlayerState::EItemType&, int&) const
extern "C" void reachstub_113() asm("_ZNK10CPlayerGun23GetBeamAmmoTypeAndCostsEbR13CStateManagerRN12CPlayerState9EItemTypeES4_Ri");
extern "C" void reachstub_113() { mpReachStub("_ZNK10CPlayerGun23GetBeamAmmoTypeAndCostsEbR13CStateManagerRN12CPlayerState9EItemTypeES4_Ri", "CPlayerGun::GetBeamAmmoTypeAndCosts(bool, CStateManager&, CPlayerState::EItemType&, CPlayerState::EItemType&, int&) const"); }

// CPlayerGun::GetPlayer(CStateManager&) const
extern "C" void reachstub_114() asm("_ZNK10CPlayerGun9GetPlayerER13CStateManager");
extern "C" void reachstub_114() { mpReachStub("_ZNK10CPlayerGun9GetPlayerER13CStateManager", "CPlayerGun::GetPlayer(CStateManager&) const"); }

// CPlayerGun::GetPlayerFromAll(CStateManager&) const
extern "C" void reachstub_115() asm("_ZNK10CPlayerGun16GetPlayerFromAllER13CStateManager");
extern "C" void reachstub_115() { mpReachStub("_ZNK10CPlayerGun16GetPlayerFromAllER13CStateManager", "CPlayerGun::GetPlayerFromAll(CStateManager&) const"); }

// CPlayerGun::GetTargetId(CStateManager&)
extern "C" void reachstub_116() asm("_ZN10CPlayerGun11GetTargetIdER13CStateManager");
extern "C" void reachstub_116() { mpReachStub("_ZN10CPlayerGun11GetTargetIdER13CStateManager", "CPlayerGun::GetTargetId(CStateManager&)"); }

// CPlayerGun::InPhazon(CStateManager&, CTriggerData const&)
extern "C" void reachstub_117() asm("_ZN10CPlayerGun8InPhazonER13CStateManagerRK12CTriggerData");
extern "C" void reachstub_117() { mpReachStub("_ZN10CPlayerGun8InPhazonER13CStateManagerRK12CTriggerData", "CPlayerGun::InPhazon(CStateManager&, CTriggerData const&)"); }

// CPlayerGun::InitiateCombo(CStateManager&, CTriggerData const&)
extern "C" void reachstub_118() asm("_ZN10CPlayerGun13InitiateComboER13CStateManagerRK12CTriggerData");
extern "C" void reachstub_118() { mpReachStub("_ZN10CPlayerGun13InitiateComboER13CStateManagerRK12CTriggerData", "CPlayerGun::InitiateCombo(CStateManager&, CTriggerData const&)"); }

// CPlayerGun::IsOutOfAmmoToShoot(CStateManager&) const
extern "C" void reachstub_119() asm("_ZNK10CPlayerGun18IsOutOfAmmoToShootER13CStateManager");
extern "C" void reachstub_119() { mpReachStub("_ZNK10CPlayerGun18IsOutOfAmmoToShootER13CStateManager", "CPlayerGun::IsOutOfAmmoToShoot(CStateManager&) const"); }

// CPlayerGun::Main(CStateManager&, EStateMsg, float)
extern "C" void reachstub_120() asm("_ZN10CPlayerGun4MainER13CStateManager9EStateMsgf");
extern "C" void reachstub_120() { mpReachStub("_ZN10CPlayerGun4MainER13CStateManager9EStateMsgf", "CPlayerGun::Main(CStateManager&, EStateMsg, float)"); }

// CPlayerGun::PlayAnim(CStateManager&, int, int)
extern "C" void reachstub_121() asm("_ZN10CPlayerGun8PlayAnimER13CStateManagerii");
extern "C" void reachstub_121() { mpReachStub("_ZN10CPlayerGun8PlayAnimER13CStateManagerii", "CPlayerGun::PlayAnim(CStateManager&, int, int)"); }

// CPlayerGun::Recoil(CStateManager&, EStateMsg, float)
extern "C" void reachstub_122() asm("_ZN10CPlayerGun6RecoilER13CStateManager9EStateMsgf");
extern "C" void reachstub_122() { mpReachStub("_ZN10CPlayerGun6RecoilER13CStateManager9EStateMsgf", "CPlayerGun::Recoil(CStateManager&, EStateMsg, float)"); }

// CPlayerGun::ResetCharge(CStateManager&, bool)
extern "C" void reachstub_123() asm("_ZN10CPlayerGun11ResetChargeER13CStateManagerb");
extern "C" void reachstub_123() { mpReachStub("_ZN10CPlayerGun11ResetChargeER13CStateManagerb", "CPlayerGun::ResetCharge(CStateManager&, bool)"); }

// CPlayerGun::StopChargeSound(CStateManager&, bool)
extern "C" void reachstub_124() asm("_ZN10CPlayerGun15StopChargeSoundER13CStateManagerb");
extern "C" void reachstub_124() { mpReachStub("_ZN10CPlayerGun15StopChargeSoundER13CStateManagerb", "CPlayerGun::StopChargeSound(CStateManager&, bool)"); }

// CPlayerGun::fn_801C72B4(CStateManager&, float)
extern "C" void reachstub_125() asm("_ZN10CPlayerGun11fn_801C72B4ER13CStateManagerf");
extern "C" void reachstub_125() { mpReachStub("_ZN10CPlayerGun11fn_801C72B4ER13CStateManagerf", "CPlayerGun::fn_801C72B4(CStateManager&, float)"); }

// CPlayerGun::fn_801CA734(CStateManager&)
extern "C" void reachstub_126() asm("_ZN10CPlayerGun11fn_801CA734ER13CStateManager");
extern "C" void reachstub_126() { mpReachStub("_ZN10CPlayerGun11fn_801CA734ER13CStateManager", "CPlayerGun::fn_801CA734(CStateManager&)"); }

// CPlayerGun::fn_801CD55C(CStateManager&, bool)
extern "C" void reachstub_127() asm("_ZN10CPlayerGun11fn_801CD55CER13CStateManagerb");
extern "C" void reachstub_127() { mpReachStub("_ZN10CPlayerGun11fn_801CD55CER13CStateManagerb", "CPlayerGun::fn_801CD55C(CStateManager&, bool)"); }

// CPlayerGun::fn_801CE0DC(CStateManager&)
extern "C" void reachstub_128() asm("_ZN10CPlayerGun11fn_801CE0DCER13CStateManager");
extern "C" void reachstub_128() { mpReachStub("_ZN10CPlayerGun11fn_801CE0DCER13CStateManager", "CPlayerGun::fn_801CE0DC(CStateManager&)"); }

// CPlayerGun::fn_801CE5C0(CFinalInput const&, CStateManager&)
extern "C" void reachstub_129() asm("_ZN10CPlayerGun11fn_801CE5C0ERK11CFinalInputR13CStateManager");
extern "C" void reachstub_129() { mpReachStub("_ZN10CPlayerGun11fn_801CE5C0ERK11CFinalInputR13CStateManager", "CPlayerGun::fn_801CE5C0(CFinalInput const&, CStateManager&)"); }

// CPlayerGun::fn_801DE430(CStateManager&)
extern "C" void reachstub_130() asm("_ZN10CPlayerGun11fn_801DE430ER13CStateManager");
extern "C" void reachstub_130() { mpReachStub("_ZN10CPlayerGun11fn_801DE430ER13CStateManager", "CPlayerGun::fn_801DE430(CStateManager&)"); }

// CPlayerGunUnk570::fn_801D6D8C()
extern "C" void reachstub_131() asm("_ZN16CPlayerGunUnk57011fn_801D6D8CEv");
extern "C" void reachstub_131() { mpReachStub("_ZN16CPlayerGunUnk57011fn_801D6D8CEv", "CPlayerGunUnk570::fn_801D6D8C()"); }

// CPlayerGunUnk570::fn_801D6ED0(int, CStateManager&, float, bool)
extern "C" void reachstub_132() asm("_ZN16CPlayerGunUnk57011fn_801D6ED0EiR13CStateManagerfb");
extern "C" void reachstub_132() { mpReachStub("_ZN16CPlayerGunUnk57011fn_801D6ED0EiR13CStateManagerfb", "CPlayerGunUnk570::fn_801D6ED0(int, CStateManager&, float, bool)"); }

// CPlayerGunUnk578::fn_801D5DD0(int, CStateManager&)
extern "C" void reachstub_133() asm("_ZN16CPlayerGunUnk57811fn_801D5DD0EiR13CStateManager");
extern "C" void reachstub_133() { mpReachStub("_ZN16CPlayerGunUnk57811fn_801D5DD0EiR13CStateManager", "CPlayerGunUnk578::fn_801D5DD0(int, CStateManager&)"); }

// CPlayerGunUnk578::fn_801D6894(CStateManager&, bool)
extern "C" void reachstub_134() asm("_ZN16CPlayerGunUnk57811fn_801D6894ER13CStateManagerb");
extern "C" void reachstub_134() { mpReachStub("_ZN16CPlayerGunUnk57811fn_801D6894ER13CStateManagerb", "CPlayerGunUnk578::fn_801D6894(CStateManager&, bool)"); }

// CPlayerGunUnk578::fn_801D6924() const
extern "C" void reachstub_135() asm("_ZNK16CPlayerGunUnk57811fn_801D6924Ev");
extern "C" void reachstub_135() { mpReachStub("_ZNK16CPlayerGunUnk57811fn_801D6924Ev", "CPlayerGunUnk578::fn_801D6924() const"); }

// CPlayerGunUnk578::fn_801D6930(TUniqueId)
extern "C" void reachstub_136() asm("_ZN16CPlayerGunUnk57811fn_801D6930E9TUniqueId");
extern "C" void reachstub_136() { mpReachStub("_ZN16CPlayerGunUnk57811fn_801D6930E9TUniqueId", "CPlayerGunUnk578::fn_801D6930(TUniqueId)"); }

// CPlayerGunUnk624::fn_80320978()
extern "C" void reachstub_137() asm("_ZN16CPlayerGunUnk62411fn_80320978Ev");
extern "C" void reachstub_137() { mpReachStub("_ZN16CPlayerGunUnk62411fn_80320978Ev", "CPlayerGunUnk624::fn_80320978()"); }

// CPlayerGunUnk624::fn_80320A04()
extern "C" void reachstub_138() asm("_ZN16CPlayerGunUnk62411fn_80320A04Ev");
extern "C" void reachstub_138() { mpReachStub("_ZN16CPlayerGunUnk62411fn_80320A04Ev", "CPlayerGunUnk624::fn_80320A04()"); }

// CQuaternion::BuildInverted() const
extern "C" void reachstub_139() asm("_ZNK11CQuaternion13BuildInvertedEv");
extern "C" void reachstub_139() { mpReachStub("_ZNK11CQuaternion13BuildInvertedEv", "CQuaternion::BuildInverted() const"); }

// CResFactory::AsyncIdle(unsigned int, bool)
extern "C" void reachstub_140() asm("_ZN11CResFactory9AsyncIdleEjb");
extern "C" void reachstub_140() { mpReachStub("_ZN11CResFactory9AsyncIdleEjb", "CResFactory::AsyncIdle(unsigned int, bool)"); }

// CRumbleManager::Rumble(CStateManager&, ERumbleFxId, float, ERumblePriority)
extern "C" void reachstub_141() asm("_ZN14CRumbleManager6RumbleER13CStateManager11ERumbleFxIdf15ERumblePriority");
extern "C" void reachstub_141() { mpReachStub("_ZN14CRumbleManager6RumbleER13CStateManager11ERumbleFxIdf15ERumblePriority", "CRumbleManager::Rumble(CStateManager&, ERumbleFxId, float, ERumblePriority)"); }

// CRumbleManager::StopRumble(short)
extern "C" void reachstub_142() asm("_ZN14CRumbleManager10StopRumbleEs");
extern "C" void reachstub_142() { mpReachStub("_ZN14CRumbleManager10StopRumbleEs", "CRumbleManager::StopRumble(short)"); }

// CSaveGameScreen::CSaveGameScreen(int, unsigned long)
extern "C" void reachstub_143() asm("_ZN15CSaveGameScreenC1Eim");
extern "C" void reachstub_143() { mpReachStub("_ZN15CSaveGameScreenC1Eim", "CSaveGameScreen::CSaveGameScreen(int, unsigned long)"); }

// CSaveGameScreen::~CSaveGameScreen()
extern "C" void reachstub_144() asm("_ZN15CSaveGameScreenD1Ev");
extern "C" void reachstub_144() { mpReachStub("_ZN15CSaveGameScreenD1Ev", "CSaveGameScreen::~CSaveGameScreen()"); }

// CScriptEffect* TCastToPtr<CScriptEffect>(CEntity*)
extern "C" void reachstub_145() asm("_Z10TCastToPtrI13CScriptEffectEPT_P7CEntity");
extern "C" void reachstub_145() { mpReachStub("_Z10TCastToPtrI13CScriptEffectEPT_P7CEntity", "CScriptEffect* TCastToPtr<CScriptEffect>(CEntity*)"); }

// CScriptEffect::CScriptEffect(TUniqueId, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, CEntityInfo const&, CTransform4f const&, CVector3f const&, unsigned int, int, int, int, int, float, float, float, float, bool, float, float, float, bool, bool, bool, CLightParameters const&, bool, CScriptEffect::ParamStruct const&, bool, bool, bool, int)
extern "C" void reachstub_146() asm("_ZN13CScriptEffectC1E9TUniqueIdRKN4rstl12basic_stringIcNS1_11char_traitsIcEENS1_17rmemory_allocatorEEERK11CEntityInfoRK12CTransform4fRK9CVector3fjiiiiffffbfffbbbRK16CLightParametersbRKNS_11ParamStructEbbbi");
extern "C" void reachstub_146() { mpReachStub("_ZN13CScriptEffectC1E9TUniqueIdRKN4rstl12basic_stringIcNS1_11char_traitsIcEENS1_17rmemory_allocatorEEERK11CEntityInfoRK12CTransform4fRK9CVector3fjiiiiffffbfffbbbRK16CLightParametersbRKNS_11ParamStructEbbbi", "CScriptEffect::CScriptEffect(TUniqueId, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, CEntityInfo const&, CTransform4f const&, CVector3f const&, unsigned int, int, int, int, int, float, float, float, float, bool, float, float, float, bool, bool, bool, CLightParameters const&, bool, CScriptEffect::ParamStruct const&, bool, bool, bool, int)"); }

// CSfxManager::AddEmitter(CAudioSys::C3DEmitterParmData&, bool, short, bool, int)
extern "C" void reachstub_147() asm("_ZN11CSfxManager10AddEmitterERN9CAudioSys18C3DEmitterParmDataEbsbi");
extern "C" void reachstub_147() { mpReachStub("_ZN11CSfxManager10AddEmitterERN9CAudioSys18C3DEmitterParmDataEbsbi", "CSfxManager::AddEmitter(CAudioSys::C3DEmitterParmData&, bool, short, bool, int)"); }

// CSfxManager::PitchBend(CSfxHandle, int)
extern "C" void reachstub_148() asm("_ZN11CSfxManager9PitchBendE10CSfxHandlei");
extern "C" void reachstub_148() { mpReachStub("_ZN11CSfxManager9PitchBendE10CSfxHandlei", "CSfxManager::PitchBend(CSfxHandle, int)"); }

// CSfxManager::RemoveEmitter(CSfxHandle)
extern "C" void reachstub_149() asm("_ZN11CSfxManager13RemoveEmitterE10CSfxHandle");
extern "C" void reachstub_149() { mpReachStub("_ZN11CSfxManager13RemoveEmitterE10CSfxHandle", "CSfxManager::RemoveEmitter(CSfxHandle)"); }

// CSfxManager::SfxStart(unsigned short, short, short, bool, short, bool, int)
extern "C" void reachstub_150() asm("_ZN11CSfxManager8SfxStartEtssbsbi");
extern "C" void reachstub_150() { mpReachStub("_ZN11CSfxManager8SfxStartEtssbsbi", "CSfxManager::SfxStart(unsigned short, short, short, bool, short, bool, int)"); }

// CSfxManager::TranslateSFXID(unsigned short)
extern "C" void reachstub_151() asm("_ZN11CSfxManager14TranslateSFXIDEt");
extern "C" void reachstub_151() { mpReachStub("_ZN11CSfxManager14TranslateSFXIDEt", "CSfxManager::TranslateSFXID(unsigned short)"); }

// CSfxManager::UpdateEmitter(CSfxHandle, CVector3f const&, CVector3f const&, unsigned char)
extern "C" void reachstub_152() asm("_ZN11CSfxManager13UpdateEmitterE10CSfxHandleRK9CVector3fS3_h");
extern "C" void reachstub_152() { mpReachStub("_ZN11CSfxManager13UpdateEmitterE10CSfxHandleRK9CVector3fS3_h", "CSfxManager::UpdateEmitter(CSfxHandle, CVector3f const&, CVector3f const&, unsigned char)"); }

// CSimplePool::fn_8029c7e8(SObjectTag const&)
extern "C" void reachstub_153() asm("_ZN11CSimplePool11fn_8029c7e8ERK10SObjectTag");
extern "C" void reachstub_153() { mpReachStub("_ZN11CSimplePool11fn_8029c7e8ERK10SObjectTag", "CSimplePool::fn_8029c7e8(SObjectTag const&)"); }

// CSimpleShadow::GetBounds() const
extern "C" void reachstub_154() asm("_ZNK13CSimpleShadow9GetBoundsEv");
extern "C" void reachstub_154() { mpReachStub("_ZNK13CSimpleShadow9GetBoundsEv", "CSimpleShadow::GetBounds() const"); }

// CSimpleShadow::GetTransform() const
extern "C" void reachstub_155() asm("_ZNK13CSimpleShadow12GetTransformEv");
extern "C" void reachstub_155() { mpReachStub("_ZNK13CSimpleShadow12GetTransformEv", "CSimpleShadow::GetTransform() const"); }

// CSimpleShadow::Valid() const
extern "C" void reachstub_156() asm("_ZNK13CSimpleShadow5ValidEv");
extern "C" void reachstub_156() { mpReachStub("_ZNK13CSimpleShadow5ValidEv", "CSimpleShadow::Valid() const"); }

// CSkinnedModel::ClearPointGeneratorFunc()
extern "C" void reachstub_157() asm("_ZN13CSkinnedModel23ClearPointGeneratorFuncEv");
extern "C" void reachstub_157() { mpReachStub("_ZN13CSkinnedModel23ClearPointGeneratorFuncEv", "CSkinnedModel::ClearPointGeneratorFunc()"); }

// CSortedListManager::BuildColliderList(rstl::reserved_vector<TUniqueId, 1024>&, CActor const&, CAABox const&) const
extern "C" void reachstub_158() asm("_ZNK18CSortedListManager17BuildColliderListERN4rstl15reserved_vectorI9TUniqueIdLi1024EEERK6CActorRK6CAABox");
extern "C" void reachstub_158() { mpReachStub("_ZNK18CSortedListManager17BuildColliderListERN4rstl15reserved_vectorI9TUniqueIdLi1024EEERK6CActorRK6CAABox", "CSortedListManager::BuildColliderList(rstl::reserved_vector<TUniqueId, 1024>&, CActor const&, CAABox const&) const"); }

// CSortedListManager::BuildNearList(rstl::reserved_vector<TUniqueId, 1024>&, CAABox const&, CMaterialFilter const&, CActor const*) const
extern "C" void reachstub_159() asm("_ZNK18CSortedListManager13BuildNearListERN4rstl15reserved_vectorI9TUniqueIdLi1024EEERK6CAABoxRK15CMaterialFilterPK6CActor");
extern "C" void reachstub_159() { mpReachStub("_ZNK18CSortedListManager13BuildNearListERN4rstl15reserved_vectorI9TUniqueIdLi1024EEERK6CAABoxRK15CMaterialFilterPK6CActor", "CSortedListManager::BuildNearList(rstl::reserved_vector<TUniqueId, 1024>&, CAABox const&, CMaterialFilter const&, CActor const*) const"); }

// CSortedListManager::BuildNearList(rstl::reserved_vector<TUniqueId, 1024>&, CVector3f const&, CVector3f const&, float, CMaterialFilter const&, CActor const*) const
extern "C" void reachstub_160() asm("_ZNK18CSortedListManager13BuildNearListERN4rstl15reserved_vectorI9TUniqueIdLi1024EEERK9CVector3fS7_fRK15CMaterialFilterPK6CActor");
extern "C" void reachstub_160() { mpReachStub("_ZNK18CSortedListManager13BuildNearListERN4rstl15reserved_vectorI9TUniqueIdLi1024EEERK9CVector3fS7_fRK15CMaterialFilterPK6CActor", "CSortedListManager::BuildNearList(rstl::reserved_vector<TUniqueId, 1024>&, CVector3f const&, CVector3f const&, float, CMaterialFilter const&, CActor const*) const"); }

// CStateManager::AddObject(CEntity&)
extern "C" void reachstub_161() asm("_ZN13CStateManager9AddObjectER7CEntity");
extern "C" void reachstub_161() { mpReachStub("_ZN13CStateManager9AddObjectER7CEntity", "CStateManager::AddObject(CEntity&)"); }

// CStateManager::DisplayAlertAboutOutOfAmmo(CPlayer const&, CPlayerState::EItemType) const
extern "C" void reachstub_162() asm("_ZNK13CStateManager26DisplayAlertAboutOutOfAmmoERK7CPlayerN12CPlayerState9EItemTypeE");
extern "C" void reachstub_162() { mpReachStub("_ZNK13CStateManager26DisplayAlertAboutOutOfAmmoERK7CPlayerN12CPlayerState9EItemTypeE", "CStateManager::DisplayAlertAboutOutOfAmmo(CPlayer const&, CPlayerState::EItemType) const"); }

// CStateManager::GetObjectByIdFromListAll(TUniqueId)
extern "C" void reachstub_163() asm("_ZN13CStateManager24GetObjectByIdFromListAllE9TUniqueId");
extern "C" void reachstub_163() { mpReachStub("_ZN13CStateManager24GetObjectByIdFromListAllE9TUniqueId", "CStateManager::GetObjectByIdFromListAll(TUniqueId)"); }

// CStateManager::RayCollideWorldInternal(CVector3f const&, CVector3f const&, CMaterialFilter const&, rstl::reserved_vector<TUniqueId, 1024> const&, CActor const*) const
extern "C" void reachstub_164() asm("_ZNK13CStateManager23RayCollideWorldInternalERK9CVector3fS2_RK15CMaterialFilterRKN4rstl15reserved_vectorI9TUniqueIdLi1024EEEPK6CActor");
extern "C" void reachstub_164() { mpReachStub("_ZNK13CStateManager23RayCollideWorldInternalERK9CVector3fS2_RK15CMaterialFilterRKN4rstl15reserved_vectorI9TUniqueIdLi1024EEEPK6CActor", "CStateManager::RayCollideWorldInternal(CVector3f const&, CVector3f const&, CMaterialFilter const&, rstl::reserved_vector<TUniqueId, 1024> const&, CActor const*) const"); }

// CStateManager::UpdateActorInSortedLists(CActor*)
extern "C" void reachstub_165() asm("_ZN13CStateManager24UpdateActorInSortedListsEP6CActor");
extern "C" void reachstub_165() { mpReachStub("_ZN13CStateManager24UpdateActorInSortedListsEP6CActor", "CStateManager::UpdateActorInSortedLists(CActor*)"); }

// CStateManager::UpdateObjectInLists(CEntity&)
extern "C" void reachstub_166() asm("_ZN13CStateManager19UpdateObjectInListsER7CEntity");
extern "C" void reachstub_166() { mpReachStub("_ZN13CStateManager19UpdateObjectInListsER7CEntity", "CStateManager::UpdateObjectInLists(CEntity&)"); }

// CStateManager::fn_800366e4(CActor*)
extern "C" void reachstub_167() asm("_ZN13CStateManager11fn_800366e4EP6CActor");
extern "C" void reachstub_167() { mpReachStub("_ZN13CStateManager11fn_800366e4EP6CActor", "CStateManager::fn_800366e4(CActor*)"); }

// CStateManager::fn_8003C4B8(CVector3f const&, int)
extern "C" void reachstub_168() asm("_ZN13CStateManager11fn_8003C4B8ERK9CVector3fi");
extern "C" void reachstub_168() { mpReachStub("_ZN13CStateManager11fn_8003C4B8ERK9CVector3fi", "CStateManager::fn_8003C4B8(CVector3f const&, int)"); }

// CStateManager::fn_8003dd88(CActor&, TUniqueId, CDamageInfo const&, bool, int)
extern "C" void reachstub_169() asm("_ZN13CStateManager11fn_8003dd88ER6CActor9TUniqueIdRK11CDamageInfobi");
extern "C" void reachstub_169() { mpReachStub("_ZN13CStateManager11fn_8003dd88ER6CActor9TUniqueIdRK11CDamageInfobi", "CStateManager::fn_8003dd88(CActor&, TUniqueId, CDamageInfo const&, bool, int)"); }

// CStateManager::fn_800412EC(TUniqueId)
extern "C" void reachstub_170() asm("_ZN13CStateManager11fn_800412ECE9TUniqueId");
extern "C" void reachstub_170() { mpReachStub("_ZN13CStateManager11fn_800412ECE9TUniqueId", "CStateManager::fn_800412EC(TUniqueId)"); }

// CStateManager::fn_801EDD8C(TUniqueId) const
extern "C" void reachstub_171() asm("_ZNK13CStateManager11fn_801EDD8CE9TUniqueId");
extern "C" void reachstub_171() { mpReachStub("_ZNK13CStateManager11fn_801EDD8CE9TUniqueId", "CStateManager::fn_801EDD8C(TUniqueId) const"); }

// CStateManagerContainer::~CStateManagerContainer()
extern "C" void reachstub_172() asm("_ZN22CStateManagerContainerD1Ev");
extern "C" void reachstub_172() { mpReachStub("_ZN22CStateManagerContainerD1Ev", "CStateManagerContainer::~CStateManagerContainer()"); }

// CStateManagerUnk2900::~CStateManagerUnk2900()
extern "C" void reachstub_173() asm("_ZN20CStateManagerUnk2900D1Ev");
extern "C" void reachstub_173() { mpReachStub("_ZN20CStateManagerUnk2900D1Ev", "CStateManagerUnk2900::~CStateManagerUnk2900()"); }

// CStaticInterference::CStaticInterference(int)
extern "C" void reachstub_174() asm("_ZN19CStaticInterferenceC1Ei");
extern "C" void reachstub_174() { mpReachStub("_ZN19CStaticInterferenceC1Ei", "CStaticInterference::CStaticInterference(int)"); }

// CStaticInterference::Update(CStateManager const&, float)
extern "C" void reachstub_175() asm("_ZN19CStaticInterference6UpdateERK13CStateManagerf");
extern "C" void reachstub_175() { mpReachStub("_ZN19CStaticInterference6UpdateERK13CStateManagerf", "CStaticInterference::Update(CStateManager const&, float)"); }

// CStreamAudioManager::FadeBackIn(int, float)
extern "C" void reachstub_176() asm("_ZN19CStreamAudioManager10FadeBackInEif");
extern "C" void reachstub_176() { mpReachStub("_ZN19CStreamAudioManager10FadeBackInEif", "CStreamAudioManager::FadeBackIn(int, float)"); }

// CStreamAudioManager::SetCurrentAudio(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, float, float, unsigned char)
extern "C" void reachstub_177() asm("_ZN19CStreamAudioManager15SetCurrentAudioERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEffh");
extern "C" void reachstub_177() { mpReachStub("_ZN19CStreamAudioManager15SetCurrentAudioERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEffh", "CStreamAudioManager::SetCurrentAudio(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, float, float, unsigned char)"); }

// CStreamAudioManager::SetDefaultAudio(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, float, float, unsigned char)
extern "C" void reachstub_178() asm("_ZN19CStreamAudioManager15SetDefaultAudioERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEffh");
extern "C" void reachstub_178() { mpReachStub("_ZN19CStreamAudioManager15SetDefaultAudioERKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEffh", "CStreamAudioManager::SetDefaultAudio(rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, float, float, unsigned char)"); }

// CStreamAudioManager::Start(int, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, unsigned char, bool, float, float)
extern "C" void reachstub_179() asm("_ZN19CStreamAudioManager5StartEiRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEhbff");
extern "C" void reachstub_179() { mpReachStub("_ZN19CStreamAudioManager5StartEiRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEEhbff", "CStreamAudioManager::Start(int, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&, unsigned char, bool, float, float)"); }

// CStreamAudioManager::Stop(int, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)
extern "C" void reachstub_180() asm("_ZN19CStreamAudioManager4StopEiRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE");
extern "C" void reachstub_180() { mpReachStub("_ZN19CStreamAudioManager4StopEiRKN4rstl12basic_stringIcNS0_11char_traitsIcEENS0_17rmemory_allocatorEEE", "CStreamAudioManager::Stop(int, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)"); }

// CStreamAudioManager::TemporaryFadeOut(int, float)
extern "C" void reachstub_181() asm("_ZN19CStreamAudioManager16TemporaryFadeOutEif");
extern "C" void reachstub_181() { mpReachStub("_ZN19CStreamAudioManager16TemporaryFadeOutEif", "CStreamAudioManager::TemporaryFadeOut(int, float)"); }

// CStreamAudioManager::sub_803653f8(float)
extern "C" void reachstub_182() asm("_ZN19CStreamAudioManager12sub_803653f8Ef");
extern "C" void reachstub_182() { mpReachStub("_ZN19CStreamAudioManager12sub_803653f8Ef", "CStreamAudioManager::sub_803653f8(float)"); }

// CStreamAudioManager::sub_80365424(float)
extern "C" void reachstub_183() asm("_ZN19CStreamAudioManager12sub_80365424Ef");
extern "C" void reachstub_183() { mpReachStub("_ZN19CStreamAudioManager12sub_80365424Ef", "CStreamAudioManager::sub_80365424(float)"); }

// CStreamAudioManager::sub_8036590c(float)
extern "C" void reachstub_184() asm("_ZN19CStreamAudioManager12sub_8036590cEf");
extern "C" void reachstub_184() { mpReachStub("_ZN19CStreamAudioManager12sub_8036590cEf", "CStreamAudioManager::sub_8036590c(float)"); }

// CTexture::InvalidateTexmap(GXTexMapID)
extern "C" void reachstub_185() asm("_ZN8CTexture16InvalidateTexmapE10GXTexMapID");
extern "C" void reachstub_185() { mpReachStub("_ZN8CTexture16InvalidateTexmapE10GXTexMapID", "CTexture::InvalidateTexmap(GXTexMapID)"); }

// CTweakGame::GetPakFile()
extern "C" void reachstub_186() asm("_ZN10CTweakGame10GetPakFileEv");
extern "C" void reachstub_186() { mpReachStub("_ZN10CTweakGame10GetPakFileEv", "CTweakGame::GetPakFile()"); }

// CTweakGame::GetTotalPercentage()
extern "C" void reachstub_187() asm("_ZN10CTweakGame18GetTotalPercentageEv");
extern "C" void reachstub_187() { mpReachStub("_ZN10CTweakGame18GetTotalPercentageEv", "CTweakGame::GetTotalPercentage()"); }

// CTweakPlayerGun::GetMaxAbsorbedPhazonShots()
extern "C" void reachstub_188() asm("_ZN15CTweakPlayerGun25GetMaxAbsorbedPhazonShotsEv");
extern "C" void reachstub_188() { mpReachStub("_ZN15CTweakPlayerGun25GetMaxAbsorbedPhazonShotsEv", "CTweakPlayerGun::GetMaxAbsorbedPhazonShots()"); }

// CVParamTransfer::Null()
extern "C" void reachstub_189() asm("_ZN15CVParamTransfer4NullEv");
extern "C" void reachstub_189() { mpReachStub("_ZN15CVParamTransfer4NullEv", "CVParamTransfer::Null()"); }

// CVector3f::Cross(CVector3f const&, CVector3f const&)
extern "C" void reachstub_190() asm("_ZN9CVector3f5CrossERKS_S1_");
extern "C" void reachstub_190() { mpReachStub("_ZN9CVector3f5CrossERKS_S1_", "CVector3f::Cross(CVector3f const&, CVector3f const&)"); }

// CWeaponMgr::GetNumActive(TUniqueId, EWeaponType) const
extern "C" void reachstub_191() asm("_ZNK10CWeaponMgr12GetNumActiveE9TUniqueId11EWeaponType");
extern "C" void reachstub_191() { mpReachStub("_ZNK10CWeaponMgr12GetNumActiveE9TUniqueId11EWeaponType", "CWeaponMgr::GetNumActive(TUniqueId, EWeaponType) const"); }

// CWeaponMgr::fn_800B321C(TUniqueId, EWeaponType)
extern "C" void reachstub_192() asm("_ZN10CWeaponMgr11fn_800B321CE9TUniqueId11EWeaponType");
extern "C" void reachstub_192() { mpReachStub("_ZN10CWeaponMgr11fn_800B321CE9TUniqueId11EWeaponType", "CWeaponMgr::fn_800B321C(TUniqueId, EWeaponType)"); }

// CWeaponMgr::fn_800B32E0(TUniqueId, EWeaponType)
extern "C" void reachstub_193() asm("_ZN10CWeaponMgr11fn_800B32E0E9TUniqueId11EWeaponType");
extern "C" void reachstub_193() { mpReachStub("_ZN10CWeaponMgr11fn_800B32E0E9TUniqueId11EWeaponType", "CWeaponMgr::fn_800B32E0(TUniqueId, EWeaponType)"); }

// CWorld::SetLoadPauseState(bool)
extern "C" void reachstub_194() asm("_ZN6CWorld17SetLoadPauseStateEb");
extern "C" void reachstub_194() { mpReachStub("_ZN6CWorld17SetLoadPauseStateEb", "CWorld::SetLoadPauseState(bool)"); }

// IElement::CElementAllocator::Alloc(unsigned long, char const*, char const*)
extern "C" void reachstub_195() asm("_ZN8IElement17CElementAllocator5AllocEmPKcS2_");
extern "C" void reachstub_195() { mpReachStub("_ZN8IElement17CElementAllocator5AllocEmPKcS2_", "IElement::CElementAllocator::Alloc(unsigned long, char const*, char const*)"); }

// IElement::CElementAllocator::Free(void*, unsigned long)
extern "C" void reachstub_196() asm("_ZN8IElement17CElementAllocator4FreeEPvm");
extern "C" void reachstub_196() { mpReachStub("_ZN8IElement17CElementAllocator4FreeEPvm", "IElement::CElementAllocator::Free(void*, unsigned long)"); }

// LdrToEntityInfo(CEntityInfo const&, SLdrEditorProperties const&)
extern "C" void reachstub_197() asm("_Z15LdrToEntityInfoRK11CEntityInfoRK20SLdrEditorProperties");
extern "C" void reachstub_197() { mpReachStub("_Z15LdrToEntityInfoRK11CEntityInfoRK20SLdrEditorProperties", "LdrToEntityInfo(CEntityInfo const&, SLdrEditorProperties const&)"); }

// LdrToEntityInfo(CEntityInfo&, SLdrEditorProperties const&)
extern "C" void reachstub_198() asm("_Z15LdrToEntityInfoR11CEntityInfoRK20SLdrEditorProperties");
extern "C" void reachstub_198() { mpReachStub("_Z15LdrToEntityInfoR11CEntityInfoRK20SLdrEditorProperties", "LdrToEntityInfo(CEntityInfo&, SLdrEditorProperties const&)"); }

// LoadActorParameters(SLdrActorParameters const&)
extern "C" void reachstub_199() asm("_Z19LoadActorParametersRK19SLdrActorParameters");
extern "C" void reachstub_199() { mpReachStub("_Z19LoadActorParametersRK19SLdrActorParameters", "LoadActorParameters(SLdrActorParameters const&)"); }

// LoadCAABox(CStateManager&, TAreaId const&, CVector3f const&, CVector3f const&)
extern "C" void reachstub_200() asm("_Z10LoadCAABoxR13CStateManagerRK7TAreaIdRK9CVector3fS6_");
extern "C" void reachstub_200() { mpReachStub("_Z10LoadCAABoxR13CStateManagerRK7TAreaIdRK9CVector3fS6_", "LoadCAABox(CStateManager&, TAreaId const&, CVector3f const&, CVector3f const&)"); }

// LoadEchoParameters(SLdrEchoParameters const&)
extern "C" void reachstub_201() asm("_Z18LoadEchoParametersRK18SLdrEchoParameters");
extern "C" void reachstub_201() { mpReachStub("_Z18LoadEchoParametersRK18SLdrEchoParameters", "LoadEchoParameters(SLdrEchoParameters const&)"); }

// LoadEditorTransform(SLdrEditorProperties const&)
extern "C" void reachstub_202() asm("_Z19LoadEditorTransformRK20SLdrEditorProperties");
extern "C" void reachstub_202() { mpReachStub("_Z19LoadEditorTransformRK20SLdrEditorProperties", "LoadEditorTransform(SLdrEditorProperties const&)"); }

// LoadModelData(CVector3f const&, unsigned int, SLdrAnimationParameters const&, bool)
extern "C" void reachstub_203() asm("_Z13LoadModelDataRK9CVector3fjRK23SLdrAnimationParametersb");
extern "C" void reachstub_203() { mpReachStub("_Z13LoadModelDataRK9CVector3fjRK23SLdrAnimationParametersb", "LoadModelData(CVector3f const&, unsigned int, SLdrAnimationParameters const&, bool)"); }

// LoadTypedefEditorProperties(SLdrEditorProperties&, CInputStream&)
extern "C" void reachstub_204() asm("_Z27LoadTypedefEditorPropertiesR20SLdrEditorPropertiesR12CInputStream");
extern "C" void reachstub_204() { mpReachStub("_Z27LoadTypedefEditorPropertiesR20SLdrEditorPropertiesR12CInputStream", "LoadTypedefEditorProperties(SLdrEditorProperties&, CInputStream&)"); }

// LoadTypedefSLdrActorParameters(SLdrActorParameters&, CInputStream&)
extern "C" void reachstub_205() asm("_Z30LoadTypedefSLdrActorParametersR19SLdrActorParametersR12CInputStream");
extern "C" void reachstub_205() { mpReachStub("_Z30LoadTypedefSLdrActorParametersR19SLdrActorParametersR12CInputStream", "LoadTypedefSLdrActorParameters(SLdrActorParameters&, CInputStream&)"); }

// LoadTypedefSLdrAnimationParameters(SLdrAnimationParameters&, CInputStream&)
extern "C" void reachstub_206() asm("_Z34LoadTypedefSLdrAnimationParametersR23SLdrAnimationParametersR12CInputStream");
extern "C" void reachstub_206() { mpReachStub("_Z34LoadTypedefSLdrAnimationParametersR23SLdrAnimationParametersR12CInputStream", "LoadTypedefSLdrAnimationParameters(SLdrAnimationParameters&, CInputStream&)"); }

// LoadTypedefSLdrCameraShakerData(SLdrCameraShakerData&, CInputStream&)
extern "C" void reachstub_207() asm("_Z31LoadTypedefSLdrCameraShakerDataR20SLdrCameraShakerDataR12CInputStream");
extern "C" void reachstub_207() { mpReachStub("_Z31LoadTypedefSLdrCameraShakerDataR20SLdrCameraShakerDataR12CInputStream", "LoadTypedefSLdrCameraShakerData(SLdrCameraShakerData&, CInputStream&)"); }

// LoadTypedefSLdrEchoParameters(SLdrEchoParameters&, CInputStream&)
extern "C" void reachstub_208() asm("_Z29LoadTypedefSLdrEchoParametersR18SLdrEchoParametersR12CInputStream");
extern "C" void reachstub_208() { mpReachStub("_Z29LoadTypedefSLdrEchoParametersR18SLdrEchoParametersR12CInputStream", "LoadTypedefSLdrEchoParameters(SLdrEchoParameters&, CInputStream&)"); }

// LoadTypedefSLdrPlayerItem(SLdrPlayerItem&, CInputStream&)
extern "C" void reachstub_209() asm("_Z25LoadTypedefSLdrPlayerItemR14SLdrPlayerItemR12CInputStream");
extern "C" void reachstub_209() { mpReachStub("_Z25LoadTypedefSLdrPlayerItemR14SLdrPlayerItemR12CInputStream", "LoadTypedefSLdrPlayerItem(SLdrPlayerItem&, CInputStream&)"); }

// LoadTypedefSLdrTBallTransitionResources(SLdrTBallTransitionResources&, CInputStream&)
extern "C" void reachstub_210() asm("_Z39LoadTypedefSLdrTBallTransitionResourcesR28SLdrTBallTransitionResourcesR12CInputStream");
extern "C" void reachstub_210() { mpReachStub("_Z39LoadTypedefSLdrTBallTransitionResourcesR28SLdrTBallTransitionResourcesR12CInputStream", "LoadTypedefSLdrTBallTransitionResources(SLdrTBallTransitionResources&, CInputStream&)"); }

// LoadTypedefSLdrTBeamInfo(SLdrTBeamInfo&, CInputStream&)
extern "C" void reachstub_211() asm("_Z24LoadTypedefSLdrTBeamInfoR13SLdrTBeamInfoR12CInputStream");
extern "C" void reachstub_211() { mpReachStub("_Z24LoadTypedefSLdrTBeamInfoR13SLdrTBeamInfoR12CInputStream", "LoadTypedefSLdrTBeamInfo(SLdrTBeamInfo&, CInputStream&)"); }

// LoadTypedefSLdrTDamageInfo(SLdrTDamageInfo&, CInputStream&)
extern "C" void reachstub_212() asm("_Z26LoadTypedefSLdrTDamageInfoR15SLdrTDamageInfoR12CInputStream");
extern "C" void reachstub_212() { mpReachStub("_Z26LoadTypedefSLdrTDamageInfoR15SLdrTDamageInfoR12CInputStream", "LoadTypedefSLdrTDamageInfo(SLdrTDamageInfo&, CInputStream&)"); }

// LoadTypedefSLdrTGunResources(SLdrTGunResources&, CInputStream&)
extern "C" void reachstub_213() asm("_Z28LoadTypedefSLdrTGunResourcesR17SLdrTGunResourcesR12CInputStream");
extern "C" void reachstub_213() { mpReachStub("_Z28LoadTypedefSLdrTGunResourcesR17SLdrTGunResourcesR12CInputStream", "LoadTypedefSLdrTGunResources(SLdrTGunResources&, CInputStream&)"); }

// LoadTypedefSLdrTIcon_Configurations(SLdrTIcon_Configurations&, CInputStream&)
extern "C" void reachstub_214() asm("_Z35LoadTypedefSLdrTIcon_ConfigurationsR24SLdrTIcon_ConfigurationsR12CInputStream");
extern "C" void reachstub_214() { mpReachStub("_Z35LoadTypedefSLdrTIcon_ConfigurationsR24SLdrTIcon_ConfigurationsR12CInputStream", "LoadTypedefSLdrTIcon_Configurations(SLdrTIcon_Configurations&, CInputStream&)"); }

// LoadTypedefSLdrTweakAutoMapper_Base(SLdrTweakAutoMapper_Base&, CInputStream&)
extern "C" void reachstub_215() asm("_Z35LoadTypedefSLdrTweakAutoMapper_BaseR24SLdrTweakAutoMapper_BaseR12CInputStream");
extern "C" void reachstub_215() { mpReachStub("_Z35LoadTypedefSLdrTweakAutoMapper_BaseR24SLdrTweakAutoMapper_BaseR12CInputStream", "LoadTypedefSLdrTweakAutoMapper_Base(SLdrTweakAutoMapper_Base&, CInputStream&)"); }

// LoadTypedefSLdrTweakAutoMapper_DoorColors(SLdrTweakAutoMapper_DoorColors&, CInputStream&)
extern "C" void reachstub_216() asm("_Z41LoadTypedefSLdrTweakAutoMapper_DoorColorsR30SLdrTweakAutoMapper_DoorColorsR12CInputStream");
extern "C" void reachstub_216() { mpReachStub("_Z41LoadTypedefSLdrTweakAutoMapper_DoorColorsR30SLdrTweakAutoMapper_DoorColorsR12CInputStream", "LoadTypedefSLdrTweakAutoMapper_DoorColors(SLdrTweakAutoMapper_DoorColors&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_BoostBall(SLdrTweakBall_BoostBall&, CInputStream&)
extern "C" void reachstub_217() asm("_Z34LoadTypedefSLdrTweakBall_BoostBallR23SLdrTweakBall_BoostBallR12CInputStream");
extern "C" void reachstub_217() { mpReachStub("_Z34LoadTypedefSLdrTweakBall_BoostBallR23SLdrTweakBall_BoostBallR12CInputStream", "LoadTypedefSLdrTweakBall_BoostBall(SLdrTweakBall_BoostBall&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_Camera(SLdrTweakBall_Camera&, CInputStream&)
extern "C" void reachstub_218() asm("_Z31LoadTypedefSLdrTweakBall_CameraR20SLdrTweakBall_CameraR12CInputStream");
extern "C" void reachstub_218() { mpReachStub("_Z31LoadTypedefSLdrTweakBall_CameraR20SLdrTweakBall_CameraR12CInputStream", "LoadTypedefSLdrTweakBall_Camera(SLdrTweakBall_Camera&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_CannonBall(SLdrTweakBall_CannonBall&, CInputStream&)
extern "C" void reachstub_219() asm("_Z35LoadTypedefSLdrTweakBall_CannonBallR24SLdrTweakBall_CannonBallR12CInputStream");
extern "C" void reachstub_219() { mpReachStub("_Z35LoadTypedefSLdrTweakBall_CannonBallR24SLdrTweakBall_CannonBallR12CInputStream", "LoadTypedefSLdrTweakBall_CannonBall(SLdrTweakBall_CannonBall&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_DeathBall(SLdrTweakBall_DeathBall&, CInputStream&)
extern "C" void reachstub_220() asm("_Z34LoadTypedefSLdrTweakBall_DeathBallR23SLdrTweakBall_DeathBallR12CInputStream");
extern "C" void reachstub_220() { mpReachStub("_Z34LoadTypedefSLdrTweakBall_DeathBallR23SLdrTweakBall_DeathBallR12CInputStream", "LoadTypedefSLdrTweakBall_DeathBall(SLdrTweakBall_DeathBall&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_Misc(SLdrTweakBall_Misc&, CInputStream&)
extern "C" void reachstub_221() asm("_Z29LoadTypedefSLdrTweakBall_MiscR18SLdrTweakBall_MiscR12CInputStream");
extern "C" void reachstub_221() { mpReachStub("_Z29LoadTypedefSLdrTweakBall_MiscR18SLdrTweakBall_MiscR12CInputStream", "LoadTypedefSLdrTweakBall_Misc(SLdrTweakBall_Misc&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_Movement(SLdrTweakBall_Movement&, CInputStream&)
extern "C" void reachstub_222() asm("_Z33LoadTypedefSLdrTweakBall_MovementR22SLdrTweakBall_MovementR12CInputStream");
extern "C" void reachstub_222() { mpReachStub("_Z33LoadTypedefSLdrTweakBall_MovementR22SLdrTweakBall_MovementR12CInputStream", "LoadTypedefSLdrTweakBall_Movement(SLdrTweakBall_Movement&, CInputStream&)"); }

// LoadTypedefSLdrTweakBall_ScrewAttack(SLdrTweakBall_ScrewAttack&, CInputStream&)
extern "C" void reachstub_223() asm("_Z36LoadTypedefSLdrTweakBall_ScrewAttackR25SLdrTweakBall_ScrewAttackR12CInputStream");
extern "C" void reachstub_223() { mpReachStub("_Z36LoadTypedefSLdrTweakBall_ScrewAttackR25SLdrTweakBall_ScrewAttackR12CInputStream", "LoadTypedefSLdrTweakBall_ScrewAttack(SLdrTweakBall_ScrewAttack&, CInputStream&)"); }

// LoadTypedefSLdrTweakGame_CoinLimitChoices(SLdrTweakGame_CoinLimitChoices&, CInputStream&)
extern "C" void reachstub_224() asm("_Z41LoadTypedefSLdrTweakGame_CoinLimitChoicesR30SLdrTweakGame_CoinLimitChoicesR12CInputStream");
extern "C" void reachstub_224() { mpReachStub("_Z41LoadTypedefSLdrTweakGame_CoinLimitChoicesR30SLdrTweakGame_CoinLimitChoicesR12CInputStream", "LoadTypedefSLdrTweakGame_CoinLimitChoices(SLdrTweakGame_CoinLimitChoices&, CInputStream&)"); }

// LoadTypedefSLdrTweakGame_FragLimitChoices(SLdrTweakGame_FragLimitChoices&, CInputStream&)
extern "C" void reachstub_225() asm("_Z41LoadTypedefSLdrTweakGame_FragLimitChoicesR30SLdrTweakGame_FragLimitChoicesR12CInputStream");
extern "C" void reachstub_225() { mpReachStub("_Z41LoadTypedefSLdrTweakGame_FragLimitChoicesR30SLdrTweakGame_FragLimitChoicesR12CInputStream", "LoadTypedefSLdrTweakGame_FragLimitChoices(SLdrTweakGame_FragLimitChoices&, CInputStream&)"); }

// LoadTypedefSLdrTweakGame_TimeLimitChoices(SLdrTweakGame_TimeLimitChoices&, CInputStream&)
extern "C" void reachstub_226() asm("_Z41LoadTypedefSLdrTweakGame_TimeLimitChoicesR30SLdrTweakGame_TimeLimitChoicesR12CInputStream");
extern "C" void reachstub_226() { mpReachStub("_Z41LoadTypedefSLdrTweakGame_TimeLimitChoicesR30SLdrTweakGame_TimeLimitChoicesR12CInputStream", "LoadTypedefSLdrTweakGame_TimeLimitChoices(SLdrTweakGame_TimeLimitChoices&, CInputStream&)"); }

// LoadTypedefSLdrTweakGuiColors_HUDColorsTypedef(SLdrTweakGuiColors_HUDColorsTypedef&, CInputStream&)
extern "C" void reachstub_227() asm("_Z46LoadTypedefSLdrTweakGuiColors_HUDColorsTypedefR35SLdrTweakGuiColors_HUDColorsTypedefR12CInputStream");
extern "C" void reachstub_227() { mpReachStub("_Z46LoadTypedefSLdrTweakGuiColors_HUDColorsTypedefR35SLdrTweakGuiColors_HUDColorsTypedefR12CInputStream", "LoadTypedefSLdrTweakGuiColors_HUDColorsTypedef(SLdrTweakGuiColors_HUDColorsTypedef&, CInputStream&)"); }

// LoadTypedefSLdrTweakGuiColors_Misc(SLdrTweakGuiColors_Misc&, CInputStream&)
extern "C" void reachstub_228() asm("_Z34LoadTypedefSLdrTweakGuiColors_MiscR23SLdrTweakGuiColors_MiscR12CInputStream");
extern "C" void reachstub_228() { mpReachStub("_Z34LoadTypedefSLdrTweakGuiColors_MiscR23SLdrTweakGuiColors_MiscR12CInputStream", "LoadTypedefSLdrTweakGuiColors_Misc(SLdrTweakGuiColors_Misc&, CInputStream&)"); }

// LoadTypedefSLdrTweakGuiColors_Multiplayer(SLdrTweakGuiColors_Multiplayer&, CInputStream&)
extern "C" void reachstub_229() asm("_Z41LoadTypedefSLdrTweakGuiColors_MultiplayerR30SLdrTweakGuiColors_MultiplayerR12CInputStream");
extern "C" void reachstub_229() { mpReachStub("_Z41LoadTypedefSLdrTweakGuiColors_MultiplayerR30SLdrTweakGuiColors_MultiplayerR12CInputStream", "LoadTypedefSLdrTweakGuiColors_Multiplayer(SLdrTweakGuiColors_Multiplayer&, CInputStream&)"); }

// LoadTypedefSLdrTweakGuiColors_TurretHudTypedef(SLdrTweakGuiColors_TurretHudTypedef&, CInputStream&)
extern "C" void reachstub_230() asm("_Z46LoadTypedefSLdrTweakGuiColors_TurretHudTypedefR35SLdrTweakGuiColors_TurretHudTypedefR12CInputStream");
extern "C" void reachstub_230() { mpReachStub("_Z46LoadTypedefSLdrTweakGuiColors_TurretHudTypedefR35SLdrTweakGuiColors_TurretHudTypedefR12CInputStream", "LoadTypedefSLdrTweakGuiColors_TurretHudTypedef(SLdrTweakGuiColors_TurretHudTypedef&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_Completion(SLdrTweakGui_Completion&, CInputStream&)
extern "C" void reachstub_231() asm("_Z34LoadTypedefSLdrTweakGui_CompletionR23SLdrTweakGui_CompletionR12CInputStream");
extern "C" void reachstub_231() { mpReachStub("_Z34LoadTypedefSLdrTweakGui_CompletionR23SLdrTweakGui_CompletionR12CInputStream", "LoadTypedefSLdrTweakGui_Completion(SLdrTweakGui_Completion&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_Credits(SLdrTweakGui_Credits&, CInputStream&)
extern "C" void reachstub_232() asm("_Z31LoadTypedefSLdrTweakGui_CreditsR20SLdrTweakGui_CreditsR12CInputStream");
extern "C" void reachstub_232() { mpReachStub("_Z31LoadTypedefSLdrTweakGui_CreditsR20SLdrTweakGui_CreditsR12CInputStream", "LoadTypedefSLdrTweakGui_Credits(SLdrTweakGui_Credits&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_DarkWorld(SLdrTweakGui_DarkWorld&, CInputStream&)
extern "C" void reachstub_233() asm("_Z33LoadTypedefSLdrTweakGui_DarkWorldR22SLdrTweakGui_DarkWorldR12CInputStream");
extern "C" void reachstub_233() { mpReachStub("_Z33LoadTypedefSLdrTweakGui_DarkWorldR22SLdrTweakGui_DarkWorldR12CInputStream", "LoadTypedefSLdrTweakGui_DarkWorld(SLdrTweakGui_DarkWorld&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_EchoVisor(SLdrTweakGui_EchoVisor&, CInputStream&)
extern "C" void reachstub_234() asm("_Z33LoadTypedefSLdrTweakGui_EchoVisorR22SLdrTweakGui_EchoVisorR12CInputStream");
extern "C" void reachstub_234() { mpReachStub("_Z33LoadTypedefSLdrTweakGui_EchoVisorR22SLdrTweakGui_EchoVisorR12CInputStream", "LoadTypedefSLdrTweakGui_EchoVisor(SLdrTweakGui_EchoVisor&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_HudColorTypedef(SLdrTweakGui_HudColorTypedef&, CInputStream&)
extern "C" void reachstub_235() asm("_Z39LoadTypedefSLdrTweakGui_HudColorTypedefR28SLdrTweakGui_HudColorTypedefR12CInputStream");
extern "C" void reachstub_235() { mpReachStub("_Z39LoadTypedefSLdrTweakGui_HudColorTypedefR28SLdrTweakGui_HudColorTypedefR12CInputStream", "LoadTypedefSLdrTweakGui_HudColorTypedef(SLdrTweakGui_HudColorTypedef&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_LogBook(SLdrTweakGui_LogBook&, CInputStream&)
extern "C" void reachstub_236() asm("_Z31LoadTypedefSLdrTweakGui_LogBookR20SLdrTweakGui_LogBookR12CInputStream");
extern "C" void reachstub_236() { mpReachStub("_Z31LoadTypedefSLdrTweakGui_LogBookR20SLdrTweakGui_LogBookR12CInputStream", "LoadTypedefSLdrTweakGui_LogBook(SLdrTweakGui_LogBook&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_Misc(SLdrTweakGui_Misc&, CInputStream&)
extern "C" void reachstub_237() asm("_Z28LoadTypedefSLdrTweakGui_MiscR17SLdrTweakGui_MiscR12CInputStream");
extern "C" void reachstub_237() { mpReachStub("_Z28LoadTypedefSLdrTweakGui_MiscR17SLdrTweakGui_MiscR12CInputStream", "LoadTypedefSLdrTweakGui_Misc(SLdrTweakGui_Misc&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_MovieVolumes(SLdrTweakGui_MovieVolumes&, CInputStream&)
extern "C" void reachstub_238() asm("_Z36LoadTypedefSLdrTweakGui_MovieVolumesR25SLdrTweakGui_MovieVolumesR12CInputStream");
extern "C" void reachstub_238() { mpReachStub("_Z36LoadTypedefSLdrTweakGui_MovieVolumesR25SLdrTweakGui_MovieVolumesR12CInputStream", "LoadTypedefSLdrTweakGui_MovieVolumes(SLdrTweakGui_MovieVolumes&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_ScanVisor(SLdrTweakGui_ScanVisor&, CInputStream&)
extern "C" void reachstub_239() asm("_Z33LoadTypedefSLdrTweakGui_ScanVisorR22SLdrTweakGui_ScanVisorR12CInputStream");
extern "C" void reachstub_239() { mpReachStub("_Z33LoadTypedefSLdrTweakGui_ScanVisorR22SLdrTweakGui_ScanVisorR12CInputStream", "LoadTypedefSLdrTweakGui_ScanVisor(SLdrTweakGui_ScanVisor&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_ScannableObjectDownloadTimes(SLdrTweakGui_ScannableObjectDownloadTimes&, CInputStream&)
extern "C" void reachstub_240() asm("_Z52LoadTypedefSLdrTweakGui_ScannableObjectDownloadTimesR41SLdrTweakGui_ScannableObjectDownloadTimesR12CInputStream");
extern "C" void reachstub_240() { mpReachStub("_Z52LoadTypedefSLdrTweakGui_ScannableObjectDownloadTimesR41SLdrTweakGui_ScannableObjectDownloadTimesR12CInputStream", "LoadTypedefSLdrTweakGui_ScannableObjectDownloadTimes(SLdrTweakGui_ScannableObjectDownloadTimes&, CInputStream&)"); }

// LoadTypedefSLdrTweakGui_VisorColorSchemeTypedef(SLdrTweakGui_VisorColorSchemeTypedef&, CInputStream&)
extern "C" void reachstub_241() asm("_Z47LoadTypedefSLdrTweakGui_VisorColorSchemeTypedefR36SLdrTweakGui_VisorColorSchemeTypedefR12CInputStream");
extern "C" void reachstub_241() { mpReachStub("_Z47LoadTypedefSLdrTweakGui_VisorColorSchemeTypedefR36SLdrTweakGui_VisorColorSchemeTypedefR12CInputStream", "LoadTypedefSLdrTweakGui_VisorColorSchemeTypedef(SLdrTweakGui_VisorColorSchemeTypedef&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerControls_Booleans(SLdrTweakPlayerControls_Booleans&, CInputStream&)
extern "C" void reachstub_242() asm("_Z43LoadTypedefSLdrTweakPlayerControls_BooleansR32SLdrTweakPlayerControls_BooleansR12CInputStream");
extern "C" void reachstub_242() { mpReachStub("_Z43LoadTypedefSLdrTweakPlayerControls_BooleansR32SLdrTweakPlayerControls_BooleansR12CInputStream", "LoadTypedefSLdrTweakPlayerControls_Booleans(SLdrTweakPlayerControls_Booleans&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerControls_Controls(SLdrTweakPlayerControls_Controls&, CInputStream&)
extern "C" void reachstub_243() asm("_Z43LoadTypedefSLdrTweakPlayerControls_ControlsR32SLdrTweakPlayerControls_ControlsR12CInputStream");
extern "C" void reachstub_243() { mpReachStub("_Z43LoadTypedefSLdrTweakPlayerControls_ControlsR32SLdrTweakPlayerControls_ControlsR12CInputStream", "LoadTypedefSLdrTweakPlayerControls_Controls(SLdrTweakPlayerControls_Controls&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_Arm_Position(SLdrTweakPlayerGun_Arm_Position&, CInputStream&)
extern "C" void reachstub_244() asm("_Z42LoadTypedefSLdrTweakPlayerGun_Arm_PositionR31SLdrTweakPlayerGun_Arm_PositionR12CInputStream");
extern "C" void reachstub_244() { mpReachStub("_Z42LoadTypedefSLdrTweakPlayerGun_Arm_PositionR31SLdrTweakPlayerGun_Arm_PositionR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_Arm_Position(SLdrTweakPlayerGun_Arm_Position&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_Beam_Combo(SLdrTweakPlayerGun_Beam_Combo&, CInputStream&)
extern "C" void reachstub_245() asm("_Z40LoadTypedefSLdrTweakPlayerGun_Beam_ComboR29SLdrTweakPlayerGun_Beam_ComboR12CInputStream");
extern "C" void reachstub_245() { mpReachStub("_Z40LoadTypedefSLdrTweakPlayerGun_Beam_ComboR29SLdrTweakPlayerGun_Beam_ComboR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_Beam_Combo(SLdrTweakPlayerGun_Beam_Combo&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_Beam_Misc(SLdrTweakPlayerGun_Beam_Misc&, CInputStream&)
extern "C" void reachstub_246() asm("_Z39LoadTypedefSLdrTweakPlayerGun_Beam_MiscR28SLdrTweakPlayerGun_Beam_MiscR12CInputStream");
extern "C" void reachstub_246() { mpReachStub("_Z39LoadTypedefSLdrTweakPlayerGun_Beam_MiscR28SLdrTweakPlayerGun_Beam_MiscR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_Beam_Misc(SLdrTweakPlayerGun_Beam_Misc&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_Holstering(SLdrTweakPlayerGun_Holstering&, CInputStream&)
extern "C" void reachstub_247() asm("_Z40LoadTypedefSLdrTweakPlayerGun_HolsteringR29SLdrTweakPlayerGun_HolsteringR12CInputStream");
extern "C" void reachstub_247() { mpReachStub("_Z40LoadTypedefSLdrTweakPlayerGun_HolsteringR29SLdrTweakPlayerGun_HolsteringR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_Holstering(SLdrTweakPlayerGun_Holstering&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_Misc(SLdrTweakPlayerGun_Misc&, CInputStream&)
extern "C" void reachstub_248() asm("_Z34LoadTypedefSLdrTweakPlayerGun_MiscR23SLdrTweakPlayerGun_MiscR12CInputStream");
extern "C" void reachstub_248() { mpReachStub("_Z34LoadTypedefSLdrTweakPlayerGun_MiscR23SLdrTweakPlayerGun_MiscR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_Misc(SLdrTweakPlayerGun_Misc&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_Position(SLdrTweakPlayerGun_Position&, CInputStream&)
extern "C" void reachstub_249() asm("_Z38LoadTypedefSLdrTweakPlayerGun_PositionR27SLdrTweakPlayerGun_PositionR12CInputStream");
extern "C" void reachstub_249() { mpReachStub("_Z38LoadTypedefSLdrTweakPlayerGun_PositionR27SLdrTweakPlayerGun_PositionR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_Position(SLdrTweakPlayerGun_Position&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerGun_RicochetDamage_Factor(SLdrTweakPlayerGun_RicochetDamage_Factor&, CInputStream&)
extern "C" void reachstub_250() asm("_Z51LoadTypedefSLdrTweakPlayerGun_RicochetDamage_FactorR40SLdrTweakPlayerGun_RicochetDamage_FactorR12CInputStream");
extern "C" void reachstub_250() { mpReachStub("_Z51LoadTypedefSLdrTweakPlayerGun_RicochetDamage_FactorR40SLdrTweakPlayerGun_RicochetDamage_FactorR12CInputStream", "LoadTypedefSLdrTweakPlayerGun_RicochetDamage_Factor(SLdrTweakPlayerGun_RicochetDamage_Factor&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerRes_AutoMapperIcons(SLdrTweakPlayerRes_AutoMapperIcons&, CInputStream&)
extern "C" void reachstub_251() asm("_Z45LoadTypedefSLdrTweakPlayerRes_AutoMapperIconsR34SLdrTweakPlayerRes_AutoMapperIconsR12CInputStream");
extern "C" void reachstub_251() { mpReachStub("_Z45LoadTypedefSLdrTweakPlayerRes_AutoMapperIconsR34SLdrTweakPlayerRes_AutoMapperIconsR12CInputStream", "LoadTypedefSLdrTweakPlayerRes_AutoMapperIcons(SLdrTweakPlayerRes_AutoMapperIcons&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayerRes_MapScreenIcons(SLdrTweakPlayerRes_MapScreenIcons&, CInputStream&)
extern "C" void reachstub_252() asm("_Z44LoadTypedefSLdrTweakPlayerRes_MapScreenIconsR33SLdrTweakPlayerRes_MapScreenIconsR12CInputStream");
extern "C" void reachstub_252() { mpReachStub("_Z44LoadTypedefSLdrTweakPlayerRes_MapScreenIconsR33SLdrTweakPlayerRes_MapScreenIconsR12CInputStream", "LoadTypedefSLdrTweakPlayerRes_MapScreenIcons(SLdrTweakPlayerRes_MapScreenIcons&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_AimStuff(SLdrTweakPlayer_AimStuff&, CInputStream&)
extern "C" void reachstub_253() asm("_Z35LoadTypedefSLdrTweakPlayer_AimStuffR24SLdrTweakPlayer_AimStuffR12CInputStream");
extern "C" void reachstub_253() { mpReachStub("_Z35LoadTypedefSLdrTweakPlayer_AimStuffR24SLdrTweakPlayer_AimStuffR12CInputStream", "LoadTypedefSLdrTweakPlayer_AimStuff(SLdrTweakPlayer_AimStuff&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Collision(SLdrTweakPlayer_Collision&, CInputStream&)
extern "C" void reachstub_254() asm("_Z36LoadTypedefSLdrTweakPlayer_CollisionR25SLdrTweakPlayer_CollisionR12CInputStream");
extern "C" void reachstub_254() { mpReachStub("_Z36LoadTypedefSLdrTweakPlayer_CollisionR25SLdrTweakPlayer_CollisionR12CInputStream", "LoadTypedefSLdrTweakPlayer_Collision(SLdrTweakPlayer_Collision&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_DarkWorld(SLdrTweakPlayer_DarkWorld&, CInputStream&)
extern "C" void reachstub_255() asm("_Z36LoadTypedefSLdrTweakPlayer_DarkWorldR25SLdrTweakPlayer_DarkWorldR12CInputStream");
extern "C" void reachstub_255() { mpReachStub("_Z36LoadTypedefSLdrTweakPlayer_DarkWorldR25SLdrTweakPlayer_DarkWorldR12CInputStream", "LoadTypedefSLdrTweakPlayer_DarkWorld(SLdrTweakPlayer_DarkWorld&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_FirstPersonCamera(SLdrTweakPlayer_FirstPersonCamera&, CInputStream&)
extern "C" void reachstub_256() asm("_Z44LoadTypedefSLdrTweakPlayer_FirstPersonCameraR33SLdrTweakPlayer_FirstPersonCameraR12CInputStream");
extern "C" void reachstub_256() { mpReachStub("_Z44LoadTypedefSLdrTweakPlayer_FirstPersonCameraR33SLdrTweakPlayer_FirstPersonCameraR12CInputStream", "LoadTypedefSLdrTweakPlayer_FirstPersonCamera(SLdrTweakPlayer_FirstPersonCamera&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Frozen(SLdrTweakPlayer_Frozen&, CInputStream&)
extern "C" void reachstub_257() asm("_Z33LoadTypedefSLdrTweakPlayer_FrozenR22SLdrTweakPlayer_FrozenR12CInputStream");
extern "C" void reachstub_257() { mpReachStub("_Z33LoadTypedefSLdrTweakPlayer_FrozenR22SLdrTweakPlayer_FrozenR12CInputStream", "LoadTypedefSLdrTweakPlayer_Frozen(SLdrTweakPlayer_Frozen&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Grapple(SLdrTweakPlayer_Grapple&, CInputStream&)
extern "C" void reachstub_258() asm("_Z34LoadTypedefSLdrTweakPlayer_GrappleR23SLdrTweakPlayer_GrappleR12CInputStream");
extern "C" void reachstub_258() { mpReachStub("_Z34LoadTypedefSLdrTweakPlayer_GrappleR23SLdrTweakPlayer_GrappleR12CInputStream", "LoadTypedefSLdrTweakPlayer_Grapple(SLdrTweakPlayer_Grapple&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_GrappleBeam(SLdrTweakPlayer_GrappleBeam&, CInputStream&)
extern "C" void reachstub_259() asm("_Z38LoadTypedefSLdrTweakPlayer_GrappleBeamR27SLdrTweakPlayer_GrappleBeamR12CInputStream");
extern "C" void reachstub_259() { mpReachStub("_Z38LoadTypedefSLdrTweakPlayer_GrappleBeamR27SLdrTweakPlayer_GrappleBeamR12CInputStream", "LoadTypedefSLdrTweakPlayer_GrappleBeam(SLdrTweakPlayer_GrappleBeam&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Misc(SLdrTweakPlayer_Misc&, CInputStream&)
extern "C" void reachstub_260() asm("_Z31LoadTypedefSLdrTweakPlayer_MiscR20SLdrTweakPlayer_MiscR12CInputStream");
extern "C" void reachstub_260() { mpReachStub("_Z31LoadTypedefSLdrTweakPlayer_MiscR20SLdrTweakPlayer_MiscR12CInputStream", "LoadTypedefSLdrTweakPlayer_Misc(SLdrTweakPlayer_Misc&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Motion(SLdrTweakPlayer_Motion&, CInputStream&)
extern "C" void reachstub_261() asm("_Z33LoadTypedefSLdrTweakPlayer_MotionR22SLdrTweakPlayer_MotionR12CInputStream");
extern "C" void reachstub_261() { mpReachStub("_Z33LoadTypedefSLdrTweakPlayer_MotionR22SLdrTweakPlayer_MotionR12CInputStream", "LoadTypedefSLdrTweakPlayer_Motion(SLdrTweakPlayer_Motion&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Orbit(SLdrTweakPlayer_Orbit&, CInputStream&)
extern "C" void reachstub_262() asm("_Z32LoadTypedefSLdrTweakPlayer_OrbitR21SLdrTweakPlayer_OrbitR12CInputStream");
extern "C" void reachstub_262() { mpReachStub("_Z32LoadTypedefSLdrTweakPlayer_OrbitR21SLdrTweakPlayer_OrbitR12CInputStream", "LoadTypedefSLdrTweakPlayer_Orbit(SLdrTweakPlayer_Orbit&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_ScanVisor(SLdrTweakPlayer_ScanVisor&, CInputStream&)
extern "C" void reachstub_263() asm("_Z36LoadTypedefSLdrTweakPlayer_ScanVisorR25SLdrTweakPlayer_ScanVisorR12CInputStream");
extern "C" void reachstub_263() { mpReachStub("_Z36LoadTypedefSLdrTweakPlayer_ScanVisorR25SLdrTweakPlayer_ScanVisorR12CInputStream", "LoadTypedefSLdrTweakPlayer_ScanVisor(SLdrTweakPlayer_ScanVisor&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_Shield(SLdrTweakPlayer_Shield&, CInputStream&)
extern "C" void reachstub_264() asm("_Z33LoadTypedefSLdrTweakPlayer_ShieldR22SLdrTweakPlayer_ShieldR12CInputStream");
extern "C" void reachstub_264() { mpReachStub("_Z33LoadTypedefSLdrTweakPlayer_ShieldR22SLdrTweakPlayer_ShieldR12CInputStream", "LoadTypedefSLdrTweakPlayer_Shield(SLdrTweakPlayer_Shield&, CInputStream&)"); }

// LoadTypedefSLdrTweakPlayer_SuitDamageReduction(SLdrTweakPlayer_SuitDamageReduction&, CInputStream&)
extern "C" void reachstub_265() asm("_Z46LoadTypedefSLdrTweakPlayer_SuitDamageReductionR35SLdrTweakPlayer_SuitDamageReductionR12CInputStream");
extern "C" void reachstub_265() { mpReachStub("_Z46LoadTypedefSLdrTweakPlayer_SuitDamageReductionR35SLdrTweakPlayer_SuitDamageReductionR12CInputStream", "LoadTypedefSLdrTweakPlayer_SuitDamageReduction(SLdrTweakPlayer_SuitDamageReduction&, CInputStream&)"); }

// LoadTypedefSLdrTweakTargeting_Charge_Gauge(SLdrTweakTargeting_Charge_Gauge&, CInputStream&)
extern "C" void reachstub_266() asm("_Z42LoadTypedefSLdrTweakTargeting_Charge_GaugeR31SLdrTweakTargeting_Charge_GaugeR12CInputStream");
extern "C" void reachstub_266() { mpReachStub("_Z42LoadTypedefSLdrTweakTargeting_Charge_GaugeR31SLdrTweakTargeting_Charge_GaugeR12CInputStream", "LoadTypedefSLdrTweakTargeting_Charge_Gauge(SLdrTweakTargeting_Charge_Gauge&, CInputStream&)"); }

// LoadTypedefSLdrTweakTargeting_LockDagger(SLdrTweakTargeting_LockDagger&, CInputStream&)
extern "C" void reachstub_267() asm("_Z40LoadTypedefSLdrTweakTargeting_LockDaggerR29SLdrTweakTargeting_LockDaggerR12CInputStream");
extern "C" void reachstub_267() { mpReachStub("_Z40LoadTypedefSLdrTweakTargeting_LockDaggerR29SLdrTweakTargeting_LockDaggerR12CInputStream", "LoadTypedefSLdrTweakTargeting_LockDagger(SLdrTweakTargeting_LockDagger&, CInputStream&)"); }

// LoadTypedefSLdrTweakTargeting_LockFire(SLdrTweakTargeting_LockFire&, CInputStream&)
extern "C" void reachstub_268() asm("_Z38LoadTypedefSLdrTweakTargeting_LockFireR27SLdrTweakTargeting_LockFireR12CInputStream");
extern "C" void reachstub_268() { mpReachStub("_Z38LoadTypedefSLdrTweakTargeting_LockFireR27SLdrTweakTargeting_LockFireR12CInputStream", "LoadTypedefSLdrTweakTargeting_LockFire(SLdrTweakTargeting_LockFire&, CInputStream&)"); }

// LoadTypedefSLdrTweakTargeting_OuterBeamIcon(SLdrTweakTargeting_OuterBeamIcon&, CInputStream&)
extern "C" void reachstub_269() asm("_Z43LoadTypedefSLdrTweakTargeting_OuterBeamIconR32SLdrTweakTargeting_OuterBeamIconR12CInputStream");
extern "C" void reachstub_269() { mpReachStub("_Z43LoadTypedefSLdrTweakTargeting_OuterBeamIconR32SLdrTweakTargeting_OuterBeamIconR12CInputStream", "LoadTypedefSLdrTweakTargeting_OuterBeamIcon(SLdrTweakTargeting_OuterBeamIcon&, CInputStream&)"); }

// PortDebug::RequestReset()
extern "C" void reachstub_270() asm("_ZN9PortDebug12RequestResetEv");
extern "C" void reachstub_270() { mpReachStub("_ZN9PortDebug12RequestResetEv", "PortDebug::RequestReset()"); }

// REL_loader_CannonBall
extern "C" void reachstub_271() asm("REL_loader_CannonBall");
extern "C" void reachstub_271() { mpReachStub("REL_loader_CannonBall", "REL_loader_CannonBall"); }

// StreamNewGameState__5CMainFR12CInputStreami
extern "C" void reachstub_272() asm("StreamNewGameState__5CMainFR12CInputStreami");
extern "C" void reachstub_272() { mpReachStub("StreamNewGameState__5CMainFR12CInputStreami", "StreamNewGameState__5CMainFR12CInputStreami"); }

// __nw__FUlPCcPCc
extern "C" void reachstub_273() asm("__nw__FUlPCcPCc");
extern "C" void reachstub_273() { mpReachStub("__nw__FUlPCcPCc", "__nw__FUlPCcPCc"); }

// fn_80020478
extern "C" void reachstub_274() asm("fn_80020478");
extern "C" void reachstub_274() { mpReachStub("fn_80020478", "fn_80020478"); }

// fn_800214A0
extern "C" void reachstub_275() asm("fn_800214A0");
extern "C" void reachstub_275() { mpReachStub("fn_800214A0", "fn_800214A0"); }

// fn_80022C74
extern "C" void reachstub_276() asm("fn_80022C74");
extern "C" void reachstub_276() { mpReachStub("fn_80022C74", "fn_80022C74"); }

// fn_80038624
extern "C" void reachstub_277() asm("fn_80038624");
extern "C" void reachstub_277() { mpReachStub("fn_80038624", "fn_80038624"); }

// fn_80041518(queryOutput&, MapWorldInfoAreas&, unsigned short)
extern "C" void reachstub_278() asm("_Z11fn_80041518R11queryOutputR17MapWorldInfoAreast");
extern "C" void reachstub_278() { mpReachStub("_Z11fn_80041518R11queryOutputR17MapWorldInfoAreast", "fn_80041518(queryOutput&, MapWorldInfoAreas&, unsigned short)"); }

// fn_80048EA4
extern "C" void reachstub_279() asm("fn_80048EA4");
extern "C" void reachstub_279() { mpReachStub("fn_80048EA4", "fn_80048EA4"); }

// fn_8004935C
extern "C" void reachstub_280() asm("fn_8004935C");
extern "C" void reachstub_280() { mpReachStub("fn_8004935C", "fn_8004935C"); }

// fn_80049ED8(CActor*, CStateManager&)
extern "C" void reachstub_281() asm("_Z11fn_80049ED8P6CActorR13CStateManager");
extern "C" void reachstub_281() { mpReachStub("_Z11fn_80049ED8P6CActorR13CStateManager", "fn_80049ED8(CActor*, CStateManager&)"); }

// fn_8004F770
extern "C" void reachstub_282() asm("fn_8004F770");
extern "C" void reachstub_282() { mpReachStub("fn_8004F770", "fn_8004F770"); }

// fn_800CB764
extern "C" void reachstub_283() asm("fn_800CB764");
extern "C" void reachstub_283() { mpReachStub("fn_800CB764", "fn_800CB764"); }

// fn_801423A8
extern "C" void reachstub_284() asm("fn_801423A8");
extern "C" void reachstub_284() { mpReachStub("fn_801423A8", "fn_801423A8"); }

// fn_80143884
extern "C" void reachstub_285() asm("fn_80143884");
extern "C" void reachstub_285() { mpReachStub("fn_80143884", "fn_80143884"); }

// fn_80143E88
extern "C" void reachstub_286() asm("fn_80143E88");
extern "C" void reachstub_286() { mpReachStub("fn_80143E88", "fn_80143E88"); }

// fn_80180598
extern "C" void reachstub_287() asm("fn_80180598");
extern "C" void reachstub_287() { mpReachStub("fn_80180598", "fn_80180598"); }

// fn_80192808
extern "C" void reachstub_288() asm("fn_80192808");
extern "C" void reachstub_288() { mpReachStub("fn_80192808", "fn_80192808"); }

// fn_80193E08
extern "C" void reachstub_289() asm("fn_80193E08");
extern "C" void reachstub_289() { mpReachStub("fn_80193E08", "fn_80193E08"); }

// fn_801C5990
extern "C" void reachstub_290() asm("fn_801C5990");
extern "C" void reachstub_290() { mpReachStub("fn_801C5990", "fn_801C5990"); }

// fn_801CA0F8__10CPlayerGunFv
extern "C" void reachstub_291() asm("fn_801CA0F8__10CPlayerGunFv");
extern "C" void reachstub_291() { mpReachStub("fn_801CA0F8__10CPlayerGunFv", "fn_801CA0F8__10CPlayerGunFv"); }

// fn_801D9F90
extern "C" void reachstub_292() asm("fn_801D9F90");
extern "C" void reachstub_292() { mpReachStub("fn_801D9F90", "fn_801D9F90"); }

// fn_801EBBC8
extern "C" void reachstub_293() asm("fn_801EBBC8");
extern "C" void reachstub_293() { mpReachStub("fn_801EBBC8", "fn_801EBBC8"); }

// fn_801F47F4
extern "C" void reachstub_294() asm("fn_801F47F4");
extern "C" void reachstub_294() { mpReachStub("fn_801F47F4", "fn_801F47F4"); }

// fn_8029AF00
extern "C" void reachstub_295() asm("fn_8029AF00");
extern "C" void reachstub_295() { mpReachStub("fn_8029AF00", "fn_8029AF00"); }

// fn_802BBDB8
extern "C" void reachstub_296() asm("fn_802BBDB8");
extern "C" void reachstub_296() { mpReachStub("fn_802BBDB8", "fn_802BBDB8"); }

// fn_802CB608
extern "C" void reachstub_297() asm("fn_802CB608");
extern "C" void reachstub_297() { mpReachStub("fn_802CB608", "fn_802CB608"); }

// fn_802FA1BC
extern "C" void reachstub_298() asm("fn_802FA1BC");
extern "C" void reachstub_298() { mpReachStub("fn_802FA1BC", "fn_802FA1BC"); }

// fn_802FA7D4
extern "C" void reachstub_299() asm("fn_802FA7D4");
extern "C" void reachstub_299() { mpReachStub("fn_802FA7D4", "fn_802FA7D4"); }

// fn_802FAAE4
extern "C" void reachstub_300() asm("fn_802FAAE4");
extern "C" void reachstub_300() { mpReachStub("fn_802FAAE4", "fn_802FAAE4"); }

// fn_80310F38
extern "C" void reachstub_301() asm("fn_80310F38");
extern "C" void reachstub_301() { mpReachStub("fn_80310F38", "fn_80310F38"); }

// fn_803111A4
extern "C" void reachstub_302() asm("fn_803111A4");
extern "C" void reachstub_302() { mpReachStub("fn_803111A4", "fn_803111A4"); }

// fn_803115F8
extern "C" void reachstub_303() asm("fn_803115F8");
extern "C" void reachstub_303() { mpReachStub("fn_803115F8", "fn_803115F8"); }

// fn_8033CEE8
extern "C" void reachstub_304() asm("fn_8033CEE8");
extern "C" void reachstub_304() { mpReachStub("fn_8033CEE8", "fn_8033CEE8"); }

// mp_coin
extern "C" void reachstub_305() asm("mp_coin");
extern "C" void reachstub_305() { mpReachStub("mp_coin", "mp_coin"); }

// mp_coin_exit
extern "C" void reachstub_306() asm("mp_coin_exit");
extern "C" void reachstub_306() { mpReachStub("mp_coin_exit", "mp_coin_exit"); }

// mp_cswarmbasics
extern "C" void reachstub_307() asm("mp_cswarmbasics");
extern "C" void reachstub_307() { mpReachStub("mp_cswarmbasics", "mp_cswarmbasics"); }

// mp_cswarmbasics_exit
extern "C" void reachstub_308() asm("mp_cswarmbasics_exit");
extern "C" void reachstub_308() { mpReachStub("mp_cswarmbasics_exit", "mp_cswarmbasics_exit"); }

// mp_metaree
extern "C" void reachstub_309() asm("mp_metaree");
extern "C" void reachstub_309() { mpReachStub("mp_metaree", "mp_metaree"); }

// mp_metaree_exit
extern "C" void reachstub_310() asm("mp_metaree_exit");
extern "C" void reachstub_310() { mpReachStub("mp_metaree_exit", "mp_metaree_exit"); }

// mp_playeractorexit
extern "C" void reachstub_311() asm("mp_playeractorexit");
extern "C" void reachstub_311() { mpReachStub("mp_playeractorexit", "mp_playeractorexit"); }

// mp_playeractormain
extern "C" void reachstub_312() asm("mp_playeractormain");
extern "C" void reachstub_312() { mpReachStub("mp_playeractormain", "mp_playeractormain"); }

// mp_playerproxy
extern "C" void reachstub_313() asm("mp_playerproxy");
extern "C" void reachstub_313() { mpReachStub("mp_playerproxy", "mp_playerproxy"); }

// mp_playerproxy_exit
extern "C" void reachstub_314() asm("mp_playerproxy_exit");
extern "C" void reachstub_314() { mpReachStub("mp_playerproxy_exit", "mp_playerproxy_exit"); }

// mp_puffer
extern "C" void reachstub_315() asm("mp_puffer");
extern "C" void reachstub_315() { mpReachStub("mp_puffer", "mp_puffer"); }

// mp_puffer_exit
extern "C" void reachstub_316() asm("mp_puffer_exit");
extern "C" void reachstub_316() { mpReachStub("mp_puffer_exit", "mp_puffer_exit"); }

// mp_riftportal
extern "C" void reachstub_317() asm("mp_riftportal");
extern "C" void reachstub_317() { mpReachStub("mp_riftportal", "mp_riftportal"); }

// mp_riftportal_exit
extern "C" void reachstub_318() asm("mp_riftportal_exit");
extern "C" void reachstub_318() { mpReachStub("mp_riftportal_exit", "mp_riftportal_exit"); }

// mp_rsfaudio
extern "C" void reachstub_319() asm("mp_rsfaudio");
extern "C" void reachstub_319() { mpReachStub("mp_rsfaudio", "mp_rsfaudio"); }

// mp_rsfaudio_exit
extern "C" void reachstub_320() asm("mp_rsfaudio_exit");
extern "C" void reachstub_320() { mpReachStub("mp_rsfaudio_exit", "mp_rsfaudio_exit"); }

// mp_safezone
extern "C" void reachstub_321() asm("mp_safezone");
extern "C" void reachstub_321() { mpReachStub("mp_safezone", "mp_safezone"); }

// mp_safezone_exit
extern "C" void reachstub_322() asm("mp_safezone_exit");
extern "C" void reachstub_322() { mpReachStub("mp_safezone_exit", "mp_safezone_exit"); }

// mp_scriptguisetup
extern "C" void reachstub_323() asm("mp_scriptguisetup");
extern "C" void reachstub_323() { mpReachStub("mp_scriptguisetup", "mp_scriptguisetup"); }

// mp_scriptguisetup_exit
extern "C" void reachstub_324() asm("mp_scriptguisetup_exit");
extern "C" void reachstub_324() { mpReachStub("mp_scriptguisetup_exit", "mp_scriptguisetup_exit"); }

// mp_skyripple
extern "C" void reachstub_325() asm("mp_skyripple");
extern "C" void reachstub_325() { mpReachStub("mp_skyripple", "mp_skyripple"); }

// mp_skyripple_exit
extern "C" void reachstub_326() asm("mp_skyripple_exit");
extern "C" void reachstub_326() { mpReachStub("mp_skyripple_exit", "mp_skyripple_exit"); }

// mp_swarm
extern "C" void reachstub_327() asm("mp_swarm");
extern "C" void reachstub_327() { mpReachStub("mp_swarm", "mp_swarm"); }

// mp_swarm_exit
extern "C" void reachstub_328() asm("mp_swarm_exit");
extern "C" void reachstub_328() { mpReachStub("mp_swarm_exit", "mp_swarm_exit"); }

// mp_wallcrawler
extern "C" void reachstub_329() asm("mp_wallcrawler");
extern "C" void reachstub_329() { mpReachStub("mp_wallcrawler", "mp_wallcrawler"); }

// mp_wallcrawler_exit
extern "C" void reachstub_330() asm("mp_wallcrawler_exit");
extern "C" void reachstub_330() { mpReachStub("mp_wallcrawler_exit", "mp_wallcrawler_exit"); }


// Data objects. A vtable or typeinfo stub is zero-filled: harmless to take the
// address of, and a crash if used - which unreachable means it is not.
//
// The `= {}` is load-bearing. A tentative definition with no initialiser is
// discarded as unused and the symbol never reaches the object file, which
// looks exactly like the stub not working. Measured, not assumed.

// vtable for CSimplePool
extern "C" char reachstub_data_0[64] asm("_ZTV11CSimplePool") = {};
