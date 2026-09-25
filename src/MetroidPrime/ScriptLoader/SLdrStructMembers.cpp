// The 136 `SLdr*` script-loader struct constructors and destructors the port's link gap lists.
//
// ## Why this file is not a `configure.py` unit
//
// `tools/link_gap.py` reports `_ZN17SLdrTweakGui_MiscC1Ev` and friends as MISSING, and it is
// tempting to read that as "retail has these functions and we have not written them". Retail does
// **not** define those names. Not one of the 136 appears in `config/G2ME01/symbols.txt` or in any
// `config/G2ME01/rels/*/symbols.txt`. What retail defines is MWCC's *implicit* constructor and
// destructor helper pair for the same classes, under different names entirely:
//
//     port (GCC, Itanium ABI)   _ZN17SLdrTweakGui_MiscC1Ev   _ZN17SLdrTweakGui_MiscD1Ev
//     retail (MWCC)             __ct__17SLdrTweakGui_MiscFv   __dt__17SLdrTweakGui_MiscFv
//
// Of the 68 classes, 56 have their retail bodies in the `Tweaks` REL module and 5 in the DOL, and
// `build/G2ME01/Tweaks/obj/auto_00_00006EC4_text.o` is where you read them (that object's addresses
// are the module's minus 0x6EC4). Both the port and the decompilation build therefore need *a*
// default constructor and destructor per class, under whatever name their own compiler mangles -
// but only one of the two names exists in the retail binary. So there is no retail range for a
// `Matching` unit to claim, no `splits.txt` block to write, and no `Object(Matching, ...)` line to
// add: this file is deliberately **absent from `configure.py`**, the same way
// `src/MetroidPrime/PortGlobals.cpp` is. It is listed in `files.cmake`, so the port's `mp_game`
// compiles it and the link gap moves.
//
// Defining any of this inside a decompilation unit instead would be worse than useless twice over:
// the definitions would be emitted under `__ct__`/`__dt__` by MWCC, colliding with the `Tweaks`
// module's own copies, and the host build would still see the gap.
//
// ## What the bodies are, and what they are not
//
// The measurements are in `docs/research/sldr_ctors.md`. The short version: **these are not
// trivial.** Of the 68 constructors, 0 are no-ops - 14 store immediate defaults, 16 copy defaults
// out of `.rodata`, and 30 both construct members (`CColor::Green`, `__ct__6CColorFffff`,
// `__ct__10SLdrSplineFv`, `__ct__15SLdrTDamageInfoFv`, ...) and store defaults. Of the 68
// destructors, 40 are the same byte-identical 60-byte MWCC delete-flag stub, 20 are
// class-specific, and 8 have no retail counterpart at all.
//
// An empty body here is C++'s own default construction and destruction, which is the *member* half
// of what retail does, and it is exactly right for the three classes retail constructs trivially or
// not at all. What it is **not** is retail's second half: the default *tweak values*. Those live
// in immediates and in a per-module `.rodata` table, and all 60 constructors that have a retail
// counterpart store them. Four are reproduced verbatim below because they are immediates and their
// member order is unambiguous; the other 64 are listed per class in
// `docs/research/sldr_ctors.md` and are not invented here. In the shipped game that half is dead
// anyway - `LoadTypedefSLdrTweak*` overwrites every field from the config stream, and so does
// `Tweaks.cpp` - but a field the loader does not read would be left uninitialised where retail
// leaves it at its default, and that is the honest limit of this file.
//
// Members are initialised in declaration order, which is what the retail stores are, so the four
// faithful bodies below are checked against the member lists in the generated headers: 1 store for
// `SLdrScannableParameters`' single member, 8 for `SLdrTweakGui_MovieVolumes`, 5 for each of the
// two `SLdrTweakGame_*LimitChoices` structs. The other bodies are `{}`; the four that are not
// carry the retail address and size in a comment, so the difference between "reproduced" and
// "left to the loader" is visible per class.

