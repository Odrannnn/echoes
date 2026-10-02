#include "MetroidPrime/ScriptObjects/CScriptSound.hpp"

#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/CWorld.hpp"
#include "Collision/CMaterialFilter.hpp"
#include "Collision/CRayCastResult.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Math/CQuad.hpp"

bool CScriptSound::sFirstInFrame;

// Defined at the **end** of this file - see the comment there for why (mwcceppc emits definitions
// in reverse source order, and 0x8009DCE8 is this unit's lowest retail offset).
extern "C" short fn_8009DCE8(short volume);

// The three functions at the foot of this file - `fn_8009E0EC`, `fn_8009DD94`, `fn_8009DCE8` -
// are declared there, not here: mwcceppc emits definitions in **reverse** source order, so this
// unit's three lowest retail offsets (0x8009DCE8, 0x8009DD94, 0x8009E0EC) have to be the last
// three definitions in the file, written here in the order `fn_8009E0EC`, `fn_8009DD94`,
// `fn_8009DCE8`, to land in `.text` in the order `fn_8009DCE8`, `fn_8009DD94`, `fn_8009E0EC`
// (`tools/check_decl_order.py`, and the "Declare in reverse" rule in
// `docs/RUNNING_THE_DECOMP.md`). With objdiff at 100% for all three a permutation would be
// invisible until the flip, and would break the module hash on a few bytes.


CScriptSound::CScriptSound(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                           const CTransform4f& xf, ushort soundId, float maxDist, float distComp,
                           float startDelay, short minVolume, short volume, short unknown198,
                           short darkVisorVolume, short priority, short pan, short surroundPan,
                           short unknown1a2, bool looped, bool nonEmitter, bool playerRelativePan,
                           bool autoStart, bool occlusionTest, bool acoustics, bool worldSfx,
                           bool allowDuplicates, bool allAreas, bool scaleByMusicVolume, int pitch)
: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(kMT_Trigger), CActorParameters::None(),
         kInvalidUniqueId)
, mOcclusionUpdateTimer(0.f)
, mSfxHandle()
, mMaxVolume(0)
, mCurrentMaxVolume(0)
, mVolumeDelta(0)
, mEmitterPosition(xf.GetTranslation())
, mStartDelay(startDelay)
, mSoundId(soundId)
, mMaxDistance(maxDist)
, mDistanceCompensation(distComp)
, mMinVolume(minVolume)
, mVolume(volume)
, x198_(unknown198)
, mDarkVisorVolume(darkVisorVolume)
, mPriority(priority)
, mPan(pan)
, mSurroundPan(surroundPan)
, x1a2_(unknown1a2)
, mPitch(pitch + 8192)
, mPlayRequested(false)
, mLooped(looped)
, mNonEmitter(nonEmitter)
, mAutoStart(autoStart)
, mOcclusionTest(occlusionTest)
, mAcoustics(acoustics)
, mWorldSfx(worldSfx)
, mSelfFree(false)
, mAllowDuplicates(allowDuplicates)
, mProcessedThisFrame(false)
, mPlayerRelativePan(playerRelativePan)
, x1a9_27_(false)
, mAllAreas(allAreas)
, mScaleByMusicVolume(scaleByMusicVolume) {
  if (mWorldSfx && !mNonEmitter) {
    mWorldSfx = false;
  }
}

void CScriptSound::PreThink(float dt, CStateManager& mgr) {
  CEntity::PreThink(dt, mgr);
  sFirstInFrame = true;
  mProcessedThisFrame = false;
}

CScriptSound::~CScriptSound() {}

void CScriptSound::Think(float dt, CStateManager& mgr) {
  // TODO: lifetime, moving emitters, throttled occlusion, pitch and visor-volume interpolation.
}

