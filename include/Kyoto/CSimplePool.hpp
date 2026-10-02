#ifndef _CSIMPLEPOOL
#define _CSIMPLEPOOL

#include "types.h"

#include "rstl/map.hpp"
#include "rstl/vector.hpp"

#include "Kyoto/CToken.hpp"
#include "Kyoto/IObjectStore.hpp"

class IFactory;

class CSimplePool : public IObjectStore {
  struct TagIdLess {
    bool operator()(const SObjectTag& a, const SObjectTag& b) const { return a.id < b.id; }
  };
  typedef rstl::map< SObjectTag, CObjectReference*, TagIdLess > ResourceMap;

public:
  CSimplePool(IFactory& factory);
  ~CSimplePool();

  void DebugDumpPool() const;
  virtual CToken GetObj(const SObjectTag& tag, const CVParamTransfer& xfer);
  virtual CToken GetObj(const SObjectTag& tag);
  virtual CToken GetObj(const char* name);
  virtual CToken GetObj(const char* name, const CVParamTransfer& xfer);
  virtual bool HasObject(const SObjectTag& tag) const;
  virtual bool ObjectIsLive(const SObjectTag& tag) const;
  virtual IFactory& GetFactory() { return *mFactory; }
  virtual void Flush();
  virtual void ObjectUnreferenced(const SObjectTag& tag);
  rstl::vector< SObjectTag > GetReferencedTags();

  void fn_8029c7e8(const SObjectTag& tag);

private:
  ResourceMap mResources;
  IFactory* mFactory;
  CVParamTransfer mParamXfr;
};
CHECK_SIZEOF(CSimplePool, 0x24)

// `CSimplePool`'s node value is the one pair in the tree that needs a *full* specialization
// rather than an overload in `rstl/pair.hpp`: retail's `create_node` for it (`fn_80301360`,
// 0x70 bytes) copies the 12-byte value straight to `this + 0x10` with the three source loads
// hoisted between the four node stores and no `addic.`/`beq` guard, which is what assignment
// emits and what the guarded placement new of the generic `construct_impl` does not. Spelled as
// a specialization of the one instantiation because adding a candidate to `construct_impl`'s
// overload set in that header moved three unrelated functions' out-of-line copies - one of them
// `CGuiTextPane`'s `vector<SObjectTag>` ctor, from 100% to 45.53% - and this one is the only
// instantiation in the tree that needs it.
namespace rstl {
template <>
inline void construct_impl(void* dest, const pair< SObjectTag, CObjectReference* >& src) {
  *static_cast< pair< SObjectTag, CObjectReference* >* >(dest) = src;
}
} // namespace rstl

extern CSimplePool* gpSimplePool;

#ifdef TARGET_PC
// Port: the stand-in registry in src/MetroidPrime/PortPoolStandIns.cpp. CSimplePool::GetObj and
// CResFactory::GetResourceIdByName consult it before the real resource chain.
class IObj;
namespace port {
namespace pool {
const SObjectTag* FindStandInTag(const char* name);
IObj* CreateStandInObject(const SObjectTag& tag);
} // namespace pool
} // namespace port
#endif

#endif // _CSIMPLEPOOL
