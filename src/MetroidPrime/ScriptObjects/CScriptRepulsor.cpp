#include "MetroidPrime/ScriptObjects/CScriptRepulsor.hpp"

#include "MetroidPrime/CActorParameters.hpp"

// Retail tests `flags & 1` and then loads material 5 on both paths (`clrlwi.` with no branch),
// so both arms of the conditional are kMT_Pillar here; the arm retail meant is not recoverable.
CScriptRepulsor::CScriptRepulsor(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                                 const CTransform4f& transform, float radius, float strength,
                                 EShape shape, uint flags)
: CActor(uid, name, info, 0, transform, CModelData::CModelDataNull(), CMaterialList((flags & 1) ? kMT_Pillar : kMT_Pillar), CActorParameters::None(),
         kInvalidUniqueId)
, mRadius(radius)
, mStrength(strength)
, mShape(shape)
, mFlags(flags) {
  SetCallTouch(false);
}

rstl::optional_object< CAABox > CScriptRepulsor::GetTouchBounds() const {
  return CAABox(GetTranslation(), GetTranslation());
}

CScriptRepulsor::~CScriptRepulsor() {}

float CScriptRepulsor::GetRadius() const { return mRadius; }

float CScriptRepulsor::GetStrength() const { return mStrength; }

CScriptRepulsor::EShape CScriptRepulsor::GetShape() const { return mShape; }