// Retail 0x8009EB48, 0xC8 = 200 bytes. Prime 1 has no `SetMaxVolume` at all (`prime-ref`'s
// `CScriptSound` is Matching and stops at `StopSound`), so this is Echoes-only and is read
// straight off the instruction:
//
//   sth   r4,406(r3)              mVolume = volume
//   lwz   r0,364(r3) / cmplwi    mSfxHandle
//   ...   bit 26 (mNonEmitter) and bit 29 (mScaleByMusicVolume) of the two bool bytes ...
//   bl    fn_8009DCE8             scale by the music-volume spline
//   bl    SfxVolume               non-emitter: just move the volume
//   bl    UpdateEmitter           emitter: move position and volume together
//
// The two arms are what make it: a non-emitter has no position to move, so only its volume
// changes, while an emitter's `UpdateEmitter` re-seats both at once. `mMaxVolume`/`mCurrentMaxVolume`
// are written in the *emitter* arm only, and in the order the two `lha`/`sth` pairs at 0xED8-0xEE4
// give - `mCurrentMaxVolume` first, then `mMaxVolume` from it, i.e. `mMaxVolume = mCurrentMaxVolume`
// written as a chained assignment, which is why the second store reloads `+0x172`.
void CScriptSound::SetMaxVolume(short volume) {
  mVolume = volume;
  if (!mSfxHandle) {
    return;
  }
  if (mNonEmitter) {
    const short scaled = mScaleByMusicVolume ? fn_8009DCE8(mVolume) : mVolume;
    CSfxManager::SfxVolume(mSfxHandle, (uchar)scaled);
  } else {
    const CVector3f pos = GetTranslation();
    mCurrentMaxVolume = mVolume;
    mMaxVolume = mCurrentMaxVolume;
    const uchar emitterVolume = (uchar)mVolume;
    CSfxManager::UpdateEmitter(mSfxHandle, pos, CVector3f::Zero(), emitterVolume);
  }
}

// Retail 0x8009EB08, 0x8009EB18 and 0x8009EB24, 0x10 + 0xC + 0xC bytes: three accessors on
// `CScriptSound`'s tail, spelled here with the retail symbol names because
// `config/G2ME01/symbols.txt:3115-3117` carries no mangled name for any of the three - only dtk's
// `fn_` placeholders - and objdiff pairs functions **by name** (the same reason
// `src/MetroidPrime/main.cpp:1766` spells `fn_80009224` with `extern "C"`). Without these the unit
// emits nothing for the three addresses, so all three read 0.00% however well the bodies are
// written.
//
// Which member each one touches is read straight off the instruction, using this repo's own
// measured MWCC 2.7 bitfield convention (confirmed against the 100%-matched `PreThink` and
// `StopSound` in this same file, and against the constructor's fourteen stores):
//
//   * **write** of a 1-bit field at word bit N: `lbz`/`rlwimi rD,rS,31-N,N,N` / `stb`, with the
//     value in `rS` bit 0. `rlwimi r0,r3,7,24,24` in `StopSound` clears `mPlayRequested`, bit 24.
//   * **read** of a 1-bit field at word bit N: `lbz` / `rlwinm. rD,rS,N+1,31,31` (+ `rlwinm`
//     without the `.` when the bool is returned rather than branched on). `rlwinm. r0,r3,31,31,31`
//     in `StopSound` tests `mWorldSfx`, bit 30.
//
// So `rlwimi r0,r4,4,27,27` (N=27, 31-N=4) writes word bit 27 of the second bool byte, which the
// constructor clears and no other code in the unit sets: the field the header names `x1a9_27_`,
// the fourth of the six bools packed into +0x1A9. `rlwinm r3,r0,27,31,31` (N=26) reads word bit 26
// of the first bool byte, which is `mNonEmitter`. And `lwz r0,364(r4)` / `stw r0,0(r3)` copies the
// single word at +0x16C, `mSfxHandle`, from the second argument into the first - the four-byte
// assignment of a `CSfxHandle`, emitted out of line because retail defines it in this unit
// (symbols.txt calls it `T`, a strong global, not the weak `__a` copy the header would emit).
//
// **A same-layout view rather than friend declarations**, as in `src/MetroidPrime/main.cpp:1761`:
// the members are `private`, the header is shared with `TypesMatch.cpp`, and the packing is
// retail's - the view reproduces the header's declaration order, which is retail's bit order
// (the constructor's fourteen stores walk 0x1A8 bits 24..31 then 0x1A9 bits 24..29 in exactly
// this order, and `CHECK_SIZEOF(CScriptSound, 0x1b0)` already pins the size). No layout changes.
struct SCscriptSoundTail {
  uchar x000[0x16c];
  CSfxHandle mSfxHandle;
  uchar x170[0x38];
  bool mPlayRequested : 1;
  bool mLooped : 1;
  bool mNonEmitter : 1;
  bool mAutoStart : 1;
  bool mOcclusionTest : 1;
  bool mAcoustics : 1;
  bool mWorldSfx : 1;
  bool mSelfFree : 1;
  bool mAllowDuplicates : 1;
  bool mProcessedThisFrame : 1;
  bool mPlayerRelativePan : 1;
  bool x1a9_27_ : 1;
  bool mAllAreas : 1;
  bool mScaleByMusicVolume : 1;
};

static inline SCscriptSoundTail& AsTail(CScriptSound* self) {
  return *reinterpret_cast< SCscriptSoundTail* >(self);
}
static inline const SCscriptSoundTail& AsTail(const CScriptSound* self) {
  return *reinterpret_cast< const SCscriptSoundTail* >(self);
}

