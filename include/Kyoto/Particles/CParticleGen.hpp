#ifndef _CPARTICLEGEN
#define _CPARTICLEGEN

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/Particles/CWarp.hpp"

#include "rstl/list.hpp"
#include "rstl/optional_object.hpp"
#include "rstl/pair.hpp"

class CWarp;

class CParticleGen {
public:
  CParticleGen() : mDrawFlags(0) {}
  virtual ~CParticleGen() = 0;
  virtual const bool Update(double) = 0;
  virtual void Render() = 0;
  virtual void SetOrientation(const CTransform4f& orientation) = 0;
  virtual void SetTranslation(const CVector3f& translation) = 0;
  virtual void SetGlobalOrientation(const CTransform4f& orientation) = 0;
  virtual void SetGlobalTranslation(const CVector3f& translation) = 0;
  virtual void SetGlobalScale(const CVector3f& scale) = 0;
  virtual void SetLocalScale(const CVector3f& scale) = 0;
  virtual void SetParticleEmission(bool emission) = 0;
  virtual void SetModulationColor(const CColor& col) = 0;
  // The five methods with bodies below - SetGeneratorRate, SetDrawFlags, GetGeneratorRate,
  // GetDrawFlags and ShouldDraw - are **not** inline in retail. `config/G2ME01/symbols.txt:1614`
  // puts all five at `.text` just past `CExplosion`'s last constructor, and
  // `build/G2ME01/obj/MetroidPrime/CExplosion.o` defines them `T` while every other object that
  // touches the class (`CElementGen.o`, `CParticleElectric.o`, `CParticleSwoosh.o`,
  // `CParticleGen.o`) references them `U`. Defining them out of line in `CExplosion.cpp` is what
  // puts them there; a body in the class makes mwcceppc emit a weak copy in every including object
  // instead. It also moves `__vt__12CParticleGen`, because SetGeneratorRate is the first virtual
  // with a definition in declaration order and so is CParticleGen's key function - retail defines
  // the vtable in `CExplosion.o` too.
  virtual void SetGeneratorRate(float rate);
  // Names of the draw-flag methods are hypotheses based on the sDrawFlags/sDrawMask Wii exports.
  virtual void SetDrawFlags(uint flags);
  virtual const CTransform4f& GetOrientation() const = 0;
  virtual const CVector3f& GetTranslation() const = 0;
  virtual const CTransform4f& GetGlobalOrientation() const = 0;
  virtual const CVector3f& GetGlobalTranslation() const = 0;
  virtual const CVector3f& GetGlobalScale() const = 0;
  virtual bool GetParticleEmission() const = 0;
  virtual const CColor& GetModulationColor() const = 0;
  virtual float GetGeneratorRate() const;
  virtual int GetEmitterTime() const = 0;
  virtual uint GetDrawFlags() const;
  virtual bool ShouldDraw() const;
  virtual int GetSystemCount() = 0;
  virtual bool IsSystemDeletable() = 0;
  virtual rstl::optional_object< CAABox > GetBounds() = 0;
  virtual int GetParticleCount() = 0;
  virtual bool SystemHasLight() = 0;
  virtual CLight GetLight() = 0;
  virtual void DestroyParticles() = 0;
  virtual void AddModifier(CWarp*);
  virtual uint Get4CharId() const = 0;

protected:
  rstl::list< CWarp* > mModifiersList;

private:
  uint mDrawFlags;
};

inline CParticleGen::~CParticleGen() {}

#endif // _CPARTICLEGEN
