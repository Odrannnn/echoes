#ifndef _CSIMPLEPOOL
#define _CSIMPLEPOOL

#include "types.h"

#include "rstl/hash_map.hpp"
#include "rstl/rc_ptr.hpp"

#include "Kyoto/CToken.hpp"
#include "Kyoto/IObjectStore.hpp"

class IFactory;

// Retail's `CSimplePool` constructor is 0x80301008, `size:0x150`, and it is called exactly once
// in the DOL: `addi r3,r31,228 ; addi r4,r31,4 ; bl 80301008` at 0x800084AC-0x800084B4, i.e. on
// `CGameGlobalObjects`+0xE4 with `&resFactory` at +0x04. Two `stw r0,0(r31)` at the top of
// 0x80301008 are the two vptrs, so **this constructor has to stay declared-only for the same
// reason `CResFactory`'s does** - an inline body puts `__vt__12IObjectStore` and the whole
// `rstl::hash_map` default construction in the caller, 15 instructions where retail has two.
//
// `fn_80301008` is **renamed in `config/G2ME01/symbols.txt`** to that constructor's mangled
// name, so the call mwcceppc emits resolves against the DOL.
class CSimplePool : public IObjectStore {
public:
  CSimplePool(IFactory& factory);
  ~CSimplePool();

  virtual CToken GetObj(const SObjectTag& tag, CVParamTransfer xfer);
  virtual CToken GetObj(const SObjectTag& tag);
  virtual CToken GetObj(const char* name);
  virtual CToken GetObj(const char* name, CVParamTransfer xfer);
  virtual bool HasObject(const SObjectTag& tag);
  virtual bool ObjectIsLive(const SObjectTag& tag);
  virtual IFactory& GetFactory() { return x18_factory; }
  virtual void Flush();
  virtual void ObjectUnreferenced(const SObjectTag& tag);

  void fn_8029c7e8(const SObjectTag& tag);

  typedef rstl::hash_map< SObjectTag, CObjectReference*, void, void > ResourceMap;

private:
  uchar x4_;
  uchar x5_;
  // Keyed by tag, one `CObjectReference` per live resource. Retail's hash and equality functors
  // are unnamed, so they stay `void`; the port's lookup hashes `SObjectTag::id` itself
  // (src/Kyoto/CSimplePoolPort.cpp). Retail's constructor zeroes this member's four words.
  ResourceMap x8_resources;
  IFactory& x18_factory;
  // A `CVParamTransfer`, which is one `rstl::rc_ptr< IVParamObj >`. It used to be declared
  // `rstl::rc_ptr< CVParamTransfer >`, but retail's constructor (0x80301008) stores an 8-byte
  // object with a vtable and a `CSimplePool*` into it - a `TObjOwnerParam< IObjectStore* >(this)`
  // - and `CVParamTransfer` has no vtable of its own. Both are 8 bytes, so the layout is
  // unchanged. See src/Kyoto/CSimplePoolCtor.cpp.
  CVParamTransfer x1c_paramXfr;
};
// 0x24, not 0x20: `x1c_paramXfr` is retail's 8-byte `rstl::rc_ptr`. Measured with mwcceppc's own
// flags into `.data` and read with `objdump -s` - the host compiler's 64-bit `rstl` gives a
// different answer. See docs/research/rc_ptr.md.
CHECK_SIZEOF(CSimplePool, 0x24)

extern CSimplePool* gpSimplePool;

// ---------------------------------------------------------------------------
// Port-only: the object pool's stand-in registry
// ---------------------------------------------------------------------------
//
// `CSimplePool::GetObj(const char*)` resolves a name through the factory, and on a PC there is
// no factory that can: `CResFactory::GetResourceIdByName` returns null because no pak is loaded
// (`src/Kyoto/CResFactoryPortVirtuals.cpp`), and the two statements that dereference the result
// test nothing. Retail does the same - it has `Strings.pak` behind the lookup.
//
// So the port has a registry of named stand-ins, and `src/Kyoto/CSimplePoolPort.cpp` - the port's
// own copy of this class, `configure.py` never claims it - asks it before the factory. The
// registry itself, its ten entries and the reason each object is a stand-in are in
// `src/MetroidPrime/PortPoolStandIns.cpp`; `src/MetroidPrime/PortTweakGlobals.cpp` is the same
// pattern for `gpTweakPlayerA` and says why it is a stand-in there.
//
// **The eight `TXTR_*`/`CMDL_*` names in that registry are why this block exists in its current
// form.** `CCubeRenderer`'s constructor (`fn_80271238`, retail 0x80271238) asks the store for
// them by name and then calls `CToken::GetObj()` on the result, which dereferences `x0_objRef`
// with no null test - so with no pak loaded it faults, and it is retail's own behaviour. The fix
// is upstream: the pool has to have something real under the name. `CreateStandInObject` answers
// per **class** and not per name, because `TLockedToken<T>::GetT()` hands the caller a `T*`.
//
// **This block is declarations only, on purpose.** `include/Kyoto/CSimplePool.hpp` is included
// by `src/Kyoto/CSimplePoolCtor.cpp`, a `Matching` unit, so nothing added here may add an
// `#include`, a member, or anything else that can change a byte of `main.dol`. `IObj` and
// `SObjectTag` are already complete here - `Kyoto/CToken.hpp` pulls in `Kyoto/IObj.hpp`, which
// includes `Kyoto/SObjectTag.hpp` - so the two signatures need nothing further. The ten entries
// and the classes they stand in for are in the registry, which is port-only and mwcceppc never
// sees.
namespace port {
namespace pool {

// The tag retail's factory table would answer for `name`, or null when the registry has never
// heard of it. The returned pointer is to storage with static lifetime, so it is stable.
const SObjectTag* FindStandInTag(const char* name);

// A newly allocated stand-in `IObj` for `tag`, or null when the registry answers the name but
// not the object. **Ownership passes to the caller**: the pool wraps it in the
// `rstl::auto_ptr<IObj>` it hands `CObjectReference`, which deletes it when the last token lets
// go. Each call makes a new object, which is what a load does. The match is on the WHOLE tag -
// `type` and `id` - because the map is keyed on both and one FourCC covers several names.
IObj* CreateStandInObject(const SObjectTag& tag);

} // namespace pool
} // namespace port

#endif // _CSIMPLEPOOL
