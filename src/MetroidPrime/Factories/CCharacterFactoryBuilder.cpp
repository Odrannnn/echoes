/**
 * `CCharacterFactoryBuilder` and its `CDummyFactory` - retail `.text` 0x80031E60..0x80032230.
 *
 * `CGameGlobalObjects`'s constructor builds one at +0x108 (`bl 80032008` at 0x800084B8) and
 * publishes it as `gpCharacterFactoryBuilder`, so the constructor is boot step 7. What it
 * constructs is a `CSimplePool` whose factory is the `CDummyFactory` in front of it, and that is
 * why the port needs the whole of `CDummyFactory`: the constructor stores its vptr, and a vtable
 * is only defined by a unit that defines its key function - here `Build`.
 *
 *   0x80031E60  0x08  CDummyFactory::CanBuild             li r3,1
 *   0x80031E68  0x08  CDummyFactory::GetResourceIdByName  li r3,0
 *   0x80031E70  0x5C  CDummyFactory::~CDummyFactory       the vtable's copy
 *   0x80031ECC  0x9C  CCharacterFactoryBuilder::GetFactory               - not written
 *   0x80031F68  0x24  the null `rc_ptr<CVParamTransfer>` GetFactory builds - not written
 *   0x80031F8C  0x7C  CCharacterFactoryBuilder::~CCharacterFactoryBuilder
 *   0x80032008  0x54  CCharacterFactoryBuilder::CCharacterFactoryBuilder
 *   0x8003205C  0x04  CDummyFactory::CancelBuild          blr
 *   0x80032060  0x8C  CDummyFactory::BuildAsync
 *   0x800320EC  0x144 CDummyFactory::Build
 *
 * **The definitions are in descending retail order**, which is the order mwcceppc emits them
 * in (`docs/research/decl_order.md`); `tools/check_decl_order.py` fails otherwise.
 *
 * The four functions after 0x80032230 (`CFactoryFnReturn(CCharacterFactory*)`, the
 * `TObjOwnerDerivedFromIObj<CCharacterFactory>` destructor and `GetIObjObjectFor`) are template
 * instances this unit reproduces from `Kyoto/CFactoryMgr.hpp` and `Kyoto/TToken.hpp`; the range
 * stops before them.
 *
 * ## What it costs the port, and what it does not
 *
 * `Build` constructs a `CCharacterFactory`, whose constructor is retail `fn_80030410`: 0x560 bytes
 * and the root of 114 unwritten functions (measured by walking retail's call graph against the
 * port's defined symbols). It is declared in `MetroidPrime/Factories/CCharacterFactory.hpp` and
 * **not written**, so this unit trades `fn_80032008` for one undefined constructor and makes
 * `gpCharacterFactoryBuilder` a real object. Nothing on the boot path calls `Build`: it runs only
 * when something asks the builder's store for an `ANCS`.
 */

#include "MetroidPrime/Factories/CCharacterFactoryBuilder.hpp"

#include "MetroidPrime/Factories/CCharacterFactory.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CFactoryMgr.hpp"

// The merged `.rodata` pool `Build`'s `operator new` passes as its file operand (`lis r3,-32710 ;
// addi r4,r3,25768` at 0x80032154 is 0x803A64A8) - one pool per DOL `.rodata` blob, the same thing
// `CGameStateCtor.cpp` and `CGameGlobalObjectsCtor.cpp` name for theirs. On the host `new` is the
// ordinary one.
#ifdef __MWERKS__
extern "C" const char lbl_803A64A8[];
#define CHARACTER_FACTORY_NEW new (lbl_803A64A8, 0)
#else
#define CHARACTER_FACTORY_NEW new
#endif

// 0x800320EC: the tag's id as an `ANCS`, fetched through `gpSimplePool`'s second virtual
// (`GetObj(const SObjectTag&)`, slot 0xC), locked, and given to a new 0x8C-byte
// `CCharacterFactory` together with `gpSimplePool` itself.
//
// Upstream's `IFactory::Build` returns `rstl::auto_ptr< IObj >`; the object is wrapped through
// `CFactoryFnReturn` (which picks the `TObjOwnerDerivedFromIObj` for the type) and its owner
// handed out. **Since the upstream merge (2026-09-28) this is port code only**: retail's
// two-temporary shape, ranked exact with `tools/try_batch.py` against 0x800320EC, returned a
// `CFactoryFnReturn` and no longer fits the interface.
rstl::auto_ptr< IObj > CCharacterFactoryBuilder::CDummyFactory::Build(const SObjectTag& tag,
                                                                      const CVParamTransfer&) {
  CAssetId id = tag.id;
  TToken< CAnimCharacterSet > ancs(gpSimplePool->GetObj(SObjectTag('ANCS', id)));
  CFactoryFnReturn ret(CHARACTER_FACTORY_NEW CCharacterFactory(
      *gpSimplePool, TLockedToken< CAnimCharacterSet >(ancs), id));
  return ret.GetObjForTransfer();
}

// 0x80032060: `Build` through the vtable (slot 0xC) into a frame temporary, then the pointer
// is handed out and the temporary's ownership flag cleared - `rstl::auto_ptr::release`.
void CCharacterFactoryBuilder::CDummyFactory::BuildAsync(const SObjectTag& tag,
                                                        const CVParamTransfer& xfer, IObj** out) {
  *out = Build(tag, xfer).release();
}

// 0x8003205C.
void CCharacterFactoryBuilder::CDummyFactory::CancelBuild(const SObjectTag&) {}

// 0x80032008: the dummy factory's two vptr stores (IFactory's, then its own), then
// `CSimplePool(IFactory&)` on `this+4` with `this` - the factory is the first member.
CCharacterFactoryBuilder::CCharacterFactoryBuilder() : mDummyStore(mDummyFactory) {}

// 0x80031F8C: the store's destructor (`fn_80300EF4` on `this+4`), then the dummy factory's two
// vptr stores inlined, then `Free` for the deleting form. All of that is what the compiler emits
// for an empty body.
CCharacterFactoryBuilder::~CCharacterFactoryBuilder() {}

// 0x80031E68 and 0x80031E60, `GetResourceIdByName` and `CanBuild`, are inline in upstream's
// header.
