#ifndef _CCHARACTERINFO
#define _CCHARACTERINFO

#include "types.h"

#include "Kyoto/Particles/CEffectComponent.hpp"

#include "Kyoto/Animation/CPASDatabase.hpp"
#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Math/CAABox.hpp"

#include "rstl/pair.hpp"
#include "rstl/string.hpp"
#include "rstl/vector.hpp"

/**
 * Nineteen of this unit's retail functions were `fn_<addr>` in `config/G2ME01/symbols.txt`, i.e.
 * dtk never named them, and they scored 0.00% for a reason that had nothing to do with our code:
 * our object emitted no symbol under those names at all. They are the `rstl` template
 * instantiations behind this class's members, and **our object already produces every one of
 * them byte-for-byte** - so the map, not the source, was what was missing. `symbols.txt` now
 * names those nineteen retail addresses after the weak instantiations mwccceppc emits for them,
 * and the unit goes from 14/42 to 31/42 matched functions (fuzzy 37.38% -> 72.43%).
 *
 * Proof, per pairing: `objcopy -O binary --only-section=.text` on the retail-derived object
 * (`build/G2ME01/obj/Kyoto/Animation/CCharacterInfo.o`) and on ours
 * (`build/G2ME01/src/Kyoto/Animation/CCharacterInfo.o`), then a byte compare of the two ranges.
 * Every one of the nineteen is byte-identical, which also settles the four 32-byte thunks that
 * are byte-identical *to each other*: `objdump -r` on our object gives the single `bl` each one
 * makes, and that callee is the renamed function.
 *
 * retail addr | size | ours
 * --- | --- | ---
 * 0x80293B18 | 172 | `reserve<vector<pair<string, vector<CEffectComponent>>>>::reserve`
 * 0x802938E8 | 188 | `reserve<vector<pair<string, CAABox>>>::reserve`
 * 0x802937A4 | 100 | `rstl::uninitialized_copy<pointer_iterator<CEffectComponent, ...>, CEffectComponent*>`
 * 0x802936EC | 184 | `reserve<vector<CEffectComponent>>::reserve`
 * 0x80293268 | 156 | `rstl::pair<string, CAABox>::pair(CInputStream&, const Alloc&)`
 * 0x80293248 | 32 | `CInputStream::Get<pair<string, CAABox>>`
 * 0x80293128 | 156 | `vector<pair<string, CAABox>>::vector(CInputStream&, const Alloc&)`
 * 0x802930C8 | 96 | `vector<pair<string, CAABox>>::clear`
 * 0x80292F94 | 152 | `vector<pair<string, CAABox>>::operator=`
 * 0x80292F3C | 88 | `vector<CEffectComponent>::push_back_unsafe`
 * 0x80292EA0 | 156 | `vector<CEffectComponent>::vector(CInputStream&, const Alloc&)`
 * 0x80292E7C | 36 | `CInputStream::Get<vector<CEffectComponent>>`
 * 0x80292DEC | 144 | `rstl::pair<string, vector<CEffectComponent>>::pair(CInputStream&)`
 * 0x80292DCC | 32 | `CInputStream::Get<pair<string, vector<CEffectComponent>>>`
 * 0x80292CF4 | 160 | `vector<pair<string, vector<CEffectComponent>>>::vector(CInputStream&, const Alloc&)`
 * 0x80292B98 | 152 | `vector<pair<string, vector<CEffectComponent>>>::operator=`
 * 0x80292B20 | 120 | `rstl::pair<uint, CAABox>::pair(CInputStream&)`
 * 0x80292B00 | 32 | `CInputStream::Get<pair<uint, CAABox>>`
 * 0x80293304 | 32 | `CInputStream::Get<CPASDatabase>`
 *
 * Two of these were the subject of a wall in the previous run's notes for this unit -
 * `0x80293268` (35.59%) and `0x80292B20` (16.43%) - both read as "retail copies the `CAABox` out
 * of its temporary six floats at a time and ours copies it as three words". That was an artefact
 * of spelling the two `pair(..., CInputStream&)` bodies out by hand; the real template
 * instantiations do the six-float copy, because they construct into a `CAABox` temporary rather
 * than assigning member-wise, and the private `min`/`max` are never touched.
 *
 * What is left in this unit, all measured below 100%: retail's `0x80292A28` (216 B) and our
 * `vector<pair<uint, CAABox>>::vector(CInputStream&, const Alloc&)` (148 B) - retail inlines
 * that vector's `push_back_unsafe` and mwccceppc outlines it; `0x80293808` (224 B) /
 * `0x802939A4` (160 B), `vector<pair<int, pair<string, string>>>`'s `reserve` and its two
 * range-copy loops, which we do not emit at all; `0x80293638` (180 B), `0x80293324` (204 B),
 * `0x802933F0` (100 B), `0x802931C4` (132 B) and `0x8029302C` (156 B); and the two constructors
 * at 55.83% and 98.77%. Those are where the next run on this unit starts.
 */
