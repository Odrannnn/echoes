#ifndef _CMODELDATA
#define _CMODELDATA

#include "types.h"

#include "MetroidPrime/TGameTypes.hpp"

#include "Kyoto/Animation/IAnimReader.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/auto_ptr.hpp"
#include "rstl/optional_object.hpp"
#include "rstl/pair.hpp"
#include "rstl/string.hpp"

class CAABox;
class CActorLights;
class CAnimData;
class CAnimRes;
class CFrustumPlanes;
class CModel;
class CModelFlags;
class CStateManager;
class CSkinnedModel;
class CRandom16;

// TODO move
#include "Kyoto/Math/CQuaternion.hpp"
struct CAdvancementDeltas {
public:
  CAdvancementDeltas(const CVector3f& posDelta, const CQuaternion& rotDelta)
  : x0_posDelta(posDelta), xc_rotDelta(rotDelta) {}

  const CVector3f& GetOffsetDelta() const { return x0_posDelta; }
  const CQuaternion& GetOrientationDelta() const { return xc_rotDelta; }

private:
  CVector3f x0_posDelta;
  CQuaternion xc_rotDelta;
};
CHECK_SIZEOF(CAdvancementDeltas, 0x1c)

class CStaticRes {
  CAssetId x0_cmdlId;
  CVector3f x4_scale;

public:
  CStaticRes(CAssetId id, const CVector3f& scale) : x0_cmdlId(id), x4_scale(scale) {}
};

class CModelData {
public:
  enum EWhichModel {
    kWM_Normal,
    kWM_XRay,
    kWM_Thermal,
    kWM_ThermalHot,
  };

  // TODO these probably aren't real
  bool HasNormalModel() const { return x1c_normalModel; }

  CModelData();
  CModelData(const CAnimRes&);
  CModelData(const CStaticRes&);
  CModelData(const CModelData& other);
  ~CModelData();

  CAdvancementDeltas AdvanceAnimation(float dt, CStateManager& mgr, TAreaId aid, bool advTree);
  void AdvanceParticles(const CTransform4f& xf, float dt, CStateManager& mgr);
  void RenderParticles(const CFrustumPlanes& planes) const;
  void RenderUnsortedParts(EWhichModel which, const CTransform4f& xf, const CActorLights* lights,
                           const CModelFlags& flags) const;
  void RenderThermal(const CTransform4f& xf, const CColor& mulColor, const CColor& addColor,
                     const CModelFlags& flags) const;
  void Render(const CStateManager&, const CTransform4f&, const CActorLights*,
              const CModelFlags&) const;
  void Render(EWhichModel, const CTransform4f&, const CActorLights*, const CModelFlags&) const;
  void FlatDraw(EWhichModel which, const CTransform4f& xf, bool unsortedOnly,
                const CModelFlags& flags) const;
  CSkinnedModel& PickAnimatedModel(EWhichModel which) const;
  void Touch(const CStateManager& mgr, int) const;
  SAdvancementDeltas AdvanceAnimationIgnoreParticles(float dt, CRandom16& rand, bool advTree);

  const CAnimData* GetAnimationData() const { return xc_animData.get(); }
  CAnimData* AnimationData() { return xc_animData.get(); }
  CAABox GetBounds(const CTransform4f& xf) const;
  CAABox GetBounds() const;
  bool IsLoaded(int shaderIdx) const;
  bool IsDefinitelyOpaque(EWhichModel which) const;

  CTransform4f GetLocatorTransform(const rstl::string& name) const;
  CTransform4f GetScaledLocatorTransform(const rstl::string& name) const;
  CTransform4f GetScaledLocatorTransformDynamic(const rstl::string& name,
                                                const CCharAnimTime* time) const;

  bool HasAnimation() const { return !xc_animData.null(); }
  bool IsNull() const { return xc_animData.null() && !x1c_normalModel; }

  void SetXRayModel(const rstl::pair< CAssetId, CAssetId >& assets);
  void SetInfraModel(const rstl::pair< CAssetId, CAssetId >& assets);

  void SetAmbientColor(const CColor& color) { x18_ambientColor = color; }
  bool GetSortThermal() const { return x14_flags.x25_sortThermal; }
  void SetSortThermal(bool b) { x14_flags.x25_sortThermal = b; }

  CVector3f GetScale() const { return x0_scale; }
  void SetScale(const CVector3f& scale) { x0_scale = scale; }

  bool GetIsLoop() const;
  void EnableLooping(bool enable);
  static CModelData CModelDataNull();
  static EWhichModel GetRenderingModel(const CStateManager& mgr);

  // Public because retail's default constructor (0x800E6AD0) is one `stfs`/`stb` per member and
  // is reproduced as a flat body, and because the port's own `CModelData::CModelData()` is a
  // one-line call into it. `private:` here buys nothing: every member below is a store in it.
  CVector3f x0_scale;
  rstl::auto_ptr< CAnimData > xc_animData; //!< 0x0C = x0_has, **0x10 = x4_item, the CAnimData*
  // The four flag bits live in a **named struct**, not loose in the class, and that is not a
  // style choice - it is the only spelling MWCC 2.7 gives retail's two different code shapes.
  // Measured with the port's own flags on 2026-09-26:
  //
  //   * the default constructor (0x800E6AD0) writes them as four separate `lbz`/`rlwimi`/`stb`
  //     triples, which is what a *nested struct's* inlined default constructor assigning each bit
  //     compiles to, and also what four loose bit-fields assigned in a body compile to;
  //   * the copy constructor (0x80019010) copies them as **one `lbz`/`stb` pair** on the whole
  //     byte - and MWCC 2.7 emits exactly that pair, and nothing else, for a struct of four
  //     one-bit bit-fields copied as a unit. Four loose bit-fields in a mem-init list give four
  //     read-modify-write chains instead, 28 instructions against retail's 2.
  //
  // So loose bit-fields match the default constructor and cannot match the copy constructor, and
  // the struct matches both. Measured as `tools/bfprobe` shapes V4 (struct) and V1 (loose).
  struct SFlags {
    bool x24_renderSorted : 1;
    bool x25_sortThermal : 1;
    // Bits 2 and 3. Nothing in the tree reads them, and 0x800E6AD0 is the only code in the DOL
    // that writes them - bits 0, 1 and 3 to 0 and bit 2 to 1.
    bool x26_ : 1;
    bool x27_ : 1;
  };
  SFlags x14_flags;
  CColor x18_ambientColor;
  // `TLockedToken`, not `TCachedToken`. Retail's copy constructor (0x80018FBC) copies each of
  // these three with `__ct__6CTokenFRC6CToken` + `dst.x8 = src.x8` + `Lock__6CTokenFv`, and the
  // `Lock()` is `TLockedToken`'s copy constructor, not `TCachedToken`'s. Same size, same offsets.
  rstl::optional_object< TLockedToken< CModel > > x1c_normalModel;
  rstl::optional_object< TLockedToken< CModel > > x2c_xrayModel;
  rstl::optional_object< TLockedToken< CModel > > x3c_infraModel;
};
CHECK_SIZEOF(CModelData, 0x4c)

#endif // _CMODELDATA