#include "MetroidPrime/ScriptLoader/SLdrAreaAttributes.hpp"

#include "MetroidPrime/ScriptLoader/SLdrPickup.hpp"
#include "MetroidPrime/ScriptLoader/SLdrSequenceTimer.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakAutoMapper.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakBall.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakGame.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakGui.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakGuiColors.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayerGun.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayerRes.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakTargeting.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrCameraShakerData.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrDamageInfo.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEchoParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrLightParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrPlayerItem.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrScannableParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrTDamageInfo.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrVisorParameters.hpp"

#include "Kyoto/Math/CMayaSpline.hpp"

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp - SLdrTransform ---
// A member of SLdrEditorProperties, so it is only referenced once the parent above is defined.
// Three CVector3f, no defaults in any retail body that names it.
SLdrTransform::SLdrTransform() {}
SLdrTransform::~SLdrTransform() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrLightParameters.hpp ---
SLdrLightParameters::SLdrLightParameters() {}
SLdrLightParameters::~SLdrLightParameters() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrVisorParameters.hpp ---
SLdrVisorParameters::SLdrVisorParameters() {}
SLdrVisorParameters::~SLdrVisorParameters() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp ---
// Members SLdrLightParameters and SLdrVisorParameters, so it needs both of the above.
SLdrActorParameters::SLdrActorParameters() {}
SLdrActorParameters::~SLdrActorParameters() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrDamageInfo.hpp - 2 classes ---
SLdrWeaponType::SLdrWeaponType() {}
SLdrWeaponType::~SLdrWeaponType() {}

SLdrDamageInfo::SLdrDamageInfo() {}
SLdrDamageInfo::~SLdrDamageInfo() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrPlayerItem.hpp ---
SLdrPlayerItem::SLdrPlayerItem() {}
SLdrPlayerItem::~SLdrPlayerItem() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrEchoParameters.hpp ---
SLdrEchoParameters::SLdrEchoParameters() {}
SLdrEchoParameters::~SLdrEchoParameters() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp - 13 classes ---
// Tweaks __ct__/__dt__ sizes 44..636 bytes; all 13 store default values, none is a no-op.
SLdrTweakPlayer_AimStuff::SLdrTweakPlayer_AimStuff() {}
SLdrTweakPlayer_AimStuff::~SLdrTweakPlayer_AimStuff() {}

SLdrTweakPlayer_Collision::SLdrTweakPlayer_Collision() {}
SLdrTweakPlayer_Collision::~SLdrTweakPlayer_Collision() {}

SLdrTweakPlayer_DarkWorld::SLdrTweakPlayer_DarkWorld() {}
SLdrTweakPlayer_DarkWorld::~SLdrTweakPlayer_DarkWorld() {}

SLdrTweakPlayer_FirstPersonCamera::SLdrTweakPlayer_FirstPersonCamera() {}
SLdrTweakPlayer_FirstPersonCamera::~SLdrTweakPlayer_FirstPersonCamera() {}

SLdrTweakPlayer_Frozen::SLdrTweakPlayer_Frozen() {}
SLdrTweakPlayer_Frozen::~SLdrTweakPlayer_Frozen() {}

SLdrTweakPlayer_Grapple::SLdrTweakPlayer_Grapple() {}
SLdrTweakPlayer_Grapple::~SLdrTweakPlayer_Grapple() {}

SLdrTweakPlayer_GrappleBeam::SLdrTweakPlayer_GrappleBeam() {}
SLdrTweakPlayer_GrappleBeam::~SLdrTweakPlayer_GrappleBeam() {}

SLdrTweakPlayer_Misc::SLdrTweakPlayer_Misc() {}
SLdrTweakPlayer_Misc::~SLdrTweakPlayer_Misc() {}

SLdrTweakPlayer_Motion::SLdrTweakPlayer_Motion() {}
SLdrTweakPlayer_Motion::~SLdrTweakPlayer_Motion() {}

