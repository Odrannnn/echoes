#ifndef _CLIGHT
#define _CLIGHT

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Math/CVector3f.hpp"

enum ELightType {
  kLT_Spot = 0,
  kLT_Point = 1,
  kLT_Directional = 2,
  kLT_LocalAmbient = 3,
  kLT_Custom = 4,
  kLT_Hard = 5, // Name inferred from the Wii BuildHard export and G2ME01 factory.
};

enum EFalloffType { kFT_Constant, kFT_Linear, kFT_Quadratic };

class CLight {
  static const CVector3f kDefaultPosition;
  static const CVector3f kDefaultDirection;

  float CalculateLightRadius() const;

public:
  CLight(ELightType type, const CVector3f& position, const CVector3f& direction,
         const CColor& color, float cutoff);
  CLight(ELightType type, const CVector3f& pos, const CVector3f& direction, const CColor& color,
         float distC, float distL, float distQ, float angleC, float angleL, float angleQ);
  CLight(const CLight&);

  void SetPosition(const CVector3f& pos);
  const CVector3f& GetPosition() const { return mPos; }
  void SetDirection(const CVector3f& dir);
  const CVector3f& GetDirection() const { return mDir; }
  void SetColor(const CColor& col);
  void SetAttenuation(float constant, float linear, float quadratic);
  void SetSpotCutoff(float cutoff); // Guessed name.
  float GetSpotCutoff() const { return mSpotCutoff; }
  float GetAttenuationConstant() const { return mDistC; }
  float GetAttenuationLinear() const { return mDistL; }
  float GetAttenuationQuadratic() const { return mDistQ; }

  void SetAngleAttenuation(float constant, float linear, float quadratic);
  float GetAngleAttenuationConstant() const { return mAngleC; }
  float GetAngleAttenuationLinear() const { return mAngleL; }
  float GetAngleAttenuationQuadratic() const { return mAngleQ; }

  ELightType GetType() const { return mType; }
  uint GetId() const { return mLightId; }
  float GetIntensity() const;
  float GetRadius() const;
  const CColor& GetColor() const { return mColor; }

  int GetPriority() const { return mPriority; }
  void SetPriority(uint priority) { mPriority = priority; }
  void SetLightId(uint lightId) { mLightId = lightId; }

  CVector3f GetNormalIndependentLightingAtPoint(const CVector3f& point) const;

  static CLight BuildDirectional(const CVector3f& dir, const CColor& color);
  static CLight BuildSpot(const CVector3f& pos, const CVector3f& dir, const CColor& color,
                          float cutoff);
  static CLight BuildPoint(const CVector3f& pos, const CColor& color);
  static CLight BuildCustom(const CVector3f& pos, const CVector3f& dir, const CColor& color,
                            float distC, float distL, float distQ, float angleC, float angleL,
                            float angleQ);
  static CLight BuildHard(const CVector3f& pos, const CColor& color, float radius);
  static CLight BuildLocalAmbient(const CVector3f& pos, const CColor& color);

private:
  CVector3f mPos;
  CVector3f mDir;
  CColor mColor;
  ELightType mType;
  float mSpotCutoff;
  float mDistC;
  float mDistL;
  float mDistQ;
  float mAngleC;
  float mAngleL;
  float mAngleQ;
  int mPriority;
  uint mLightId;
  mutable float mCachedRadius;
  mutable float mCachedIntensity;

  // Retail's copy constructor (0x80038C9C) moves the two dirty flags as one byte - a single
  // lbz/stb at 0x4C - while a memberwise initialiser list of two separate `bool : 1` members
  // makes mwcceppc read-modify-write each bit in turn (85.24%, 8 instructions too many). They are
  // therefore one object, still 1 byte at 0x4C with mIntensityDirty at bit 7 and mRadiusDirty at
  // bit 6: `SetSpotCutoff` read-modify-writes the same two bits and is unchanged at 100%.
  struct SDirtyFlags {
    bool mIntensityDirty : 1;
    bool mRadiusDirty : 1;
    SDirtyFlags(bool intensity, bool radius)
    : mIntensityDirty(intensity)
    , mRadiusDirty(radius) {}
  };
  mutable SDirtyFlags mDirty;
};
CHECK_SIZEOF(CLight, 0x50)

namespace rstl {
// Measured on CActorLights (2026-09-30): retail's `clear()` and `~CActorLights` (0x800DE64C, 0x3C
// bytes) destroy no element and loop over nothing - the destructor is the `CMemory::Free` stub
// alone. Only the destructible half of the trait is declared: declaring the constructible half too
// replaces placement new with copy assignment, and then mwcceppc outlines `push_back` instead of
// inlining the element copy (measured: `AddOverflowToLights` 96.16% -> 91.72%, `BuildFakeLightList`
// 96.91% -> 72.29%, and an out-of-line `push_back__Q24rstl26reserved_vector<6CLight,4>FRC6CLight`
// appears in our object that retail does not have).
RSTL_DECLARE_TRIVIALLY_DESTRUCTIBLE(CLight)
} // namespace rstl

#endif // _CLIGHT
