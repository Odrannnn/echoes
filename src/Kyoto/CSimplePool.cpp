#include "Kyoto/CSimplePool.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CResFactory.hpp"

rstl::vector< SObjectTag > CSimplePool::GetReferencedTags() {
  rstl::vector< SObjectTag > tags;
  tags.reserve(mResources.size());
  ResourceMap::iterator it = mResources.begin();
  ResourceMap::iterator end = mResources.end();
  for (; it != end; ++it) {
    tags.push_back_unsafe(it->first);
  }
  return tags;
}

void CSimplePool::DebugDumpPool() const {
  ResourceMap::const_iterator it = mResources.begin();
  for (; it != mResources.end(); ++it) {
  }
}

void CSimplePool::Flush() {}

bool CSimplePool::ObjectIsLive(const SObjectTag& tag) const {
  ResourceMap::const_iterator it = mResources.find(tag);
  if (it == mResources.end()) {
    return false;
  }
  return it->second->IsLoaded();
}

bool CSimplePool::HasObject(const SObjectTag& tag) const {
  ResourceMap::const_iterator it = mResources.find(tag);
  bool result = true;
  if (!(it != mResources.end())) {
    const bool canBuild = mFactory != nullptr && mFactory->CanBuild(tag);
    if (!canBuild) {
      result = false;
    }
  }
  return result;
}

CToken CSimplePool::GetObj(const char* name, const CVParamTransfer& xfer) {
#ifdef TARGET_PC
  // Port: names the stand-in registry answers never reach the factory.
  if (const SObjectTag* standIn = port::pool::FindStandInTag(name)) {
    return CSimplePool::GetObj(*standIn, xfer);
  }
#endif
  const SObjectTag* tag = CSimplePool::GetFactory().GetResourceIdByName(name);
  return CSimplePool::GetObj(*tag, xfer);
}

CToken CSimplePool::GetObj(const char* name) { return CSimplePool::GetObj(name, mParamXfr); }

CToken CSimplePool::GetObj(const SObjectTag& tag) { return CSimplePool::GetObj(tag, mParamXfr); }

CToken CSimplePool::GetObj(const SObjectTag& tag, const CVParamTransfer& xfer) {
  ResourceMap::iterator it = mResources.find(tag);
  if (it != mResources.end()) {
    return CToken(it->second);
  }

#ifdef TARGET_PC
  // Port: seed the reference with a stand-in object where the registry has one.
  CObjectReference* ref = rs_new CObjectReference(
      *this, rstl::auto_ptr< IObj >(port::pool::CreateStandInObject(tag)), tag, xfer);
#else
  CObjectReference* ref =
      rs_new CObjectReference(*this, rstl::auto_ptr< IObj >(nullptr), tag, xfer);
#endif
  ResourceMap::value_type item(tag, ref);
  mResources.insert(item);
  return CToken(ref);
}

void CSimplePool::ObjectUnreferenced(const SObjectTag& tag) {
  ResourceMap::iterator it = mResources.find(tag);
  mResources.erase(it);
}

CSimplePool::~CSimplePool() {
  Flush();
  if (mResources.size() > 0) {
    DebugDumpPool();
  }
}

CSimplePool::CSimplePool(IFactory& factory)
: mFactory(&factory), mParamXfr(CVParamTransfer::Null()) {
  mParamXfr = CVParamTransfer(rs_new TObjOwnerParam< IObjectStore* >(this));
}
