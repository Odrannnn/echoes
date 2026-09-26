#ifndef _CCHARACTERFACTORYBUILDER
#define _CCHARACTERFACTORYBUILDER

#include "types.h"

#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/TToken.hpp"

class CAnimRes;
class CCharacterFactory;

// `CGameGlobalObjects`+0x108, 0x28 bytes: a factory whose only product is `CCharacterFactory`,
// and a private `CSimplePool` that uses it. Retail's names for all of it are Metroid Prime's
// (`include/MetroidPrime/Factories/CCharacterFactoryBuilder.hpp` in the sibling tree); Echoes'
// class is the same class with Echoes' 0x24-byte `CSimplePool`, so 0x28 here against Prime's 0x24.
//
// The vtables, read out of retail `.data`:
//
//   0x803B19B8  IFactory                 0, 0, then six zero slots (every virtual pure)
//   0x803B1A10  CDummyFactory            0, 0, 80031E70 dtor, 800320EC Build,
//                                        80032060 BuildAsync, 8003205C CancelBuild,
//                                        80031E60 CanBuild, 80031E68 GetResourceIdByName
//
// **The constructor is declared, not inline.** Retail's `CGameGlobalObjects` constructor emits
// `addi r3,r31,264 ; bl 80032008` and nothing else for this member, and the two vptr stores
// and the `CSimplePool` call are inside 0x80032008 (`src/MetroidPrime/Factories/
// CCharacterFactoryBuilder.cpp`). So `fn_80032008` is renamed in `config/G2ME01/symbols.txt` to
// `__ct__24CCharacterFactoryBuilderFv`, which is the call a declared-only constructor makes - the
// same arrangement as `CResFactory` and `CSimplePool` (`Kyoto/CResFactory.hpp`).
class CCharacterFactoryBuilder {
public:
  class CDummyFactory : public IFactory {
  public:
    // Inline, and retail agrees twice: `~CCharacterFactoryBuilder` (0x80031F8C) has this
    // destructor's two vptr stores inlined after the `CSimplePool` destructor call, and the
    // out-of-line copy the vtable needs is 0x80031E70.
    ~CDummyFactory() {}
    CFactoryFnReturn Build(const SObjectTag& tag, const CVParamTransfer& xfer);
    void BuildAsync(const SObjectTag& tag, const CVParamTransfer& xfer, IObj** out);
    void CancelBuild(const SObjectTag& tag);
    bool CanBuild(const SObjectTag& tag);
    const SObjectTag* GetResourceIdByName(const char* name) const;
  };

  CCharacterFactoryBuilder();
  ~CCharacterFactoryBuilder();

  // Retail 0x80031ECC, 0x9C: `x4_dummyStore.GetObj(SObjectTag('ANCS', res.x0), CVParamTransfer
  // Null)` through the store's first virtual. Declared only; nothing on the boot path calls it.
  TToken< CCharacterFactory > GetFactory(const CAnimRes& res);

private:
  CDummyFactory x0_dummyFactory;
  CSimplePool x4_dummyStore;
};
CHECK_SIZEOF(CCharacterFactoryBuilder, 0x28)

extern CCharacterFactoryBuilder* gpCharacterFactoryBuilder;

#endif // _CCHARACTERFACTORYBUILDER