SLdrTweakPlayer_Orbit::SLdrTweakPlayer_Orbit() {}
SLdrTweakPlayer_Orbit::~SLdrTweakPlayer_Orbit() {}

SLdrTweakPlayer_ScanVisor::SLdrTweakPlayer_ScanVisor() {}
SLdrTweakPlayer_ScanVisor::~SLdrTweakPlayer_ScanVisor() {}

SLdrTweakPlayer_Shield::SLdrTweakPlayer_Shield() {}
SLdrTweakPlayer_Shield::~SLdrTweakPlayer_Shield() {}

SLdrTweakPlayer_SuitDamageReduction::SLdrTweakPlayer_SuitDamageReduction() {}
SLdrTweakPlayer_SuitDamageReduction::~SLdrTweakPlayer_SuitDamageReduction() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakPlayerGun.hpp - 8 classes ---
SLdrTBeamInfo::SLdrTBeamInfo() {}
SLdrTBeamInfo::~SLdrTBeamInfo() {}

SLdrTweakPlayerGun_Arm_Position::SLdrTweakPlayerGun_Arm_Position() {}
SLdrTweakPlayerGun_Arm_Position::~SLdrTweakPlayerGun_Arm_Position() {}

SLdrTweakPlayerGun_Beam_Combo::SLdrTweakPlayerGun_Beam_Combo() {}
SLdrTweakPlayerGun_Beam_Combo::~SLdrTweakPlayerGun_Beam_Combo() {}

SLdrTweakPlayerGun_Beam_Misc::SLdrTweakPlayerGun_Beam_Misc() {}
SLdrTweakPlayerGun_Beam_Misc::~SLdrTweakPlayerGun_Beam_Misc() {}

SLdrTweakPlayerGun_Holstering::SLdrTweakPlayerGun_Holstering() {}
SLdrTweakPlayerGun_Holstering::~SLdrTweakPlayerGun_Holstering() {}

SLdrTweakPlayerGun_Misc::SLdrTweakPlayerGun_Misc() {}
SLdrTweakPlayerGun_Misc::~SLdrTweakPlayerGun_Misc() {}

SLdrTweakPlayerGun_Position::SLdrTweakPlayerGun_Position() {}
SLdrTweakPlayerGun_Position::~SLdrTweakPlayerGun_Position() {}

SLdrTweakPlayerGun_RicochetDamage_Factor::SLdrTweakPlayerGun_RicochetDamage_Factor() {}
SLdrTweakPlayerGun_RicochetDamage_Factor::~SLdrTweakPlayerGun_RicochetDamage_Factor() {}

SLdrTWeaponDamage::SLdrTWeaponDamage() {}
SLdrTWeaponDamage::~SLdrTWeaponDamage() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakGui.hpp - 9 classes ---
SLdrTweakGui_Completion::SLdrTweakGui_Completion() {}
SLdrTweakGui_Completion::~SLdrTweakGui_Completion() {}

SLdrTweakGui_Credits::SLdrTweakGui_Credits() {}
SLdrTweakGui_Credits::~SLdrTweakGui_Credits() {}

SLdrTweakGui_DarkWorld::SLdrTweakGui_DarkWorld() {}
SLdrTweakGui_DarkWorld::~SLdrTweakGui_DarkWorld() {}

SLdrTweakGui_EchoVisor::SLdrTweakGui_EchoVisor() {}
SLdrTweakGui_EchoVisor::~SLdrTweakGui_EchoVisor() {}

SLdrTweakGui_LogBook::SLdrTweakGui_LogBook() {}
SLdrTweakGui_LogBook::~SLdrTweakGui_LogBook() {}

SLdrTweakGui_Misc::SLdrTweakGui_Misc() {}
SLdrTweakGui_Misc::~SLdrTweakGui_Misc() {}

SLdrTweakGui_ScanVisor::SLdrTweakGui_ScanVisor() {}
SLdrTweakGui_ScanVisor::~SLdrTweakGui_ScanVisor() {}

