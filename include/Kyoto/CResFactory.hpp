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
  CFactoryMgr& GetFactoryMgr() { return x74_factoryMgr; }
  FourCC GetResourceTypeById(CAssetId id) { return GetResLoader().GetResourceTypeById(id); }

private:
  // `CResLoader` is **0x70** bytes, not 0x58 or 0x60 - see the four unnamed words at the end of
  // its member list in `Kyoto/CResLoader.hpp` for the two measurements that fix it.
  CResLoader x4_resLoader; // +0x04, 0x70
  // `CFactoryMgr` is at **+0x74**, which is what all 36 factory registrations in
  // `CGameGlobalObjects::AddPaksAndFactories` address: `addi r3, r31, 116` at 0x80007510 and
  // 0x80007528 and 34 more times, with `r31` = `gpResourceFactory`. `CFactoryMgr`'s own two
  // registrars then use `this+0x00` and `this+0x14` for their two `rstl::map`s
  // (`fn_802F96E0` at 0x802F971C-0x802F9720 and `fn_802F963C` at 0x802F967C-0x802F9680), which is
  // the layout `Kyoto/CFactoryMgr.hpp` already carries and which is a `Matching` unit at 100%.
  // The header used to say +0x64, which is 0x10 too low; the 36 registrations are what prove it.
  CFactoryMgr x74_factoryMgr; // +0x74, 0x38
  // 0x38 bytes retail constructs nothing in that this tree has a name for. They are here to
  // reach the measured size and nothing may be inserted between them: `CResFactory` is
  // **0xE4**, because `CGameGlobalObjects::CGameGlobalObjects` puts the member it builds after
  // the factory at `this+0xE4` and the factory is at `this+0`.
  uchar xac_[0x38];
};
// 0xE4, CHECK_SIZEOF-confirmed with mwcceppc. It *contains* two members: CResLoader at
// +0x04 (0x70 bytes) and CFactoryMgr at +0x74 - they are not siblings of this class in
// CGameGlobalObjects, which is the misreading that produced a wrong CResLoader size
// for a whole session. See the correction at the end of docs/research/paks.md.
CHECK_SIZEOF(CResFactory, 0xe4);

extern CResFactory* gpResourceFactory;

#endif // _CRESFACTORY