extern "C" void fn_8009EB24(CSfxHandle* dest, const CScriptSound* src) {
  *dest = AsTail(src).mSfxHandle;
}

extern "C" bool fn_8009EB18(const CScriptSound* self) {
  return AsTail(self).mNonEmitter;
}

extern "C" void fn_8009EB08(CScriptSound* self, bool value) {
  AsTail(self).x1a9_27_ = value;
}

void CScriptSound::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  CActor::AcceptScriptMsg(mgr, msg);
  switch (msg.GetMessage()) {
  case kSM_Activate:
    if (mAutoStart) {
      mPlayRequested = true;
    }
    break;
  case kSM_Deactivate:
    StopSound(mgr);
    break;
  case kSM_Play:
    if (GetActive()) {
      PlaySound(mgr, &msg);
    }
    break;
  case kSM_Stop:
    if (GetActive()) {
      StopSound(mgr);
    }
    break;
  case kSM_XCRT:
    if (GetActive() && mAutoStart) {
      mPlayRequested = true;
    }
    // TODO: manager generation flag controls self-free behavior.
    break;
  case kSM_XDelete:
    if (!mWorldSfx) {
      StopSound(mgr);
    }
    break;
  case kSM_XALD:
    // TODO: resolve connected sound-position sources.
    break;
  default:
    break;
  }
}

void CScriptSound::PlaySound(CStateManager& mgr, const CScriptMsg* msg) {
  // TODO: emitter/non-emitter setup, multiplayer panning and duplicate suppression.
}

void CScriptSound::StopSound(CStateManager& mgr) {
  mPlayRequested = false;
  if (mWorldSfx && mNonEmitter) {
    mgr.World()->StopGlobalSound(GetSoundId());
    mSfxHandle.Clear();
  } else if (mSfxHandle) {
    CSfxManager::RemoveEmitter(mSfxHandle);
    mSfxHandle.Clear();
  }
}

float CScriptSound::GetOccludedVolumeAmount(const CVector3f& pos, const CStateManager& mgr) {
  if (mgr.IsMultiplayer()) {
    return 1.f;
  }
  const CTransform4f camXf = mgr.GetCameraManager(0)->GetCurrentCameraTransform(mgr, true);
  const CVector3f soundToCam = camXf.GetTranslation() - pos;
  const float soundToCamMag = soundToCam.Magnitude();
  const float invMag = 1.f / soundToCamMag;
  const CVector3f soundToCamNorm = soundToCam * invMag;
  const CVector3f up = CVector3f::Up();
  const CVector3f thirdEdge = up - soundToCamNorm * CVector3f::Dot(up, soundToCamNorm);
  const CVector3f cross = CVector3f::Cross(soundToCamNorm, thirdEdge);
  static const float kInfluenceAmount = 3.f / soundToCamMag;
  static const float kInfluenceIncrement = kInfluenceAmount;
  static CMaterialFilter kSolidFilter = CMaterialFilter::MakeIncludeExclude(
      CMaterialList(kMT_Unknown59), CMaterialList(kMT_NoPlatformCollision));
  int totalCount = 0;
  int invalCount = 0;
  for (float i = -kInfluenceAmount; i <= kInfluenceAmount; i += kInfluenceIncrement) {
    for (float j = -kInfluenceAmount; j <= kInfluenceAmount; j += kInfluenceIncrement) {
      ++totalCount;
      const CVector3f rayDir = (soundToCamNorm + i * thirdEdge) + j * cross;
      const CRayCastResult result =
          mgr.RayStaticIntersection(pos, rayDir.AsNormalized(), soundToCamMag, kSolidFilter);
      if (!result.IsValid()) {
        ++invalCount;
      }
    }
  }
  return invalCount / static_cast< float >(totalCount) * (1.f - 0.58f) + 0.58f;
}