SLdrTweakGui_ScannableObjectDownloadTimes::SLdrTweakGui_ScannableObjectDownloadTimes() {}
SLdrTweakGui_ScannableObjectDownloadTimes::~SLdrTweakGui_ScannableObjectDownloadTimes() {}

// Reproduced from Tweaks __ct__25SLdrTweakGui_MovieVolumesFv (0x452C, 40 bytes):
// eight `li r0,127` / `stw r0,N(r3)` pairs, offsets 0..0x1C, and no call. The header declares
// exactly eight ints, so the member order is unambiguous.
SLdrTweakGui_MovieVolumes::SLdrTweakGui_MovieVolumes() {
  unknown_0xae149646 = 127;
  unknown_0xc1a2e858 = 127;
  unknown_0x138c3bb8 = 127;
  unknown_0xe5587648 = 127;
  unknown_0x9ed00248 = 127;
  unknown_0x6f135424 = 127;
  unknown_0xdb2260b7 = 127;
  unknown_0xf38093f5 = 127;
}
SLdrTweakGui_MovieVolumes::~SLdrTweakGui_MovieVolumes() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakGuiColors.hpp - 6 classes ---
SLdrTweakGuiColors_HUDColorsTypedef::SLdrTweakGuiColors_HUDColorsTypedef() {}
SLdrTweakGuiColors_HUDColorsTypedef::~SLdrTweakGuiColors_HUDColorsTypedef() {}

SLdrTweakGuiColors_Misc::SLdrTweakGuiColors_Misc() {}
SLdrTweakGuiColors_Misc::~SLdrTweakGuiColors_Misc() {}

SLdrTweakGuiColors_Multiplayer::SLdrTweakGuiColors_Multiplayer() {}
SLdrTweakGuiColors_Multiplayer::~SLdrTweakGuiColors_Multiplayer() {}

SLdrTweakGuiColors_TurretHudTypedef::SLdrTweakGuiColors_TurretHudTypedef() {}
SLdrTweakGuiColors_TurretHudTypedef::~SLdrTweakGuiColors_TurretHudTypedef() {}

SLdrTweakGui_HudColorTypedef::SLdrTweakGui_HudColorTypedef() {}
SLdrTweakGui_HudColorTypedef::~SLdrTweakGui_HudColorTypedef() {}

SLdrTweakGui_VisorColorSchemeTypedef::SLdrTweakGui_VisorColorSchemeTypedef() {}
SLdrTweakGui_VisorColorSchemeTypedef::~SLdrTweakGui_VisorColorSchemeTypedef() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakBall.hpp - 7 classes ---
SLdrTweakBall_BoostBall::SLdrTweakBall_BoostBall() {}
SLdrTweakBall_BoostBall::~SLdrTweakBall_BoostBall() {}

SLdrTweakBall_Camera::SLdrTweakBall_Camera() {}
SLdrTweakBall_Camera::~SLdrTweakBall_Camera() {}

SLdrTweakBall_CannonBall::SLdrTweakBall_CannonBall() {}
SLdrTweakBall_CannonBall::~SLdrTweakBall_CannonBall() {}

SLdrTweakBall_DeathBall::SLdrTweakBall_DeathBall() {}
SLdrTweakBall_DeathBall::~SLdrTweakBall_DeathBall() {}

SLdrTweakBall_Misc::SLdrTweakBall_Misc() {}
SLdrTweakBall_Misc::~SLdrTweakBall_Misc() {}

SLdrTweakBall_Movement::SLdrTweakBall_Movement() {}
SLdrTweakBall_Movement::~SLdrTweakBall_Movement() {}

SLdrTweakBall_ScrewAttack::SLdrTweakBall_ScrewAttack() {}
SLdrTweakBall_ScrewAttack::~SLdrTweakBall_ScrewAttack() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakTargeting.hpp - 5 classes ---
SLdrTIcon_Configurations::SLdrTIcon_Configurations() {}
SLdrTIcon_Configurations::~SLdrTIcon_Configurations() {}

