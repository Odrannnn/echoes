#ifndef _WEAPONCOMMON
#define _WEAPONCOMMON

#include "Kyoto/SObjectTag.hpp"

#include "rstl/set.hpp"
#include "rstl/pair.hpp"
#include "rstl/vector.hpp"

class CToken;
class CSfxHandle;
class CAnimData;
class CStateManager;
class CPrimitive;
class CVector3f;

namespace NWeaponTypes {

CAssetId get_asset_id_from_name(const char* name);
void lock_tokens(rstl::vector< CToken >& tokens);
// Retail's `fn_8018A6EC`: the `Unlock`-per-element twin of `lock_tokens` above. It has no
// name in symbols.txt, so it is declared under the `fn_` name retail's object has; the body
// is in `src/MetroidPrime/Weapons/NWeaponTypesTokens.cpp`.
extern "C" void fn_8018A6EC(rstl::vector< CToken >* tokens);
// Retail's `fn_8018A7E8` (0x8018A7E8, 0xE8 bytes): the `get_token_vector` that walks a caller-built
// list of animation ids rather than an id range. `CGunMotion::LoadAnimations` is its only caller
// in this tree, and it is the Echoes replacement for Prime 1's range form (which built no list).
// It has no name in symbols.txt, so it is declared under the `fn_` name retail's object has; the
// body is not written yet, so it stays on the port link gap list.
extern "C" void fn_8018A7E8(const CAnimData& animData, const rstl::vector< int >& animIds,
                            rstl::vector< CToken >& tokensOut, bool preLock);
bool are_tokens_ready(const rstl::vector< CToken >& tokens);
void do_sound_event(rstl::pair< ushort, CSfxHandle >& sound, int& pitch, bool doPitchBend,
                    uint soundId, float weight, uint flags, float falloff, float maxDistance,
                    uchar minVolume, uchar maxVolume, const CVector3f& posToCamera,
                    const CVector3f& pos, int areaId, short pan, CStateManager& mgr);

enum EGunAnimType {
  kGAT_BasePosition,
  kGAT_Shoot,
  kGAT_ChargeUp,
  kGAT_ChargeLoop,
  kGAT_ChargeShoot,
  kGAT_FromMissile,
  kGAT_ToMissile,
  kGAT_MissileShoot,
  kGAT_MissileReload,
  kGAT_FromBeam,
  kGAT_ToBeam
};

} // namespace NWeaponTypes

#endif // _WEAPONCOMMON
