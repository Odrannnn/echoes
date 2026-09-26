#ifndef _IWEAPONRENDERER
#define _IWEAPONRENDERER

#include "types.h"

class CParticleGen;

// `CCubeRenderer`'s second base, read out of retail rather than recalled from Metroid Prime.
// Its constructor stores `lbl_803B8B70` at +0x04 (0x8027126C) before overwriting it with
// `__vt__13CCubeRenderer + 0x140`, and its destructor puts `lbl_803B8B70` back (0x80270A18).
// That table is `.data` 0x803B8B70, 0x10 bytes: two header words and **two zero slots**, i.e.
// an interface with two pure virtuals. The derived table's secondary half names both:
// 0x803B8D58 is `@4@__dt__13CCubeRendererFv` and 0x803B8D5C is
// `@4@AddParticleGen__13CCubeRendererFRC12CParticleGen` - a destructor, and the one
// `AddParticleGen` overload that `CCubeRenderer` overrides for both of its bases at once.
class IWeaponRenderer {
public:
  virtual ~IWeaponRenderer() = 0;
  virtual void AddParticleGen(const CParticleGen& gen) = 0;
};

// A pure destructor still has to exist, because every derived destructor calls it. Retail's
// `~CCubeRenderer` restores this base's vptr and calls nothing, so the body is empty and inline.
inline IWeaponRenderer::~IWeaponRenderer() {}

#endif // _IWEAPONRENDERER