SLdrTweakTargeting_Charge_Gauge::SLdrTweakTargeting_Charge_Gauge() {}
SLdrTweakTargeting_Charge_Gauge::~SLdrTweakTargeting_Charge_Gauge() {}

SLdrTweakTargeting_LockDagger::SLdrTweakTargeting_LockDagger() {}
SLdrTweakTargeting_LockDagger::~SLdrTweakTargeting_LockDagger() {}

SLdrTweakTargeting_LockFire::SLdrTweakTargeting_LockFire() {}
SLdrTweakTargeting_LockFire::~SLdrTweakTargeting_LockFire() {}

SLdrTweakTargeting_OuterBeamIcon::SLdrTweakTargeting_OuterBeamIcon() {}
SLdrTweakTargeting_OuterBeamIcon::~SLdrTweakTargeting_OuterBeamIcon() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakPlayerRes.hpp - 4 classes ---
SLdrTBallTransitionResources::SLdrTBallTransitionResources() {}
SLdrTBallTransitionResources::~SLdrTBallTransitionResources() {}

SLdrTGunResources::SLdrTGunResources() {}
SLdrTGunResources::~SLdrTGunResources() {}

SLdrTweakPlayerRes_AutoMapperIcons::SLdrTweakPlayerRes_AutoMapperIcons() {}
SLdrTweakPlayerRes_AutoMapperIcons::~SLdrTweakPlayerRes_AutoMapperIcons() {}

SLdrTweakPlayerRes_MapScreenIcons::SLdrTweakPlayerRes_MapScreenIcons() {}
SLdrTweakPlayerRes_MapScreenIcons::~SLdrTweakPlayerRes_MapScreenIcons() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakGame.hpp - 3 classes ---
// Both *LimitChoices constructors are immediate-only and their member order is unambiguous:
// five ints, contiguous from offset 0.
SLdrTweakGame_CoinLimitChoices::SLdrTweakGame_CoinLimitChoices() {
  coinLimit0 = 200;
  coinLimit1 = 400;
  coinLimit2 = 600;
  coinLimit3 = 800;
  coinLimit4 = 1000;
}
SLdrTweakGame_CoinLimitChoices::~SLdrTweakGame_CoinLimitChoices() {}

SLdrTweakGame_FragLimitChoices::SLdrTweakGame_FragLimitChoices() {
  fragLimit0 = 0;
  fragLimit1 = 5;
  fragLimit2 = 10;
  fragLimit3 = 15;
  fragLimit4 = 20;
}
SLdrTweakGame_FragLimitChoices::~SLdrTweakGame_FragLimitChoices() {}

SLdrTweakGame_TimeLimitChoices::SLdrTweakGame_TimeLimitChoices() {}
SLdrTweakGame_TimeLimitChoices::~SLdrTweakGame_TimeLimitChoices() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.hpp - 2 classes ---
// Retail names these SLdrTweakPlayerControls_UnknownStruct1 and _UnknownStruct2.
SLdrTweakPlayerControls_Booleans::SLdrTweakPlayerControls_Booleans() {}
SLdrTweakPlayerControls_Booleans::~SLdrTweakPlayerControls_Booleans() {}

SLdrTweakPlayerControls_Controls::SLdrTweakPlayerControls_Controls() {}
SLdrTweakPlayerControls_Controls::~SLdrTweakPlayerControls_Controls() {}

SLdrControllerMapping::SLdrControllerMapping() {}
SLdrControllerMapping::~SLdrControllerMapping() {}

// --- include/MetroidPrime/ScriptLoader/SLdrTweakAutoMapper.hpp - 2 classes ---
SLdrTweakAutoMapper_Base::SLdrTweakAutoMapper_Base() {}
SLdrTweakAutoMapper_Base::~SLdrTweakAutoMapper_Base() {}