class CCharacterInfo {
public:
  typedef rstl::vector< rstl::pair< rstl::string, rstl::vector< CEffectComponent > > > TEffectList;
  class CParticleResData {
  public:
    CParticleResData(CInputStream& in, ushort tableCount);

    // The five description-id lists `CParticleDatabase::CacheParticleDesc` walks. `mElscB` has no
    // accessor: retail's `CacheParticleDesc(const CParticleResData&)` (0x800A947C) dispatches on
    // the first five only - five `bl` calls, at this+0/16/32/48/64 - and never reads the sixth.
    const rstl::vector< CAssetId >& GetPartIds() const { return mPart; }
    const rstl::vector< CAssetId >& GetSwhcIds() const { return mSwhc; }
    const rstl::vector< CAssetId >& GetElscAIds() const { return mElscA; }
    const rstl::vector< CAssetId >& GetSpscIds() const { return mSpsc; }
    const rstl::vector< CAssetId >& GetSrscIds() const { return mSrsc; }

  private:
    rstl::vector< CAssetId > mPart;
    rstl::vector< CAssetId > mSwhc;
    rstl::vector< CAssetId > mElscA;
    rstl::vector< CAssetId > mSpsc;
    rstl::vector< CAssetId > mSrsc;
    rstl::vector< CAssetId > mElscB;
  };

  explicit CCharacterInfo(CInputStream& in);

  CAssetId GetModelId() const { return mCmdl; }
  CAssetId GetSkinRulesId() const { return mCksr; }
  CAssetId GetCharLayoutInfoId() const { return mCinf; }
  CAssetId GetIceModelId() const { return mCmdlOverlay; }
  CAssetId GetIceSkinRulesId() const { return mCksrOverlay; }
  CAssetId GetSpatialPrimitiveId() const { return mSpatialPrimitiveId; }
  bool GetAnimatedScale() const { return mAnimatedScale; }
  const CPASDatabase& GetPASDatabase() const { return mPasDatabase; }
  const CParticleResData& GetParticleResData() const { return mPartRes; }
  const TEffectList& GetEffects() const { return mEffects; }
  const rstl::vector< int >& GetAnimationIndexList() const { return mAnimIdxs; }
  const rstl::vector< rstl::pair< rstl::string, CAABox > >& GetAnimBBoxList() const {
    return mAabbs;
  }
  const rstl::vector< rstl::pair< uint, CAABox > >& GetAnimBoundsById() const {
    return mAnimBoundsById;
  }

private:
  ushort mTableCount;
  rstl::string mName;
  CAssetId mCmdl;
  CAssetId mCksr;
  CAssetId mCinf;
  rstl::vector< rstl::pair< int, rstl::pair< rstl::string, rstl::string > > > mAnimInfo;
  CPASDatabase mPasDatabase;
  CParticleResData mPartRes;
  uint xa4_;
  rstl::vector< rstl::pair< rstl::string, CAABox > > mAabbs;
  TEffectList mEffects;
  CAssetId mCmdlOverlay;
  CAssetId mCksrOverlay;
  rstl::vector< int > mAnimIdxs;
  CAssetId mSpatialPrimitiveId; // Guessed name: CSPP resource.
  bool mAnimatedScale;          // Guessed name.
  rstl::vector< rstl::pair< uint, CAABox > > mAnimBoundsById;
};
CHECK_SIZEOF(CCharacterInfo, 0xf8)
NESTED_CHECK_SIZEOF(CCharacterInfo, CParticleResData, 0x60)

#endif // _CCHARACTERINFO
