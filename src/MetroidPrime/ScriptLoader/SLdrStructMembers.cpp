// The `SLdr*` script-loader struct constructors and destructors the port's link gap lists.
//
// **Superseded in part (2026-09-29, upstream sync a14f961):** every `SLdrTweak*`/`SLdrT*` pair
// that used to be here - 63 classes, including the three faithful Tweak bodies the notes below
// describe - is now defined by upstream's generated `ScriptLoader/Tweaks.cpp`, and was removed
// from this file to keep the host link free of duplicates. `SLdrWeaponType` and
// `SLdrControllerMapping` no longer exist upstream. What the notes below say about the Tweak
// classes is history; the rest still holds.
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

#include "MetroidPrime/ScriptLoader/SLdrCannonBall.hpp"
#include "MetroidPrime/ScriptLoader/SLdrHUDMemo.hpp"
#include "MetroidPrime/ScriptLoader/SLdrRelay.hpp"
#include "MetroidPrime/ScriptLoader/SLdrStreamedAudio.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTimeKeyframe.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrIngPossessionData.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrDamageVulnerability.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrHealthInfo.hpp"
#include "MetroidPrime/ScriptLoader/SLdrPickup.hpp"
#include "MetroidPrime/ScriptLoader/SLdrSequenceTimer.hpp"
#include "MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrActorParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrCameraShakerData.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrDamageInfo.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEchoParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrLightParameters.hpp"
#include "MetroidPrime/ScriptLoader/Structs/SLdrScannableParameters.hpp"
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

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrDamageInfo.hpp ---

SLdrDamageInfo::SLdrDamageInfo() {}
SLdrDamageInfo::~SLdrDamageInfo() {}

// --- include/MetroidPrime/ScriptLoader/Structs/SLdrEchoParameters.hpp ---
SLdrEchoParameters::SLdrEchoParameters() {}
SLdrEchoParameters::~SLdrEchoParameters() {}

// --- include/MetroidPrime/ScriptLoader/SLdrSequenceTimer.hpp - 2 classes ---
// Retail constructs neither out of line; SLdrSequenceTimer has a 100-byte __dt__ and no __ct__.
SLdrConnection::SLdrConnection() {}
SLdrConnection::~SLdrConnection() {}

SLdrSequenceTimer::SLdrSequenceTimer() {}
SLdrSequenceTimer::~SLdrSequenceTimer() {}

// --- Loader structs upstream's ScriptObjects TUs construct (2026-09-28 merge) ---
// Retail defines none of these names out of line (no __ct__/__dt__ in any symbols.txt); upstream's
// CScriptAreaProperties/HUDMemo/Relay/StreamedMusic/TimeKeyframe loaders construct one on the stack.
SLdrAreaAttributes::SLdrAreaAttributes() {}
SLdrAreaAttributes::~SLdrAreaAttributes() {}

SLdrHUDMemo::SLdrHUDMemo() {}
SLdrHUDMemo::~SLdrHUDMemo() {}

SLdrRelay::SLdrRelay() {}
SLdrRelay::~SLdrRelay() {}

SLdrStreamedAudio::SLdrStreamedAudio() {}
SLdrStreamedAudio::~SLdrStreamedAudio() {}

SLdrTimeKeyframe::SLdrTimeKeyframe() {}
SLdrTimeKeyframe::~SLdrTimeKeyframe() {}

SLdrIngPossessionData::SLdrIngPossessionData() {}
SLdrIngPossessionData::~SLdrIngPossessionData() {}

// Upstream's CPatterned/CPlayer paths build a CHealthInfo and a CDamageVulnerability from these;
// retail has no out-of-line pair for any of the three either.
SLdrHealthInfo::SLdrHealthInfo() {}
SLdrHealthInfo::~SLdrHealthInfo() {}

SLdrWeaponVulnerability::SLdrWeaponVulnerability() {}
SLdrWeaponVulnerability::~SLdrWeaponVulnerability() {}

SLdrDamageVulnerability::SLdrDamageVulnerability() {}
SLdrDamageVulnerability::~SLdrDamageVulnerability() {}

// --- include/MetroidPrime/ScriptLoader/SLdrCannonBall.hpp ---
// Only the constructor lives here: retail has no __ct__14SLdrCannonBallFv, but it does have the
// destructor, in the ScriptCannonBall REL, so CScriptCannonBall.cpp defines that one.
SLdrCannonBall::SLdrCannonBall() {}

// --- include/MetroidPrime/ScriptLoader/SLdrPickup.hpp ---
// Retail has a 328-byte __ct__10SLdrPickupFv and no __dt__ at all.
SLdrPickup::SLdrPickup() {}
SLdrPickup::~SLdrPickup() {}

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
// Nothing to define: SLdrSpline::SLdrSpline() is in src/Kyoto/Math/CMayaSpline.cpp (Matching),
// and upstream's header defines ~SLdrSpline() inline and leaves the copy assignment implicit -
// retail has neither __dt__10SLdrSplineFv nor an __as__ for the type.
