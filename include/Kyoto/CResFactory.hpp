#ifndef _CRESFACTORY
#define _CRESFACTORY

#include "types.h"

#include "rstl/list.hpp"

#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/CResLoader.hpp"

class IFactory {
public:
  virtual ~IFactory() {}
  virtual CFactoryFnReturn Build(const SObjectTag&, const CVParamTransfer&) = 0;
  virtual void BuildAsync(const SObjectTag&, const CVParamTransfer&, IObj**) = 0;
  virtual void CancelBuild(const SObjectTag&) = 0;
  virtual bool CanBuild(const SObjectTag&) = 0;
  virtual const SObjectTag* GetResourceIdByName(const char* name) const = 0;
  // TODO
};

class CResFactory : public IFactory {
public:
  CResFactory();

  ~CResFactory() {}
  CFactoryFnReturn Build(const SObjectTag&, const CVParamTransfer&);
  void BuildAsync(const SObjectTag&, const CVParamTransfer&, IObj**);
  void CancelBuild(const SObjectTag&);
  bool CanBuild(const SObjectTag&);
  const SObjectTag* GetResourceIdByName(const char* name) const;

  uint ResourceSize(const SObjectTag& tag) const { return x4_resLoader.ResourceSize(tag); }

  void AsyncIdle(uint time, bool);

  CResLoader& GetResLoader() { return x4_resLoader; }
  FourCC GetResourceTypeById(CAssetId id) { return GetResLoader().GetResourceTypeById(id); }

private:
  // `CResLoader` is **0x60** bytes, not 0x58 - see the comment on its members in
  // `Kyoto/CResLoader.hpp`, which is what moved every offset below up by 8 from the ones this
  // header used to carry (`x5c_factoryMgr` -> `x64_factoryMgr`, and so on).
  CResLoader x4_resLoader; // +0x04, 0x60
  CFactoryMgr x64_factoryMgr; // +0x64, 0x38
  uint x9c_;
  uint xa0_;
  uint xa4_;
  uint xa8_;
  uint xac_;
  uint xb0_;
  uint xb4_;
  rstl::list< unkptr > xb8_; // +0xB8
};
// UNCONFIRMED. g1 set 0xd0 from a partial read of CResLoader and g3 claimed 0xe4;
// neither measured this class, and retail's CGameGlobalObjects ctor places the
// *next* member at +0xE4 rather than measuring this one's extent. What is measured
// is that CResFactory is at CGameGlobalObjects+0 and CResLoader at +4. See the
// adjudication at the end of docs/research/paks.md.
CHECK_SIZEOF(CResFactory, 0xd0);

extern CResFactory* gpResourceFactory;

#endif // _CRESFACTORY