SLdrTweakAutoMapper_DoorColors::SLdrTweakAutoMapper_DoorColors() {}
SLdrTweakAutoMapper_DoorColors::~SLdrTweakAutoMapper_DoorColors() {}

// --- include/MetroidPrime/ScriptLoader/SLdrSequenceTimer.hpp - 2 classes ---
// Retail constructs neither out of line; SLdrSequenceTimer has a 100-byte __dt__ and no __ct__.
SLdrConnection::SLdrConnection() {}
SLdrConnection::~SLdrConnection() {}

SLdrSequenceTimer::SLdrSequenceTimer() {}
SLdrSequenceTimer::~SLdrSequenceTimer() {}

// --- include/MetroidPrime/ScriptLoader/SLdrPickup.hpp ---
// Retail has a 328-byte __ct__10SLdrPickupFv and no __dt__ at all.
SLdrPickup::SLdrPickup() {}
SLdrPickup::~SLdrPickup() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrTDamageInfo.hpp ---
SLdrTDamageInfo::SLdrTDamageInfo() {}
SLdrTDamageInfo::~SLdrTDamageInfo() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrCameraShakerData.hpp ---
SLdrCameraShakerData::SLdrCameraShakerData() {}
SLdrCameraShakerData::~SLdrCameraShakerData() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp ---
SLdrEditorProperties::SLdrEditorProperties() {}
SLdrEditorProperties::~SLdrEditorProperties() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrScannableParameters.hpp ---
// Reproduced from DOL __ct__23SLdrScannableParametersFv (0x8024156C, 12 bytes):
// `li r0,-1` / `stw r0,0(r3)` / `blr`. The header's single member is a CAssetId (a uint), and
// 0xFFFFFFFF is kInvalidAssetId.
SLdrScannableParameters::SLdrScannableParameters() { scannableInfo0 = kInvalidAssetId; }
SLdrScannableParameters::~SLdrScannableParameters() {}

// --- include/MetroidPrime/ScriptLoader/SLdrAreaAttributes.hpp ---
// Retail has no SLdrAreaAttributes type at all; its area attributes are loaded by
// LoadAreaAttributes(CStateManager&, CInputStream&, const CEntityInfo&). See the note in
// docs/research/sldr_ctors.md - the type is the loader generator's, and its layout is unverified.
// Its constructor and destructor are deliberately NOT defined here: retail's loader at 0x8013C2E4
// calls __ct__20SLdrEditorPropertiesFv on the member in place and __dt__20SLdrEditorPropertiesFv
// on the way out, so the aggregate's pair must stay implicit. Measured: declaring them added a
// call to a symbol retail does not define and moved the local 4 bytes down the frame.

// --- include/Kyoto/Math/CMayaSpline.hpp - SLdrSpline ---
// Retail defines __ct__10SLdrSplineFv (64 bytes) but neither __dt__10SLdrSplineFv nor an
// __as__ for the type, so the destructor and the copy assignment below have no retail body to
// match. SLdrSpline::SLdrSpline() is already defined in src/Kyoto/Math/CMayaSpline.cpp, which is
// a Matching unit, so it is deliberately not repeated here.
SLdrSpline::~SLdrSpline() {}

SLdrSpline& SLdrSpline::operator=(const SLdrSpline& other) {
  m_preInfinity = other.m_preInfinity;
  m_postInfinity = other.m_postInfinity;
  m_knots = other.m_knots;
  m_clampMode = other.m_clampMode;
  m_minAmplitudeTime = other.m_minAmplitudeTime;
  m_maxAmplitudeTime = other.m_maxAmplitudeTime;
  m_cachedKnotIndex = other.m_cachedKnotIndex;
  x28_cachedSegmentIndex = other.x28_cachedSegmentIndex;
  m_dirty = other.m_dirty;
  m_cachedMinTime = other.m_cachedMinTime;
  for (int i = 0; i < 4; i++) {
    m_cachedHermitCoefs[i] = other.m_cachedHermitCoefs[i];
  }
  return *this;
}