// Retail 0x8009E0EC, 0x84 = 132 bytes: the out-of-line `CQuad` **copy constructor**, called twice
// from `fn_8009DD94` (0x164 and 0x254) as `fn_8009E0EC(&scratch, &box.GetQuad(face))`.
//
// The body is 16 `lfs`/`stfs` pairs and nothing else, which is `CHECK_SIZEOF(CQuad, 0x40)` read as
// 64 bytes = 16 floats. MWCC alternates `f1`/`f0` for the two halves of each 8-byte pair rather than
// reusing one register, and that is the shape a flat float copy of this class produces.
//
// **`extern "C"` under retail's name** for the same reason as the two above:
// `config/G2ME01/symbols.txt:3108` carries only dtk's `fn_8009E0EC`, and the two `bl` relocations
// in `fn_8009DD94` have to name the symbol retail names.
//
// A same-layout view of `CQuad` as the sixteen floats, not `*dest = *src` on the class: the class
// copy gets the first sixteen bytes right and then falls back to word moves from `+0x10` on
// (56.36% measured), because MWCC's member-wise copy of the four `CVector3f` corners goes through
// the copy it emits elsewhere. Retail's is a flat float copy of all 64 bytes. No layout changes -
// `CPlane` is `CUnitVector3f` + `float` (0x10) and `mA`..`mD` are `CVector3f` (0xC each), which is
// 0x10 + 4 * 0xC = 0x40.
struct SQuadFloats {
  float f[16];
};

extern "C" void fn_8009E0EC(CQuad* dest, const CQuad* src) {
  SQuadFloats& d = *reinterpret_cast< SQuadFloats* >(dest);
  const SQuadFloats& s = *reinterpret_cast< const SQuadFloats* >(src);
  for (int i = 0; i < 16; ++i) {
    d.f[i] = s.f[i];
  }
}

// Retail 0x8009DD94, 0x358 = 856 bytes: the listener-position search over the connected sound
// volumes, called from `Think` and `PlaySound`. **Spelled `extern "C"` under retail's name** for
// the same reason as `fn_8009DCE8` above - `config/G2ME01/symbols.txt:3109` carries only dtk's
// `fn_8009DD94`, objdiff pairs functions **by name**, and the previous `static` spelling mangled to
// `fn_8009dd94__FRC13CStateManagerRCQ24rstl45vector...` and paired with nothing (0.00%). It also
// has to *exist* under that name for the two `bl fn_8009E0EC` relocations inside it to be retail's.
//
// **The body is still a TODO and this is deliberately not a match.** Retail's walks the sound
// sources, `CAABox::GetQuad`s each connected volume, `CQuad::GetTri`s it and takes the point
// closest to the listener - it needs the `CScriptTriggerOrientated` cast path and the tweak
// helpers (`fn_801B81C0`, `lbl_8041B000`), none of which this unit owns. The signature is what the
// two call sites settle.
extern "C" CVector3f fn_8009DD94(const CStateManager& mgr,
                                 const rstl::vector< TUniqueId >& sources) {
  // TODO: find the closest point in the connected sound volumes to any player's listener.
  return CVector3f::Zero();
}

// Retail 0x8009DCE8, 0xAC = 172 bytes: the music-volume scaler, called from `PlaySound` (0xA7C),
// `SetMaxVolume` (0xE88) and `Think` (0x1460), each time as `fn_8009DCE8(mCurrentMaxVolume)`.
// **Declared last in this file** because mwcceppc emits definitions in reverse source order and
// 0x8009DCE8 is this unit's lowest retail offset, so it has to be the last definition to land
// first in `.text` (`tools/check_decl_order.py` - see the note by `sFirstInFrame` at the top).
//
// **Spelled `extern "C"` under retail's own name** for the same reason as the three accessors
// above: `config/G2ME01/symbols.txt:3104` carries only dtk's `fn_8009DCE8` placeholder, and objdiff
// pairs functions **by name**, so a `static` definition mangles to `fn_8009dce8__Fi` and pairs
// with nothing. The call sites in this unit are the reason it matters beyond the score: their
// `bl` relocations have to name the symbol retail names.
//
// **The body is still a TODO and this is deliberately not a match.** Retail's reads the game-state
// music-volume float through a `double` round-trip, hands it to `fn_80216CC4` (0x80216CC4, 0xC
// bytes) and then to `CMayaSpline::EvaluateAt`, and divides the product by the float **127.0** at
// `.sdata2:0x8041AFF4` (measured with `tools/dol_read.py 0x8041AFF4 0x10`, which prints
// `f32: 127 176 ...`). That needs the Tweaks block layout and `fn_80216CC4`, neither of which this
// unit owns; `src/MetroidPrime/Tweaks/CTweakGameHardModeDamageMultiplier.cpp` is the precedent for
// why the layout is not guessed here. What *is* settled is the signature, which the three call
// sites pin exactly: they pass `mCurrentMaxVolume` (`+0x172`) and feed the result straight to
// `SfxVolume(handle, (uchar)...)`, and the callee's own epilogue is `lha r3,8(r1)` - a `short` in,
// a `short` out.
extern "C" short fn_8009DCE8(short volume) {
  // TODO: scale volume with the music-volume tweak spline.
  return volume;
}
