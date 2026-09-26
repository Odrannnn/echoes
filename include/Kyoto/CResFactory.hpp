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
  // 0x34 bytes retail neither names in this tree nor reads a field out of. They are here to
  // reach the measured size and nothing may be inserted between them: `CResFactory` is
  // **0xE0**, because `CGameGlobalObjects` puts the member it builds after the factory at
  // `this+0xE4` and the factory is at `this+0x04` (see the `pad0` note in
  // `MetroidPrime/CGameGlobalObjects.hpp`).
  //
  // It was 0xE4, four bytes too much; those four bytes are the first four of the member that
  // follows the factory. `CResFactory::CResFactory` (retail `fn_802FB154`, 0x802FB154) is the
  // constructor that ends here, and its last write is `stw r6,220(r31)` at 0x802FB1E4 = +0xDC,
  // four bytes, so the object ends at +0xE0 and not one byte later. `~CResFactory`
  // (`fn_802FB038`, 0x802FB038) never destroys anything past +0xC8, so nothing is hiding in the
  // last four bytes either.
  uchar xac_[0x34];
};
// **0xE0**, not 0xE4. Three independent measurements, all from the same base
// (`gpResourceFactory`, which the `CGameGlobalObjects` constructor stores as `this+0x04` at
// 0x80008528/0x80008534):
//
//  * the constructor's last store is at +0xDC, i.e. four bytes, so the extent is 0xE0;
//  * `CGameGlobalObjects::CGameGlobalObjects` builds `CSimplePool` on `this+0xE4` with
//    `this+0x04` as its `IFactory&` (0x800084AC-0x800084B4), so 0xE4 - 0x04 = 0xE0;
//  * `CGameGlobalObjects::CGameGlobalObjects` builds `CCharacterFactoryBuilder` on `this+0x108`
//    (0x800084B8) and `CSimplePool` is `CHECK_SIZEOF(..., 0x24)`, so 0xE4 + 0x24 = 0x108.
//
// It *contains* two members: `CResLoader` at +0x04 (0x70 bytes) and `CFactoryMgr` at +0x74 -
// they are not siblings of this class in `CGameGlobalObjects`, which is the misreading that
// produced a wrong CResLoader size for a whole session. Both offsets are the ctor's own
// `addi r3,r31,4` / `addi r3,r31,116` at 0x802FB17C and 0x802FB188.
CHECK_SIZEOF(CResFactory, 0xe0);
// The 36 registrations' base, +0x74, is the one offset in this class retail states outright
// (`addi r3, r31, 116` with `r31` = `gpResourceFactory`, 36 times in `AddPaksAndFactories`).
// It has no `CHECK_OFFSETOF`, and cannot have one: mwcceppc 2.7 will not name a private member
// outside its class ("illegal access to protected/private member"), which is the same reason
// `MetroidPrime/CStateManager.hpp`'s two are commented out. The `x74_` in the member's name and
// the comment above it are the record.
//
// CHECK_OFFSETOF(CResFactory, x74_factoryMgr, 0x74);
// CHECK_OFFSETOF(CResFactory, x4_resLoader, 0x4);

extern CResFactory* gpResourceFactory;

#endif // _CRESFACTORY
